/**
 * @file wifi_prov_ui.c
 * @brief WiFi 配网 UI 状态机 — 实现（深色主题 + 大字号 + 图标 + 动画）
 *
 * 在 ST7789 320×240 屏幕上显示配网过程，使用非阻塞轮询：
 *   - AP 模式：大标题 "WiFi Setup" + WiFi 图标 + 卡片信息
 *   - 连接中：大标题 "Connecting" + 动画点 + 目标 SSID 卡片
 *   - 连接成功：大标题 "Connected" + 绿色图标 + 网络信息卡片
 */

#include "wifi_prov_ui.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lcd_ui.h"
#include "wifi_provisioning.h"

static const char *TAG = "wifi_prov_ui";

/* ---- 颜色定义（RGB565，深色主题） ---- */
#define COLOR_BG         0x0000   /* 纯黑背景 */
#define COLOR_WHITE      0xFFFF   /* 主文字 */
#define COLOR_CYAN       0x07FF   /* 青色强调 */
#define COLOR_GREEN      0x07E0   /* 成功色 */
#define COLOR_YELLOW     0xFFE0   /* 警告/提示色 */
#define COLOR_GRAY       0x8410   /* 次要文字 */
#define COLOR_CARD_BG    0x18E3   /* 深灰蓝卡片背景 */

/* ---- 布局常量 ---- */
#define TITLE_Y          16
#define ICON_Y           60
#define STATUS_Y         110
#define CARD_X           20
#define CARD_Y           138
#define CARD_W           (LCD_UI_WIDTH - 40)
#define CARD_H           72
#define CARD_R           8
#define CARD_PAD         8
#define CARD_LINE1_Y     (CARD_Y + CARD_PAD + 12)
#define CARD_LINE2_Y     (CARD_LINE1_Y + 24)
#define HINT_Y           222

/* ===================================================================
 *  画面 1 — AP 配网模式
 * =================================================================== */
static void draw_ap_screen(const char *ap_ssid, const char *url) {
    lcd_ui_clear(COLOR_BG);

    /* 标题：2x "WiFi Setup" */
    lcd_ui_center_text_scaled(TITLE_Y, "WiFi Setup", 2, COLOR_WHITE, COLOR_BG);

    /* WiFi 图标：灰色，level=1 */
    lcd_ui_draw_wifi_icon(144, ICON_Y, 32, 1);

    /* 状态：1x 黄色 "AP Mode" */
    lcd_ui_center_text(STATUS_Y, "AP Mode", COLOR_YELLOW, COLOR_BG);

    /* SSID 大字居中 */
    lcd_ui_center_text_scaled(CARD_Y + 8, ap_ssid ? ap_ssid : "N/A",
                              2, COLOR_WHITE, COLOR_BG);

    /* URL 居中 */
    lcd_ui_center_text(CARD_Y + 48, url ? url : "192.168.4.1",
                       COLOR_CYAN, COLOR_BG);

    /* 底部提示 */
    lcd_ui_center_text(HINT_Y, "Connect phone to hotspot",
                       COLOR_GRAY, COLOR_BG);
    lcd_ui_center_text(HINT_Y + 16, "Open browser -> config page",
                       COLOR_GRAY, COLOR_BG);
}

/* ===================================================================
 *  画面 2 — 连接中（含动画点）
 * =================================================================== */
static void draw_connecting_screen(const char *target_ssid, int dot_count) {
    lcd_ui_clear(COLOR_BG);

    /* 标题：2x "Connecting" */
    lcd_ui_center_text_scaled(TITLE_Y, "Connecting", 2, COLOR_WHITE, COLOR_BG);

    /* WiFi 图标：黄色，level=2 */
    lcd_ui_draw_wifi_icon(144, ICON_Y, 32, 2);

    /* 状态：白色 "connecting" + 动画点 */
    char status_str[32];
    snprintf(status_str, sizeof(status_str), "connecting%.*s", dot_count, "....");
    lcd_ui_center_text(STATUS_Y, status_str, COLOR_WHITE, COLOR_BG);

    /* SSID 大字居中显示，无卡片背景 */
    lcd_ui_center_text_scaled(CARD_Y + 16, target_ssid ? target_ssid : "...",
                              2, COLOR_CYAN, COLOR_BG);
}

/* ===================================================================
 *  画面 3 — 连接成功
 * =================================================================== */
