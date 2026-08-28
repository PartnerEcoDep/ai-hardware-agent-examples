/**
 * @file convai_bridge.c
 * @brief ESP32 implementation of the convai_bridge API.
 *
 * Owns:
 *   - the SDK engine handle
 *   - SDK event/status/audio/message callbacks
 *   - the mic-capture FreeRTOS task (lifecycle bound to start/stop)
 *   - uplink statistics (frames sent / dropped)
 *   - state mirroring for the public getters
 *   - the configurable startup_config and device_name slots
 *
 * Does NOT call platform-specific init — the app layer (sdk_init.c)
 * installs the platform HAL before calling convai_bridge_init().
 */
#include "convai_bridge.h"

#include "ai_chat_ui.h"
#include "audio_init.h"
#include "board_init.h"
#include "board_lckfb_szpi.h"
#include "convai_bridge_defaults.h"
#include "convai_codec_g711a.h"
#include "convai_config_file.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/idf_additions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "convai_bridge";

/* ---- internal state ---- */
static convai_engine_t          g_engine        = NULL;
static convai_status_e          g_status        = CONVAI_STATUS_IDLE;
static volatile int             g_started       = 0;
static convai_bridge_status_cb  g_status_cb     = NULL;
static convai_bridge_event_cb   g_event_cb      = NULL;
static convai_bridge_message_cb g_message_cb    = NULL;

/* Device name injected by app layer (e.g. WiFi MAC). NULL -> use default. */
#define DEVICE_NAME_MAX  64
static char g_device_name[DEVICE_NAME_MAX] = {0};

/* Startup config (set by settings UI, consumed by start). */
#define STARTUP_CONFIG_MAX  2048
static char g_startup_config[STARTUP_CONFIG_MAX] = {0};

/* Uplink stats. _Atomic is overkill on a single-core Xtensa LX7 — plain
 * unsigned is fine. Protected by g_started ordering. */
static volatile unsigned int s_frames_sent    = 0;
static volatile unsigned int s_frames_dropped = 0;
static volatile int          s_capture_running = 0;

/* Downlink playback ring buffer. The SDK audio callback only enqueues decoded
 * PCM here; a dedicated high-priority task drains it to the codec, smoothing
 * network/decoding jitter and keeping the I2S driver fed at its own pace.
 * 8k stereo = 32KB/s, 512KB ≈ 16s — high headroom for bursty downlink.
 * ponytail: fixed at 512KB, bump to 1MB if a single burst ever exceeds it. */
#define PLAYBACK_RING_SIZE (512 * 1024)
#define PLAYBACK_READ_CHUNK 4096
#define PLAYBACK_POLL_MS 10
/* 开播前先攒够这么多 PCM 再出声. 8k stereo = 32000B/s, 1.5s = 48KB.
 * ring 512KB = 16s, 预缓冲只吸收开头短抖动. 短回答由 s_playback_force
 * 兜底(回答结束时强播). */
#define PLAYBACK_PREBUFFER (48 * 1024)
static RingbufHandle_t s_playback_rb = NULL;
static TaskHandle_t s_playback_task = NULL;
static volatile unsigned int s_playback_dropped = 0;
static volatile int s_playback_flush = 0;
/* ANSWER_FINISHED 置位: 本轮音频已全部入 ring, 强制开播(跳过预缓冲),
 * 否则短回答(<3s)永远攒不够阈值, 音频滞留 ring 拖到下一句一起播. */
static volatile int s_playback_force = 0;

