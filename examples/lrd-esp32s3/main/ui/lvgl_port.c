/**
 * @file lvgl_port.c
 * @brief LVGL 9.x 移植层 — ST7789 320x240, PSRAM 双缓冲 PARTIAL 模式
 *
 * 通过 esp_lcd_panel_draw_bitmap 将 LVGL 渲染结果直推 ST7789。
 * draw_bitmap 内部走 SPI 命令队列，函数返回即表示数据已入队。
 */

#include "lvgl_port.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"

#include "lvgl.h"
#include "ai_chat_ui.h"
#include "board_init.h"
#include "board_lckfb_szpi.h"

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

    /* 耗时/失败监控: 定位 lv_timer_handler 卡顿 (LVGL 锁被长期持有) 是否
     * 来自 SPI draw_bitmap 阻塞. 正常单帧 PARTIAL 传输应 <10ms. */
    int64_t t0 = esp_timer_get_time();
    esp_err_t ret = esp_lcd_panel_draw_bitmap(g_lcd_panel,
                                              area->x1, area->y1,
                                              area->x1 + w, area->y1 + h,
                                              px_map);
    int64_t dt_us = esp_timer_get_time() - t0;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "draw_bitmap failed: %s (area %d,%d %dx%d)",
                 esp_err_to_name(ret), area->x1, area->y1, w, h);
    } else if (dt_us > 50000) {   /* >50ms 单帧传输过慢 */
        ESP_LOGW(TAG, "draw_bitmap took %lld ms (area %d,%d %dx%d) — SPI slow",
                 (long long)(dt_us / 1000), area->x1, area->y1, w, h);
    }

    lv_display_flush_ready(disp);
}

/* ===================================================================
 *  LVGL Tick 定时器 — 2ms (原 1ms)
 *
 *  tick 粒度放宽到 2ms 以降低定时器中断开销 (低性能设备降耗)。
 *  LV_DEF_REFR_PERIOD=66ms 下动画步进按 tick 推进, 2ms 精度足够。
 * =================================================================== */
static void lv_tick_timer_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(2);
}

static esp_timer_handle_t s_tick_timer = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static i2c_master_dev_handle_t s_touch_i2c_dev = NULL;

