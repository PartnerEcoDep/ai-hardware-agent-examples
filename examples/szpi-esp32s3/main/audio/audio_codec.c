/**
 * @file audio/audio_codec.c
 * @brief Audio codec registry implementation.
 *
 * Concrete codecs (one per supported board) call
 * audio_codec_factory_register() during startup; audio_init.c then picks the
 * one matching the compiled board without knowing its implementation.
 */

#include "audio_codec.h"

#include "esp_log.h"

#include <string.h>

static const char *TAG = "audio_codec";

static audio_codec_t *s_codecs[AUDIO_CODEC_MAX_REGISTERED];
static int s_codec_count = 0;

esp_err_t audio_codec_factory_register(audio_codec_t *codec) {
  if (codec == NULL || codec->name == NULL || codec->init == NULL) {
    ESP_LOGE(TAG, "register: invalid codec");
    return ESP_ERR_INVALID_ARG;
  }
  /* Re-registering the same instance is a no-op (init may run twice). */
  for (int i = 0; i < s_codec_count; i++) {
    if (s_codecs[i] == codec) {
      return ESP_OK;
    }
  }
  if (s_codec_count >= AUDIO_CODEC_MAX_REGISTERED) {
    ESP_LOGE(TAG, "register: registry full (%d)", AUDIO_CODEC_MAX_REGISTERED);
    return ESP_ERR_NO_MEM;
  }
  s_codecs[s_codec_count++] = codec;
  ESP_LOGI(TAG, "registered codec: %s", codec->name);
  return ESP_OK;
}

audio_codec_t *audio_codec_factory_get(const char *name) {
  if (s_codec_count == 0) {
    return NULL;
  }
  if (name == NULL) {
    return s_codecs[0];
  }
  for (int i = 0; i < s_codec_count; i++) {
    if (strcmp(s_codecs[i]->name, name) == 0) {
      return s_codecs[i];
    }
  }
  return NULL;
}