static void audio_playback_task(void *arg) {
  (void)arg;
  /* 诊断: 消费侧节奏. 每次 drain 轮打印 ring 已用字节(可用=512KB-已用),
   * 用于判断消费是否跟得上到达: 持续接近满 = 消费慢/到达快,
   * 持续撮近空 = 到达慢/消费快 (欠货). 300 轮一次避免刷屏. */
  unsigned drain_log_cnt = 0;
  while (1) {
    /* 预热: 数据不足 PLAYBACK_PREBUFFER 时只攒不播 (flush/force 时放行) */
    if (!s_playback_flush && !s_playback_force && s_playback_rb != NULL &&
        xRingbufferGetCurFreeSize(s_playback_rb) >
            (PLAYBACK_RING_SIZE - PLAYBACK_PREBUFFER)) {
      vTaskDelay(pdMS_TO_TICKS(PLAYBACK_POLL_MS));
      continue;
    }
    /* Drain the ring in a loop, not one 1KB chunk per 10ms poll: a downlink
     * burst (~5x real-time) used to fill the ring faster than this task
     * drained it → ring full → tail audio dropped (see goldieos
     * convai_audio_downlink.c "Draining in a loop keeps the ring low"). */
    size_t drained_bytes = 0;
    for (;;) {
      size_t item_size = 0;
      void *item = xRingbufferReceiveUpTo(s_playback_rb, &item_size, 0,
                                          PLAYBACK_READ_CHUNK);
      if (item == NULL) {
        break; /* ring drained */
      }
      if (s_playback_flush) {
        vRingbufferReturnItem(s_playback_rb, item);
        continue;
      }
      if (g_started && g_engine != NULL) {
        audio_codec_t *codec = audio_codec_active();
        if (codec != NULL && codec->write != NULL) {
          codec->write(codec, item, (int)item_size);
          drained_bytes += item_size;
        }
      }
      vRingbufferReturnItem(s_playback_rb, item);
    }
    if (s_playback_flush) {
      s_playback_flush = 0;
    }
    /* 强播已排空: 复位, 下一轮重新走预缓冲门槛 */
    if (s_playback_force &&
        xRingbufferGetCurFreeSize(s_playback_rb) == PLAYBACK_RING_SIZE) {
      s_playback_force = 0;
    }
    if (++drain_log_cnt % 300 == 0) {
      size_t used = PLAYBACK_RING_SIZE -
                    xRingbufferGetCurFreeSize(s_playback_rb);
      ESP_LOGI(TAG, "playback: ring_used=%u drained_this_round=%u",
               (unsigned)used, (unsigned)drained_bytes);
    }
    vTaskDelay(pdMS_TO_TICKS(PLAYBACK_POLL_MS));
  }
}

static void playback_ring_init(void) {
  if (s_playback_rb != NULL) {
    return;
  }

  s_playback_rb = xRingbufferCreate(PLAYBACK_RING_SIZE, RINGBUF_TYPE_BYTEBUF);
  if (s_playback_rb == NULL) {
    ESP_LOGE(TAG, "playback ringbuffer create failed (fallback: direct write)");
    return;
  }

  BaseType_t ok = xTaskCreate(audio_playback_task, "audio_play",
                             8192, NULL, 6, &s_playback_task);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "playback task create failed");
    vRingbufferDelete(s_playback_rb);
    s_playback_rb = NULL;
    s_playback_task = NULL;
  }
}

/* ---- SDK callbacks ---- */

static void on_sdk_event(convai_engine_t engine, convai_event_t *event,
                         void *user_data) {
  (void)engine; (void)user_data;
  static const char *names[] = {
      [CONVAI_EV_CONNECTED]    = "CONNECTED",
      [CONVAI_EV_DISCONNECTED] = "DISCONNECTED",
      [CONVAI_EV_FAILED]       = "FAILED",
      [CONVAI_EV_UPDATED]      = "UPDATED",
  };
  const char *name = (event->code < (int)(sizeof(names) /
                          sizeof(names[0])))
                          ? names[event->code] : "UNKNOWN";
  const char *info = (event && event->data.details) ? event->data.details : "";
  ESP_LOGI(TAG, "event: %s (%s)", name, info);

  /* Forward to UI layer. */
  if (g_event_cb) g_event_cb(event->code, info);

  switch (event->code) {
    case CONVAI_EV_DISCONNECTED:
      /* SDK gone — stop the bridge (mirror goldieos: bridge_cleanup
       * halts audio threads and resets local state). */
      convai_bridge_stop();
      break;
    case CONVAI_EV_FAILED:
    case CONVAI_EV_CONNECTED:
    case CONVAI_EV_UPDATED:
    default:
      break;
  }
}