static void lvgl_task(void *arg)
{
    (void)arg;
    uint32_t hb_cnt = 0;
    while (1) {
        bool got_lock = lvgl_port_lock(pdMS_TO_TICKS(20));
        if (got_lock) {
            /* 架构修复: SDK 线程 set_state 非阻塞化后, pending 状态在此
             * 持锁应用 — 状态更新不再依赖 SDK 线程抢锁, lvgl_task 每跑
             * 一拍即收敛, 从根上消除锁竞争导致的 UI 卡死。 */
            ai_chat_ui_apply_pending();

            /* 看门狗诊断: lv_timer_handler 单次执行过久会长时间持有 LVGL 锁,
             * 导致 SDK 线程 set_state/set_cloud 抢锁超时、UI 卡死. 超阈值打日志. */
            int64_t t0 = esp_timer_get_time();
            lv_timer_handler();
            int64_t dt_us = esp_timer_get_time() - t0;
            if (dt_us > 200000) {   /* >200ms */
                ESP_LOGW(TAG, "lv_timer_handler took %lld ms (>200ms) — "
                         "LVGL lock held too long, UI stall risk",
                         (long long)(dt_us / 1000));
            }
            lvgl_port_unlock();
        } else {
            ESP_LOGW(TAG, "lvgl lock timeout (>20ms) — main loop or other task holding it");
        }
        /* 20ms 周期驱动 lv_timer_handler (原 10ms): 低性能设备降耗,
         * 事件处理与动画步进频率减半; 实际屏幕刷新受 LV_DEF_REFR_PERIOD
         * (66ms) 节流, 20ms 周期足以覆盖刷新与触摸轮询. */
        vTaskDelay(pdMS_TO_TICKS(20));

        /* Heartbeat every ~10s (500 x 20ms) to confirm lvgl_task is alive
         * and capture stack HWM.  Frequency raised from 30s → 10s so that
         * "界面卡死" incidents leave a fresh log entry right before stall. */
        if (++hb_cnt >= 500) {
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

    /* 4. Tick 定时器 — 2ms (低性能设备降耗, 中断开销减半) */
    const esp_timer_create_args_t timer_args = {
        .callback = lv_tick_timer_cb,
        .name = "lv_tick",
    };
    esp_timer_create(&timer_args, &s_tick_timer);
    esp_timer_start_periodic(s_tick_timer, 2000);  /* 2ms = 2000us */

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

    /* 7. LVGL 任务 — 20ms 周期驱动事件循环，钉 CPU1。
     * 周期放宽到 20ms 以降低渲染 CPU 压力（低性能设备降耗）；
     * 实际屏幕刷新受 LV_DEF_REFR_PERIOD(66ms) 节流。
     *
     * 优先级 11 是优先级反转修复 (原为 5): lv_timer_handler 持有 LVGL
     * 递归锁期间, 若 lvgl_task 被更高优先级任务抢占 (SDK IO 线程=10,
     * playback=6, capture=5), 锁被无限期持有 → SDK 线程 set_state 抢锁
     * 超时 → UI 永久卡死 (看门狗曾抓到 lv_timer_handler took 1164ms,
     * 且 lvgl heartbeat 完全消失 = lvgl_task 被饿死)。
     * 提到 11 (低于 lwIP=18/WiFi=23) 后, lvgl_task 持锁可快速完成单轮
     * lv_timer_handler (~20ms) 并释放锁, IO 线程等锁最多一个周期。
     * lvgl_task 每轮 vTaskDelay(20ms) 主动让出, 不会饿死音频任务。 */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 24576, NULL, 11,
                            &s_lvgl_task_handle, 1);

    ESP_LOGI(TAG, "LVGL ready (display %dx%d)", LCD_H_RES, LCD_V_RES);
    return 0;
}

/* ===================================================================
 *  触摸屏驱动 — CST836U @ I2C 0x15
 *
 *  寄存器 (与 WizBlock touch_hal.c 一致):
 *    0x02 TD_STATUS (低 4 位=触摸点数, 高 4 位=事件标志)
 *    0x03-0x06 触摸点1 坐标: XH/XL/YH/YL (各 12-bit, 高 4 位为 0)
 *  原生坐标 240x320 (竖屏), 由 touch_read_cb 旋转映射到 320x240 横屏。
 * =================================================================== */

#define TOUCH_I2C_ADDR_LOCAL     0x15
#define TOUCH_REG_TD_STATUS      0x02
#define TOUCH_TOUCH_TIMEOUT_MS   50

/* 连续 I2C 失败阈值: 超过后暂时禁用触摸读取, 避免触摸芯片不响应时
 * 反复拖慢轮询任务。触摸读取已移出 lv_timer_handler (LVGL 锁内),
 * 由独立 touch_poll_task 执行, 即使 I2C 挂死也不会阻塞 LVGL 锁。 */
#define TOUCH_FAIL_LIMIT       10
#define TOUCH_FAIL_BACKOFF_MS  500
#define TOUCH_POLL_PERIOD_MS   20

/* 触摸轮询任务与 LVGL 回调共享的状态 (volatile, 单写者单读者) */
static volatile bool s_touch_pressed = false;
static volatile int  s_touch_x = 0;
static volatile int  s_touch_y = 0;

/* 诊断寄存器 */
#define CST836U_REG_FW_VER       0xA6
#define CST836U_REG_MODULE_ID    0xA8

static esp_err_t touch_read_regs(uint8_t start_reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_touch_i2c_dev, &start_reg, 1, buf, len,
                                       TOUCH_TOUCH_TIMEOUT_MS);
}

/* ===================================================================
 *  触摸轮询任务 — 独立于 LVGL 锁执行 I2C 读取
 *
 *  背景: touch_read_cb 之前直接在 lv_timer_handler (LVGL 锁内) 里做
 *  I2C 读取, 若触摸芯片不响应/总线异常, 每次读取可能远超 50ms 甚至
 *  不返回, 把 lvgl_task 拖死 → LVGL 锁被永久占用 → SDK 线程的
 *  set_state/set_cloud 全部抢锁失败, UI 冻结在"AI 播报"。
 *  改为独立任务后, 即使 I2C 挂死也只挂触摸任务, lvgl_task 不受影响。
 * =================================================================== */
