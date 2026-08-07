#include "voice_config.h"
#include "convai_api.h"  /* convai_update */
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "voice_cfg";

/* ===================================================================
 *  音色表：name 和 voice_type 下标一一对应
 * =================================================================== */
static const voice_entry_t s_voices[VOICE_COUNT] = {
    { "Warm Girl",      "Chinese (Mandarin)_Warm_Girl"       },
    { "Bashful Girl",   "Chinese (Mandarin)_BashfulGirl"      },
    { "Hearted Girl",   "Chinese (Mandarin)_Warm_HeartedGirl" },
    { "Kind Elder",     "Chinese (Mandarin)_Kind-hearted_Elder"},
    { "Gentleman",      "Chinese (Mandarin)_Gentleman"        },
    { "Humorous Man",   "Chinese (Mandarin)_Humorous_Elder"   },
    { "Stubborn Boy",   "Chinese (Mandarin)_Stubborn_Friend"  },
    { "Pure Boy",       "Chinese (Mandarin)_Pure-hearted_Boy" },
    { "Cute Spirit",    "Chinese (Mandarin)_Cute_Spirit"     },
    { "Robot Armor",    "Robot_Armor"                         },
};

static int s_voice_id = 0;  /* 当前音色索引 */

const voice_entry_t *voice_config_get_list(void) {
    return s_voices;
}

int voice_config_count(void) {
    return VOICE_COUNT;
}

/* ---- NVS ---- */
static const char *NVS_NS = "voice";
static const char *NVS_KEY = "voice_id";

static int nvs_load_voice_id(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &handle);
    if (err != ESP_OK) return 0;

    int32_t val = 0;
    err = nvs_get_i32(handle, NVS_KEY, &val);
    nvs_close(handle);

    if (err != ESP_OK || val < 0 || val >= VOICE_COUNT) return 0;
    return (int)val;
}

static void nvs_save_voice_id(int id) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_i32(handle, NVS_KEY, (int32_t)id);
    nvs_commit(handle);
    nvs_close(handle);
}

/* ---- 公开 API ---- */

int voice_config_init(void) {
    s_voice_id = nvs_load_voice_id();
    printf("[%s] loaded voice_id=%d -> %s\n", TAG,
           s_voice_id, s_voices[s_voice_id].name);
    return s_voice_id;
}

int voice_config_get(void) {
    return s_voice_id;
}

const char *voice_config_get_type(void) {
    return s_voices[s_voice_id].voice_type;
}

int voice_config_set(convai_engine_t engine, int voice_id) {
    if (voice_id < 0 || voice_id >= VOICE_COUNT) return -1;

    s_voice_id = voice_id;
    nvs_save_voice_id(voice_id);
    printf("[%s] set voice_id=%d -> %s\n", TAG,
           voice_id, s_voices[voice_id].name);

    /* 构建 JSON */
    char json[512];
    int n = voice_config_build_json(json, sizeof(json),
                                    "你的名字叫小荷，你可以帮小朋友解决小烦恼哦。");
    if (n <= 0) return -1;

    /* 运行时更新引擎（engine 可为 NULL，仅存储） */
    if (engine) {
        int ret = convai_update(engine, json);
        if (ret != 0) {
            printf("[%s] convai_update failed: %d\n", TAG, ret);
            return -1;
        }
        printf("[%s] convai_update OK\n", TAG);
    }

    return 0;
}

int voice_config_build_json(char *buf, size_t size,
                            const char *system_message) {
    const char *vt = s_voices[s_voice_id].voice_type;
    return snprintf(buf, size,
        "{"
          "\"config\":{"
            "\"llm_config\":{"
              "\"system_messages\":[\"%s\"]"
            "},"
            "\"tts_config\":{"
              "\"provider_params\":{"
                "\"audio\":{"
                  "\"voice_type\":\"%s\""
                "}"
              "}"
            "}"
          "}"
        "}",
        system_message, vt);
}
