/**
 * @file audio_init.c
 * @brief Audio subsystem bring-up.
 *
 * Registers the board codec with the audio_codec factory, initializes it and
 * keeps the resulting handle for the rest of the application. Also owns the
 * power-amplifier callback that toggles PCA9557 bit1.
 */

#include "audio_init.h"

#include "audio_codec_lckfb.h"
#include "board_init.h"
#include "esp_log.h"

static const char *TAG = "audio_init";

/** Default playback volume applied right after codec bring-up (%). */
#define AUDIO_INIT_DEFAULT_VOLUME 70

/* Codec selected by audio_init(); NULL until initialization succeeds. */
static audio_codec_t *s_codec = NULL;

/* Power-amplifier enable callback wired into the codec init. */
static void pa_enable_cb(int en, void *ctx) {
  pca9557_t *pca = (pca9557_t *)ctx;
  pca9557_pa_en(pca, en ? 1 : 0);
}

esp_err_t audio_init(void) {
  esp_err_t err = audio_codec_lckfb_register();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "codec register failed: %s", esp_err_to_name(err));
    return err;
  }

  audio_codec_t *codec = audio_codec_factory_get(AUDIO_CODEC_LCKFB_SZPI_NAME);
  if (codec == NULL) {
    ESP_LOGE(TAG, "codec '%s' not registered", AUDIO_CODEC_LCKFB_SZPI_NAME);
    return ESP_ERR_NOT_FOUND;
  }

  err = codec->init(codec, g_i2c_bus, pa_enable_cb, &g_pca9557);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "codec init failed: %s", esp_err_to_name(err));
    return err;
  }

  s_codec = codec;

  /* Apply the default playback volume (legacy code set 70% here). */
  err = codec->set_volume(codec, AUDIO_INIT_DEFAULT_VOLUME);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "set default volume %d%% failed: %s",
             AUDIO_INIT_DEFAULT_VOLUME, esp_err_to_name(err));
  }

  ESP_LOGI(TAG, "audio subsystem ready (codec=%s)", codec->name);
  return ESP_OK;
}

audio_codec_t *audio_codec_active(void) { return s_codec; }
