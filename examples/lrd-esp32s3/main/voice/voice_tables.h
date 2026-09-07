/*
 * @file voice_tables.h
 * @brief 音色数据表 — 唯一数据源, 增删音色只改这里
 *
 * 每份供应商表是一个 voice_entry_t 数组, 长度由
 *   #define ACTIVE_VOICE_COUNT (sizeof(g_active_voice_table)/sizeof(voice_entry_t))
 * 自动推导, 不再手改 VOICE_COUNT.
 *
 * 用法:
 *   - 加音色:   数组末尾加一行 {name, voice_type, desc, tags, code}
 *   - 删音色:   删对应行 (数量自动变)
 *   - 切供应商: 改 g_active_voice_table 指向的表 (默认 iflytek)
 *
 * 当前激活: IFLYTEK (讯飞)
 * 停用保留: MiniMax (goldieos 旧表, 在文件底部)
 */
#pragma once

#include <stddef.h>
#include "voice_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 讯飞 (IFLYTEK) — 当前激活 ----
 * 来源: goldieos commit 99314f9 "fix: update IFLYTEK voices" */
static const voice_entry_t s_voice_table_iflytek[] = {
    { "小青",   "AISJINGER", "讯飞女声 自然清晰", "女声 自然", "F01" },
    { "小燕",   "X4_XIAOYAN", "讯飞女声 活泼明亮", "女声 活泼", "F02" },
    { "叶子",   "X4_YEZI", "讯飞女声 温柔沉稳", "女声 温柔", "F03" },
    { "许小宝", "AISBABYXU", "讯飞男声 阳光青年", "男声 阳光", "M01" },
    { "许久",   "AISJIUXU", "讯飞男声 沉稳成熟", "男声 沉稳", "M02" },
};

/* ---- MiniMax — 停用保留 ----
 * 原 ESP32 默音色, 需要回切时把下面指针改回本表即可 */
static const voice_entry_t s_voice_table_minimax[] = {
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

/* ---- 激活表 (改这里切供应商) ---- */
static const voice_entry_t *const g_active_voice_table = s_voice_table_iflytek;
#define ACTIVE_VOICE_COUNT \
  (sizeof(s_voice_table_iflytek) / sizeof(voice_entry_t))

/* ---- 当前激活表的 gender 映射 (voice_id 索引, -1 结尾) ----
 * 行对应 VOICE_GENDER_FEMALE/MALE. 新增音色后注意补 id. */
static const int g_active_gender_voices[VOICE_GENDER_COUNT][6] = {
    { 0, 1, 2, -1, -1, -1 },    /* female: 小青/小燕/叶子 */
    { 3, 4, -1, -1, -1, -1 },   /* male:   许小宝/许久 */
};

#ifdef __cplusplus
}
#endif