static void on_status(convai_engine_t engine, convai_status_e status,
                      void *user_data) {
  (void)engine; (void)user_data;
  static const char *names[] = {
      [CONVAI_STATUS_IDLE]            = "IDLE",
      [CONVAI_STATUS_LISTENING]       = "LISTENING",
      [CONVAI_STATUS_THINKING]        = "THINKING",
      [CONVAI_STATUS_ANSWERING]       = "ANSWERING",
      [CONVAI_STATUS_INTERRUPTED]     = "INTERRUPTED",
      [CONVAI_STATUS_ANSWER_FINISHED] = "ANSWER_FINISHED",
  };
  const char *name = (status < (int)(sizeof(names) /
                          sizeof(names[0])))
                          ? names[status] : "UNKNOWN";
  ESP_LOGI(TAG, "status: %s", name);

  g_status = status;
  if (g_status_cb) g_status_cb(status);

  /* End-of-turn accounting + UI sync (mirrors goldieos cloud_status_callback
   * but without the talk_page/avatar layer). */
  switch (status) {
    case CONVAI_STATUS_ANSWER_FINISHED: {
      unsigned int sent = 0, dropped = 0;
      convai_bridge_get_uplink_stats(&sent, &dropped);
      if ((sent + dropped) > 0) {
        ESP_LOGI(TAG, "uplink: sent=%u dropped=%u drop_rate=%u%%",
                 sent, dropped, dropped * 100 / (sent + dropped));
      }
      s_playback_force = 1;   /* 本轮音频已收齐, 强制开播 */
      ai_chat_ui_set_state(STATE_IDLE);
      board_led_set(0);
      break;
    }
    case CONVAI_STATUS_LISTENING:
      board_led_set(1);
      ai_chat_ui_set_state(STATE_LISTENING);
      break;
    case CONVAI_STATUS_THINKING:
      board_led_set(1);
      ai_chat_ui_set_state(STATE_THINKING);
      break;
    case CONVAI_STATUS_ANSWERING:
      s_playback_force = 0;   /* 新回答重新走预缓冲门槛 */
      board_led_set(0);
      ai_chat_ui_set_state(STATE_SPEAKING);
      break;
    case CONVAI_STATUS_IDLE:
      board_led_set(0);
      ai_chat_ui_set_state(STATE_IDLE);
      break;
    case CONVAI_STATUS_INTERRUPTED:
      s_playback_flush = 1;
      board_led_set(0);
      ai_chat_ui_set_state(STATE_IDLE);
      break;
    default:
      board_led_set(0);
      ai_chat_ui_set_state(STATE_IDLE);
      break;
  }
}

/* 下行 PCM 缓冲 (对齐 goldieos g_pcm_decode_buf): 单声道 16-bit PCM,
 * 足够容纳 SDK 单帧 G711A 解码结果。G711A 每字节 → 2 字节 PCM。 */
#define DOWNLINK_PCM_MAX  4096
/* 单声道 → 立体声扩展后的缓冲 (8k, ×2) */
#define DOWNLINK_ST_MAX   (DOWNLINK_PCM_MAX * 2)

