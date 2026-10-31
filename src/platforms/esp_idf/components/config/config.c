#include "esp_config.h"
#include "esp_device.h"
#include "cJSON.h"
#include "sdkconfig.h"
#include <string.h>

int esp_cloud_config(ai_cloud_config_t *config) {
    static char json[3072];
    char device[32];
    if (!CONFIG_AI_AGENT_ID[0] || (!CONFIG_AI_API_KEY[0] &&
        (!CONFIG_AI_PRODUCT_ID[0] || !CONFIG_AI_PRODUCT_KEY[0] || !CONFIG_AI_PRODUCT_SECRET[0])))
        return -7;
    if (esp32_device_id(device, sizeof(device)) <= 0) return -7;
    cJSON *root = cJSON_CreateObject();
    if (!root) return -7;
    cJSON *info = cJSON_AddObjectToObject(root, "info");
    cJSON *ws = cJSON_AddObjectToObject(root, "ws");
    cJSON *audio = ws ? cJSON_AddObjectToObject(ws, "audio") : NULL;
    bool ok = info && ws && audio;
    if (ok && CONFIG_AI_API_KEY[0])
        ok = cJSON_AddStringToObject(info, "api_key", CONFIG_AI_API_KEY) != NULL;
    else if (ok)
        ok = cJSON_AddStringToObject(info, "product_id", CONFIG_AI_PRODUCT_ID) &&
             cJSON_AddStringToObject(info, "product_key", CONFIG_AI_PRODUCT_KEY) &&
             cJSON_AddStringToObject(info, "product_secret", CONFIG_AI_PRODUCT_SECRET) &&
             cJSON_AddStringToObject(info, "device_name", device);
    if (ok && CONFIG_AI_SERVER_URL[0])
        ok = cJSON_AddStringToObject(ws, "url", CONFIG_AI_SERVER_URL) != NULL;
    if (ok) ok = cJSON_AddNumberToObject(audio, "codec", 0) != NULL;
    if (ok) ok = cJSON_PrintPreallocated(root, json, sizeof(json), false);
    cJSON_Delete(root);
    if (!ok) return -7;
    config->config_json = json;
    config->agent_id = CONFIG_AI_AGENT_ID;
    /* Keep server-side agent defaults. No voice catalog or personality UI. */
    config->startup_json = NULL;
    return 0;
}
