/**
 * @file voice_factory.h
 * @brief Voice descriptor factory (registration pattern).
 *
 * The built-in 10-voice / gender-classified set lives in @c voice_config.c.
 * This factory wraps it behind a stable interface so future voice providers
 * (cloud catalogs, user presets) can be plugged in without touching callers.
 *
 * UI code must use the @c voice_factory_* helpers below and never call
 * @c voice_config_* directly.
 */

#ifndef VOICE_FACTORY_H
#define VOICE_FACTORY_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "voice_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A registered provider of voice descriptors. */
typedef struct voice_factory_s {
  const char *name; /**< Factory name (e.g. "builtin-10"). */

  /* ---- Catalog ---- */
  const voice_entry_t *(*get)(int index);
  int (*count)(void);

  /* ---- Gender grouping (used by the voice selector UI) ---- */
  voice_gender_t (*get_gender)(int voice_id);
  const char *(*gender_name)(voice_gender_t gender);
  int (*gender_voice_count)(voice_gender_t gender);
  const char *(*gender_voice_name)(voice_gender_t gender, int idx);
  int (*gender_voice_id)(voice_gender_t gender, int idx);

  /* ---- Current selection ---- */
  int (*init)(void);
  int (*current_id)(void);
  int (*select)(convai_engine_t engine, int voice_id);

  /* ---- Startup payload ---- */
  int (*build_config_json)(char *buf, size_t size, const char *system_message);
} voice_factory_t;

/**
 * @brief Register a voice factory (replaces any previous registration).
 * @param factory Provider to register (must not be NULL).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p factory is NULL.
 */
esp_err_t voice_factory_register(const voice_factory_t *factory);

/**
 * @brief Get the active voice factory.
 * @return The registered factory, or the built-in default if none registered.
 */
const voice_factory_t *voice_factory_default(void);

/** @brief Load the persisted selection (delegates to active factory). */
int voice_factory_init(void);

/** @brief Get voice descriptor by index (delegates to active factory). */
const voice_entry_t *voice_factory_get(int index);

/** @brief Get total voice count (delegates to active factory). */
int voice_factory_count(void);

/** @brief Get gender of a voice id (delegates to active factory). */
voice_gender_t voice_factory_get_gender(int voice_id);

/** @brief Get the display name of a gender group. */
const char *voice_factory_gender_name(voice_gender_t gender);

/** @brief Get how many voices belong to a gender group. */
int voice_factory_gender_voice_count(voice_gender_t gender);

/** @brief Get the display name of the @p idx -th voice in a gender group. */
const char *voice_factory_gender_voice_name(voice_gender_t gender, int idx);

/** @brief Get the global voice id of the @p idx -th voice in a group. */
int voice_factory_gender_voice_id(voice_gender_t gender, int idx);

/** @brief Get the currently selected voice id. */
int voice_factory_current_id(void);

/**
 * @brief Select a voice (persist + push to the engine when non-NULL).
 * @param engine   SDK engine handle, or NULL to only persist.
 * @param voice_id Voice id to activate.
 * @return 0 on success.
 */
int voice_factory_select(convai_engine_t engine, int voice_id);

/** @brief Build startup JSON (delegates to active factory). */
int voice_factory_build_config_json(char *buf, size_t size,
                                    const char *system_message);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_FACTORY_H */
