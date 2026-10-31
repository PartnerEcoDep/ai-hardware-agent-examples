#include "esp_media.h"
#include "ai_audio.h"
#include "ai_playback.h"
#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static SemaphoreHandle_t tx_lock, rx_lock, output_lock;
static bool enabled;
static uint32_t capture_epoch, session_id;
static ai_uplink_fn uplink;
static void *uplink_ctx;
static ai_playback_t playback;
static void (*post_event)(void *, const ai_event_t *);
static void *post_ctx;

static void enable(void *ctx, bool active, ai_uplink_fn send, void *send_ctx) {
    (void)ctx;
    xSemaphoreTakeRecursive(tx_lock, portMAX_DELAY);
    ++capture_epoch;
    enabled = active;
    uplink = send;
    uplink_ctx = send_ctx;
    xSemaphoreGiveRecursive(tx_lock);
}
static void reset(void *ctx) {
    (void)ctx;
    xSemaphoreTake(output_lock, portMAX_DELAY);
    xSemaphoreTake(rx_lock, portMAX_DELAY);
    ai_playback_reset(&playback);
    xSemaphoreGive(rx_lock);
    xSemaphoreGive(output_lock);
}
static int push(void *ctx, const uint8_t *data, size_t size) {
    (void)ctx;
    xSemaphoreTake(rx_lock, portMAX_DELAY);
    int rc = ai_playback_push(&playback, data, size);
    xSemaphoreGive(rx_lock);
    return rc;
}
static void finish(void *ctx) {
    (void)ctx;
    xSemaphoreTake(rx_lock, portMAX_DELAY);
    ai_playback_finish(&playback);
    xSemaphoreGive(rx_lock);
}

static void capture_task(void *arg) {
    (void)arg;
    int16_t raw[160 * 4];
    uint8_t encoded[160 * 2];
    unsigned failures = 0;
    unsigned sent = 0, rejected = 0;
    int64_t last_stats = esp_timer_get_time();
    for (;;) {
        xSemaphoreTakeRecursive(tx_lock, portMAX_DELAY);
        bool active = enabled;
        uint32_t epoch = capture_epoch;
        xSemaphoreGiveRecursive(tx_lock);
        /* Keep draining DMA while idle so a new session never uploads old audio. */
        int count = board_audio_read(raw, sizeof(raw) / sizeof(raw[0]));
        if (count <= 0 || count > (int)(sizeof(raw) / sizeof(raw[0])) || count % 4) {
            if (active && ++failures >= 30) {
                xSemaphoreTakeRecursive(tx_lock, portMAX_DELAY);
                if (enabled && epoch == capture_epoch) {
                    enabled = false;
                    ai_event_t e = {AI_FAILED, session_id, -4};
                    post_event(post_ctx, &e);
                }
                xSemaphoreGiveRecursive(tx_lock);
                failures = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (!active) { failures = 0; continue; }
        int size = ai_audio_encode_slots(raw, (size_t)count / 4, 4,
            board_profile.mic_slot, board_profile.reference_slot, encoded, sizeof(encoded));
        xSemaphoreTakeRecursive(tx_lock, portMAX_DELAY);
        if (enabled && epoch == capture_epoch && uplink) {
            int rc = size < 0 ? -1 : uplink(uplink_ctx, encoded, (size_t)size);
            if (rc < 0) ++rejected;
            else if (rc == 0) ++sent;
            if (rc < 0) {
                if (!failures) ESP_LOGW("ai_media", "uplink rejected rc=%d", rc);
                if (++failures >= 30) {
                    ESP_LOGE("ai_media", "uplink failed 30 consecutive frames, last rc=%d", rc);
                    enabled = false;
                    ai_event_t e = {AI_FAILED, session_id, -5};
                    post_event(post_ctx, &e);
                }
            } else failures = 0;
        }
        xSemaphoreGiveRecursive(tx_lock);
        int64_t now = esp_timer_get_time();
        if (now - last_stats >= 5000000) {
            ESP_LOGI("ai_media", "uplink frames=%u rejected=%u in %lld ms",
                     sent, rejected, (long long)((now - last_stats) / 1000));
            sent = rejected = 0; last_stats = now;
        }
    }
}

static void playback_task(void *arg) {
    (void)arg;
    uint8_t encoded[160];
    int16_t pcm[320];
    for (;;) {
        /* Serialize reset with a hardware write, but never hold the queue lock
         * across I2S waiting: SDK audio callbacks must enqueue without waiting
         * for the speaker clock or starving the SDK's full-duplex IO loop. */
        xSemaphoreTake(output_lock, portMAX_DELAY);
        xSemaphoreTake(rx_lock, portMAX_DELAY);
        uint32_t epoch = playback.epoch;
        size_t n = ai_playback_pull(&playback, encoded, sizeof(encoded));
        bool drained = playback.finished && !playback.size;
        xSemaphoreGive(rx_lock);
        if (n) {
            ai_audio_decode_stereo(encoded, n, pcm, sizeof(pcm) / sizeof(pcm[0]));
            if (board_audio_write(pcm, n * 2)) {
                xSemaphoreTake(rx_lock, portMAX_DELAY);
                ai_playback_reset(&playback);
                xSemaphoreGive(rx_lock);
                ai_event_t e = {AI_FAILED, session_id, -6};
                post_event(post_ctx, &e);
            }
        }
        xSemaphoreGive(output_lock);
        if (drained) {
            /* Existing hardware uses six 240-frame DMA descriptors at 8 kHz. */
            vTaskDelay(pdMS_TO_TICKS(180));
            xSemaphoreTake(rx_lock, portMAX_DELAY);
            if (playback.epoch == epoch && playback.finished && !playback.size) {
                playback.finished = false;
                ai_event_t e = {AI_TURN_FINISHED, session_id, 0};
                post_event(post_ctx, &e);
            }
            xSemaphoreGive(rx_lock);
        }
        if (!n) vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int esp_media_init(void (*post)(void *, const ai_event_t *), void *ctx) {
    if (!post) return -1;
    post_event = post; post_ctx = ctx;
    tx_lock = xSemaphoreCreateRecursiveMutex();
    rx_lock = xSemaphoreCreateMutex();
    output_lock = xSemaphoreCreateMutex();
    uint8_t *storage = heap_caps_malloc(CONFIG_AI_PLAYBACK_KB * 1024,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tx_lock || !rx_lock || !output_lock || !storage) return -1;
    if (ai_playback_init(&playback, storage, CONFIG_AI_PLAYBACK_KB * 1024, 1600)) return -1;
    if (xTaskCreate(capture_task, "ai_capture", 4096, NULL, 5, NULL) != pdPASS) return -1;
    if (xTaskCreate(playback_task, "ai_playback", 4096, NULL, 6, NULL) != pdPASS) return -1;
    return 0;
}

void esp_media_set_session(uint32_t session) {
    /* Called only between sessions, after enable(false) and reset barriers. */
    xSemaphoreTakeRecursive(tx_lock, portMAX_DELAY);
    xSemaphoreTake(rx_lock, portMAX_DELAY);
    session_id = session;
    xSemaphoreGive(rx_lock);
    xSemaphoreGiveRecursive(tx_lock);
}
ai_media_ops_t esp_media_ops(void) {
    return (ai_media_ops_t){.enable = enable, .reset = reset, .push = push, .finish = finish};
}
