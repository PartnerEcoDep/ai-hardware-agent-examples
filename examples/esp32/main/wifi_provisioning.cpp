/**
 * @file wifi_provisioning.c
 * @brief C 语言 WiFi 配网实现 — 封装 esp-wifi-connect (C++)
 *
 * 通过 extern "C" 回调 + FreeRTOS Event Group 桥接
 * C++ WifiManager 的异步事件流与 C 语言同步调用。
 */

#include "wifi_provisioning.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

/* ---- C++ headers (wrapped) ---- */
#ifdef __cplusplus
extern "C" {
#endif
/* C++ headers are included inside the .cpp guard block below */
#ifdef __cplusplus
}
#endif

/* ===================================================================
 *  C++ → C 桥接 (内部实现)
 *
 *  此文件以 .c 扩展名存在，但实际内容为 C++。
 *  利用 IDF 构建系统自动识别 .c 文件中的 C++ 代码
 *  (当组件依赖包含 C++ 组件时)。
 *  或者通过 extern "C" 包装整个 C++ 实现。
 * =================================================================== */

#ifdef __cplusplus

#include "wifi_manager.h"
#include "ssid_manager.h"

static const char *TAG = "wifi_prov";

#define WIFI_PROV_CONNECTED_BIT   BIT0
#define WIFI_PROV_CONFIG_EXIT_BIT BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;

/* ---- WifiManager 事件回调 ---- */
static void wifi_prov_event_callback(WifiEvent event, const std::string& data) {
    switch (event) {
    case WifiEvent::Connected:
        ESP_LOGI(TAG, "WiFi connected: %s", data.c_str());
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_PROV_CONNECTED_BIT);
        }
        break;

    case WifiEvent::Disconnected:
        ESP_LOGW(TAG, "WiFi disconnected: %s", data.c_str());
        break;

    case WifiEvent::ConfigModeEnter:
        ESP_LOGI(TAG, "Config AP mode entered");
        break;

    case WifiEvent::ConfigModeExit:
        ESP_LOGI(TAG, "Config AP mode exited");
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_PROV_CONFIG_EXIT_BIT);
        }
        break;

    case WifiEvent::Connecting:
        ESP_LOGI(TAG, "Connecting to: %s", data.c_str());
        break;

    case WifiEvent::Scanning:
        ESP_LOGD(TAG, "Scanning...");
        break;
    }
}

/* ---- 公开 C API 实现 ---- */

int wifi_prov_init(void) {
    if (s_wifi_event_group != NULL) {
        ESP_LOGW(TAG, "Already initialized");
        return 0;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return -1;
    }

    auto& wifi = WifiManager::GetInstance();

    wifi.SetEventCallback(wifi_prov_event_callback);

    WifiManagerConfig config;
    config.ssid_prefix = "XinZhi";
    config.language    = "zh-CN";

    if (!wifi.Initialize(config)) {
        ESP_LOGE(TAG, "WifiManager init failed");
        return -1;
    }

    /* 参考 xiaozhi-esp32 wifi_board.cc TryWifiConnect()：
     * 有已保存 SSID → 直接 Station 模式连接；
     * 无已保存 SSID → 进入 AP 配网模式。 */
    auto& ssid_mgr = SsidManager::GetInstance();
    bool have_ssid = !ssid_mgr.GetSsidList().empty();

    if (have_ssid) {
        ESP_LOGI(TAG, "Found saved SSIDs, starting station...");
        wifi.StartStation();
    } else {
        ESP_LOGI(TAG, "No saved SSIDs, entering config AP mode...");
        vTaskDelay(pdMS_TO_TICKS(1500));
        wifi.StartConfigAp();
        ESP_LOGI(TAG, "Config AP SSID: %s", wifi.GetApSsid().c_str());
    }

    return 0;
}

