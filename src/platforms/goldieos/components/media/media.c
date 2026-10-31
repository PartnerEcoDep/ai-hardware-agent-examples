#include "ws_media.h"
#include "ws_app_events.h"
#include "service_manager.h"
#include "ws63_config.h"
#include "ai_audio.h"
#include "ai_playback.h"
#include "goldie_osal.h"
#include "audio_service.h"
#include "ws_board.h"
#include <stdio.h>
#ifdef PLATFORM_TYPE_WS63
#include "lwip/sys.h"
/* Temporary WS63 diagnostics. All shared fields use rx; never query the
 * device from the SDK callback. Unsigned subtraction handles tick wrap. */
static struct {
    uint32_t last_loop, received, consumed;
    uint32_t last_push, push_gap_max, pushes;
    bool push_seen;
    unsigned level;
} diag;
#endif

static AudioService *hw;
static ws_board_capture_t capture_format;
static goldie_mutex tx, rx, output;
static bool enabled, playing;
static uint32_t session, capture_epoch;
static ai_uplink_fn send_audio;
static void *send_ctx;
static ai_playback_t queue;
static uint8_t storage[WS_AI_PLAYBACK_BYTES];

static void enable(void *ctx, bool on, ai_uplink_fn send, void *arg) {
    (void)ctx;
    goldie_mutex_lock(&tx);
    ++capture_epoch; enabled = on; send_audio = send; send_ctx = arg;
    goldie_mutex_unlock(&tx);
}
static void reset(void *ctx) {
    (void)ctx;
    goldie_mutex_lock(&output);
    goldie_mutex_lock(&rx);
    ai_playback_reset(&queue);
#ifdef PLATFORM_TYPE_WS63
    diag.push_seen = false; diag.push_gap_max = diag.pushes = 0;
#endif
    goldie_mutex_unlock(&rx);
    if (playing) hw->play_stop();
    playing = false;
    hw->reset_playbuffer();
    goldie_mutex_unlock(&output);
}
static int push(void *ctx, const uint8_t *data, size_t size) {
    (void)ctx;
    goldie_mutex_lock(&rx);
    int rc = ai_playback_push(&queue, data, size);
    size_t queued = queue.size, capacity = queue.capacity;
#ifdef PLATFORM_TYPE_WS63
    uint32_t push_now = sys_now();
    if (!rc) {
        diag.received += (uint32_t)size;
        uint32_t gap = push_now - diag.last_push;
        if (diag.push_seen && gap > diag.push_gap_max) diag.push_gap_max = gap;
        diag.last_push = push_now; diag.push_seen = true; ++diag.pushes;
    }
    uint32_t age = push_now - diag.last_loop;
    unsigned device_level = diag.level;
#endif
    goldie_mutex_unlock(&rx);
    if (rc) printf("[ws_media] downlink queue rejected: incoming=%lu queued=%lu capacity=%lu\n",
                   (unsigned long)size, (unsigned long)queued,
                   (unsigned long)capacity);
#ifdef PLATFORM_TYPE_WS63
    if (rc) printf("[ws_media_diag] t_ms=%lu reject last_loop_age_ms=%lu last_device_bytes=%u\n",
                   (unsigned long)push_now, (unsigned long)age, device_level);
#endif
    return rc;
}
static void finish(void *ctx) {
    (void)ctx;
    goldie_mutex_lock(&rx); ai_playback_finish(&queue); goldie_mutex_unlock(&rx);
}
static int capture(void *ctx) {
    (void)ctx;
    int16_t pcm[320]; uint8_t encoded[320];
    unsigned failures = 0;
#ifdef PLATFORM_TYPE_WS63
    uint32_t report = sys_now(), read_max = 0, send_max = 0;
    unsigned reads = 0, bad_reads = 0, sends = 0, send_failed = 0;
#endif
    hw->record_start();
    for (;;) {
        goldie_mutex_lock(&tx);
        uint32_t epoch = capture_epoch;
        goldie_mutex_unlock(&tx);
#ifdef PLATFORM_TYPE_WS63
        uint32_t read_start = sys_now();
#endif
        int bytes = hw->audio_read(pcm, sizeof(pcm));
#ifdef PLATFORM_TYPE_WS63
        uint32_t read_ms = (uint32_t)sys_now() - read_start;
        if (read_ms > read_max) read_max = read_ms;
        ++reads; if (bytes <= 0) ++bad_reads;
#endif
        goldie_mutex_lock(&tx);
        if (enabled && epoch == capture_epoch) {
            int rc = -1;
            if (bytes > 0 && bytes <= (int)sizeof(pcm) && bytes % (int)(capture_format.channels * sizeof(int16_t)) == 0) {
                int size = ai_audio_encode_slots(pcm, (size_t)bytes / (capture_format.channels * sizeof(int16_t)),
                                                 capture_format.channels, capture_format.microphone, capture_format.reference,
                                                 encoded, sizeof(encoded));
                if (size > 0) {
#ifdef PLATFORM_TYPE_WS63
                    uint32_t send_start = sys_now();
#endif
                    rc = send_audio(send_ctx, encoded, (size_t)size);
#ifdef PLATFORM_TYPE_WS63
                    uint32_t send_ms = (uint32_t)sys_now() - send_start;
                    if (send_ms > send_max) send_max = send_ms;
                    ++sends; if (rc < 0) ++send_failed;
#endif
                }
            }
            if (rc >= 0) failures = 0;
            else if (++failures >= 30) {
                enabled = false;
                ai_event_t event = {AI_FAILED, session, -5};
                ws_post(NULL, &event);
            }
        } else failures = 0;
#ifdef PLATFORM_TYPE_WS63
        bool capture_on = enabled;
#endif
        goldie_mutex_unlock(&tx);
#ifdef PLATFORM_TYPE_WS63
        uint32_t now = sys_now();
        if (now - report >= 1000) {
            if (capture_on || sends)
                printf("[ws_capture_diag] t_ms=%lu window_ms=%lu enabled=%d reads=%u bad_reads=%u sends=%u failed=%u read_max_ms=%lu send_max_ms=%lu\n",
                       (unsigned long)now, (unsigned long)(now - report), capture_on, reads, bad_reads, sends, send_failed,
                       (unsigned long)read_max, (unsigned long)send_max);
            report = now; reads = bad_reads = sends = send_failed = 0;
            read_max = send_max = 0;
        }
#endif
        if (bytes <= 0) goldie_msleep(10);
    }
    return 0;
}
static int playback(void *ctx) {
    (void)ctx;
    uint8_t encoded[512]; int16_t pcm[512];
#ifdef PLATFORM_TYPE_WS63
    uint32_t previous = sys_now(), report = previous;
    uint32_t gap_max = 0, lock_max = 0, write_max = 0, level_max_ms = 0;
    uint32_t received_before = 0, consumed_before = 0;
    unsigned writes = 0;
    int last_rc = 0;
#endif
    for (;;) {
#ifdef PLATFORM_TYPE_WS63
        uint32_t now = sys_now(), gap = now - previous;
        previous = now;
        if (gap > gap_max) gap_max = gap;
#endif
        /* Output/reset serialize device access. Never hold rx while a BSP
         * operation waits: SDK callbacks must be able to enqueue meanwhile. */
        goldie_mutex_lock(&output);
#ifdef PLATFORM_TYPE_WS63
        uint32_t lock_ms = (uint32_t)sys_now() - now;
        if (lock_ms > lock_max) lock_max = lock_ms;
#endif
        for (unsigned batch = 0; batch < 8; ++batch) {
#ifdef PLATFORM_TYPE_WS63
            uint32_t level_start = sys_now();
#endif
            unsigned level = playing ? hw->get_valid_length(NULL) : 0;
#ifdef PLATFORM_TYPE_WS63
            uint32_t level_ms = (uint32_t)sys_now() - level_start;
            if (level_ms > level_max_ms) level_max_ms = level_ms;
#endif
            if (level >= 4096) break;
            goldie_mutex_lock(&rx);
            size_t n = ai_playback_pull(&queue, encoded, sizeof(encoded));
#ifdef PLATFORM_TYPE_WS63
            diag.consumed += (uint32_t)n;
#endif
            goldie_mutex_unlock(&rx);
            if (!n) break;
            if (!playing) {
                if (hw->audio_output_config) hw->audio_output_config(8000);
                hw->play_start(); playing = true;
            }
            for (size_t i = 0; i < n; ++i) pcm[i] = ai_alaw_decode(encoded[i]);
            /* WS63 output is mono PCM16. Refill to the existing device water
             * mark before sleeping, with a bounded batch for reset latency. */
#ifdef PLATFORM_TYPE_WS63
            uint32_t write_start = sys_now();
#endif
            int rc = hw->audio_write(pcm, (unsigned)(n * sizeof(pcm[0])));
#ifdef PLATFORM_TYPE_WS63
            uint32_t write_ms = (uint32_t)sys_now() - write_start;
            if (write_ms > write_max) write_max = write_ms;
            ++writes; last_rc = rc;
#endif
            if (rc < 0) {
                goldie_mutex_lock(&rx);
                ai_playback_reset(&queue);
                goldie_mutex_unlock(&rx);
                hw->play_stop(); playing = false; hw->reset_playbuffer();
                ai_event_t event = {AI_FAILED, session, -6}; ws_post(NULL, &event);
                break;
            }
        }
        unsigned remaining = playing ? hw->get_valid_length(NULL) : 0;
        bool device_empty = !remaining;
        goldie_mutex_lock(&rx);
#ifdef PLATFORM_TYPE_WS63
        diag.last_loop = sys_now(); diag.level = remaining;
        uint32_t received = diag.received, consumed = diag.consumed;
        size_t queued = queue.size;
        bool report_due = diag.last_loop - report >= 1000;
        uint32_t pushes = diag.pushes, push_gap_max = diag.push_gap_max;
        uint32_t push_age = diag.push_seen ? diag.last_loop - diag.last_push : 0;
        if (report_due) diag.pushes = diag.push_gap_max = 0;
#endif
        bool drained = queue.finished && !queue.size && device_empty;
        if (drained) queue.finished = false;
        goldie_mutex_unlock(&rx);
        if (drained) {
            if (playing) hw->play_stop();
            playing = false;
            ai_event_t event = {AI_TURN_FINISHED, session, 0}; ws_post(NULL, &event);
        }
        goldie_mutex_unlock(&output);
#ifdef PLATFORM_TYPE_WS63
        now = sys_now();
        if (report_due) {
            if (received != received_before || consumed != consumed_before || queued || remaining)
                printf("[ws_media_diag] t_ms=%lu window_ms=%lu in_g711=%lu pulled_g711=%lu queue=%lu device_pcm=%u gap_max_ms=%lu output_wait_max_ms=%lu write_max_ms=%lu level_max_ms=%lu writes=%u last_rc=%d pushes=%lu push_gap_max_ms=%lu push_age_ms=%lu\n",
                       (unsigned long)now, (unsigned long)(now - report), (unsigned long)(received - received_before),
                       (unsigned long)(consumed - consumed_before), (unsigned long)queued, remaining,
                       (unsigned long)gap_max, (unsigned long)lock_max, (unsigned long)write_max,
                       (unsigned long)level_max_ms, writes, last_rc,
                       (unsigned long)pushes, (unsigned long)push_gap_max, (unsigned long)push_age);
            received_before = received; consumed_before = consumed; report = now;
            gap_max = lock_max = write_max = level_max_ms = 0; writes = 0;
        }
#endif
        goldie_msleep(5);
    }
    return 0;
}
void ws_media_prepare(void) { hw = wait_service(AUDIO_SERVICE_INDEX); }
static void *media_thread_create(goldie_thread_handler fn, const char *name, unsigned priority) {
#ifdef PLATFORM_TYPE_WS63
    /* Set the original BSP priorities before the new task can run.
     * Lower numbers run first: BSP audio=20, playback=21, capture=22. */
    goldie_thread_lock();
    void *thread = goldie_thread_create(fn, NULL, name, 8192);
    int rc = thread ? goldie_thread_set_priority(thread, priority) : -1;
    goldie_thread_unlock();
    if (thread) printf("[ws_media] thread=%s priority=%u set_rc=%d\n", name, priority, rc);
    return thread;
#else
    (void)priority;
    return goldie_thread_create(fn, NULL, name, 8192);
#endif
}
int ws_media_init(void) {
    capture_format = ws_board_capture_format();
    if (!capture_format.channels || capture_format.channels > 8 ||
        capture_format.microphone >= capture_format.channels ||
        (capture_format.reference != AI_AUDIO_NO_REFERENCE && capture_format.reference >= capture_format.channels)) return -21;
    if (!hw) return -10;
    if (!hw->audio_read) return -11;
    if (!hw->audio_write) return -12;
    if (!hw->record_start) return -13;
    if (!hw->play_start) return -14;
    if (!hw->play_stop) return -15;
    /* audio_output_config is optional; fixed-rate BSPs already use 8 kHz. */
    if (!hw->get_valid_length) return -17;
    if (!hw->reset_playbuffer) return -18;
    goldie_mutex_init(&tx); goldie_mutex_init(&rx); goldie_mutex_init(&output);
    if (ai_playback_init(&queue, storage, sizeof(storage), 1600)) return -1;
    if (!media_thread_create(capture, "ai_capture", 22)) return -19;
    if (!media_thread_create(playback, "ai_playback", 21)) return -20;
    return 0;
}
void ws_media_session(uint32_t id) {
    goldie_mutex_lock(&tx); goldie_mutex_lock(&output);
    session = id;
    goldie_mutex_unlock(&output); goldie_mutex_unlock(&tx);
}
ai_media_ops_t ws_media_ops(void) {
    ai_media_ops_t ops = {.enable = enable, .reset = reset, .push = push, .finish = finish};
    return ops;
}
