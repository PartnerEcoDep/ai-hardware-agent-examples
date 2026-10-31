#include "ssid_manager.h"
#include "esp_wifi_store.h"
static bool save(void *, ai_wifi_profiles_t *next, const ai_wifi_profiles_t *current) {
    return esp_wifi_store_save(next, current);
}
static const ai_wifi_store_ops_t store_ops = {nullptr, save};
SsidManager::SsidManager() : storage_ready_(esp_wifi_store_load(&profiles_)) {}
bool SsidManager::AddSsid(const std::string& ssid, const std::string& password) {
    std::lock_guard<std::mutex> guard(mutex_);
    return storage_ready_ && ai_wifi_store_put(&profiles_, &store_ops, ssid.c_str(), password.c_str());
}
bool SsidManager::RemoveSsid(int index) {
    std::lock_guard<std::mutex> guard(mutex_);
    return storage_ready_ && index >= 0 && ai_wifi_store_remove(&profiles_, &store_ops, (unsigned)index);
}
bool SsidManager::SetDefaultSsid(int index) {
    std::lock_guard<std::mutex> guard(mutex_);
    return storage_ready_ && index >= 0 && ai_wifi_store_default(&profiles_, &store_ops, (unsigned)index);
}
std::vector<SsidItem> SsidManager::GetSsidList() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::vector<SsidItem> result;
    for (unsigned i = 0; i < profiles_.count; ++i)
        result.push_back({profiles_.items[i].ssid, profiles_.items[i].password});
    return result;
}
