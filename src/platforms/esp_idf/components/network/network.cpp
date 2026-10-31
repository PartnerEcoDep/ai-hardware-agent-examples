#include "esp_network.h"
#include "ai_network_status.h"
#include "wifi_manager.h"
#include "ssid_manager.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include <atomic>
#include <cstdio>
#include <ctime>

static std::atomic<bool> online{false}, retry_station{false};
static bool sntp_started;
static int64_t connecting_since;

int esp_network_init(void) {
    auto &wifi = WifiManager::GetInstance();
    wifi.SetEventCallback([](WifiEvent event, const std::string &) {
        if (event == WifiEvent::Connected) online = true;
        if (event == WifiEvent::Disconnected || event == WifiEvent::ConfigModeEnter)
            online = false;
        if (event == WifiEvent::ConfigModeExit) retry_station = true;
    });
    WifiManagerConfig config;
    config.ssid_prefix = "AI-Setup";
    config.language = "zh-CN";
    if (!wifi.Initialize(config)) return -1;
    connecting_since = esp_timer_get_time();
    if (SsidManager::GetInstance().GetSsidList().empty()) wifi.StartConfigAp();
    else {
        wifi.StartStation();
        wifi.SetPowerSaveLevel(WifiPowerSaveLevel::PERFORMANCE);
    }
    return 0;
}
void esp_network_poll(void) {
    auto &wifi = WifiManager::GetInstance();
    if (retry_station.exchange(false)) {
        wifi.StartStation();
        wifi.SetPowerSaveLevel(WifiPowerSaveLevel::PERFORMANCE);
        connecting_since = esp_timer_get_time();
    }
    if (online && !sntp_started) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, const_cast<char *>("pool.ntp.org"));
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
        esp_sntp_setservername(1, const_cast<char *>("ntp.aliyun.com"));
#endif
        esp_sntp_init();
        sntp_started = true;
    }
    if (online) connecting_since = esp_timer_get_time();
    else if (!wifi.IsConfigMode() && esp_timer_get_time() - connecting_since > 30000000)
        wifi.StartConfigAp();
}
bool esp_network_connected(void) { return online.load() && !WifiManager::GetInstance().IsConfigMode(); }
void esp_network_provision(void) {
    online = false;
    WifiManager::GetInstance().StartConfigAp();
}
bool esp_network_time_ready(void) { return std::time(nullptr) >= 1704067200; }
static void esp_network_status(ai_network_status_t *status) {
    if (!status) return;
    *status = {};
    auto &wifi = WifiManager::GetInstance();
    if (wifi.IsConfigMode()) {
        status->state = AI_NET_PROVISIONING;
        std::snprintf(status->setup_ssid, sizeof(status->setup_ssid), "%s", wifi.GetApSsid().c_str());
        std::snprintf(status->setup_url, sizeof(status->setup_url), "%s", wifi.GetApWebUrl().c_str());
    } else if (online) {
        status->state = AI_NET_CONNECTED;
        std::snprintf(status->address, sizeof(status->address), "%s", wifi.GetIpAddress().c_str());
    } else status->state = AI_NET_CONNECTING;
}
void esp_network_description(char *text, size_t capacity) {
    ai_network_status_t status;
    esp_network_status(&status);
    ai_network_format(&status, text, capacity);
}