static void on_audio(convai_engine_t engine, const void *data, size_t len,
                     const convai_audio_frame_info_t *info, void *user_data) {
  (void)engine; (void)info; (void)user_data;
  /* 诊断: 周期打印下行数据到达情况 (每 20 帧) + 到达节奏.
   * 20 帧 = 600ms 音频, 若批次间隔 ≈600ms 即 1x 匀速到达;
   * 间隔明显 >600ms = 服务器/微服务下发慢 (欠货),
   * 明显 <600ms = 服务器突发倒灌. 据此区分"没收到"与"消费快". */
  static unsigned dl_cnt = 0;
  static int64_t dl_last_us = 0;
  if ((++dl_cnt % 20) == 0) {
    int64_t now_us = esp_timer_get_time();
    int64_t gap_ms = (dl_last_us > 0) ? (now_us - dl_last_us) / 1000 : 0;
    dl_last_us = now_us;
    ESP_LOGI(TAG, "downlink on_audio: len=%u data_type=%d batch_gap_ms=%lld",
             (unsigned)len, info ? (int)info->data_type : -1,
             (long long)gap_ms);
  }

  audio_codec_t *codec = audio_codec_active();
  if (codec == NULL) {
    ESP_LOGW(TAG, "downlink: no active codec");
    return;
  }

  /* ---- 下行: G.711A → 8k mono PCM → 立体声(8k) → 播放 ----
   * SDK 下发 G.711A → convai_g711a_decode 还原 8k 单声道 16-bit PCM,
   * 直接扩展为立体声(L=R) 写入 8k TX. 与硬件采样率 1:1 对齐, 无需重采样. */
  static uint8_t mono[DOWNLINK_PCM_MAX];
  static uint8_t st[DOWNLINK_ST_MAX];   /* 8k stereo */
  size_t mono_len = 0;
  if (convai_g711a_decode((const uint8_t *)data, len,
                          mono, sizeof(mono), &mono_len) != 0 ||
      mono_len == 0) {
    ESP_LOGW(TAG, "downlink g711 decode failed (len=%u)",
             (unsigned)len);
    return;
  }
  size_t n8 = mono_len / sizeof(int16_t);   /* 8k 单声道帧数 */
  const int16_t *m = (const int16_t *)mono;
  int16_t *s          = (int16_t *)st;
  for (size_t i = 0; i < n8; i++) {
    s[i * 2]     = m[i];   /* L */
    s[i * 2 + 1] = m[i];   /* R = L (单声道→立体声) */
  }
  size_t pcm_bytes = n8 * 2 * sizeof(int16_t);   /* 8k stereo 字节数 */
  if (s_playback_flush) {
    s_playback_dropped += pcm_bytes;
    return;
  }
  if (s_playback_rb != NULL && s_playback_task != NULL) {
    if (xRingbufferSend(s_playback_rb, st, pcm_bytes, 0) != pdTRUE) {
      /* ring 满: 等播放任务排空再写入, 不丢帧。文本结束后服务器会把
       * 积压 TTS 音频以数倍速率倒灌, 阻塞写入让 TCP 背压把它压回
       * 1x 实时, 长回答尾部不再被丢弃。2s 兜底防播放任务异常挂死。 */
      TickType_t waited = 0;
      while (!s_playback_flush && g_started &&
             xRingbufferSend(s_playback_rb, st, pcm_bytes,
                             pdMS_TO_TICKS(20)) != pdTRUE) {
        waited += 20;
        if (waited > 2000) {
          break;
        }
      }
      if (s_playback_flush || waited > 2000) {
        s_playback_dropped += pcm_bytes;
        if ((dl_cnt % 20) == 0) {
          ESP_LOGW(TAG, "playback ring full, dropped=%u",
                   (unsigned)s_playback_dropped);
        }
      }
    }
  } else {
    int w = codec->write(codec, st, (int)pcm_bytes);
    if (w < 0) {
      ESP_LOGW(TAG, "downlink write failed (n8=%u)", (unsigned)n8);
    }
  }
  if ((dl_cnt % 20) == 0) {
    ESP_LOGI(TAG, "downlink write: n8=%u dropped=%u", (unsigned)n8,
             (unsigned)s_playback_dropped);
  }
}

static void on_message(convai_engine_t engine, const void *data, size_t len,
                       const convai_message_info_t *info, void *user_data) {
  (void)engine; (void)info; (void)user_data;
  /* The SDK only delivers a non-NULL pointer + length for the lifetime of
   * this callback, so copy out if a registered callback needs it past. */
  if (g_message_cb) {
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
      memcpy(copy, data, len);
      copy[len] = '\0';
      g_message_cb(copy);
      free(copy);
    }
  } else {
    ESP_LOGI(TAG, "message: %.*s", (int)len, (const char *)data);
  }
}

/* ===================================================================
 *  G.711 A-law decode + audio level metering (mic input path)
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
    if (s < 0) s = (int16_t)-s;
    if (s > peak) peak = s;
  }
  uint8_t v = (uint8_t)(peak * 100 / 32767);
  smooth = (uint8_t)(smooth * 6 / 10 + v * 4 / 10); /* EMA 0.6/0.4 */
  return smooth;
}

