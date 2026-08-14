/**
 * @file sdk_init.c
 * @brief SDK bring-up: platform HAL, engine creation and audio path.
 *
 * Pulled out of the former monolithic main.c. Owns the @c g_engine handle
 * and all SDK event/status callbacks plus the microphone capture task.
 */

#include "sdk_init.h"

#include "ai_chat_ui.h"
#include "audio_init.h"
#include "board_init.h"
#include "board_lckfb_szpi.h"
#include "convai_bridge_defaults.h"
#include "convai_platform_esp32.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "sdk_init";

/* Shared SDK engine handle. */
convai_engine_t g_engine = NULL;

/* ===================================================================
 *  SDK event callbacks
 * =================================================================== */
static void on_sdk_event(convai_engine_t engine,
                         convai_event_t *event, void *user_data) {
  (void)engine;
  (void)user_data;
  static const char *names[] = {
      [CONVAI_EV_CONNECTED]    = "CONNECTED",
      [CONVAI_EV_DISCONNECTED] = "DISCONNECTED",
      [CONVAI_EV_FAILED]       = "FAILED",
      [CONVAI_EV_UPDATED]      = "UPDATED",
  };
  const char *name = (event->code < (int)(sizeof(names) /
                          sizeof(names[0])))
                          ? names[event->code]
                          : "UNKNOWN";
  ESP_LOGI(TAG, "SDK event: %s", name);

  switch (event->code) {
    case CONVAI_EV_CONNECTED:
    case CONVAI_EV_DISCONNECTED:
    case CONVAI_EV_FAILED:
    default:
      break;
  }
}

static void on_conversation_status(convai_engine_t engine,
                                   convai_status_e status, void *user_data) {
  (void)engine;
  (void)user_data;
  static const char *names[] = {
      [CONVAI_STATUS_IDLE]          = "IDLE",
      [CONVAI_STATUS_LISTENING]     = "LISTENING",
      [CONVAI_STATUS_THINKING]      = "THINKING",
      [CONVAI_STATUS_ANSWERING]     = "ANSWERING",
      [CONVAI_STATUS_INTERRUPTED]   = "INTERRUPTED",
      [CONVAI_STATUS_ANSWER_FINISHED] = "ANSWER_FINISHED",
  };
  const char *name = (status < (int)(sizeof(names) /
                          sizeof(names[0])))
                          ? names[status]
                          : "UNKNOWN";
  ESP_LOGI(TAG, "Status: %s", name);

  switch (status) {
    case CONVAI_STATUS_LISTENING:
      board_led_set(1);
      ai_chat_ui_set_state(STATE_LISTENING);
      break;
    case CONVAI_STATUS_THINKING:
      board_led_set(1);
      ai_chat_ui_set_state(STATE_THINKING);
      break;
    case CONVAI_STATUS_ANSWERING:
      board_led_set(0);
      ai_chat_ui_set_state(STATE_SPEAKING);
      break;
    case CONVAI_STATUS_IDLE:
    case CONVAI_STATUS_ANSWER_FINISHED:
    default:
      board_led_set(0);
      ai_chat_ui_set_state(STATE_IDLE);
      break;
  }
}

static void on_audio_data(convai_engine_t engine,
                          const void *data, size_t len,
                          const convai_audio_frame_info_t *info,
                          void *user_data) {
  (void)engine;
  (void)info;
  (void)user_data;
  audio_codec_t *codec = audio_codec_active();
  if (codec != NULL) {
    codec->write(codec, data, (int)len);
  }
}

static void on_message_data(convai_engine_t engine,
                            const void *data, size_t len,
                            const convai_message_info_t *info,
                            void *user_data) {
  (void)engine;
  (void)info;
  (void)user_data;
  ESP_LOGI(TAG, "Message: %.*s", (int)len, (const char *)data);
}

/* ===================================================================
 *  G.711 A-law decode + audio level metering
 * =================================================================== */