static void draw_connected_screen(const char *ssid, const char *ip) {
    lcd_ui_clear(COLOR_BG);

    /* 标题：2x "Connected" */
    lcd_ui_center_text_scaled(TITLE_Y, "Connected", 2, COLOR_WHITE, COLOR_BG);

    /* WiFi 图标：绿色，level=3 */
    lcd_ui_draw_wifi_icon(144, ICON_Y, 32, 3);

    /* 状态：绿色 "Success" */
    lcd_ui_center_text(STATUS_Y, "Success", COLOR_GREEN, COLOR_BG);

    /* WiFi 名大字居中 */
    lcd_ui_center_text_scaled(CARD_Y + 8, ssid ? ssid : "N/A",
                              2, COLOR_WHITE, COLOR_BG);

    /* IP 居中 */
    lcd_ui_center_text(CARD_Y + 48, ip ? ip : "0.0.0.0",
                       COLOR_CYAN, COLOR_BG);

    /* 底部 */
    lcd_ui_center_text(HINT_Y, "Enjoy your WiFi!",
                       COLOR_GRAY, COLOR_BG);
}

/* ===================================================================
 *  主入口：完整配网流程
 * =================================================================== */
void wifi_prov_ui_run(void) {
    ESP_LOGI(TAG, "Starting WiFi provisioning UI (dark theme)");

    /* 1. 初始化 LCD UI */
    lcd_ui_init();

    // 启动画面
    lcd_ui_clear(0x0000);
    lcd_ui_center_text_scaled(80, "AI Agent", 2, 0xFFFF, 0x0000);
    lcd_ui_draw_rect(60, 118, 200, 2, 0x07FF);   // 青色横线
    lcd_ui_center_text(140, "Starting...", 0x8410, 0x0000);
    lcd_ui_flush();
    vTaskDelay(pdMS_TO_TICKS(800));

    /* 2. 初始化 WiFi 配网 */
    if (wifi_prov_init() != 0) {
        ESP_LOGE(TAG, "WiFi provisioning init failed");

        /* 错误画面 */
        lcd_ui_clear(0xF800);
        lcd_ui_center_text(100, "WiFi Init Failed", COLOR_WHITE, 0xF800);
        lcd_ui_flush();
        return;
    }

    /* 3. 动画帧计数器（静态，跨轮询保持） */
    static int anim_frame = 0;

    /* 4. 判断当前模式并进入主轮询 */
    bool ap_mode = wifi_prov_is_ap_mode();
    anim_frame = 0;

    if (ap_mode) {
        const char *ap_ssid = wifi_prov_get_ap_ssid();
        ESP_LOGI(TAG, "AP mode active, SSID: %s", ap_ssid);
        draw_ap_screen(ap_ssid, "192.168.4.1");
    } else {
        const char *ssid = wifi_prov_get_ssid();
        ESP_LOGI(TAG, "Station mode, connecting...");
        draw_connecting_screen(strlen(ssid) > 0 ? ssid : "...", 0);
    }
    lcd_ui_flush();

    /* 5. 非阻塞轮询 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        anim_frame++;

        int ret = wifi_prov_wait_connected_timeout(0);

        if (ret == 0) {
            /* 连接成功 */
            ESP_LOGI(TAG, "WiFi connected!");
            const char *ssid = wifi_prov_get_ssid();
            const char *ip   = wifi_prov_get_ip();
            ESP_LOGI(TAG, "SSID: %s, IP: %s", ssid, ip);

            draw_connected_screen(ssid, ip);
            lcd_ui_flush();

            /* 停留 2 秒让用户看到成功画面 */
            vTaskDelay(pdMS_TO_TICKS(2000));
            return;
        }

        if (ret == -1) {
            ESP_LOGE(TAG, "WiFi provisioning error");
            lcd_ui_clear(0xF800);
            lcd_ui_center_text(100, "WiFi Connect Error", COLOR_WHITE, 0xF800);
            lcd_ui_flush();
            return;
        }

        /* ret == 1: 仍在等待 */

        /* 检查 AP 模式是否切换 */
        bool now_ap = wifi_prov_is_ap_mode();
        if (now_ap != ap_mode) {
            ap_mode = now_ap;
            anim_frame = 0; /* 切画面重置动画帧 */
            if (ap_mode) {
                const char *ap_ssid = wifi_prov_get_ap_ssid();
                draw_ap_screen(ap_ssid, "192.168.4.1");
            } else {
                const char *ssid = wifi_prov_get_ssid();
                draw_connecting_screen(strlen(ssid) > 0 ? ssid : "...", 0);
            }
            lcd_ui_flush();
            continue;
        }

        /* 连接中画面的动画刷新 */
        if (!ap_mode) {
            const char *ssid = wifi_prov_get_ssid();
            int dots = anim_frame % 4;
            draw_connecting_screen(strlen(ssid) > 0 ? ssid : "...", dots);
            lcd_ui_flush();
        }
    }
}
