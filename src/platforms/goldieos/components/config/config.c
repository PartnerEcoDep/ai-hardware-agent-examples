#include "ws_config.h"
#include "ws63_config.h"
#include "cJSON.h"
#include "runtime.h"
#include <stdio.h>
static int config_failure(int code, const char *stage) {
    printf("[unified] Cloud configuration failed: %s (%d)\n", stage, code);
    return code;
}
int ws_cloud_config(ai_cloud_config_t *cfg) {
    static char json[3072];
    char device[32] = {0};
    if (!WS_AI_AGENT_ID[0] || (!WS_AI_API_KEY[0] &&
        (!WS_AI_PRODUCT_ID[0] || !WS_AI_PRODUCT_KEY[0] || !WS_AI_PRODUCT_SECRET[0])))
        return config_failure(-7, "missing credentials");
    if (ws_runtime_device_id(device, sizeof(device)) <= 0)
        return config_failure(-71, "device identity");
    cJSON *root = cJSON_CreateObject();
    if (!root) return config_failure(-72, "JSON allocation");
    cJSON *info = cJSON_AddObjectToObject(root, "info");
    cJSON *ws = cJSON_AddObjectToObject(root, "ws");
    cJSON *audio = ws ? cJSON_AddObjectToObject(ws, "audio") : NULL;
    bool ok = info && ws && audio;
    if (ok && WS_AI_API_KEY[0]) ok = cJSON_AddStringToObject(info, "api_key", WS_AI_API_KEY) != NULL;
    else if (ok) ok = cJSON_AddStringToObject(info, "product_id", WS_AI_PRODUCT_ID) &&
        cJSON_AddStringToObject(info, "product_key", WS_AI_PRODUCT_KEY) &&
        cJSON_AddStringToObject(info, "product_secret", WS_AI_PRODUCT_SECRET) &&
        cJSON_AddStringToObject(info, "device_name", device);
    if (ok && WS_AI_SERVER_URL[0]) ok = cJSON_AddStringToObject(ws, "url", WS_AI_SERVER_URL) != NULL;
    /* Constant numeric JSON token; avoid embedded libc floating-point
     * printf support just to serialize the fixed codec ID zero. */
    if (ok) ok = cJSON_AddRawToObject(audio, "codec", "0") != NULL;
    if (!ok) { cJSON_Delete(root); return config_failure(-73, "JSON fields"); }
    ok = cJSON_PrintPreallocated(root, json, sizeof(json), 0);
    cJSON_Delete(root);
    if (!ok) return config_failure(-74, "JSON serialization");
    cfg->config_json = json; cfg->agent_id = WS_AI_AGENT_ID; cfg->startup_json = NULL;
    return 0;
}
