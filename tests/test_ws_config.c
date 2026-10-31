#include "ws_config.h"
#include "cJSON.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
static bool device_failed;
int ws_runtime_device_id(char *text, size_t capacity) {
    if (device_failed) return -1;
    assert(capacity > 12); strcpy(text, "test-device"); return 11;
}
static void *no_memory(size_t n) { (void)n; return NULL; }
int main(void) {
    ai_cloud_config_t cfg = {0};
    device_failed = true; assert(ws_cloud_config(&cfg) == -71);
    device_failed = false;
    cJSON_Hooks hooks = {no_memory, free}; cJSON_InitHooks(&hooks);
    assert(ws_cloud_config(&cfg) == -72); cJSON_InitHooks(NULL);
    assert(!ws_cloud_config(&cfg));
    cJSON *root = cJSON_Parse(cfg.config_json); assert(root);
    cJSON *info = cJSON_GetObjectItemCaseSensitive(root, "info");
    assert(!strcmp(cJSON_GetObjectItemCaseSensitive(info,"product_secret")->valuestring,"test-secret"));
    cJSON *ws = cJSON_GetObjectItemCaseSensitive(root,"ws");
    cJSON *codec = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(ws,"audio"),"codec");
    assert(cJSON_IsNumber(codec) && codec->valueint == 0);
    cJSON_Delete(root);
}
