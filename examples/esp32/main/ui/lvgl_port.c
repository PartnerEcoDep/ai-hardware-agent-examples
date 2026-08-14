/**
 * @file lvgl_port.c
 * @brief LVGL 9.x 移植层 — ST7789 320x240, PSRAM 双缓冲 PARTIAL 模式
 *
 * 通过 esp_lcd_panel_draw_bitmap 将 LVGL 渲染结果直推 ST7789。
 * draw_bitmap 内部走 SPI 命令队列，函数返回即表示数据已入队。
 */

#include "lvgl_port.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"

#include "lvgl.h"
#include "ai_chat_ui.h"

/* main.c 导出的 LCD panel 句柄 */
extern esp_lcd_panel_handle_t g_lcd_panel;

static const char *TAG = "lvgl_port";

/* ===================================================================
 *  帧缓冲 — 内部 SRAM 双缓冲, PARTIAL 模式
 *
 *  之前用整帧 FULL 缓冲（153600B）放 PSRAM（MALLOC_CAP_SPIRAM|DMA），
 *  每次 flush 被 ST7789 按 max_transfer_sz 切多段，SPI 驱动判定 PSRAM
 *  源缓冲不可直接 DMA，逐段从内部 RAM 现分配私有缓冲
 *  （spicommon_dma_setup_priv_buffer）。动画密集（AI speaking 多个圆
 *  同时脉动）时刷新频率飙高，内部 RAM DMA 池被耗尽 → "Failed to
 *  allocate priv TX buffer"，LCD 刷新失败。
 *
 *  改为 PARTIAL + 内部 SRAM DMA 双缓冲（40 行 ≈ 25.6KB ×2，正好等于
 *  SPI max_transfer_sz，单段传输），源缓冲在内部 RAM 可被 GDMA 直接
 *  访问，无需逐段私有缓冲拷贝，从根本上消除该错误。
 * =================================================================== */
#define LCD_H_RES  320
#define LCD_V_RES  240
#define BUF_LINES  40           /* PARTIAL: 每次渲染 40 行 (== SPI max_transfer_sz 的段高) */
#define BUF_SIZE   (LCD_H_RES * BUF_LINES * sizeof(lv_color_t))  /* 25600 bytes */

static lv_color_t *s_buf1 = NULL;
static lv_color_t *s_buf2 = NULL;

/* ===================================================================
 *  LVGL 显示刷新回调 — 同步通知
 *
 *  esp_lcd_panel_draw_bitmap 将数据写入 SPI 命令队列后返回。
 *  队列有硬件 DMA 保障，数据不会丢失，直接通知 LVGL 完成即可。
 * =================================================================== */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map)
{
    int w = lv_area_get_width(area);
    int h = lv_area_get_height(area);

    esp_lcd_panel_draw_bitmap(g_lcd_panel,
                              area->x1, area->y1,
                              area->x1 + w, area->y1 + h,
                              px_map);

    lv_display_flush_ready(disp);
}

/* ===================================================================
 *  LVGL Tick 定时器
 * =================================================================== */
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static esp_timer_handle_t s_tick_timer = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;

