#include "voice_config.h"
#include "voice_tables.h"   /* 音色数据源 + gender 映射 (增删音色只改这里) */
#include "convai_api.h"  /* convai_update */
#include "convai_bridge.h" /* startup config + engine access */
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "voice_cfg";

/* 默认人设/系统提示词，与 convai_bridge_defaults.c 的 DEFAULT_STARTUP_CONFIG 保持一致 */
#define DEFAULT_SYSTEM_MESSAGE "你的名字叫小荷，你可以帮小朋友解决小烦恼哦。"

/* 激活表 (voice_tables.h): 表指针 + 数量 + gender 映射 */
#define s_voices            g_active_voice_table
#define s_gender_voices     g_active_gender_voices
#define VOICE_COUNT         ACTIVE_VOICE_COUNT

static int s_voice_id = 0;  /* 当前音色索引 */

/* 性别名: 仅 F/M 两档 */
static const char *s_gender_names[VOICE_GENDER_COUNT] = {
    "女声",
    "男声",
};

const voice_entry_t *voice_config_get_list(void) {
    return s_voices;
}

int voice_config_count(void) {
    return VOICE_COUNT;
}

voice_gender_t voice_config_get_gender(int voice_id) {
    for (int g = 0; g < VOICE_GENDER_COUNT; g++) {
        for (int i = 0; i < 6 && s_gender_voices[g][i] >= 0; i++) {
            if (s_gender_voices[g][i] == voice_id) return (voice_gender_t)g;
        }
    }
    return VOICE_GENDER_FEMALE;
}

const char *voice_config_get_gender_name(voice_gender_t gender) {
    if (gender < 0 || gender >= VOICE_GENDER_COUNT) return "Unknown";
    return s_gender_names[gender];
}

int voice_config_get_gender_voice_count(voice_gender_t gender) {
    if (gender < 0 || gender >= VOICE_GENDER_COUNT) return 0;
    int cnt = 0;
    while (cnt < 6 && s_gender_voices[gender][cnt] >= 0) cnt++;
    return cnt;
}

const char *voice_config_get_gender_voice_name(voice_gender_t gender, int idx) {
    if (gender < 0 || gender >= VOICE_GENDER_COUNT) return "?";
    if (idx < 0 || idx >= 6) return "?";
    int vid = s_gender_voices[gender][idx];
    if (vid < 0) return "?";
    return s_voices[vid].name;
}

int voice_config_get_gender_voice_id(voice_gender_t gender, int idx) {
    if (gender < 0 || gender >= VOICE_GENDER_COUNT) return -1;
    if (idx < 0 || idx >= 6) return -1;
    return s_gender_voices[gender][idx];
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

/* NVS 持久化 (flash 写会禁用 cache), 只能在内部 RAM 栈的任务里调用。
 * voice_apply_task 是 PSRAM 栈, 不能在这里做 flash 写; 由 LVGL 事件回调调用。 */
void voice_config_persist(int voice_id) {
    nvs_save_voice_id(voice_id);
}

/* ---- 公开 API ---- */

int voice_config_init(void) {
    s_voice_id = nvs_load_voice_id();
    printf("[%s] loaded voice_id=%d -> %s\n", TAG,
           s_voice_id, s_voices[s_voice_id].name);

    /* 同步到 bridge 的 startup config, 确保引擎启动时用的是 NVS 保存的音色;
     * 否则 g_startup_config 为空, 引擎会回退到硬编码默认音色, 与 UI 显示不一致 */
    char json[512];
    if (voice_config_build_json(json, sizeof(json), DEFAULT_SYSTEM_MESSAGE) > 0) {
        convai_bridge_set_startup_config(json);
    }

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

    int old_id = s_voice_id;
    s_voice_id = voice_id;
    printf("[%s] set voice_id=%d -> %s\n", TAG,
           voice_id, s_voices[voice_id].name);

    /* 构建 JSON */
    char json[512];
    int n = voice_config_build_json(json, sizeof(json), DEFAULT_SYSTEM_MESSAGE);
    if (n <= 0) {
        s_voice_id = old_id;
        return -1;
    }

    /* 参考 goldieos: 新配置总是先存到 bridge, 下次 start() 时使用 */
    convai_bridge_set_startup_config(json);

    /* 会话未连接时 SDK 的 convai_update 会返回 INVALID_STATE, 只保存配置 */
    if (!convai_bridge_is_started()) {
        printf("[%s] session not started, config saved for next connect\n", TAG);
        return 0;
    }

    /* 会话已连接: 立即通过 session.update 生效 */
    int ret = convai_update(engine ? engine : convai_bridge_get_engine(), json);
    if (ret != 0) {
        printf("[%s] convai_update failed: %d\n", TAG, ret);
        s_voice_id = old_id; /* 回滚, 与 goldieos restore_ai_config 一致 */
        return -1;
    }

    printf("[%s] convai_update OK\n", TAG);

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
