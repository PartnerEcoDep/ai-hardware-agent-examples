/**
 * @file main.c
 * @brief 立创·实战派 ESP32-S3 — AI Hardware Agent 入口
 *
 * 启动流程:
 *   1. NVS 初始化
 *   2. I2C 总线 → PCA9557 (LCD_CS / PA_EN)
 *   3. LCD ST7789 320x240 初始化
 *   4. 音频 ES8311+ES7210+I2S 初始化
 *   5. Wi-Fi 连接
 *   6. SNTP 时间同步
 *   7. 注册平台 HAL → 创建 SDK → 启动会话
 *   8. 主循环: 麦克风采集 → SDK → 扬声器播放
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "convai_api.h"
#include "convai_platform_esp32.h"
#include "board_lckfb_szpi.h"
#include "pca9557.h"
#include "audio_codec_lckfb.h"
#include "wifi_provisioning.h"
#include "wifi_prov_ui.h"
#include "ai_chat_ui.h"
#include "voice_config.h"

static const char *TAG = "main";

/* ===================================================================
 *  设备凭证 — 编译前必须修改
 * =================================================================== */
#define DEVICE_PRODUCT_ID      "your_product_id"
#define DEVICE_PRODUCT_KEY     "your_product_key"
#define DEVICE_PRODUCT_SECRET  "your_product_secret"
#define DEVICE_NAME            "esp32s3_lckfb_01"

/* ===================================================================
 *  全局句柄
 * =================================================================== */
static i2c_master_bus_handle_t g_i2c_bus;
static pca9557_t               g_pca9557;
static audio_lckfb_t           g_audio;
esp_lcd_panel_handle_t  g_lcd_panel;
static esp_lcd_panel_io_handle_t g_lcd_io;
static convai_engine_t         g_engine;

/* ===================================================================
 *  按键 (GPIO0, 低电平有效) — 轮询 + 长按/短按区分
 * =================================================================== */
#define LONG_PRESS_MS  1500    /* 长按阈值 1.5s */
#define SHORT_MIN_MS   50      /* 消抖 */

static bool     s_btn_was_down     = false;
static bool     s_long_press_fired = false;
static TickType_t s_btn_press_tick = 0;

static bool button_is_down(void) {
    return gpio_get_level(BOARD_BOOT_BUTTON_GPIO) == 0;
}

static void button_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_BOOT_BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "Button GPIO%d init OK (polling)", BOARD_BOOT_BUTTON_GPIO);
}

/* ===================================================================
 *  PCA9557 功放回调
 * =================================================================== */
static void pa_enable_cb(int en, void *ctx) {
    pca9557_t *pca = (pca9557_t *)ctx;
    pca9557_set_output(pca, PCA9557_BIT_PA_EN, en ? 1 : 0);
}

/* ===================================================================
 *  LCD 初始化 (ST7789 320x240, SPI3, CS 经 PCA9557)
 * =================================================================== */