static void lvgl_task(void *arg)
{
    (void)arg;
    uint32_t hb_cnt = 0;
    while (1) {
        TickType_t t0 = xTaskGetTickCount();
        bool got_lock = lvgl_port_lock(pdMS_TO_TICKS(20));
        if (got_lock) {
            lv_timer_handler();
            lvgl_port_unlock();
        } else {
            ESP_LOGW(TAG, "lvgl lock timeout (>20ms) — main loop or other task holding it");
        }
        TickType_t t1 = xTaskGetTickCount();
        uint32_t elapsed = (t1 - t0) * portTICK_PERIOD_MS;
        vTaskDelay(pdMS_TO_TICKS(5));

        /* Heartbeat every ~10s (2000 x 5ms) to confirm lvgl_task is alive
         * and capture stack HWM.  Frequency raised from 30s → 10s so that
         * "界面卡死" incidents leave a fresh log entry right before stall. */
        if (++hb_cnt >= 2000) {
            hb_cnt = 0;
            ESP_LOGI(TAG, "lvgl heartbeat: free_heap=%u, stack_hwm=%u",
                     (unsigned)esp_get_free_heap_size(),
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}

/* ===================================================================
 *  LVGL 互斥锁
 * =================================================================== */
static SemaphoreHandle_t s_lvgl_mutex = NULL;

bool lvgl_port_lock(int timeout_ms)
{
    if (xSemaphoreTakeRecursive(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return true;
    }
    return false;
}

void lvgl_port_unlock(void)
{
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}

/* ===================================================================
 *  初始化
 * =================================================================== */
int lvgl_port_init(void)
{
    ESP_LOGI(TAG, "LVGL init (%.1f KB x2 buffer)", BUF_SIZE / 1024.0f);

    /* 1. LVGL 核心初始化 */
    lv_init();

    /* 2. 内部 SRAM 分配双缓冲 (DMA 可访问)。
     *    必须用内部 RAM (MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL)，不要用 PSRAM：
     *    S3 上 PSRAM buffer 会被 SPI 驱动判为不可直接 DMA，退回私有缓冲拷贝路径，
     *    动画密集时耗尽内部 RAM DMA 池。40 行 × 2 = 51.2KB，落在
     *    SPIRAM_MALLOC_RESERVE_INTERNAL(96KB) 内，可稳定分配。 */
    s_buf1 = (lv_color_t *)heap_caps_malloc(BUF_SIZE,
                                             MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf2 = (lv_color_t *)heap_caps_malloc(BUF_SIZE,
                                             MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_buf1 || !s_buf2) {
        ESP_LOGE(TAG, "Failed to allocate internal SRAM DMA buffer(s)");
        return -1;
    }
    memset(s_buf1, 0, BUF_SIZE);
    memset(s_buf2, 0, BUF_SIZE);

    /* 3. 创建 LVGL 显示 */
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, s_buf1, s_buf2, BUF_SIZE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 4. Tick 定时器 — 1ms */
    const esp_timer_create_args_t timer_args = {
        .callback = lv_tick_timer_cb,
        .name = "lv_tick",
    };
    esp_timer_create(&timer_args, &s_tick_timer);
    esp_timer_start_periodic(s_tick_timer, 1000);  /* 1ms = 1000us */

    /* 5. 互斥锁 — 必须在任务创建之前
     * 使用可重入互斥锁：setter 之间会嵌套调用（如 set_connection -> set_network），
     * 且主任务创建 UI 时需与 lvgl_task(CPU1) 的 lv_timer_handler 串行化，避免跨核竞争。 */
    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();

    /* 6. 设置默认主题 — 必须在 LVGL 任务启动前完成 */
    lv_theme_default_init(disp,
                          lv_color_hex(0x00BFFF),   /* 主色: 深天蓝 */
                          lv_color_hex(0xFF6B6B),   /* 辅色: 珊瑚红 */
                          true,                      /* 深色主题 */
                          &lv_font_montserrat_14);  /* 默认字体 */

    /* 7. LVGL 任务 — 5ms 周期驱动事件循环，钉 CPU1 */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 16384, NULL, 5, &s_lvgl_task_handle, 1);

    ESP_LOGI(TAG, "LVGL ready (display %dx%d)", LCD_H_RES, LCD_V_RES);
    return 0;
}

/* ===================================================================
 *  触摸屏驱动 — FT6336 @ I2C 0x38
 * =================================================================== */


static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static bool s_prev_pressed = false;

    /* The esp_lcd_touch library reads the FT6336 and applies swap_xy +
     * mirror_x (configured in lvgl_port_touch_init) to produce screen
     * coordinates directly. */
    esp_lcd_touch_point_data_t tp_point = {0};
    uint8_t tp_cnt = 0;
    esp_lcd_touch_read_data(s_touch_handle);
    esp_lcd_touch_get_data(s_touch_handle, &tp_point, &tp_cnt, 1);

    if (tp_cnt > 0) {
        int sx = tp_point.x;
        int sy = tp_point.y;
        if (sx < 0) sx = 0; else if (sx > 319) sx = 319;
        if (sy < 0) sy = 0; else if (sy > 239) sy = 239;

        if (!s_prev_pressed) {
            ESP_LOGI(TAG, "touch: screen=(%d,%d)", sx, sy);
        }
        s_prev_pressed = true;
        data->point.x = (lv_coord_t)sx;
        data->point.y = (lv_coord_t)sy;
        data->state = LV_INDEV_STATE_PRESSED;
        ai_chat_ui_touch_indicator(sx, sy);
        ai_chat_ui_touch_swipe(sx, sy, true);
    } else {
        s_prev_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
        ai_chat_ui_touch_indicator_hide();
        ai_chat_ui_touch_swipe(0, 0, false);
    }
}

int lvgl_port_touch_init(i2c_master_bus_handle_t i2c_bus)
{
    ESP_LOGI(TAG, "Initializing FT6336 touch @ 0x38");

    /* 必须套用官方 FT5x06 I2C 配置宏：它设定了
     * disable_control_phase = 1。若此项为 0，I2C 读帧格式错误，
     * 会读回乱码坐标（小点错位 + LVGL 把乱跳坐标当成拖拽手势把画面拖偏）。
     * 参考板 LCKFB SZPI 同款硬件即用此配置。 */
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_cfg.scl_speed_hz = 400000;  /* 参考板用 400kHz 稳定 */

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus, &io_cfg, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C panel IO: %s",
                 esp_err_to_name(ret));
        return -1;
    }

    /* FT6336 reports coords in its native 240x320 space; the esp_lcd_touch
     * library applies swap_xy + mirror_x to map them onto our 320x240
     * landscape panel. This is the same config as the LCKFB SZPI reference
     * board, which is known-good on this exact hardware. */
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_V_RES,   /* 240 */
        .y_max = LCD_H_RES,   /* 320 */
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 1, .mirror_x = 1, .mirror_y = 0 },
    };

    ret = esp_lcd_touch_new_i2c_ft5x06(io_handle, &tp_cfg, &s_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init FT6336 touch: %s", esp_err_to_name(ret));
        return -1;
    }
    ESP_LOGI(TAG, "FT6336 device ready (library mode)");

    /* 注册 LVGL indev */
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    /* long-press time: 1s (default 400ms is too easy to trigger by accident) */
    lv_indev_set_long_press_time(indev, 1000);

    ESP_LOGI(TAG, "FT6336 touch ready");
    return 0;
}
