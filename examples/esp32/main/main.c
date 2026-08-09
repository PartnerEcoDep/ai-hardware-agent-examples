/**
 * @file main.c
 * @brief LCKFB ESP32-S3 - AI Hardware Agent entry point
 *
 * Boot sequence:
 * 1. NVS init
 * 2. I2C bus -> PCA9557 (LCD_CS / PA_EN)
 * 3. LCD ST7789 320x240 init
 * 4. Audio: ES8311 + ES7210 + I2S init
 * 5. Wi-Fi connect
 * 6. SNTP time sync
 * 7. Register platform HAL -> create SDK -> start session
 * 8. Main loop: mic capture -> SDK -> speaker playback
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
#include "lvgl_port.h"
#include "voice_config.h"

static const char *TAG = "main";

/* ===================================================================
 * ? ? ? ? ?
 * =================================================================== */
#define DEVICE_PRODUCT_ID "your_product_id"
#define DEVICE_PRODUCT_KEY "your_product_key"
#define DEVICE_PRODUCT_SECRET "your_product_secret"
#define DEVICE_NAME "esp32s3_lckfb_01"

/* ===================================================================
 * ? ?
 * =================================================================== */
static i2c_master_bus_handle_t g_i2c_bus;
static pca9557_t g_pca9557;
static audio_lckfb_t g_audio;
esp_lcd_panel_handle_t g_lcd_panel;
esp_lcd_panel_io_handle_t g_lcd_io; /* lvgl_port.c ? extern ? */
static convai_engine_t g_engine;

/* ===================================================================
 * ? (GPIO0, ? ? + ?/?
 * =================================================================== */
#define LONG_PRESS_MS 1500 /* ?1.5s */
#define SHORT_MIN_MS 50    /* ? */

static bool s_btn_was_down = false;
static bool s_long_press_fired = false;
static TickType_t s_btn_press_tick = 0;

static bool button_is_down(void)
{
    return gpio_get_level(BOARD_BOOT_BUTTON_GPIO) == 0;
}

static void button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "Button GPIO%d init OK (polling)", BOARD_BOOT_BUTTON_GPIO);
}

/* ===================================================================
 * PCA9557 ?
 * =================================================================== */
static void pa_enable_cb(int en, void *ctx)
{
    pca9557_t *pca = (pca9557_t *)ctx;
    pca9557_set_output(pca, PCA9557_BIT_PA_EN, en ? 1 : 0);
}

/* ===================================================================
 * LCD ?(ST7789 320x240, SPI3, CS ?PCA9557)
 * =================================================================== */