static int lcd_init(void) {
    /* SPI 总线 */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_MOSI_PIN,
        .miso_io_num     = GPIO_NUM_NC,
        .sclk_io_num     = LCD_CLK_PIN,
        .quadwp_io_num   = GPIO_NUM_NC,
        .quadhd_io_num   = GPIO_NUM_NC,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* Panel IO (CS 不在这里配置, 由 PCA9557 手动控制) */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num       = GPIO_NUM_NC,    /* CS 由 PCA9557 bit0 控制 */
        .dc_gpio_num       = LCD_DC_PIN,
        .spi_mode          = LCD_SPI_MODE,
        .pclk_hz           = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_cfg, &g_lcd_io));

    /* ST7789 驱动 */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(g_lcd_io, &panel_cfg, &g_lcd_panel));

    /* 初始化序列 (与 LCKFB 官方文档 + Zephyr FT6336 版一致) */
    esp_lcd_panel_reset(g_lcd_panel);
    /* 选中 LCD (PCA9557 CS=低), 必须在 reset 之后、init 之前拉低 */
    pca9557_set_output(&g_pca9557, PCA9557_BIT_LCD_CS, 0);
    esp_lcd_panel_init(g_lcd_panel);  /* SLPOUT + MADCTL + COLMOD + RAMCTRL */

    /* ---- 针对 FT6336 版 ST7789 模组的完整厂商初始化 ---- */
    esp_lcd_panel_io_handle_t io = g_lcd_io;
    /* PORCTRL (0xB2):  porch 控制 */
    esp_lcd_panel_io_tx_param(io, 0xB2, (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    /* GCTRL (0xB7):  gate 控制 */
    esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]){0x35}, 1);
    /* VCOMS (0xBB):  VCOM 电压 */
    esp_lcd_panel_io_tx_param(io, 0xBB, (uint8_t[]){0x19}, 1);
    /* LCMCTRL (0xC0):  LCM 控制 */
    esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0x2C}, 1);
    /* VDVVRHEN (0xC2):  VDV/VRH 使能 */
    esp_lcd_panel_io_tx_param(io, 0xC2, (uint8_t[]){0x01}, 1);
    /* VRHS (0xC3):  VRH 电压 */
    esp_lcd_panel_io_tx_param(io, 0xC3, (uint8_t[]){0x12}, 1);
    /* VDVS (0xC4):  VDV 电压 */
    esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0x20}, 1);
    /* PWCTRL1 (0xD0):  电源控制 */
    esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]){0xA4, 0xA1}, 2);
    /* PVGAMCTRL (0xE0):  正 gamma */
    esp_lcd_panel_io_tx_param(io, 0xE0,
        (uint8_t[]){0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
                    0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14);
    /* NVGAMCTRL (0xE1):  负 gamma */
    esp_lcd_panel_io_tx_param(io, 0xE1,
        (uint8_t[]){0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
                    0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14);

    esp_lcd_panel_invert_color(g_lcd_panel, true);
    esp_lcd_panel_swap_xy(g_lcd_panel, true);
    esp_lcd_panel_mirror(g_lcd_panel, true, false);
    /* disp_on_off 移到填充之后调用 (见 app_main), 与官方文档顺序一致 */

    ESP_LOGI(TAG, "LCD ST7789 %dx%d initialized", LCD_WIDTH, LCD_HEIGHT);
    return 0;
}

static void lcd_fill(uint16_t color) {
    /* 简单全屏填色 — 手动字节序转换 (ESP32 小端 → ST7789 大端) */
    uint16_t *buf = (uint16_t *)heap_caps_malloc(
        LCD_WIDTH * 20 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf == NULL) {
        ESP_LOGE(TAG, "lcd_fill: malloc failed");
        return;
    }
    uint16_t swapped = (color << 8) | (color >> 8);
    for (int i = 0; i < LCD_WIDTH * 20; i++) buf[i] = swapped;

    for (int y = 0; y < LCD_HEIGHT; y += 20) {
        int end_y = y + 20;
        if (end_y > LCD_HEIGHT) end_y = LCD_HEIGHT;
        esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, y,
                                                LCD_WIDTH, end_y, buf);
        if (r != ESP_OK) {
            ESP_LOGE(TAG, "lcd_fill draw_bitmap y=%d failed: %d", y, r);
            break;
        }
    }
    free(buf);
}

/* 四色分屏诊断图: 左上红 / 右上绿 / 左下蓝 / 右下白
 * 全黑=数据没到; 色块错位=方向/偏移问题 */
static void lcd_diagnostic(void) {
    const int half_w = LCD_WIDTH / 2;
    const int half_h = LCD_HEIGHT / 2;
    struct { int x0, y0, x1, y1; uint16_t c; const char *name; } q[4] = {
        { 0,       0,       half_w,    half_h,    0xF800, "RED"   },
        { half_w,  0,       LCD_WIDTH, half_h,    0x07E0, "GREEN" },
        { 0,       half_h,  half_w,    LCD_HEIGHT,0x001F, "BLUE"  },
        { half_w,  half_h,  LCD_WIDTH, LCD_HEIGHT,0xFFFF, "WHITE" },
    };
    for (int i = 0; i < 4; i++) {
        uint16_t px = (q[i].c << 8) | (q[i].c >> 8);  /* 大端 */
        int n = (q[i].x1 - q[i].x0) * (q[i].y1 - q[i].y0);
        uint16_t *buf = (uint16_t *)heap_caps_malloc(
            n * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (buf == NULL) { ESP_LOGE(TAG, "diag %s malloc fail", q[i].name); continue; }
        for (int k = 0; k < n; k++) buf[k] = px;
        esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel,
                         q[i].x0, q[i].y0, q[i].x1, q[i].y1, buf);
        ESP_LOGI(TAG, "diag %s draw_bitmap -> %d", q[i].name, r);
        free(buf);
    }
}

