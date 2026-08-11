/**
 * @file voice_factory.c
 * @brief Voice descriptor factory implementation.
 *
 * Wraps @c voice_config (built-in 10-voice set) behind the @c voice_factory_t
 * interface so callers never depend on the concrete source of voices.
 */

#include "voice_factory.h"

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "voice_factory";

static const voice_factory_t *s_factory = NULL;

/* ===================================================================
 *  Built-in provider (delegates to voice_config)
 * =================================================================== */
static const voice_entry_t *builtin_get(int index) {
  return &voice_config_get_list()[index];
}

static int builtin_count(void) { return voice_config_count(); }

static voice_gender_t builtin_get_gender(int voice_id) {
  return voice_config_get_gender(voice_id);
}

static const char *builtin_gender_name(voice_gender_t gender) {
  return voice_config_get_gender_name(gender);
}

static int builtin_gender_voice_count(voice_gender_t gender) {
  return voice_config_get_gender_voice_count(gender);
}

static const char *builtin_gender_voice_name(voice_gender_t gender, int idx) {
  return voice_config_get_gender_voice_name(gender, idx);
}

static int builtin_gender_voice_id(voice_gender_t gender, int idx) {
  return voice_config_get_gender_voice_id(gender, idx);
}

static int builtin_init(void) { return voice_config_init(); }

static int builtin_current_id(void) { return voice_config_get(); }

static int builtin_select(convai_engine_t engine, int voice_id) {
  return voice_config_set(engine, voice_id);
}

static int builtin_build(char *buf, size_t size, const char *system_message) {
  return voice_config_build_json(buf, size, system_message);
}

static const voice_factory_t s_builtin = {
    .name = "builtin-10",
    .get = builtin_get,
    .count = builtin_count,
    .get_gender = builtin_get_gender,
    .gender_name = builtin_gender_name,
    .gender_voice_count = builtin_gender_voice_count,
    .gender_voice_name = builtin_gender_voice_name,
    .gender_voice_id = builtin_gender_voice_id,
    .init = builtin_init,
    .current_id = builtin_current_id,
    .select = builtin_select,
    .build_config_json = builtin_build,
};

/* ===================================================================
 *  Registry + delegating helpers
 * =================================================================== */
esp_err_t voice_factory_register(const voice_factory_t *factory) {
  if (factory == NULL) {
    ESP_LOGE(TAG, "register: NULL factory");
    return ESP_ERR_INVALID_ARG;
  }
  s_factory = factory;
  ESP_LOGI(TAG, "registered voice factory: %s", factory->name);
  return ESP_OK;
}

const voice_factory_t *voice_factory_default(void) {
  return s_factory ? s_factory : &s_builtin;
}

int voice_factory_init(void) { return voice_factory_default()->init(); }

const voice_entry_t *voice_factory_get(int index) {
  return voice_factory_default()->get(index);
}

int voice_factory_count(void) { return voice_factory_default()->count(); }

voice_gender_t voice_factory_get_gender(int voice_id) {
  return voice_factory_default()->get_gender(voice_id);
}

const char *voice_factory_gender_name(voice_gender_t gender) {
  return voice_factory_default()->gender_name(gender);
}

int voice_factory_gender_voice_count(voice_gender_t gender) {
  return voice_factory_default()->gender_voice_count(gender);
}

const char *voice_factory_gender_voice_name(voice_gender_t gender, int idx) {
  return voice_factory_default()->gender_voice_name(gender, idx);
}

int voice_factory_gender_voice_id(voice_gender_t gender, int idx) {
  return voice_factory_default()->gender_voice_id(gender, idx);
}

int voice_factory_current_id(void) {
  return voice_factory_default()->current_id();
}

int voice_factory_select(convai_engine_t engine, int voice_id) {
  return voice_factory_default()->select(engine, voice_id);
}

int voice_factory_build_config_json(char *buf, size_t size,
                                    const char *system_message) {
  return voice_factory_default()->build_config_json(buf, size, system_message);
}
