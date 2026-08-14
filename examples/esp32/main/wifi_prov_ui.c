/**
 * @file wifi_prov_ui.c
 * @brief WiFi 配网 UI 状态机 — 实现
 *
 * 在 ST7789 320×240 屏幕上显示配网过程，使用非阻塞轮询：
 *   - 初始化 LCD UI 和 WiFi 配网
 *   - AP 模式：显示热点 SSID + URL + 操作提示
 *   - 轮询连接状态（每 500ms）
 *   - 连接成功：显示已连接、WiFi SSID、IP 地址
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

/* ---- 颜色定义（RGB565） ---- */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_DARK_BG     0x18E3   /* 深蓝灰背景 */
#define COLOR_GREEN_BG    0x0665   /* 深绿背景 */
#define COLOR_TITLE_BG    0x39E7   /* 标题栏蓝 */
#define COLOR_ACCENT       0x07FF   /* 青色强调 */
#define COLOR_GRAY         0x8410   /* 灰色次要文字 */
#define COLOR_YELLOW       0xFFE0   /* 黄色提示 */

/* ---- 布局常量 ---- */
#define TITLE_Y         8
#define STATUS_Y        40
#define SSID_LABEL_Y    80
#define SSID_VALUE_Y    100
#define URL_LABEL_Y     130
#define URL_VALUE_Y     150
#define HINT_Y          210

/* 画标题栏 */
static void draw_title_bar(const char *title) {
    lcd_ui_draw_rect(0, 0, 320, 32, COLOR_TITLE_BG);
    lcd_ui_center_text(TITLE_Y, title, COLOR_WHITE, COLOR_TITLE_BG);
}

/* 画分割线 */
static void draw_separator(int y) {
    lcd_ui_draw_rect(0, y, 320, 2, COLOR_ACCENT);
}

/* ===================================================================
 *  AP 配网画面
 * =================================================================== */
static void draw_ap_screen(const char *ap_ssid, const char *url) {
    lcd_ui_clear(COLOR_DARK_BG);

    draw_title_bar("WiFi 配网");

    /* 状态提示 */
    lcd_ui_center_text(STATUS_Y, "等待配网...", COLOR_YELLOW, COLOR_DARK_BG);

    /* 分割线 */
    draw_separator(68);

    /* AP SSID */
    lcd_ui_draw_string(16, SSID_LABEL_Y, "热点名称:", COLOR_GRAY, COLOR_DARK_BG);
    lcd_ui_draw_string(16, SSID_VALUE_Y, ap_ssid ? ap_ssid : "N/A",
                       COLOR_WHITE, COLOR_DARK_BG);

    /* URL */
    lcd_ui_draw_string(16, URL_LABEL_Y, "配置地址:", COLOR_GRAY, COLOR_DARK_BG);
    lcd_ui_draw_string(16, URL_VALUE_Y, url ? url : "192.168.4.1",
                       COLOR_ACCENT, COLOR_DARK_BG);

    /* 底部提示 */
    lcd_ui_center_text(HINT_Y, "请用手机连接以上热点",
                       COLOR_WHITE, COLOR_DARK_BG);
    lcd_ui_center_text(HINT_Y + 18, "打开浏览器访问配置地址",
                       COLOR_GRAY, COLOR_DARK_BG);
}

/* ===================================================================
 *  连接画面（正在连接目标 SSID）
 * =================================================================== */
static void draw_connecting_screen(const char *target_ssid) {
    lcd_ui_clear(COLOR_DARK_BG);

    draw_title_bar("WiFi 连接中");

    lcd_ui_center_text(STATUS_Y, "正在连接...", COLOR_YELLOW, COLOR_DARK_BG);

    draw_separator(68);

    lcd_ui_draw_string(16, SSID_LABEL_Y, "目标网络:", COLOR_GRAY, COLOR_DARK_BG);
    lcd_ui_draw_string(16, SSID_VALUE_Y, target_ssid ? target_ssid : "N/A",
                       COLOR_WHITE, COLOR_DARK_BG);

    lcd_ui_center_text(HINT_Y, "请稍候...", COLOR_GRAY, COLOR_DARK_BG);
}