/* 用色块显示启动状态 */
static void lcd_show_status(int step) {
    static const uint16_t colors[] = {
        0xF800, 0xF840, 0xFC00, 0x07E0,
        0x07FF, 0x001F, 0x7FFF, 0xFFFF,
    };
    lcd_fill(colors[step % 8]);
}

/* 在屏幕上画几个色块 (测试图案) */
static void lcd_test_pattern(void) {
    uint16_t *buf = (uint16_t *)heap_caps_malloc(
        LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf == NULL) { lcd_fill(0xFFFF); return; }

    /* 左半屏红色, 右半屏绿色 */
    for (int y = 0; y < LCD_HEIGHT; y++) {
        for (int x = 0; x < LCD_WIDTH; x++) {
            uint16_t c;
            if (y < LCD_HEIGHT / 3)        c = 0xF800; /* 红 */
            else if (y < LCD_HEIGHT * 2/3) c = 0x07E0; /* 绿 */
            else                           c = 0x001F; /* 蓝 */
            /* RGB565 大端字节序 */
            buf[y * LCD_WIDTH + x] = (c >> 8) | (c << 8);
        }
    }
    esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, buf);
    free(buf);
}

/* ===================================================================
 *  背光 PWM (LEDC)
 * =================================================================== */
static void backlight_init(void) {
    /* 背光低电平有效: 直接 GPIO 拉低, 先不用 PWM */
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << LCD_BACKLIGHT_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(LCD_BACKLIGHT_PIN, 0);  /* 低电平 = 开启背光 */
    ESP_LOGI(TAG, "Backlight ON (GPIO%d LOW)", LCD_BACKLIGHT_PIN);
}

/* ===================================================================
 *  LED 指示
 * =================================================================== */
static void board_led_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(BOARD_LED_GPIO, 0);  /* 初始关闭 */
}

static void board_led_set(int on) {
    gpio_set_level(BOARD_LED_GPIO, on ? 1 : 0);
}

/* ===================================================================
 *  I2C 总线初始化
 * =================================================================== */
static int i2c_bus_init(void) {
    i2c_master_bus_config_t cfg = {
        .i2c_port          = BOARD_I2C_PORT,
        .sda_io_num        = BOARD_I2C_SDA_PIN,
        .scl_io_num        = BOARD_I2C_SCL_PIN,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags             = { .enable_internal_pullup = 1 },
    };
    esp_err_t ret = i2c_new_master_bus(&cfg, &g_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %d", ret);
        return -1;
    }
    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)",
             BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
    return 0;
}

/* ===================================================================
 *  SNTP
 * =================================================================== */
static void sntp_init_sync(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_init();

    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    ESP_LOGI(TAG, "SNTP %s", retry < 100 ? "synced" : "timeout");
}

/* ===================================================================
 *  SDK 事件回调
 * =================================================================== */
static void on_sdk_event(convai_engine_t engine,
                         convai_event_t *event, void *user_data) {
    const char *names[] = {
        [CONVAI_EV_CONNECTED]    = "CONNECTED",
        [CONVAI_EV_DISCONNECTED] = "DISCONNECTED",
        [CONVAI_EV_FAILED]       = "FAILED",
        [CONVAI_EV_UPDATED]      = "UPDATED",
    };
    ESP_LOGI(TAG, "SDK event: %s", names[event->code]);

    switch (event->code) {
    case CONVAI_EV_CONNECTED:
        ai_chat_ui_set_cloud_connection(true);
        break;
    case CONVAI_EV_DISCONNECTED:
    case CONVAI_EV_FAILED:
        ai_chat_ui_set_cloud_connection(false);
        break;
    default:
        break;
    }
}

