/**
 * @file audio_init.h
 * @brief Audio subsystem bring-up wrapper.
 *
 * Selects a codec through the @c audio_codec_t factory, so callers work
 * against the abstract interface instead of the board-specific driver.
 */

#ifndef AUDIO_INIT_H
#define AUDIO_INIT_H

#include "audio_codec.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register and initialize the board's audio codec.
 * @return ESP_OK on success, an esp_err_t error code otherwise.
 */
esp_err_t audio_init(void);

/**
 * @brief Get the codec initialized by audio_init().
 * @return Active codec, or NULL when audio_init() has not succeeded yet.
 */
audio_codec_t *audio_codec_active(void);

/** Default hardware playback volume in percent. */
#define AUDIO_VOLUME_DEFAULT 70U
/** Minimum hardware playback volume in percent. */
#define AUDIO_VOLUME_MIN 0U
/** Maximum hardware playback volume in percent. */
#define AUDIO_VOLUME_MAX 100U

/**
 * @brief Set hardware playback volume.
 * @param pct Volume in percent, clamped to 0..100.
 * @return ESP_OK on success.
 */
esp_err_t audio_set_volume(uint8_t pct);

/**
 * @brief Get the last applied hardware playback volume.
 * @return Volume in percent, 0..100.
 */
uint8_t audio_get_volume(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_INIT_H */
