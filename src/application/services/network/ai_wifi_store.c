#include "ai_wifi_store.h"
#include <string.h>
static bool usable(const ai_wifi_profiles_t *p, const ai_wifi_store_ops_t *ops) {
    if (!p || !ops || !ops->save || p->count > AI_WIFI_SAVED_MAX) return false;
    for (unsigned i = 0; i < p->count; ++i)
        if (!p->items[i].ssid[0] || !memchr(p->items[i].ssid, 0, sizeof(p->items[i].ssid)) ||
            !memchr(p->items[i].password, 0, sizeof(p->items[i].password))) return false;
    return true;
}
static bool commit(ai_wifi_profiles_t *current, ai_wifi_profiles_t *next, const ai_wifi_store_ops_t *ops) {
    bool ok = ops->save(ops->ctx, next, current);
    if (ok) *current = *next;
    memset(next, 0, sizeof(*next));
    return ok;
}
bool ai_wifi_store_put(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops,
                       const char *ssid, const char *password) {
    if (!usable(current, ops) || !ai_wifi_credentials(ssid, password)) return false;
    ai_wifi_profiles_t next = *current;
    unsigned i;
    for (i = 0; i < next.count; ++i) if (!strcmp(ssid, next.items[i].ssid)) break;
    if (i == next.count) {
        if (next.count < AI_WIFI_SAVED_MAX) ++next.count;
        memmove(next.items + 1, next.items, (next.count - 1) * sizeof(next.items[0]));
        i = 0;
    }
    memset(&next.items[i], 0, sizeof(next.items[i]));
    memcpy(next.items[i].ssid, ssid, strlen(ssid));
    memcpy(next.items[i].password, password, strlen(password));
    return commit(current, &next, ops);
}
bool ai_wifi_store_remove(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops, unsigned index) {
    if (!usable(current, ops) || index >= current->count) return false;
    ai_wifi_profiles_t next = *current;
    memmove(next.items + index, next.items + index + 1, (next.count - index - 1) * sizeof(next.items[0]));
    memset(&next.items[--next.count], 0, sizeof(next.items[0]));
    return commit(current, &next, ops);
}
bool ai_wifi_store_default(ai_wifi_profiles_t *current, const ai_wifi_store_ops_t *ops, unsigned index) {
    if (!usable(current, ops) || index >= current->count) return false;
    ai_wifi_profiles_t next = *current;
    ai_wifi_profile_t selected = next.items[index];
    memmove(next.items + 1, next.items, index * sizeof(selected));
    next.items[0] = selected;
    memset(&selected, 0, sizeof(selected));
    return commit(current, &next, ops);
}