static void on_conversation_status(convai_engine_t engine,
                                   convai_status_e status, void *user_data) {
    const char *names[] = {
        [CONVAI_STATUS_IDLE]       = "IDLE",
        [CONVAI_STATUS_LISTENING]  = "LISTENING",
        [CONVAI_STATUS_THINKING]   = "THINKING",
        [CONVAI_STATUS_ANSWERING]  = "ANSWERING",
        [CONVAI_STATUS_INTERRUPTED] = "INTERRUPTED",
        [CONVAI_STATUS_ANSWER_FINISHED] = "ANSWER_FINISHED",
    };
    ESP_LOGI(TAG, "Status: %s", names[status]);

    switch (status) {
    case CONVAI_STATUS_LISTENING:
        board_led_set(1);
        ai_chat_ui_set_state(CHAT_LISTENING);
        break;
    case CONVAI_STATUS_THINKING:
        board_led_set(1);
        ai_chat_ui_set_state(CHAT_THINKING);
        break;
    case CONVAI_STATUS_ANSWERING:
        board_led_set(0);
        ai_chat_ui_set_state(CHAT_SPEAKING);
        break;
    case CONVAI_STATUS_IDLE:
    case CONVAI_STATUS_ANSWER_FINISHED:
    default:
        board_led_set(0);
        ai_chat_ui_set_state(CHAT_IDLE);
        break;
    }
}

static void on_audio_data(convai_engine_t engine,
                          const void *data, size_t len,
                          const convai_audio_frame_info_t *info,
                          void *user_data) {
    audio_lckfb_playback(&g_audio, (const uint8_t *)data, len, NULL);
}

static void on_message_data(convai_engine_t engine,
                            const void *data, size_t len,
                            const convai_message_info_t *info,
                            void *user_data) {
    ESP_LOGI(TAG, "Message: %.*s", (int)len, (const char *)data);

    char *text = strndup((const char *)data, len);
    if (text) {
        ai_chat_ui_add_message(text, false);  /* assistant 消息 */
        free(text);
    }
}

/* ===================================================================
 *  音频采集任务 (麦克风 → SDK)
 * =================================================================== */
#define CAPTURE_BUF_SIZE (AUDIO_SAMPLE_RATE * 2 / 50)  /* 20ms G.711 帧 */

static void audio_capture_task(void *arg) {
    uint8_t *buf = (uint8_t *)malloc(CAPTURE_BUF_SIZE);
    if (buf == NULL) { vTaskDelete(NULL); return; }

    while (1) {
        size_t received;
        int ret = audio_lckfb_capture(&g_audio, buf, CAPTURE_BUF_SIZE, &received);
        if (ret == 0 && received > 0 && g_engine) {
            convai_audio_frame_info_t info = { .data_type = CONVAI_AUDIO_DATA_TYPE_G711A };
            convai_send_audio(g_engine, buf, received, &info);
        }
        vTaskDelay(pdMS_TO_TICKS(10));  /* 每 10ms 采集一次 */
    }
    free(buf);
    vTaskDelete(NULL);
}

/* ===================================================================
 *  WiFi 事件回调 — 驱动状态栏更新
 * =================================================================== */
static void on_wifi_event(wifi_prov_event_t event) {
    switch (event) {
    case WIFI_PROV_EV_CONNECTED:
        ai_chat_ui_set_connection(
            wifi_prov_get_ssid(),
            wifi_prov_get_ip(),
            true);
        break;
    case WIFI_PROV_EV_DISCONNECTED:
        ai_chat_ui_set_connection("", "", false);
        break;
    }
}

/* ===================================================================
 *  app_main — 主入口
 * =================================================================== */
