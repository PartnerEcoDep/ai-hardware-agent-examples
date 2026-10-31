#ifndef SSID_MANAGER_H
#define SSID_MANAGER_H
#include "ai_wifi_store.h"
#include <string>
#include <vector>
#include <mutex>
struct SsidItem { std::string ssid, password; };
class SsidManager {
public:
    static SsidManager& GetInstance() { static SsidManager instance; return instance; }
    bool AddSsid(const std::string& ssid, const std::string& password);
    bool RemoveSsid(int index);
    bool SetDefaultSsid(int index);
    std::vector<SsidItem> GetSsidList() const;
private:
    SsidManager();
    mutable std::mutex mutex_;
    ai_wifi_profiles_t profiles_{};
    bool storage_ready_;
};
#endif