/* 单次采集缓冲: 20ms @ 8kHz TDM 4 时隙 16-bit PCM。
 * 每帧 4 样本 (slot0=MIC1, slot1=MIC3 回采, slot2=MIC2, slot3=MIC4) × 2 字节。
 * 用 4 slot 保证 MCLK_256 整数分频 (256/64=4), 时钟正常 RX 才有数据。
 * = AUDIO_SAMPLE_RATE * 4 * 2 / 50 (按字节计)。 */
#define CAPTURE_BUF_SIZE (AUDIO_SAMPLE_RATE * 4 * 2 / 50)

static void audio_capture_task(void *arg) {
  (void)arg;
  audio_codec_t *codec = audio_codec_active();
  if (codec == NULL) {
    ESP_LOGE(TAG, "capture task: no active codec");
    s_capture_running = 0;
    vTaskDeleteWithCaps(NULL);
    return;
  }
  uint8_t *buf = (uint8_t *)malloc(CAPTURE_BUF_SIZE);   /* TDM 4 时隙原始数据 */
  if (buf == NULL) {
    s_capture_running = 0;
    vTaskDeleteWithCaps(NULL);
    return;
  }
  /* planar PCM 缓冲 (L 前 R 后): 只保留 slot0(MIC1)+slot1(MIC3) 两路,
   * 8k 硬件采集即 8k, 无需再降采样。
   * 大小 = 2 通道 8k = AUDIO_SAMPLE_RATE*2*2/50 字节。 */
  size_t planar_cap = (size_t)(AUDIO_SAMPLE_RATE * 2 * 2 / 50);
  uint8_t *planar_buf = (uint8_t *)malloc(planar_cap);
  if (planar_buf == NULL) {
    free(buf);
    s_capture_running = 0;
    vTaskDeleteWithCaps(NULL);
    return;
  }
  /* G.711A 编码输出缓冲: 2 通道 8k 压缩后 ≤ planar_cap。 */
  uint8_t *g711_buf = (uint8_t *)malloc(planar_cap);
  if (g711_buf == NULL) {
    free(planar_buf);
    free(buf);
    s_capture_running = 0;
    vTaskDeleteWithCaps(NULL);
    return;
  }

  int hb_cnt = 0;
  int rx_empty_cnt = 0;   /* 连续 received<=0 计数 (诊断) */
  int rx_ok_logged = 0;
  while (g_started && g_engine != NULL) {
    int received = codec->read(codec, buf, CAPTURE_BUF_SIZE);
    if (received <= 0) {
      s_frames_dropped++;
      if (++rx_empty_cnt == 100) {   /* 连续 100 次 (~1s) 无数据则报警 */
        rx_empty_cnt = 0;
        ESP_LOGW(TAG, "capture: RX 连续无数据 (I2S TDM 采集异常?)");
      }
    } else {
      rx_empty_cnt = 0;
      /* 上行声道对齐 (TDM 4 时隙, ES7210), 8k 采集即 8k 直接编码:
       *   (实测播放时 slot1 幅值飙升至 9k+, 确认物理顺序为 MIC1, MIC3, MIC2, MIC4):
       *     slot0 = MIC1  (麦克风/人声)     → 左声道
       *     slot1 = MIC3  (扬声器回采/AEC)  → 右声道
       *     slot2 = MIC2  (另一路麦克风)    → 丢弃
       *     slot3 = MIC4  (未接)            → 丢弃
       *   4-slot 是为了保证 MCLK_256 整数分频 (256/64=4), 让 ES7210 收到
       *   正确的 BCLK/LRCK, RX 才有数据。
       *   流程: 读 8k TDM → 取 slot0(MIC1)/slot1(MIC3) → planar[L=MIC1][R=MIC3]@8k
       *         → channels=2 A-law 编码 → L+R 一起上行。 */
      size_t tdm_frames = (size_t)received / (4 * sizeof(int16_t)); /* TDM 帧数@8k */
      if (tdm_frames > 0) {
        if (!rx_ok_logged) {
          rx_ok_logged = 1;
          ESP_LOGI(TAG, "capture RX ok: bytes=%d tdm_frames=%u",
                   received, (unsigned)tdm_frames);
        }
        const int16_t *samples = (const int16_t *)buf;
        int16_t *planar        = (int16_t *)planar_buf;
        for (size_t i = 0; i < tdm_frames; i++) {
          planar[i]               = samples[i * 4 + 0];   /* slot0 = MIC1 (左) */
          planar[tdm_frames + i]  = samples[i * 4 + 1];   /* slot1 = MIC3 回采 (右) */
          /* slot2(MIC2) 与 slot3(MIC4 未接) 丢弃 */
        }
        size_t l8 = tdm_frames, r8 = tdm_frames;   /* 8k 硬件, 1:1 */
        {
          int16_t *planar8 = planar;
          size_t planar8_len = (l8 + r8) * sizeof(int16_t); /* L8+R8 平面字节数 */
          size_t g711_len = 0;
          int enc = convai_g711a_encode((uint8_t *)planar8, planar8_len, 2,
                                        g711_buf, planar_cap,
                                        &g711_len);
          if (enc != 0 || g711_len == 0) {
            s_frames_dropped++;
          } else {
            /* 音量为左声道 MIC1 的响度指示 (8k 帧数 l8);
             * g711_len = 2*l8, L(MIC1) + R(MIC3 回采) 双声道一起上行。 */
            ai_chat_ui_update_volume(compute_audio_level(g711_buf, l8));
            convai_audio_frame_info_t info = {.data_type =
                                                  CONVAI_AUDIO_DATA_TYPE_G711A};
            int rc = convai_send_audio(g_engine, g711_buf, g711_len, &info);
            if (rc == CONVAI_OK) {
              s_frames_sent++;
            } else {
              s_frames_dropped++;
            }
          }
        }
      } else {
        s_frames_dropped++;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    if (++hb_cnt >= 3000) { /* ~30s */
      hb_cnt = 0;
      ESP_LOGI(TAG, "capture heartbeat: free_heap=%u, sent=%u, dropped=%u",
               (unsigned)esp_get_free_heap_size(),
               (unsigned)s_frames_sent, (unsigned)s_frames_dropped);
    }
  }
  free(g711_buf);
  free(planar_buf);
  free(buf);
  s_capture_running = 0;
  ESP_LOGI(TAG, "capture task exited (sent=%u dropped=%u)",
           (unsigned)s_frames_sent, (unsigned)s_frames_dropped);
  vTaskDeleteWithCaps(NULL);
}

/* ===================================================================
 *  Internal: start/stop the capture task
 * =================================================================== */
static void bridge_start_capture(void) {
  if (s_capture_running) return;
  s_capture_running = 1;
  /* 任务栈分配到 8MB PSRAM，缓解内部 RAM 不足导致的 xTaskCreate 失败。
   * 内部 RAM 仅 ~220KB，WiFi+LVGL+audio 占用后难以提供 8KB 连续栈块。 */
  BaseType_t ok = xTaskCreateWithCaps(
      audio_capture_task, "audio_cap", 8192, NULL, 5, NULL,
      MALLOC_CAP_SPIRAM);
  if (ok != pdPASS) {
    s_capture_running = 0;
    ESP_LOGE(TAG, "xTaskCreateWithCaps(audio_cap) failed: ret=%d "
             "free_heap=%u min_free=%u",
             (int)ok,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size());
    return;
  }
  ESP_LOGI(TAG, "capture: PSRAM-stacked task started "
           "(free_heap=%u min_free=%u)",
           (unsigned)esp_get_free_heap_size(),
           (unsigned)esp_get_minimum_free_heap_size());
}

/* ===================================================================
 *  Public API
 * =================================================================== */

void convai_bridge_init(void) {
  if (g_engine) {
    ESP_LOGW(TAG, "already initialized");
    return;
  }

  char config_json[2048];
  const char *dev_name = g_device_name[0] ? g_device_name : NULL;
  bridge_build_config_json(config_json, sizeof(config_json), dev_name);

  convai_event_handler_t cb = {
      .on_convai_event              = on_sdk_event,
      .on_convai_conversation_status = on_status,
      .on_convai_audio_data          = on_audio,
      .on_convai_message_data        = on_message,
  };

  int ret = convai_create(&g_engine, config_json, &cb, NULL);
  if (ret != CONVAI_OK) {
    ESP_LOGE(TAG, "convai_create failed: %d", ret);
    g_engine = NULL;
    return;
  }
  ESP_LOGI(TAG, "engine created (v%s)", convai_get_version());
  playback_ring_init();
}

int convai_bridge_start(void) {
  if (!g_engine) {
    ESP_LOGE(TAG, "not initialized");
    return -1;
  }
  if (g_started) {
    ESP_LOGW(TAG, "already started");
    return 0;
  }

  convai_opt_t opt = {0};
  opt.mode     = CONVAI_MODE_WS;
  opt.agent_id = bridge_get_default_agent_id();
  opt.params   = g_startup_config[0]
                     ? g_startup_config
                     : bridge_get_default_startup_config();

  ESP_LOGI(TAG, "START: agent_id=%s", opt.agent_id);

  int ret = convai_start(g_engine, &opt);
  if (ret != CONVAI_OK) {
    ESP_LOGE(TAG, "convai_start failed: %d", ret);
    return ret;
  }

  g_started = 1;
  g_status  = CONVAI_STATUS_IDLE;
  /* New session: reset downlink diagnostics so dropped counts this turn. */
  s_playback_dropped = 0;
  bridge_start_capture();
  if (g_status_cb) g_status_cb(g_status);
  return CONVAI_OK;
}

int convai_bridge_stop(void) {
  if (!g_started) return 0;

  /* Setting g_started = 0 first makes audio_capture_task exit on its next
   * loop iteration. vTaskDelay gives it a chance to delete itself. */
  g_started = 0;
  vTaskDelay(pdMS_TO_TICKS(20));
  g_status = CONVAI_STATUS_IDLE;
  if (g_status_cb) g_status_cb(g_status);

  if (g_engine) {
    int ret = convai_stop(g_engine);
    if (ret != CONVAI_OK) {
      ESP_LOGE(TAG, "convai_stop failed: %d", ret);
      return ret;
    }
  }
  ESP_LOGI(TAG, "stopped (sent=%u dropped=%u)",
           (unsigned)s_frames_sent, (unsigned)s_frames_dropped);
  return CONVAI_OK;
}

int convai_bridge_restart(void) {
  ESP_LOGI(TAG, "RESTART");
  convai_bridge_stop();
  vTaskDelay(pdMS_TO_TICKS(100));
  return convai_bridge_start();
}

convai_engine_t convai_bridge_get_engine(void)  { return g_engine; }
convai_status_e convai_bridge_get_status(void)  { return g_status; }
int convai_bridge_is_speaking(void) {
  return (g_status == CONVAI_STATUS_ANSWERING);
}
int convai_bridge_is_started(void)             { return g_started; }

int convai_bridge_get_uplink_stats(unsigned int *frames_sent,
                                   unsigned int *frames_dropped) {
  if (frames_sent == NULL || frames_dropped == NULL) return -1;
  if (s_frames_sent == 0 && s_frames_dropped == 0 && !s_capture_running) {
    return -1; /* capture never ran */
  }
  *frames_sent    = s_frames_sent;
  *frames_dropped = s_frames_dropped;
  return 0;
}

void convai_bridge_on_status(convai_bridge_status_cb cb)   { g_status_cb  = cb; }
void convai_bridge_on_event(convai_bridge_event_cb cb)     { g_event_cb   = cb; }
void convai_bridge_on_message(convai_bridge_message_cb cb) { g_message_cb = cb; }

void convai_bridge_set_startup_config(const char *config) {
  if (config == NULL) {
    g_startup_config[0] = '\0';
    return;
  }
  strncpy(g_startup_config, config, sizeof(g_startup_config) - 1);
  g_startup_config[sizeof(g_startup_config) - 1] = '\0';
  ESP_LOGI(TAG, "startup config set (%zu bytes)", strlen(g_startup_config));
}

const char *convai_bridge_get_startup_config(void) {
  return g_startup_config[0] ? g_startup_config : NULL;
}

void convai_bridge_set_device_name(const char *name) {
  if (name == NULL) {
    g_device_name[0] = '\0';
    return;
  }
  strncpy(g_device_name, name, sizeof(g_device_name) - 1);
  g_device_name[sizeof(g_device_name) - 1] = '\0';
  ESP_LOGI(TAG, "device name set: %s", g_device_name);
}