static int lcd_init(void)
{
    /* SPI ? */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = LCD_CLK_PIN,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_WIDTH * 40 * 2, /* ?40 ?= 25600 bytes, ? ?DMA pool */
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* Panel IO (CS ?, ?PCA9557 ? ?) */
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = GPIO_NUM_NC, /* CS ?PCA9557 bit0 ? */
        .dc_gpio_num = LCD_DC_PIN,
        .spi_mode = LCD_SPI_MODE,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_cfg, &g_lcd_io));

    /* ST7789 ? */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = GPIO_NUM_NC,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE, /* LVGL ?, ST7789 ? ?*/
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(g_lcd_io, &panel_cfg, &g_lcd_panel));

    /* ? ?(?LCKFB ? ? ? + Zephyr FT6336 ? */
    esp_lcd_panel_reset(g_lcd_panel);
    /* ? LCD (PCA9557 CS=?, ?reset ?it ? */
    pca9557_set_output(&g_pca9557, PCA9557_BIT_LCD_CS, 0);
    esp_lcd_panel_init(g_lcd_panel); /* SLPOUT + MADCTL + COLMOD + RAMCTRL */

    /* ---- ? FT6336 ?ST7789 ? ---- */
    esp_lcd_panel_io_handle_t io = g_lcd_io;
    /* PORCTRL (0xB2): porch ? */
    esp_lcd_panel_io_tx_param(io, 0xB2, (uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    /* GCTRL (0xB7): gate ? */
    esp_lcd_panel_io_tx_param(io, 0xB7, (uint8_t[]){0x35}, 1);
    /* VCOMS (0xBB): VCOM ? ? */
    esp_lcd_panel_io_tx_param(io, 0xBB, (uint8_t[]){0x19}, 1);
    /* LCMCTRL (0xC0): LCM ? */
    esp_lcd_panel_io_tx_param(io, 0xC0, (uint8_t[]){0x2C}, 1);
    /* VDVVRHEN (0xC2): VDV/VRH ? */
    esp_lcd_panel_io_tx_param(io, 0xC2, (uint8_t[]){0x01}, 1);
    /* VRHS (0xC3): VRH ? ? */
    esp_lcd_panel_io_tx_param(io, 0xC3, (uint8_t[]){0x12}, 1);
    /* VDVS (0xC4): VDV ? ? */
    esp_lcd_panel_io_tx_param(io, 0xC4, (uint8_t[]){0x20}, 1);
    /* PWCTRL1 (0xD0): ? ? */
    esp_lcd_panel_io_tx_param(io, 0xD0, (uint8_t[]){0xA4, 0xA1}, 2);
    /* PVGAMCTRL (0xE0): ?gamma */
    esp_lcd_panel_io_tx_param(io, 0xE0,
                              (uint8_t[]){0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
                                          0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23},
                              14);
    /* NVGAMCTRL (0xE1): ?gamma */
    esp_lcd_panel_io_tx_param(io, 0xE1,
                              (uint8_t[]){0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
                                          0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23},
                              14);

    esp_lcd_panel_invert_color(g_lcd_panel, true);
    esp_lcd_panel_swap_xy(g_lcd_panel, true);
    esp_lcd_panel_mirror(g_lcd_panel, true, false);
    /* disp_on_off ? ? (?app_main), ?*/

    ESP_LOGI(TAG, "LCD ST7789 %dx%d initialized", LCD_WIDTH, LCD_HEIGHT);
    return 0;
}

static void lcd_fill(uint16_t color)
{
    /* ? ?(ESP32 ? ?ST7789 ?) */
    uint16_t *buf = (uint16_t *)heap_caps_malloc(
        LCD_WIDTH * 20 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "lcd_fill: malloc failed");
        return;
    }
    uint16_t swapped = (color << 8) | (color >> 8);
    for (int i = 0; i < LCD_WIDTH * 20; i++)
        buf[i] = swapped;

    for (int y = 0; y < LCD_HEIGHT; y += 20)
    {
        int end_y = y + 20;
        if (end_y > LCD_HEIGHT)
            end_y = LCD_HEIGHT;
        esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, y,
                                                LCD_WIDTH, end_y, buf);
        if (r != ESP_OK)
        {
            ESP_LOGE(TAG, "lcd_fill draw_bitmap y=%d failed: %d", y, r);
            break;
        }
    }
    free(buf);
}

/* ? ? ? ?/ ?/ ?/ ?
 * ?=? ?; ?=?/? */
static void lcd_diagnostic(void)
{
    const int half_w = LCD_WIDTH / 2;
    const int half_h = LCD_HEIGHT / 2;
    struct
    {
        int x0, y0, x1, y1;
        uint16_t c;
        const char *name;
    } q[4] = {
        {0, 0, half_w, half_h, 0xF800, "RED"},
        {half_w, 0, LCD_WIDTH, half_h, 0x07E0, "GREEN"},
        {0, half_h, half_w, LCD_HEIGHT, 0x001F, "BLUE"},
        {half_w, half_h, LCD_WIDTH, LCD_HEIGHT, 0xFFFF, "WHITE"},
    };
    for (int i = 0; i < 4; i++)
    {
        uint16_t px = (q[i].c << 8) | (q[i].c >> 8); /* ? */
        int n = (q[i].x1 - q[i].x0) * (q[i].y1 - q[i].y0);
        uint16_t *buf = (uint16_t *)heap_caps_malloc(
            n * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (buf == NULL)
        {
            ESP_LOGE(TAG, "diag %s malloc fail", q[i].name);
            continue;
        }
        for (int k = 0; k < n; k++)
            buf[k] = px;
        esp_err_t r = esp_lcd_panel_draw_bitmap(g_lcd_panel,
                                                q[i].x0, q[i].y0, q[i].x1, q[i].y1, buf);
        ESP_LOGI(TAG, "diag %s draw_bitmap -> %d", q[i].name, r);
        free(buf);
    }
}

/* ? ? ?*/
static void lcd_show_status(int step)
{
    static const uint16_t colors[] = {
        0xF800,
        0xF840,
        0xFC00,
        0x07E0,
        0x07FF,
        0x001F,
        0x7FFF,
        0xFFFF,
    };
    lcd_fill(colors[step % 8]);
}

/* ? ? ?(?) */
static void lcd_test_pattern(void)
{
    uint16_t *buf = (uint16_t *)heap_caps_malloc(
        LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buf == NULL)
    {
        lcd_fill(0xFFFF);
        return;
    }

    /* ? ?*/
    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        for (int x = 0; x < LCD_WIDTH; x++)
        {
            uint16_t c;
            if (y < LCD_HEIGHT / 3)
                c = 0xF800; /* ?*/
            else if (y < LCD_HEIGHT * 2 / 3)
                c = 0x07E0; /* ?*/
            else
                c = 0x001F; /* ?*/
            /* RGB565 ?*/
            buf[y * LCD_WIDTH + x] = (c >> 8) | (c << 8);
        }
    }
    esp_lcd_panel_draw_bitmap(g_lcd_panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, buf);
    free(buf);
}

