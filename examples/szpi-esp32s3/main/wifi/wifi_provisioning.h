/**
 * @file wifi_provisioning.h
 * @brief C 语言 WiFi 配网接口 — 封装 esp-wifi-connect (C++)
 *
 * 提供纯 C API，通过 Hotspot AP 模式进行 WiFi 配网。
 * 内部使用 WifiManager / SsidManager (C++ singletons)。
 *
 * 使用流程:
 *   1. wifi_prov_init()          — 初始化并启动 Station 模式
 *   2. wifi_prov_wait_connected() — 阻塞等待连接成功或进入配网模式
 *   3. wifi_prov_is_connected()   — 查询当前连接状态
 *
 * 若设备未保存任何 SSID 或无法连接，WifiManager 将自动
 * 进入 AP 配网模式（XinZhi-XXXX），用户可连接后通过网页配置。
 */

#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi 连接状态事件
 */
typedef enum {
    WIFI_PROV_EV_CONNECTED,     /**< WiFi 已连接 */
    WIFI_PROV_EV_DISCONNECTED,  /**< WiFi 已断开 */
} wifi_prov_event_t;

/**
 * @brief WiFi 事件回调类型
 */
typedef void (*wifi_prov_callback_t)(wifi_prov_event_t event);

/**
 * @brief 注册 WiFi 状态变化回调（必须在 wifi_prov_init() 之前调用）
 *
 * 当 WiFi 连接/断开时，回调将被立即调用（在 WiFi 事件线程上下文中）。
 * 允许多次调用覆盖，只保留最后一个回调。
 *
 * @param cb 回调函数指针，传 NULL 取消注册
 */
void wifi_prov_register_callback(wifi_prov_callback_t cb);

/**
 * @brief 初始化 WiFi 配网
 *
 * 初始化 NVS、TCP/IP 协议栈和 WifiManager，
 * 启动 Station 模式尝试连接已保存的 SSID。
 * 若无已保存凭证或连接失败，自动回退到 AP 配网模式。
 *
 * @return 0 成功, 非 0 失败
 */
int wifi_prov_init(void);

/**
 * @brief 阻塞等待 WiFi 连接成功
 *
 * 等待 WifiManager 报告 Connected 或 ConfigModeExit 事件。
 * 当连接成功时返回 0；当用户通过配网页面保存 SSID 后退出配网模式
 * 时返回 1（调用方应重试 station 连接）。
 *
 * @return 0 已连接, 1 配网模式已退出（需要重新连接）
 */
int wifi_prov_wait_connected(void);

/**
 * @brief 查询当前 WiFi 是否已连接
 *
 * @return true 已连接, false 未连接
 */
bool wifi_prov_is_connected(void);

/**
 * @brief 强制进入 AP 配网模式
 *
 * 停止 Station 模式，启动配置 AP（SSID: XinZhi-XXXX）。
 * 用户可通过网页页面输入 WiFi 凭证。
 */
void wifi_prov_enter_config(void);

/**
 * @brief 获取当前连接的 SSID
 *
 * @return SSID 字符串（静态缓冲区），未连接时返回 ""
 */
const char* wifi_prov_get_ssid(void);

/**
 * @brief 获取当前 IP 地址
 *
 * @return IP 地址字符串（静态缓冲区），未连接时返回 "0.0.0.0"
 */
const char* wifi_prov_get_ip(void);

/**
 * @brief 带超时的 WiFi 连接等待（非阻塞）
 *
 * 内部用 xEventGroupWaitBits(timeout) 替代 portMAX_DELAY。
 * 适合在 UI 轮询循环中调用。
 *
 * @param timeout_ms 超时时间（毫秒），0 表示立即返回不阻塞
 * @return 0 已连接, 1 超时/等待中, -1 错误
 */
int wifi_prov_wait_connected_timeout(int timeout_ms);

/**
 * @brief 查询当前是否处于 AP 配网模式
 *
 * @return true 处于 AP 配网模式, false 处于 Station 模式
 */
bool wifi_prov_is_ap_mode(void);

/**
 * @brief 获取 AP 热点名称
 *
 * @return AP SSID 字符串（静态缓冲区），非 AP 模式时返回 ""
 */
const char* wifi_prov_get_ap_ssid(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_PROVISIONING_H */