static void touch_poll_task(void *arg)
{
    (void)arg;
    uint32_t fail_count = 0;
    uint32_t disabled_until = 0;

    while (1) {
        uint32_t now = (uint32_t)xTaskGetTickCount();

        if (now < disabled_until) {
            s_touch_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
            continue;
        }

        /* 从 0x02 一次读 7 字节: TD_STATUS + 触摸点1 完整坐标 (原厂读法) */
        uint8_t buf[7];
        if (touch_read_regs(TOUCH_REG_TD_STATUS, buf, sizeof(buf)) != ESP_OK) {
            if (++fail_count >= TOUCH_FAIL_LIMIT) {
                fail_count = 0;
                disabled_until = now + pdMS_TO_TICKS(TOUCH_FAIL_BACKOFF_MS);
                ESP_LOGW(TAG, "touch: %d consecutive I2C failures, "
                         "reads disabled for %dms", TOUCH_FAIL_LIMIT,
                         TOUCH_FAIL_BACKOFF_MS);
            }
            s_touch_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
            continue;
        }
        fail_count = 0;

        /* 低 4 位才是触摸点数, 高 4 位是事件标志 (必须掩码) */
        uint8_t points = buf[0] & 0x0F;
        if (points == 0 || points > 2) {
            s_touch_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
            continue;
        }

        /* CST836U 原生坐标 240x320(竖屏) → 320x240(横屏):
         * swap_xy + mirror_x (与 FT6336 相同旋转组合, 实机验证) */
        int raw_x = ((buf[1] & 0x0F) << 8) | buf[2];   /* 0..239 */
        int raw_y = ((buf[3] & 0x0F) << 8) | buf[4];   /* 0..319 */
        int sx = raw_y;                 /* swap_xy */
        int sy = LCD_V_RES - raw_x;     /* mirror_x (x_max=240) + swap_xy */
        if (sx < 0) sx = 0; else if (sx > (LCD_H_RES - 1)) sx = (LCD_H_RES - 1);
        if (sy < 0) sy = 0; else if (sy > (LCD_V_RES - 1)) sy = (LCD_V_RES - 1);

        s_touch_x = sx;
        s_touch_y = sy;
        s_touch_pressed = true;

        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
    }
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static bool s_prev_pressed = false;

    if (s_touch_i2c_dev == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    if (s_touch_pressed) {
        int sx = s_touch_x;
        int sy = s_touch_y;

        if (!s_prev_pressed) {
            ESP_LOGI(TAG, "touch: screen=(%d,%d)", sx, sy);
        }
        s_prev_pressed = true;
        data->point.x = (lv_coord_t)sx;
        data->point.y = (lv_coord_t)sy;
        data->state = LV_INDEV_STATE_PRESSED;
        ai_chat_ui_touch_indicator(sx, sy);
        ai_chat_ui_touch_swipe(sx, sy, true);
        return;
    }

    s_prev_pressed = false;
    data->state = LV_INDEV_STATE_RELEASED;
    ai_chat_ui_touch_indicator_hide();
    ai_chat_ui_touch_swipe(0, 0, false);
}

int lvgl_port_touch_init(i2c_master_bus_handle_t i2c_bus)
{
    ESP_LOGI(TAG, "Initializing CST836U touch @ 0x15");

    /* 复位触摸芯片 (TPT29555A TP_RST P1_3, 低有效) */
    tpt29555a_set_direction(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, true);
    tpt29555a_set_output(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    tpt29555a_set_output(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* 独立带超时的 I2C 设备句柄, 直接手动读寄存器:
     * esp_lcd_touch 库的读超时硬编码为 -1, 触摸读会永久阻塞 lvgl_task,
     * 因此这里不用库, 见 touch_read_cb。 */
    i2c_device_config_t touch_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDR_LOCAL,
        .scl_speed_hz = 100000,
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &touch_dev_cfg,
                                              &s_touch_i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add touch I2C device: %s",
                 esp_err_to_name(ret));
        return -1;
    }
    ESP_LOGI(TAG, "CST836U detected: module_id=0x%02X fw_ver=0x%02X",
             mod, fw);

    /* 芯片探测 + 读固件版本: 确认触摸芯片真的在 I2C 总线上响应 */
    uint8_t fw = 0, mod = 0;
    esp_err_t fw_ret = touch_read_regs(CST836U_REG_FW_VER, &fw, 1);
    esp_err_t mod_ret = touch_read_regs(CST836U_REG_MODULE_ID, &mod, 1);
    if (fw_ret != ESP_OK || mod_ret != ESP_OK) {
        ESP_LOGE(TAG, "CST836U not responding on I2C bus "
                 "(fw_ret=%s mod_ret=%s). Check touch chip power/reset/I2C.",
                 esp_err_to_name(fw_ret), esp_err_to_name(mod_ret));
        return -1;
    }
    ESP_LOGI(TAG, "CST836U detected: module_id=0x%02X fw_ver=0x%02X",
             mod, fw);

    /* 注册 LVGL indev (仅读共享变量, 不做 I2C, 不阻塞 lvgl_task) */
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    /* long-press time: 1s (default 400ms is too easy to trigger by accident) */
    lv_indev_set_long_press_time(indev, 1000);

    /* 启动独立触摸轮询任务 (I2C 读取在任务内, 隔离于 LVGL 锁) */
    BaseType_t tret = xTaskCreate(touch_poll_task, "touch_poll",
                                  4096, NULL, 2, NULL);
    if (tret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch poll task");
        return -1;
    }

    ESP_LOGI(TAG, "CST836U touch ready (poll task)");
    return 0;
}