/* ===================================================================
 * ? PWM (LEDC)
 * =================================================================== */
static void backlight_init(void)
{
    /* ? ? ? GPIO ?, ?PWM */
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << LCD_BACKLIGHT_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_cfg);
    gpio_set_level(LCD_BACKLIGHT_PIN, 0); /* ?= ?*/
    ESP_LOGI(TAG, "Backlight ON (GPIO%d LOW)", LCD_BACKLIGHT_PIN);
}

/* ===================================================================
 * LED ?
 * =================================================================== */
static void board_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BOARD_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(BOARD_LED_GPIO, 0); /* ? */
}

static void board_led_set(int on)
{
    gpio_set_level(BOARD_LED_GPIO, on ? 1 : 0);
}

/* ===================================================================
 * I2C ?
 * =================================================================== */
static int i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_SDA_PIN,
        .scl_io_num = BOARD_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {.enable_internal_pullup = 1},
    };
    esp_err_t ret = i2c_new_master_bus(&cfg, &g_i2c_bus);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C bus init failed: %d", ret);
        return -1;
    }
    ESP_LOGI(TAG, "I2C bus initialized (SDA=%d, SCL=%d)",
             BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
    return 0;
}

/* ===================================================================
 * SNTP
 * =================================================================== */
static void sntp_init_sync(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_init();

    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 100)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    ESP_LOGI(TAG, "SNTP %s", retry < 100 ? "synced" : "timeout");
}

/* ===================================================================
 * SDK ? ?
 * =================================================================== */
static void on_sdk_event(convai_engine_t engine,
                         convai_event_t *event, void *user_data)
{
    const char *names[] = {
        [CONVAI_EV_CONNECTED] = "CONNECTED",
        [CONVAI_EV_DISCONNECTED] = "DISCONNECTED",
        [CONVAI_EV_FAILED] = "FAILED",
        [CONVAI_EV_UPDATED] = "UPDATED",
    };
    ESP_LOGI(TAG, "SDK event: %s", names[event->code]);

    switch (event->code)
    {
    case CONVAI_EV_CONNECTED:
        /* ?ai_chat_ui_set_connection ? */
        break;
    case CONVAI_EV_DISCONNECTED:
    case CONVAI_EV_FAILED:
        /* ?ai_chat_ui_set_connection ? */
        break;
    default:
        break;
    }
}

static void on_conversation_status(convai_engine_t engine,
                                   convai_status_e status, void *user_data)
{
    const char *names[] = {
        [CONVAI_STATUS_IDLE] = "IDLE",
        [CONVAI_STATUS_LISTENING] = "LISTENING",
        [CONVAI_STATUS_THINKING] = "THINKING",
        [CONVAI_STATUS_ANSWERING] = "ANSWERING",
        [CONVAI_STATUS_INTERRUPTED] = "INTERRUPTED",
        [CONVAI_STATUS_ANSWER_FINISHED] = "ANSWER_FINISHED",
    };
    ESP_LOGI(TAG, "Status: %s", names[status]);

    switch (status)
    {
    case CONVAI_STATUS_LISTENING:
        board_led_set(1);
        ai_chat_ui_set_state(STATE_LISTENING);
        break;
    case CONVAI_STATUS_THINKING:
        board_led_set(1);
        ai_chat_ui_set_state(STATE_THINKING);
        break;
    case CONVAI_STATUS_ANSWERING:
        board_led_set(0);
        ai_chat_ui_set_state(STATE_SPEAKING);
        break;
    case CONVAI_STATUS_IDLE:
    case CONVAI_STATUS_ANSWER_FINISHED:
    default:
        board_led_set(0);
        ai_chat_ui_set_state(STATE_IDLE);
        break;
    }
}

