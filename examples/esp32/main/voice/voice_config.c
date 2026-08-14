#include "voice_config.h"
#include "convai_api.h"  /* convai_update */
#include "convai_bridge.h" /* startup config + engine access */
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "voice_cfg";

/* ===================================================================
 *  音色表：name 和 voice_type 下标一一对应
 * =================================================================== */
static const voice_entry_t s_voices[VOICE_COUNT] = {
    { "温柔少女", "Chinese (Mandarin)_Warm_Girl",
      "温柔自然 适合助手播报", "温柔 自然", "F01" },
    { "害羞女孩", "Chinese (Mandarin)_BashfulGirl",
      "甜美清新 听感舒适", "甜美 清新", "F02" },
    { "热心女孩", "Chinese (Mandarin)_Warm_HeartedGirl",
      "亲和力强 适合陪伴", "亲和 暖心", "F03" },
    { "花甲奶奶", "Chinese (Mandarin)_Kind-hearted_Elder",
      "沉稳厚重 值得信赖", "沉稳 厚重", "F04" },
    { "温润男声", "Chinese (Mandarin)_Gentleman",
      "清晰明亮 适合日常交流", "清晰 明亮", "M01" },
    { "搞笑大爷", "Chinese (Mandarin)_Humorous_Elder",
      "风趣幽默 轻松活泼", "风趣 幽默", "M02" },
    { "嘴硬竹马", "Chinese (Mandarin)_Stubborn_Friend",
      "个性鲜明 充满活力", "个性 活力", "M03" },
    { "邻家弟弟", "Chinese (Mandarin)_Pure-hearted_Boy",
      "纯净真诚 清朗自然", "纯净 真诚", "M04" },
    { "憨憨萌兽", "Chinese (Mandarin)_Cute_Spirit",
      "俏皮可爱 灵动有趣", "俏皮 灵动", "M05" },
    { "机械战甲", "Robot_Armor",
      "科技感强 机械质感", "科技 机械", "R01" },
};

static int s_voice_id = 0;  /* 当前音色索引 */

/* ===================================================================
 *  Gender → voice_id 映射表
 *  第0维=VOICE_GENDER_FEMALE, 第1维=VOICE_GENDER_MALE, 第2维=VOICE_GENDER_ROBOT
 * =================================================================== */
static const int s_gender_voices[VOICE_GENDER_COUNT][6] = {
    { 0, 1, 2, 3, -1, -1 },    /* female: 温柔少女/害羞女孩/热心女孩/花甲奶奶 */
    { 4, 5, 6, 7, 8, 9 },      /* male:   温润男声/搞笑大爷/嘴硬竹马/邻家弟弟/憨憨萌兽/机械战甲 */
    { -1, -1, -1, -1, -1, -1 }, /* robot:  (merged into male) */
};

static const char *s_gender_names[VOICE_GENDER_COUNT] = {
    "女声",
    "男声",
    "机器",
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

    int old_id = s_voice_id;
    s_voice_id = voice_id;
    printf("[%s] set voice_id=%d -> %s\n", TAG,
           voice_id, s_voices[voice_id].name);

    /* 构建 JSON */
    char json[512];
    int n = voice_config_build_json(json, sizeof(json),
                                    "你的名字叫小荷，你可以帮小朋友解决小烦恼哦。");
    if (n <= 0) {
        s_voice_id = old_id;
        return -1;
    }

    /* 参考 goldieos: 新配置总是先存到 bridge, 下次 start() 时使用 */
    convai_bridge_set_startup_config(json);

    /* 会话未连接时 SDK 的 convai_update 会返回 INVALID_STATE, 只保存配置 */
    if (!convai_bridge_is_started()) {
        printf("[%s] session not started, config saved for next connect\n", TAG);
        nvs_save_voice_id(voice_id);
        return 0;
    }

    /* 会话已连接: 立即通过 session.update 生效 */
    int ret = convai_update(engine ? engine : convai_bridge_get_engine(), json);
    if (ret != 0) {
        printf("[%s] convai_update failed: %d\n", TAG, ret);
        s_voice_id = old_id; /* 回滚, 与 goldieos restore_ai_config 一致 */
        return -1;
    }

    nvs_save_voice_id(voice_id);
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
