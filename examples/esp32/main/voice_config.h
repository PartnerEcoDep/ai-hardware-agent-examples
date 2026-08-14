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

/* 获取音色列表及数量 */
const voice_entry_t *voice_config_get_list(void);
int voice_config_count(void);

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
