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
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "lvgl.h"

/* main.c 导出的 LCD panel 句柄 */
extern esp_lcd_panel_handle_t g_lcd_panel;

static const char *TAG = "lvgl_port";

/* ===================================================================
 *  帧缓冲 — PSRAM 双缓冲, PARTIAL 模式
 *
 *  ESP32-S3 PSRAM 支持 GDMA 直接访问，加 MALLOC_CAP_DMA 标记。
 *  双缓冲 + PARTIAL: LVGL 渲染一块时 DMA 发送上一块。
 *  缓冲 40 行（~25KB ×2），省 PSRAM 且保证刷新流畅。
 * =================================================================== */
#define LCD_H_RES  320
#define LCD_V_RES  240
#define BUF_LINES  40  /* 每次刷新 40 行 */
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

/* ===================================================================
 *  LVGL 互斥锁
 * =================================================================== */
static SemaphoreHandle_t s_lvgl_mutex = NULL;

bool lvgl_port_lock(int timeout_ms)
{
    if (xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return true;
    }
    return false;
}

void lvgl_port_unlock(void)
{
    xSemaphoreGive(s_lvgl_mutex);
}

/* ===================================================================
 *  初始化
 * =================================================================== */
int lvgl_port_init(void)
{
    ESP_LOGI(TAG, "LVGL init (%.1f KB x2 buffer)", BUF_SIZE / 1024.0f);

    /* 1. LVGL 核心初始化 */
    lv_init();

    /* 2. PSRAM 分配双帧缓冲 (DMA 可访问) */
    s_buf1 = (lv_color_t *)heap_caps_malloc(BUF_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    s_buf2 = (lv_color_t *)heap_caps_malloc(BUF_SIZE,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM DMA buffer(s)");
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

    /* 5. 互斥锁 */
    s_lvgl_mutex = xSemaphoreCreateMutex();

    /* 6. 设置默认主题 */
    lv_theme_default_init(disp,
                          lv_color_hex(0x00BFFF),   /* 主色: 深天蓝 */
                          lv_color_hex(0xFF6B6B),   /* 辅色: 珊瑚红 */
                          true,                      /* 深色主题 */
                          &lv_font_source_han_sans_sc_14_cjk);  /* 默认字体 */

    ESP_LOGI(TAG, "LVGL ready (display %dx%d)", LCD_H_RES, LCD_V_RES);
    return 0;
}

/* ===================================================================
 *  触摸屏驱动 — FT6336 @ I2C 0x38
 * =================================================================== */

static esp_lcd_touch_handle_t s_touch = NULL;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint8_t cnt = 0;

    if (esp_lcd_touch_read_data(s_touch) == ESP_OK) {
        bool pressed = esp_lcd_touch_get_coordinates(
            s_touch, x, y, NULL, &cnt, 1);
        if (pressed && cnt > 0) {
            data->point.x = x[0];
            data->point.y = y[0];
            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
}

int lvgl_port_touch_init(i2c_master_bus_handle_t i2c_bus)
{
    ESP_LOGI(TAG, "Initializing FT6336 touch @ 0x38");

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = 0x38,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 0,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 0,
        },
    };

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus, &io_cfg, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C panel IO: %s",
                 esp_err_to_name(ret));
        return -1;
    }

    ret = esp_lcd_touch_new_i2c_ft5x06(io_handle, &tp_cfg, &s_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init FT6336: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 注册 LVGL indev */
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    ESP_LOGI(TAG, "FT6336 touch ready");
    return 0;
}
