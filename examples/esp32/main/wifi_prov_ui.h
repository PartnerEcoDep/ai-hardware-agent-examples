/**
 * @file wifi_prov_ui.h
 * @brief WiFi 配网 UI 状态机
 *
 * 在 ST7789 320×240 屏幕上显示配网过程：
 *   - AP 模式：热点名称 + URL + 提示
 *   - 连接中：目标 SSID
 *   - 连接成功：已连接 + IP 地址
 *
 * 依赖 lcd_ui.h 和 wifi_provisioning.h
 */

#ifndef WIFI_PROV_UI_H
#define WIFI_PROV_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 完整配网流程：初始化 → AP 模式画面 → 等待连接 → 成功画面
 *
 * 替代 main.c 中原来的 wifi_prov_init() + wifi_prov_wait_connected()，
 * 内部使用非阻塞轮询（每 500ms 检查一次连接状态），期间持续刷新 LCD。
 */
void wifi_prov_ui_run(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_PROV_UI_H */