static void on_audio_data(convai_engine_t engine,
                          const void *data, size_t len,
                          const convai_audio_frame_info_t *info,
                          void *user_data)
{
    audio_lckfb_playback(&g_audio, (const uint8_t *)data, len, NULL);
}

static void on_message_data(convai_engine_t engine,
                            const void *data, size_t len,
                            const convai_message_info_t *info,
                            void *user_data)
{
    ESP_LOGI(TAG, "Message: %.*s", (int)len, (const char *)data);

    /* ? ? SDK ? JSON ? stub?
    ?assistant ? ?VOICE ?
    ? ?*/
    (void)data;
    (void)len;
}

/* ===================================================================
 * G.711 A-law decode + audio level
 * ===================================================================
 * Standard ITU-T G.711 A-law -> 16-bit linear PCM.
 * compute_audio_level() returns a 0..100 smoothed peak level
 * for the G.711 A-law frame, suitable for driving the listening UI.
 */
static int16_t alaw_to_pcm(uint8_t a)
{
    a ^= 0x55;
    int sign = a & 0x80;
    int seg = (a >> 4) & 0x07;
    int low = a & 0x0F;
    int16_t pcm;
    if (seg == 0)
    {
        pcm = (int16_t)((low << 4) | 0x008);
    }
    else
    {
        pcm = (int16_t)((low + 0x10) << (seg + 3));
    }
    return sign ? (int16_t)-pcm : pcm;
}

static uint8_t compute_audio_level(const uint8_t *g711a, size_t n)
{
    static uint8_t smooth = 0;
    if (n == 0)
        return 0;
    int16_t peak = 0;
    for (size_t i = 0; i < n; i++)
    {
        int16_t s = alaw_to_pcm(g711a[i]);
        if (s < 0)
            s = (int16_t)-s;
        if (s > peak)
            peak = s;
    }
    uint8_t v = (uint8_t)(peak * 100 / 32767);
    /* EMA: 0.6 prev + 0.4 new */
    smooth = (uint8_t)(smooth * 6 / 10 + v * 4 / 10);
    return smooth;
}
/* ===================================================================
 * ? (?SDK)
 * =================================================================== */
#define CAPTURE_BUF_SIZE (AUDIO_SAMPLE_RATE * 2 / 50) /* 20ms G.711 ?*/

static void audio_capture_task(void *arg)
{
    uint8_t *buf = (uint8_t *)malloc(CAPTURE_BUF_SIZE);
    if (buf == NULL)
    {
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        size_t received;
        int ret = audio_lckfb_capture(&g_audio, buf, CAPTURE_BUF_SIZE, &received);
        if (ret == 0 && received > 0 && g_engine)
        {
            convai_audio_frame_info_t info = {.data_type = CONVAI_AUDIO_DATA_TYPE_G711A};
            ai_chat_ui_update_volume(compute_audio_level(buf, received));
            convai_send_audio(g_engine, buf, received, &info);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); /* ?10ms ? ? ?*/
    }
    free(buf);
    vTaskDelete(NULL);
}

/* ===================================================================
 * WiFi ? ? ? ?
 * =================================================================== */
#if 0  /* ? ?WiFi ? ?LVGL ? crash */
static void on_wifi_event(wifi_prov_event_t event) {
 switch (event) {
 case WIFI_PROV_EV_CONNECTED:
 ai_chat_ui_set_network(true);
 break;
 case WIFI_PROV_EV_DISCONNECTED:
 ai_chat_ui_set_network(false);
 break;
 }
}
#endif /* ? WiFi ? ? */

/* ===================================================================
 * app_main ?
 * =================================================================== */
