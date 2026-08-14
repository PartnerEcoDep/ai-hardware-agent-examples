#pragma once

#include <stdint.h>
#include "convai_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VOICE_COUNT    10
#define VOICE_NAME_LEN 32
#define VOICE_TYPE_LEN 64

/* 音色条目 */
typedef struct {
    const char *name;
    const char *voice_type;
} voice_entry_t;

/* Gender 分类 */
typedef enum {
  VOICE_GENDER_FEMALE = 0,
  VOICE_GENDER_MALE,
  VOICE_GENDER_ROBOT,
  VOICE_GENDER_COUNT
} voice_gender_t;

/* 获取音色列表及数量 */
const voice_entry_t *voice_config_get_list(void);
int voice_config_count(void);

/* Gender 分类查询 */
voice_gender_t voice_config_get_gender(int voice_id);
const char    *voice_config_get_gender_name(voice_gender_t gender);
int            voice_config_get_gender_voice_count(voice_gender_t gender);
const char    *voice_config_get_gender_voice_name(voice_gender_t gender,
                                                  int idx);
int            voice_config_get_gender_voice_id(voice_gender_t gender, int idx);

/* 从 NVS 加载当前音色索引（0-based），默认 0 */
int  voice_config_init(void);

/* 获取当前索引和 voice_type 字符串 */
int  voice_config_get(void);
const char *voice_config_get_type(void);

/* 切换音色：存 NVS + 调 convai_update，engine 可为 NULL（仅存） */
int  voice_config_set(convai_engine_t engine, int voice_id);

/* 生成完整 startup JSON（含 llm_config + tts_config） */
int  voice_config_build_json(char *buf, size_t size, const char *system_message);

#ifdef __cplusplus
}
#endif