void app_main(void) {
    printf("\n=== ESP32-S3 Step-by-step Init ===\n\n");

    /* GPIO48 LED */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << 48),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(48, 1);
    printf("[1/8] GPIO OK\n"); fflush(stdout);

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    printf("[2/8] NVS OK\n"); fflush(stdout);

    /* I2C */
    if (i2c_bus_init() != 0) {
        printf("[3/8] I2C FAILED — skipping hardware\n");
        goto skip_hw;
    }
    printf("[3/8] I2C OK\n"); fflush(stdout);

    /* PCA9557 */
    if (pca9557_init(&g_pca9557, g_i2c_bus, PCA9557_I2C_ADDR) != 0) {
        printf("[4/8] PCA9557 FAILED — LCD/audio won't work\n");
        goto skip_hw;
    }
    printf("[4/8] PCA9557 OK\n"); fflush(stdout);

    /* LCD */
    printf("[5/8] LCD init...\n"); fflush(stdout);
    lcd_init();
    lcd_fill(0x0000);                                      /* 先填黑 */
    esp_lcd_panel_disp_on_off(g_lcd_panel, true);          /* 再开显示 */
    backlight_init();
    printf("[5/8] LCD OK\n"); fflush(stdout);

    /* Audio */
    printf("[6/8] Audio init...\n"); fflush(stdout);
    if (audio_lckfb_init(&g_audio, g_i2c_bus, pa_enable_cb, &g_pca9557) != 0) {
        printf("[6/8] Audio FAILED\n");
    } else {
        printf("[6/8] Audio OK\n");
    }
    fflush(stdout);

    /* WiFi */
    printf("[7/8] WiFi init...\n"); fflush(stdout);
    wifi_prov_register_callback(on_wifi_event);
    wifi_prov_ui_run();
    printf("[7/8] WiFi OK\n"); fflush(stdout);

    /* HAL + SDK */
    printf("[8/8] HAL register...\n"); fflush(stdout);
    convai_platform_esp32_init();

    char config_json[256];
    snprintf(config_json, sizeof(config_json),
             "{\"product_id\":\"%s\",\"product_key\":\"%s\","
             "\"product_secret\":\"%s\",\"device_name\":\"%s\"}",
             DEVICE_PRODUCT_ID, DEVICE_PRODUCT_KEY,
             DEVICE_PRODUCT_SECRET, DEVICE_NAME);

    convai_event_handler_t handler = {
        .on_convai_event               = on_sdk_event,
        .on_convai_conversation_status = on_conversation_status,
        .on_convai_audio_data          = on_audio_data,
        .on_convai_message_data        = on_message_data,
    };

    ret = convai_create(&g_engine, config_json, &handler, NULL);
    if (ret != CONVAI_OK) {
        printf("[8/8] SDK create FAILED: %d\n", ret);
    } else {
        convai_opt_t opt = { .mode = CONVAI_MODE_WS };
        convai_start(g_engine, &opt);
        printf("[8/8] SDK started (v%s)\n", convai_get_version());
        xTaskCreate(audio_capture_task, "audio_cap", 4096, NULL, 5, NULL);
    }
    fflush(stdout);