int wifi_prov_wait_connected(void) {
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Not initialized — call wifi_prov_init() first");
        return -1;
    }

    auto& wifi = WifiManager::GetInstance();

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_PROV_CONNECTED_BIT | WIFI_PROV_CONFIG_EXIT_BIT,
            pdTRUE,   /* clear on exit */
            pdFALSE,  /* wait for any bit */
            portMAX_DELAY);

        if (bits & WIFI_PROV_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi connected successfully");
            return 0;
        }

        if (bits & WIFI_PROV_CONFIG_EXIT_BIT) {
            ESP_LOGI(TAG, "Config AP exited — retrying station...");
            wifi.StartStation();
            /* 继续等待 Connected 或下一次 ConfigModeExit */
        }
    }
}

bool wifi_prov_is_connected(void) {
    auto& wifi = WifiManager::GetInstance();
    return wifi.IsConnected();
}

void wifi_prov_enter_config(void) {
    auto& wifi = WifiManager::GetInstance();
    wifi.StartConfigAp();
    ESP_LOGI(TAG, "Config AP: %s", wifi.GetApSsid().c_str());
}

const char* wifi_prov_get_ssid(void) {
    static char ssid_buf[64];
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConnected()) {
        strncpy(ssid_buf, wifi.GetSsid().c_str(), sizeof(ssid_buf) - 1);
        ssid_buf[sizeof(ssid_buf) - 1] = '\0';
    } else {
        ssid_buf[0] = '\0';
    }
    return ssid_buf;
}

const char* wifi_prov_get_ip(void) {
    static char ip_buf[32];
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConnected()) {
        strncpy(ip_buf, wifi.GetIpAddress().c_str(), sizeof(ip_buf) - 1);
        ip_buf[sizeof(ip_buf) - 1] = '\0';
    } else {
        strncpy(ip_buf, "0.0.0.0", sizeof(ip_buf) - 1);
    }
    return ip_buf;
}

int wifi_prov_wait_connected_timeout(int timeout_ms) {
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Not initialized — call wifi_prov_init() first");
        return -1;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_PROV_CONNECTED_BIT | WIFI_PROV_CONFIG_EXIT_BIT,
        pdTRUE,   /* clear on exit */
        pdFALSE,  /* wait for any bit */
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_PROV_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        return 0;
    }

    if (bits & WIFI_PROV_CONFIG_EXIT_BIT) {
        ESP_LOGI(TAG, "Config AP exited");
        /* 自动重试 Station 连接 */
        auto& wifi = WifiManager::GetInstance();
        wifi.StartStation();
        return 1;
    }

    /* 超时（timeout_ms=0 时立即走到这里，表示仍在等待） */
    return 1;
}

bool wifi_prov_is_ap_mode(void) {
    auto& wifi = WifiManager::GetInstance();
    return wifi.IsConfigMode();
}

const char* wifi_prov_get_ap_ssid(void) {
    static char ap_ssid_buf[64];
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConfigMode()) {
        strncpy(ap_ssid_buf, wifi.GetApSsid().c_str(), sizeof(ap_ssid_buf) - 1);
        ap_ssid_buf[sizeof(ap_ssid_buf) - 1] = '\0';
    } else {
        ap_ssid_buf[0] = '\0';
    }
    return ap_ssid_buf;
}

#else
/* 纯 C 编译时 stub — 实际工程通过 IDF 组件系统以 C++ 编译此文件 */

#include <stdio.h>
#include <stdlib.h>

int wifi_prov_init(void) {
    fprintf(stderr, "wifi_prov_init: not compiled as C++ (missing esp-wifi-connect)\n");
    return -1;
}

int wifi_prov_wait_connected(void) {
    fprintf(stderr, "wifi_prov_wait_connected: not compiled as C++\n");
    return -1;
}

bool wifi_prov_is_connected(void) {
    return false;
}

void wifi_prov_enter_config(void) {
    fprintf(stderr, "wifi_prov_enter_config: not compiled as C++\n");
}

const char* wifi_prov_get_ssid(void) {
    return "";
}

const char* wifi_prov_get_ip(void) {
    return "0.0.0.0";
}

int wifi_prov_wait_connected_timeout(int timeout_ms) {
    (void)timeout_ms;
    fprintf(stderr, "wifi_prov_wait_connected_timeout: not compiled as C++\n");
    return -1;
}

bool wifi_prov_is_ap_mode(void) {
    return false;
}

const char* wifi_prov_get_ap_ssid(void) {
    return "";
}
#endif /* __cplusplus */
