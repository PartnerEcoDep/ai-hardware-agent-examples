/**
 * @file audio_init.c
 * @brief Audio subsystem bring-up.
 *
 * Registers the board codec with the audio_codec factory, initializes it and
 * keeps the resulting handle for the rest of the application. Also owns the
 * power-amplifier callback that toggles TPT29555A AMP_CTRL (P1_0).
 */

#include "audio_init.h"

#include "audio_codec_lckfb.h"
#include "board_init.h"
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "audio_init";

/* Codec selected by audio_init(); NULL until initialization succeeds. */
static audio_codec_t *s_codec = NULL;

/* Last applied hardware playback volume (%). */
static uint8_t s_volume = AUDIO_VOLUME_DEFAULT;

/* Pending NVS save flag. Set by audio_set_volume() (which may run under the
 * LVGL lock) and flushed by audio_volume_flush() from the main loop, so the
 * slow flash commit never blocks the LVGL task. */
static volatile bool s_volume_dirty = false;

/* NVS namespace/key used to persist the hardware playback volume. */
#define AUDIO_NVS_NAMESPACE "audio"
#define AUDIO_NVS_VOLUME_KEY "volume"

static uint8_t audio_volume_load(void) {
  nvs_handle_t handle;
  uint8_t volume = AUDIO_VOLUME_DEFAULT;
  if (nvs_open(AUDIO_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
    return volume;
  }
  if (nvs_get_u8(handle, AUDIO_NVS_VOLUME_KEY, &volume) != ESP_OK) {
    volume = AUDIO_VOLUME_DEFAULT;
  }
  nvs_close(handle);
  if (volume > AUDIO_VOLUME_MAX) {
    volume = AUDIO_VOLUME_MAX;
  }
  return volume;
}

static void audio_volume_save(uint8_t volume) {
  nvs_handle_t handle;
  if (nvs_open(AUDIO_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
    return;
  }
  nvs_set_u8(handle, AUDIO_NVS_VOLUME_KEY, volume);
  nvs_commit(handle);
  nvs_close(handle);
}

/* Power-amplifier enable callback wired into the codec init.
 * TPT29555A 引脚需先设方向为输出, 再写电平 (默认全输入)。 */
static void pa_enable_cb(int en, void *ctx) {
  tpt29555a_t *ioex = (tpt29555a_t *)ctx;
  tpt29555a_set_direction(ioex, IOEX_AMP_CTRL_PORT, IOEX_AMP_CTRL_PIN, true);
  tpt29555a_set_output(ioex, IOEX_AMP_CTRL_PORT, IOEX_AMP_CTRL_PIN,
                       en ? true : false);
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

  err = codec->init(codec, g_i2c_bus, pa_enable_cb, &g_tpt29555a);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "codec init failed: %s", esp_err_to_name(err));
    return err;
  }

  s_codec = codec;

  /* Restore the persisted hardware playback volume. */
  s_volume = audio_volume_load();
  err = codec->set_volume(codec, s_volume);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "set restored volume %u%% failed: %s",
             (unsigned)s_volume, esp_err_to_name(err));
  }

  ESP_LOGI(TAG, "audio subsystem ready (codec=%s)", codec->name);
  return ESP_OK;
}

audio_codec_t *audio_codec_active(void) { return s_codec; }

esp_err_t audio_set_volume(uint8_t pct) {
  if (pct > AUDIO_VOLUME_MAX) {
    pct = AUDIO_VOLUME_MAX;
  }
  if (s_codec == NULL || s_codec->set_volume == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t err = s_codec->set_volume(s_codec, pct);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "set volume %u%% failed: %s",
             (unsigned)pct, esp_err_to_name(err));
    return err;
  }

  s_volume = pct;
  s_volume_dirty = true;  /* NVS commit deferred to audio_volume_flush() */
  ESP_LOGI(TAG, "hardware volume -> %u%%", (unsigned)pct);
  return ESP_OK;
}

uint8_t audio_get_volume(void) { return s_volume; }

void audio_volume_flush(void) {
  if (!s_volume_dirty) {
    return;
  }
  s_volume_dirty = false;
  uint8_t v = s_volume;
  audio_volume_save(v);
}