skip_hw:
    /* AI Chat UI — 配网完成后进入空闲状态 */
    ai_chat_ui_init();

    /* 音色配置 — 从 NVS 恢复上次选择 */
    voice_config_init();

    /* 更新状态栏：WiFi 连接信息 */
    ai_chat_ui_set_connection(
        wifi_prov_get_ssid(),
        wifi_prov_get_ip(),
        wifi_prov_is_connected());
    printf("AI Chat UI ready (IDLE)\n");

    /* 自定义按键 (GPIO0) 初始化 */
    button_init();

    printf("\n=== Init complete — press custom key to start AI conversation ===\n");
    gpio_set_level(48, 0);

    while (1) {
        bool down = button_is_down();
        TickType_t now = xTaskGetTickCount();

        if (down && !s_btn_was_down) {
            /* 按键刚按下 */
            s_btn_press_tick = now;
            s_long_press_fired = false;

        } else if (down && s_btn_was_down) {
            /* 持续按住 — 长按检测 */
            if (!s_long_press_fired &&
                (now - s_btn_press_tick) * portTICK_PERIOD_MS >= LONG_PRESS_MS) {

                s_long_press_fired = true;

                if (ai_chat_ui_get_state() == CHAT_IDLE) {
                    /* IDLE 长按 → 打开音色面板 */
                    ESP_LOGI(TAG, "Long press → voice selector");
                    ai_chat_ui_show_voice_selector(true);
                } else if (ai_chat_ui_get_state() == CHAT_VOICE_SELECT) {
                    /* 音色面板内长按 → 确认并关闭 */
                    int idx = ai_chat_ui_voice_select_get();
                    ESP_LOGI(TAG, "Voice confirmed: #%d", idx);
                    voice_config_set(g_engine, idx);
                    ai_chat_ui_show_voice_selector(false);
                }
            }

        } else if (!down && s_btn_was_down) {
            /* 按键松开 — 短按 */
            if (!s_long_press_fired &&
                (now - s_btn_press_tick) * portTICK_PERIOD_MS >= SHORT_MIN_MS) {

                if (ai_chat_ui_get_state() == CHAT_IDLE) {
                    /* IDLE 短按 → 启动对话 */
                    ESP_LOGI(TAG, "Short press → start conversation");
                    ai_chat_ui_set_state(CHAT_LISTENING);
                } else if (ai_chat_ui_get_state() == CHAT_VOICE_SELECT) {
                    /* 音色面板内短按 → 下一项 */
                    ai_chat_ui_voice_select_next();
                }
            }
        }

        s_btn_was_down = down;
        ai_chat_ui_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

#if 0
    /* ---- 1. NVS ---- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    /* ---- 2. I2C 总线 ---- */
    if (i2c_bus_init() != 0) return;

    /* ---- 3. PCA9557 IO 扩展 ---- */
    if (pca9557_init(&g_pca9557, g_i2c_bus, PCA9557_I2C_ADDR) != 0) {
        ESP_LOGE(TAG, "PCA9557 init failed — LCD/audio won't work");
        return;
    }

    /* ---- 4. LED ---- */
    board_led_init();
    board_led_set(1);  /* 启动指示 */

    /* ---- 5. LCD ---- */
    lcd_init();
    backlight_init();
    lcd_show_status(0);  /* 红色: 开始启动 */
    ESP_LOGI(TAG, "Display ready");

    /* ---- 6. 音频 ---- */
    lcd_show_status(1);  /* 橙色: 音频初始化中 */
    if (audio_lckfb_init(&g_audio, g_i2c_bus, pa_enable_cb, &g_pca9557) != 0) {
        ESP_LOGW(TAG, "Audio init failed — voice unavailable");
    }
    audio_lckfb_set_volume(&g_audio, 70);

    /* ---- 7. Wi-Fi ---- */
    lcd_show_status(2);  /* 黄色: WiFi 连接中 */
    wifi_prov_init();
    wifi_prov_wait_connected();

    /* ---- 8. SNTP ---- */
    lcd_show_status(3);  /* 绿色: 时间同步中 */
    sntp_init_sync();

    /* ---- 9. 平台 HAL 注册 ---- */
    lcd_show_status(4);  /* 青色: HAL 注册中 */
    if (convai_platform_esp32_init() != 0) {
        ESP_LOGE(TAG, "HAL registration failed");
        lcd_fill(0xF800);  /* 红色报错 */
        return;
    }

    /* ---- 10. SDK 引擎 ---- */
    lcd_show_status(5);  /* 蓝色: SDK 创建中 */
    char config_json[512];
    snprintf(config_json, sizeof(config_json),
             "{\"product_id\":\"%s\",\"product_key\":\"%s\","
             "\"product_secret\":\"%s\",\"device_name\":\"%s\"}",
             DEVICE_PRODUCT_ID, DEVICE_PRODUCT_KEY,
             DEVICE_PRODUCT_SECRET, DEVICE_NAME);

    convai_event_handler_t handler = {
        .on_convai_event               = on_sdk_event,
        .on_convai_conversation_status = on_conversation_status,
        .on_convai_audio_data          = on_audio_data,
        .on_convai_message_data        = on_message_data,
    };

    ret = convai_create(&g_engine, config_json, &handler, NULL);
    if (ret != CONVAI_OK) {
        ESP_LOGE(TAG, "convai_create failed: %d (%s)", ret, convai_err_2_str(ret));
        return;
    }
    ESP_LOGI(TAG, "SDK engine created (v%s)", convai_get_version());

    /* ---- 11. 启动会话 ---- */
    convai_opt_t opt = { .mode = CONVAI_MODE_WS };
    ret = convai_start(g_engine, &opt);
    if (ret != CONVAI_OK) {
        ESP_LOGE(TAG, "convai_start failed: %d", ret);
        lcd_fill(0xF800);  /* 红色: 启动失败 */
        convai_destroy(g_engine);
        return;
    }
    ESP_LOGI(TAG, "Session started");

    /* ---- 12. 启动音频采集 ---- */
    xTaskCreate(audio_capture_task, "audio_cap", 4096, NULL, 5, NULL);

    /* ---- 13. 主循环 ---- */
    board_led_set(0);
    lcd_fill(0xFFFF);  /* 纯白 — 字节序无关, 能亮说明 SPI 通 */
    printf("LCD filled with WHITE (0xFFFF)\n"); fflush(stdout);
    ESP_LOGI(TAG, "Ready — press BOOT button or speak to interact");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
#endif
}