void app_main(void)
{
    printf("\n=== ESP32-S3 Step-by-step Init ===\n\n");

    /* GPIO48 LED */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << 48),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(48, 1);
    printf("[1/8] GPIO OK\n");
    fflush(stdout);

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }
    printf("[2/8] NVS OK\n");
    fflush(stdout);

    /* I2C */
    if (i2c_bus_init() != 0)
    {
        printf("[3/8] I2C FAILED ?skipping hardware\n");
        goto skip_hw;
    }
    printf("[3/8] I2C OK\n");
    fflush(stdout);

    /* PCA9557 */
    if (pca9557_init(&g_pca9557, g_i2c_bus, PCA9557_I2C_ADDR) != 0)
    {
        printf("[4/8] PCA9557 FAILED ?LCD/audio won't work\n");
        goto skip_hw;
    }
    printf("[4/8] PCA9557 OK\n");
    fflush(stdout);

    /* LCD */
    printf("[5/8] LCD init...\n");
    fflush(stdout);
    lcd_init();
    lcd_fill(0x0000);                             /* ?*/
    esp_lcd_panel_disp_on_off(g_lcd_panel, true); /* ? ? */
    backlight_init();
    printf("[5/8] LCD OK\n");
    fflush(stdout);

    /* Audio */
    printf("[6/8] Audio init...\n");
    fflush(stdout);
    if (audio_lckfb_init(&g_audio, g_i2c_bus, pa_enable_cb, &g_pca9557) != 0)
    {
        printf("[6/8] Audio FAILED\n");
    }
    else
    {
        printf("[6/8] Audio OK\n");
    }
    fflush(stdout);

    /* WiFi */
    printf("[7/8] WiFi init...\n");
    fflush(stdout);
    wifi_prov_ui_run();
    printf("[7/8] WiFi OK\n");
    fflush(stdout);

    /* LVGL + UI ?SDK ? ?SDK ? ? UI ? */
    lvgl_port_init();
    lvgl_port_touch_init(g_i2c_bus);
    ai_chat_ui_init();
    voice_config_init();
    ai_chat_ui_set_network(wifi_prov_is_connected());
    printf("AI Chat UI ready (IDLE)\n");
    fflush(stdout);

    /* HAL + SDK */
    printf("[8/8] HAL register...\n");
    fflush(stdout);
    convai_platform_esp32_init();

    char config_json[256];
    snprintf(config_json, sizeof(config_json),
             "{\"product_id\":\"%s\",\"product_key\":\"%s\","
             "\"product_secret\":\"%s\",\"device_name\":\"%s\"}",
             DEVICE_PRODUCT_ID, DEVICE_PRODUCT_KEY,
             DEVICE_PRODUCT_SECRET, DEVICE_NAME);

    convai_event_handler_t handler = {
        .on_convai_event = on_sdk_event,
        .on_convai_conversation_status = on_conversation_status,
        .on_convai_audio_data = on_audio_data,
        .on_convai_message_data = on_message_data,
    };

    ret = convai_create(&g_engine, config_json, &handler, NULL);
    if (ret != CONVAI_OK)
    {
        printf("[8/8] SDK create FAILED: %d\n", ret);
    }
    else
    {
        convai_opt_t opt = {.mode = CONVAI_MODE_WS};
        convai_start(g_engine, &opt);
        printf("[8/8] SDK started (v%s)\n", convai_get_version());
        xTaskCreate(audio_capture_task, "audio_cap", 4096, NULL, 5, NULL);
    }
    fflush(stdout);