/* ===================================================================
 *  连接成功画面
 * =================================================================== */
static void draw_connected_screen(const char *ssid, const char *ip) {
    lcd_ui_clear(COLOR_GREEN_BG);

    draw_title_bar("连接成功");

    /* 大号状态 */
    lcd_ui_center_text(STATUS_Y, "已连接", COLOR_WHITE, COLOR_GREEN_BG);

    draw_separator(68);

    /* WiFi SSID */
    lcd_ui_draw_string(16, SSID_LABEL_Y, "WiFi:", COLOR_WHITE, COLOR_GREEN_BG);
    lcd_ui_draw_string(16, SSID_VALUE_Y, ssid ? ssid : "N/A",
                       COLOR_WHITE, COLOR_GREEN_BG);

    /* IP 地址 */
    lcd_ui_draw_string(16, URL_LABEL_Y, "IP 地址:", COLOR_WHITE, COLOR_GREEN_BG);
    lcd_ui_draw_string(16, URL_VALUE_Y, ip ? ip : "0.0.0.0",
                       COLOR_ACCENT, COLOR_GREEN_BG);
}

/* ===================================================================
 *  主入口：完整配网流程
 * =================================================================== */
void wifi_prov_ui_run(void) {
    ESP_LOGI(TAG, "Starting WiFi provisioning UI");

    /* 1. 初始化 LCD UI（必须在 WiFi 初始化前，确保 framebuffer 就绪） */
    lcd_ui_init();

    /* 2. 初始化 WiFi 配网 */
    if (wifi_prov_init() != 0) {
        ESP_LOGE(TAG, "WiFi provisioning init failed");

        /* 错误画面 */
        lcd_ui_clear(0xF800);  /* 红色背景 */
        lcd_ui_center_text(100, "WiFi 初始化失败", COLOR_WHITE, 0xF800);
        lcd_ui_flush();
        return;
    }

    /* 3. 判断当前模式并进入主轮询 */
    bool ap_mode = wifi_prov_is_ap_mode();

    if (ap_mode) {
        const char *ap_ssid = wifi_prov_get_ap_ssid();
        ESP_LOGI(TAG, "AP mode active, SSID: %s", ap_ssid);
        draw_ap_screen(ap_ssid, "192.168.4.1");
    } else {
        /* Station 模式，显示连接中画面 */
        const char *ssid = wifi_prov_get_ssid();
        ESP_LOGI(TAG, "Station mode, connecting...");
        draw_connecting_screen(strlen(ssid) > 0 ? ssid : "...");
    }
    lcd_ui_flush();

    /* 4. 非阻塞轮询 */
    int prev_ret = 1; /* 上一个状态 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));

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
            /* 错误 */
            ESP_LOGE(TAG, "WiFi provisioning error");
            lcd_ui_clear(0xF800);
            lcd_ui_center_text(100, "WiFi 连接错误", COLOR_WHITE, 0xF800);
            lcd_ui_flush();
            return;
        }

        /* ret == 1: 仍在等待 / 超时 → 刷新画面以更新任何变化 */

        /* 检查 AP 模式是否已切换 */
        bool now_ap = wifi_prov_is_ap_mode();
        if (now_ap != ap_mode) {
            ap_mode = now_ap;
            if (ap_mode) {
                const char *ap_ssid = wifi_prov_get_ap_ssid();
                draw_ap_screen(ap_ssid, "192.168.4.1");
            } else {
                const char *ssid = wifi_prov_get_ssid();
                draw_connecting_screen(strlen(ssid) > 0 ? ssid : "...");
            }
            lcd_ui_flush();
        }

        prev_ret = ret;
    }
}
