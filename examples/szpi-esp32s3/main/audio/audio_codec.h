/**
 * @file audio_codec.h
 * @brief Abstract audio codec interface (factory pattern).
 *
 * Each supported board registers a concrete @c audio_codec_t instance
 * (e.g. @c audio_codec_lckfb_szpi). The application selects a codec at
 * init time through the factory without depending on a specific hardware
 * implementation, leaving room for future boards (e.g. M5Stack Core2).
 */

#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration of the abstract codec interface. */
typedef struct audio_codec_s audio_codec_t;

/** Abstract audio codec operations. */
struct audio_codec_s {
  const char *name;  /**< Human-readable codec name (e.g. "lckfb-szpi"). */

  /**
   * @brief Initialize codec hardware.
   * @param self      Codec instance.
   * @param bus       Initialized I2C master bus.
   * @param pa_enable Power-amplifier enable callback (may be NULL).
   * @param pa_ctx    Context passed to @p pa_enable.
   * @return ESP_OK on success.
   */
  esp_err_t (*init)(audio_codec_t *self,
                    i2c_master_bus_handle_t bus,
                    void (*pa_enable)(int en, void *ctx),
                    void *pa_ctx);

  /**
   * @brief Set speaker volume.
   * @param self Codec instance.
   * @param pct  Volume 0..100.
   * @return ESP_OK on success.
   */
  esp_err_t (*set_volume)(audio_codec_t *self, uint8_t pct);

  /**
   * @brief Read captured PCM samples.
   * @param self   Codec instance.
   * @param buf    Destination buffer.
   * @param samples Number of bytes to read.
   * @return Bytes read (>=0) or negative on error.
   */
  int (*read)(audio_codec_t *self, void *buf, int samples);

  /**
   * @brief Write PCM samples for playback.
   * @param self   Codec instance.
   * @param buf    Source buffer.
   * @param samples Number of bytes to write.
   * @return Bytes written (>=0) or negative on error.
   */
  int (*write)(audio_codec_t *self, const void *buf, int samples);
};

/** Maximum number of codecs that can be registered simultaneously. */
#define AUDIO_CODEC_MAX_REGISTERED 4

/**
 * @brief Register a concrete codec implementation.
 * @param codec Codec instance with a non-NULL @c name and @c init.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a malformed codec,
 *         ESP_ERR_NO_MEM when the registry is full.
 */
esp_err_t audio_codec_factory_register(audio_codec_t *codec);

/**
 * @brief Look up a registered codec.
 * @param name Codec name, or NULL to take the first registered one.
 * @return Codec instance, or NULL if no match.
 */
audio_codec_t *audio_codec_factory_get(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CODEC_H */