skip_hw:

    /* ?(GPIO0) ?*/
    button_init();

    printf("\n=== Init complete ?press custom key to start AI conversation ===\n");
    gpio_set_level(48, 0);

    while (1)
    {
        bool down = button_is_down();
        TickType_t now = xTaskGetTickCount();

        if (down && !s_btn_was_down)
        {
            /* ?*/
            s_btn_press_tick = now;
            s_long_press_fired = false;
        }
        else if (down && s_btn_was_down)
        {
            /* ? ?*/
            if (!s_long_press_fired &&
                (now - s_btn_press_tick) * portTICK_PERIOD_MS >= LONG_PRESS_MS)
            {
                s_long_press_fired = true;
            }
        }
        else if (!down && s_btn_was_down)
        {
            /* ? ? ? */
            if (!s_long_press_fired &&
                (now - s_btn_press_tick) * portTICK_PERIOD_MS >= SHORT_MIN_MS)
            {

                voice_state_t cur = ai_chat_ui_get_state();
                if (cur == STATE_IDLE)
                {
                    /* IDLE ? ? ? */
                    ESP_LOGI(TAG, "Short press ?start conversation");
                    ai_chat_ui_set_state(STATE_LISTENING);
                }
                else if (cur == STATE_LISTENING)
                {
                    /* ? / ? ? */
                    ESP_LOGI(TAG, "Short press ?cancel");
                    ai_chat_ui_set_state(STATE_IDLE);
                }
                else if (cur == STATE_VOICE_SELECT)
                {
                    /* 已移除：物理按键短按不再操作音色面板 */
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

 /* ---- 2. I2C ? ---- */
 if (i2c_bus_init() != 0) return;

 /* ---- 3. PCA9557 IO ? ---- */
 if (pca9557_init(&g_pca9557, g_i2c_bus, PCA9557_I2C_ADDR) != 0) {
 ESP_LOGE(TAG, "PCA9557 init failed ?LCD/audio won't work");
 return;
 }

 /* ---- 4. LED ---- */
 board_led_init();
 board_led_set(1); /* ? ? ? */

 /* ---- 5. LCD ---- */
 lcd_init();
 backlight_init();
 lcd_show_status(0); /* ?: ?*/
 ESP_LOGI(TAG, "Display ready");

 /* ---- 6. ? ---- */
 lcd_show_status(1); /* ?: ? */
 if (audio_lckfb_init(&g_audio, g_i2c_bus, pa_enable_cb, &g_pca9557) != 0) {
 ESP_LOGW(TAG, "Audio init failed ?voice unavailable");
 }
 audio_lckfb_set_volume(&g_audio, 70);

 /* ---- 7. Wi-Fi ---- */
 lcd_show_status(2); /* ?: WiFi ? ?*/
 wifi_prov_init();
 wifi_prov_wait_connected();

 /* ---- 8. SNTP ---- */
 lcd_show_status(3); /* ?: ?*/
 sntp_init_sync();

 /* ---- 9. ? HAL ? ---- */
 lcd_show_status(4); /* ?: HAL ?*/
 if (convai_platform_esp32_init() != 0) {
 ESP_LOGE(TAG, "HAL registration failed");
 lcd_fill(0xF800); /* ? ? */
 return;
 }

 /* ---- 10. SDK ? ---- */
 lcd_show_status(5); /* ?: SDK ?*/
 char config_json[512];
 snprintf(config_json, sizeof(config_json),
 "{\"product_id\":\"%s\",\"product_key\":\"%s\","
 "\"product_secret\":\"%s\",\"device_name\":\"%s\"}",
 DEVICE_PRODUCT_ID, DEVICE_PRODUCT_KEY,
 DEVICE_PRODUCT_SECRET, DEVICE_NAME);

 convai_event_handler_t handler = {
 .on_convai_event = on_sdk_event,
 .on_convai_conversation_status = on_conversation_status,
 .on_convai_audio_data = on_audio_data,
 .on_convai_message_data = on_message_data,
 };

 ret = convai_create(&g_engine, config_json, &handler, NULL);
 if (ret != CONVAI_OK) {
 ESP_LOGE(TAG, "convai_create failed: %d (%s)", ret, convai_err_2_str(ret));
 return;
 }
 ESP_LOGI(TAG, "SDK engine created (v%s)", convai_get_version());

 /* ---- 11. ? ? ---- */
 convai_opt_t opt = { .mode = CONVAI_MODE_WS };
 ret = convai_start(g_engine, &opt);
 if (ret != CONVAI_OK) {
 ESP_LOGE(TAG, "convai_start failed: %d", ret);
 lcd_fill(0xF800); /* ?: ? ? */
 convai_destroy(g_engine);
 return;
 }
 ESP_LOGI(TAG, "Session started");

 /* ---- 12. ? ? ---- */
 xTaskCreate(audio_capture_task, "audio_cap", 4096, NULL, 5, NULL);

 /* ---- 13. ?---- */
 board_led_set(0);
 lcd_fill(0xFFFF); /* ? ? ? SPI ?*/
 printf("LCD filled with WHITE (0xFFFF)\n"); fflush(stdout);
 ESP_LOGI(TAG, "Ready ?press BOOT button or speak to interact");

 while (1) {
 vTaskDelay(pdMS_TO_TICKS(500));
 }
#endif
}