static int16_t alaw_to_pcm(uint8_t a) {
  a ^= 0x55;
  int sign = a & 0x80;
  int seg = (a >> 4) & 0x07;
  int low = a & 0x0F;
  int16_t pcm;
  if (seg == 0) {
    pcm = (int16_t)((low << 4) | 0x008);
  } else {
    pcm = (int16_t)((low + 0x10) << (seg + 3));
  }
  return sign ? (int16_t)-pcm : pcm;
}

static uint8_t compute_audio_level(const uint8_t *g711a, size_t n) {
  static uint8_t smooth = 0;
  if (n == 0) {
    return 0;
  }
  int16_t peak = 0;
  for (size_t i = 0; i < n; i++) {
    int16_t s = alaw_to_pcm(g711a[i]);
    if (s < 0) {
      s = (int16_t)-s;
    }
    if (s > peak) {
      peak = s;
    }
  }
  uint8_t v = (uint8_t)(peak * 100 / 32767);
  smooth = (uint8_t)(smooth * 6 / 10 + v * 4 / 10); /* EMA 0.6/0.4 */
  return smooth;
}

/* 20ms G.711 frame at 24 kHz. */
#define CAPTURE_BUF_SIZE (AUDIO_SAMPLE_RATE * 2 / 50)

static void audio_capture_task(void *arg) {
  (void)arg;
  audio_codec_t *codec = audio_codec_active();
  if (codec == NULL) {
    ESP_LOGE(TAG, "capture task: no active codec");
    vTaskDelete(NULL);
    return;
  }
  uint8_t *buf = (uint8_t *)malloc(CAPTURE_BUF_SIZE);
  if (buf == NULL) {
    vTaskDelete(NULL);
    return;
  }

  int hb_cnt = 0;
  while (1) {
    int received = codec->read(codec, buf, CAPTURE_BUF_SIZE);
    if (received > 0 && g_engine) {
      convai_audio_frame_info_t info = {.data_type =
                                            CONVAI_AUDIO_DATA_TYPE_G711A};
      ai_chat_ui_update_volume(compute_audio_level(buf, (size_t)received));
      convai_send_audio(g_engine, buf, (size_t)received, &info);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if (++hb_cnt >= 3000) { /* ~30s */
      hb_cnt = 0;
      ESP_LOGI(TAG, "audio heartbeat: free_heap=%u",
               (unsigned)esp_get_free_heap_size());
    }
  }
  free(buf);
  vTaskDelete(NULL);
}

/* ===================================================================
 *  SDK bring-up
 * =================================================================== */
esp_err_t sdk_init(void) {
  /* Install the platform HAL through the factory so the implementation can
   * be swapped (e.g. for a host-side mock) without touching this file. */
  esp_err_t err = convai_platform_esp32_register();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "platform register failed: %s", esp_err_to_name(err));
    return err;
  }
  err = platform_factory_init_by_name(CONVAI_PLATFORM_ESP32_NAME);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "platform init failed: %s", esp_err_to_name(err));
    return err;
  }

  char config_json[2048];
  bridge_build_config_json(config_json, sizeof(config_json), NULL);

  convai_event_handler_t handler = {
      .on_convai_event = on_sdk_event,
      .on_convai_conversation_status = on_conversation_status,
      .on_convai_audio_data = on_audio_data,
      .on_convai_message_data = on_message_data,
  };

  /* SDK calls use the ConvAI return convention, not esp_err_t. */
  int sdk_ret = convai_create(&g_engine, config_json, &handler, NULL);
  if (sdk_ret != CONVAI_OK) {
    ESP_LOGE(TAG, "convai_create failed: %d", sdk_ret);
    return ESP_FAIL;
  }

  convai_opt_t opt = {
      .mode = CONVAI_MODE_WS,
      .agent_id = bridge_get_default_agent_id(),
      .params = bridge_get_default_startup_config(),
  };
  sdk_ret = convai_start(g_engine, &opt);
  if (sdk_ret != CONVAI_OK) {
    ESP_LOGE(TAG, "convai_start failed: %d", sdk_ret);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "SDK started (v%s)", convai_get_version());
  xTaskCreate(audio_capture_task, "audio_cap", 8192, NULL, 5, NULL);
  return ESP_OK;
}
