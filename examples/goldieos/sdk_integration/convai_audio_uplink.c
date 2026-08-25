/**
 * @file convai_audio_uplink.c
 * @brief Uplink audio pipeline: capture → stereo-to-planar → codec encode → send.
 *
 * Owns the recording thread, audio hardware handle, and uplink statistics.
 * Extracted from convai_bridge.c — behavior is bit-for-bit identical.
 */
#include "convai_audio_internal.h"
#include "convai_audio_dump.h"
#include "convai_audio_diag.h"
#include "app_codec.h"
#include "convai_memory_budget.h"
#include "audio_service.h"
#include "goldie_osal.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- Audio source state ---- */
typedef struct {
    void         *audio_service;       /* AudioService* */
    int           sample_rate;
    int           channels;
    int           bits_per_sample;
    volatile int  running;             /* flag to stop recording thread (set by stop) */
    void         *thread_handle;       /* goldie thread handle */
    goldie_sem    exit_sem;            /* posted by thread on exit (join waits) */
    unsigned int  frames_sent;         /* mic frames successfully enqueued */
    unsigned int  frames_dropped;      /* mic frames dropped (send failed) */
    convai_bridge_audio_mode_t mode;   /* AUTO or PTT (bound to the session) */
    volatile int  ptt_pressed;         /* PTT button state (press=1/release=0, app reads via is_pressed) */
} audio_source_t;

static audio_source_t g_audio_src = {0};

#define AUDIO_UPLINK_DUMP_PATH     "audio_uplink_dump.wav"

/* ---- Shared audio HW accessor (downlink module reads this) ---- */
const audio_hw_info_t *bridge_get_audio_hw(void)
{
    /* audio_hw_info_t has identical layout to the fields we care about.
     * We cast the first 4 fields which are guaranteed to match. */
    return (const audio_hw_info_t *)&g_audio_src;
}


/* Process one mic frame: read → stereo-to-planar → codec encode → send.
 * Returns 1 if a frame was successfully sent, 0 if no data / encode failed /
 * send dropped. The static buffers are safe because the recording thread is
 * single per session — AUTO runs one, PTT runs one, never concurrently
 * (mode is bound to the session, see bridge_uplink_set_audio_mode). */
static int capture_one_frame(audio_source_t *s, AudioService *audio)
{
    static uint8_t buf[CONVAI_BUDGET_AUDIO_RECORD_BYTES];
    static uint8_t planar_buf[CONVAI_BUDGET_AUDIO_RECORD_BYTES];
    static uint8_t g711_buf[CONVAI_BUDGET_AUDIO_RECORD_BYTES];

    int len = audio->audio_read(buf, CONVAI_BUDGET_AUDIO_RECORD_BYTES);
    if (len <= 0) {
        audio_diag_update(audio, NULL, 0, 0);  /* log the no-data frame */
        goldie_msleep(10);  /* no data: yield */
        return 0;
    }
    bridge_dump_write(BRIDGE_AUDIO_DUMP_UPLINK, buf, (size_t)len);
    /* Deinterleave to planar [L(n).. R(n)..].
     *   L : mic signal (sent to cloud).
     *   R : WS63 = speaker playback (AEC echo-reference, captured by the mic
     *       hardware so the cloud can cancel playback echo); Win = forced 0
     *       (no AEC ref on the simulator). */
    int sample_count = len / (int)sizeof(short);
    int frame_count = sample_count / 2;
    int16_t *samples = (int16_t *)buf;
    int16_t *planar_samples = (int16_t *)planar_buf;
    for (int i = 0; i < frame_count; i++) {
        planar_samples[i] = samples[i * 2];
#ifdef PLATFORM_TYPE_WS63
        planar_samples[frame_count + i] = samples[i * 2 + 1];
#else
        planar_samples[frame_count + i] = 0;
#endif
    }
    /* Diagnostics: read-only metrics over the planar buffer (rmsL/rmsR/dc/zeros)
     * + vad_detect probe. Does not touch planar_buf/g711_buf or the encode path. */
    audio_diag_update(audio, planar_samples, (size_t)frame_count, 1);
    int enc_len = 0;
    int enc_ret = app_codec_encode(planar_samples, sample_count,
                                   g711_buf, CONVAI_BUDGET_AUDIO_RECORD_BYTES,
                                   &enc_len);
    if (enc_ret != APP_CODEC_OK || enc_len == 0) {
        printf("[convai_bridge] WARNING: codec encode failed (ret=%d)\n", enc_ret);
        return 0;
    }
    convai_audio_frame_info_t info;
    memset(&info, 0, sizeof(info));
    info.data_type = (convai_audio_data_type_e)app_codec_get_id();
    if (bridge_uplink_send(g711_buf, (size_t)enc_len, &info) == 0) {
        s->frames_sent++;
        return 1;
    }
    s->frames_dropped++;
    return 0;
}

/* Send the PTT commit frame (triggers AI response on button release). */
static void ptt_send_commit(void)
{
    printf("[convai_bridge] PTT: sending commit=1 (triggering AI response)\n");
    convai_audio_frame_info_t info;
    memset(&info, 0, sizeof(info));
    info.data_type = (convai_audio_data_type_e)app_codec_get_id();
    info.commit    = 1;
    bridge_uplink_send(NULL, 0, &info);
}

/* AUTO record thread: continuous capture for the whole session. Created at
 * bridge_uplink_start, exits when running=0 (stop). No commit — the server's
 * VAD detects end of speech. record_start/stop bracket the whole session.
 * audio_service is validated by bridge_uplink_start before thread creation.
 * running is volatile — stop() sets it from another thread (IO/app-exit). */
static int auto_record_thread(void *arg)
{
    (void)arg;
    audio_source_t *s = &g_audio_src;
    AudioService *audio = (AudioService *)s->audio_service;

    if (audio->record_start) audio->record_start();
    printf("[convai_bridge] AUTO: capture started (sr=%d)\n", s->sample_rate);
    while (s->running) {
        capture_one_frame(s, audio);
    }
    if (audio->record_stop) audio->record_stop();
    printf("[convai_bridge] capture stopped\n");
    goldie_sem_post(&s->exit_sem);
    return 0;
}

/* PTT record thread: SESSION-LIFETIME RESIDENT. Created at bridge_uplink_start
 * (PTT mode), destroyed at bridge_uplink_stop.
 *
 * DESIGN: NO wake semaphore, NO locks. Between presses the thread polls
 * ptt_pressed at 10ms (goldie_msleep yields the task, so idle cost is ~0 CPU).
 * press/release are plain volatile writes to ptt_pressed; stop() sets running=0
 * (same as AUTO). This is intentionally symmetric with AUTO's stop path — no
 * extra synchronization primitives, no use-after-destroy window on a shared sem.
 *
 * The 10ms idle poll adds ≤10ms press-to-capture latency, but a capture frame
 * is 40ms @ 8kHz, so end-to-end press→first-frame is ≤50ms (vs ~41ms with a
 * sem) — imperceptible for push-to-talk.
 *
 * CONCURRENCY: press/release run on the UI touch thread; stop() runs on the IO
 * thread (DISCONNECTED/FAILED) or app-exit thread. The only shared mutable
 * state they touch is the volatile ints running/ptt_pressed — aligned 32-bit
 * volatile reads/writes are atomic on both targets (Win x86, WS63 RISC-V), the
 * same assumption AUTO's `while(running)` already relies on. No shared pointer
 * or semaphore is touched by press/release, so there is no use-after-destroy
 * surface even if stop() destroys the thread mid-press. */
static int ptt_record_thread(void *arg)
{
    (void)arg;
    audio_source_t *s = &g_audio_src;
    AudioService *audio = (AudioService *)s->audio_service;

    printf("[convai_bridge] PTT: resident thread ready (sr=%d)\n", s->sample_rate);
    while (s->running) {
        if (!s->ptt_pressed) {
            goldie_msleep(10);   /* idle: poll for press every 10ms */
            continue;
        }
        /* capture one utterance until release or stop */
        if (audio->record_start) audio->record_start();
        printf("[convai_bridge] PTT: capture started\n");
        int captured_any = 0;
        while (s->running && s->ptt_pressed) {
            if (capture_one_frame(s, audio)) captured_any = 1;
        }
        if (audio->record_stop) audio->record_stop();
        printf("[convai_bridge] PTT: capture stopped\n");

        /* Commit only on a clean release (still running) AND we actually
         * captured audio. stop() breaks the loop with running=0 → no commit;
         * an ultra-fast tap (no audio) → no empty commit. */
        if (s->running && captured_any) {
            ptt_send_commit();
        }
    }
    printf("[convai_bridge] PTT: resident thread exiting\n");
    goldie_sem_post(&s->exit_sem);
    return 0;
}

/* ---- Module entry points ---- */

void bridge_uplink_set_audio_source(void *src, int sr, int ch, int bits)
{
    if (!src) {
        printf("[convai_bridge] audio source cleared\n");
        memset(&g_audio_src, 0, sizeof(g_audio_src));
        return;
    }
    g_audio_src.audio_service   = src;
    g_audio_src.sample_rate     = sr;
    g_audio_src.channels        = ch;
    g_audio_src.bits_per_sample = bits;
    printf("[convai_bridge] audio source set: sr=%d ch=%d bits=%d\n", sr, ch, bits);
}

/* Common recording-thread launch: dump_open + sem_init + thread_create.
 * `fn` is the mode-specific thread function (auto_record_thread or
 * ptt_record_thread); `tag` is "AUTO"/"PTT" for logging. Returns 0 on success,
 * -1 on thread create failure (state rolled back). */
static int start_record_thread(int (*fn)(void *), const char *tag)
{
    /* Open debug dump file (desktop only, no-op on embedded) */
    int dump_ret = bridge_dump_open(BRIDGE_AUDIO_DUMP_UPLINK, AUDIO_UPLINK_DUMP_PATH,
                                    g_audio_src.sample_rate ? g_audio_src.sample_rate : 8000,
                                    g_audio_src.channels ? g_audio_src.channels : 1,
                                    g_audio_src.bits_per_sample ? g_audio_src.bits_per_sample : 16);
    if (dump_ret == 0) {
        printf("[convai_bridge] %s: audio dump file opened: %s\n", tag, AUDIO_UPLINK_DUMP_PATH);
    } else {
        printf("[convai_bridge] %s: WARNING: cannot open dump file %s\n", tag, AUDIO_UPLINK_DUMP_PATH);
    }

    goldie_sem_init(&g_audio_src.exit_sem);

    /* Set running=1 BEFORE creating the thread — the thread checks s->running
     * in its capture loop, so if we set it after thread_create the thread could
     * run with running==0 and exit immediately (silent mic failure). */
    g_audio_src.running = 1;

    g_audio_src.thread_handle = goldie_thread_create(
        (goldie_thread_handler)fn, NULL, "convai_audio",
        CONVAI_BUDGET_AUDIO_UPLINK_STACK_BYTES);
    if (g_audio_src.thread_handle) {
        goldie_thread_set_priority(g_audio_src.thread_handle, 22);
        printf("[convai_bridge] %s: record thread started\n", tag);
    } else {
        g_audio_src.running = 0;
        g_audio_src.ptt_pressed = 0;
        goldie_sem_destroy(&g_audio_src.exit_sem);
        bridge_dump_close(BRIDGE_AUDIO_DUMP_UPLINK);
        printf("[convai_bridge] ERROR: %s record thread create failed — mic input disabled\n", tag);
    }
    return g_audio_src.thread_handle ? 0 : -1;
}

/* Join the recording thread: wait for it to exit + clean up. Caller must have
 * already set running=0 BEFORE calling. AUTO/PTT both exit their loops on
 * running==0 and post exit_sem here, so a single sem_wait rejoins either.
 * bridge_uplink_stop guards against double-join (concurrent stop from the IO
 * thread + app-exit) by checking running first. */
static void join_record_thread(void)
{
    if (g_audio_src.thread_handle) {
        goldie_sem_wait(&g_audio_src.exit_sem);
        goldie_thread_destroy(g_audio_src.thread_handle);
        g_audio_src.thread_handle = NULL;
        goldie_sem_destroy(&g_audio_src.exit_sem);
    }
    g_audio_src.running = 0;
    g_audio_src.ptt_pressed = 0;

    /* Stop audio capture (in case the thread was mid-capture when stopped) */
    AudioService *audio = (AudioService *)g_audio_src.audio_service;
    if (audio && audio->record_stop) audio->record_stop();

    bridge_dump_close(BRIDGE_AUDIO_DUMP_UPLINK);
}

void bridge_uplink_start(void)
{
    /* Validate the audio source up front — if the mic is unusable we refuse to
     * create the thread at all, so thread_handle stays NULL and ptt_press's
     * guard rejects presses with a clear log (instead of silently posting a
     * dead thread's sem). */
    AudioService *audio = (AudioService *)g_audio_src.audio_service;
    if (!audio || !audio->audio_read) {
        printf("[convai_bridge] no usable audio source, skipping recording\n");
        return;
    }
    if (g_audio_src.running) return;

    /* Both modes create a SESSION-LIFETIME recording thread at start. AUTO runs
     * the capture loop continuously; PTT's thread idles polling ptt_pressed
     * (resident). Press/release are plain volatile writes, not create/destroy.
     * Downlink (playback) is started separately by bridge_setup. */
    if (g_audio_src.mode == CONVAI_BRIDGE_AUDIO_PTT) {
        start_record_thread(ptt_record_thread, "PTT");
    } else {
        start_record_thread(auto_record_thread, "AUTO");
    }
}

void bridge_uplink_stop(void)
{
    /* Idempotent guard: concurrent stop() calls (IO thread on disconnect +
     * app-exit thread) — exactly one sees running==1 and proceeds to join; the
     * other sees running==0 and returns. Same pattern as bridge_downlink_stop.
     * The check+set is not locked, but the worst case is two threads reading
     * running==1 before either writes 0 — then both would join on one exit_sem
     * and one blocks forever. That cannot actually happen here: the SDK's IO
     * thread is the only DISCONNECTED emitter, and bridge_cleanup (app-exit)
     * runs after the engine is already torn down, so the two stop paths are
     * naturally serialized in practice. Downlink relies on the same assumption. */
    if (!g_audio_src.running) return;
    g_audio_src.running = 0;   /* signal thread to exit */

    join_record_thread();
    printf("[convai_bridge] audio recording stopped\n");
}

int bridge_uplink_send(const uint8_t *data, size_t len,
                       const convai_audio_frame_info_t *info)
{
    if (!bridge_get_engine() || !bridge_is_started()) return -1;
    return convai_send_audio(bridge_get_engine(), data, len, info);
}

int bridge_uplink_get_stats(unsigned int *sent, unsigned int *dropped)
{
    *sent    = g_audio_src.frames_sent;
    *dropped = g_audio_src.frames_dropped;
    return 0;
}

/* ===================================================================
 *  PTT (push-to-talk) control
 *
 *  The PTT recording thread is SESSION-LIFETIME RESIDENT (created at start,
 *  destroyed at stop). Between presses it polls ptt_pressed at 10ms (zero CPU
 *  via goldie_msleep yield). Press and release are plain volatile writes to
 *  ptt_pressed — no semaphore, no lock, no thread create/destroy, so repeated
 *  presses don't churn the WS63 task pool and there's no shared-sem
 *  use-after-destroy surface.
 *
 *    ptt_press   → ptt_pressed=1 → thread's idle poll sees it, captures one
 *                  utterance. already-pressed → no-op (touch is 1→0→1).
 *    ptt_release → ptt_pressed=0 → capture loop exits, thread commits (if any
 *                  audio captured) and returns to idle poll. Async — release
 *                  returns immediately; AI response arrives via the event cb.
 *    bridge_uplink_stop → running=0 → thread exits WITHOUT committing (session
 *                  ended, not an utterance ended). Same path as AUTO's stop.
 * =================================================================== */

int bridge_uplink_set_audio_mode(convai_bridge_audio_mode_t mode)
{
    /* Mode is bound to a session: refuse to switch mid-session. The recording
     * thread runs under a fixed mode; changing mode needs stop+restart. */
    if (bridge_is_started()) {
        printf("[convai_bridge] set_audio_mode(%s) refused: session active, stop first\n",
               mode == CONVAI_BRIDGE_AUDIO_AUTO ? "AUTO" : "PTT");
        return -1;
    }
    if (g_audio_src.mode != mode) {
        g_audio_src.mode = mode;
        printf("[convai_bridge] audio mode set to: %s\n",
               mode == CONVAI_BRIDGE_AUDIO_AUTO ? "AUTO" : "PTT");
    }
    return 0;
}

convai_bridge_audio_mode_t bridge_uplink_get_audio_mode(void)
{
    return g_audio_src.mode;
}

void bridge_uplink_ptt_press(void)
{
    if (g_audio_src.mode != CONVAI_BRIDGE_AUDIO_PTT) {
        printf("[convai_bridge] PTT press ignored (not in PTT mode)\n");
        return;
    }
    if (!bridge_is_started()) {
        printf("[convai_bridge] PTT press ignored (engine not started)\n");
        return;
    }
    if (!g_audio_src.running) {
        /* start_record_thread failed at bridge_uplink_start (no mic, or thread
         * create failed), or stop() already ran. No shared object to corrupt —
         * just refuse the press. */
        printf("[convai_bridge] PTT press ignored (record thread not running)\n");
        return;
    }
    if (g_audio_src.ptt_pressed) {
        /* Already pressed — a touch button always goes 1→0→1 (release before
         * re-press), so reaching here means the app called press twice without
         * an intervening release. No-op: the ongoing utterance is still being
         * captured and will commit on release. */
        printf("[convai_bridge] PTT: already pressed\n");
        return;
    }
    /* Plain volatile write — the resident thread's idle poll picks this up
     * within 10ms. No semaphore to post, so no concern about stop() destroying
     * it concurrently. */
    g_audio_src.ptt_pressed = 1;
}

void bridge_uplink_ptt_release(void)
{
    if (g_audio_src.mode != CONVAI_BRIDGE_AUDIO_PTT) {
        printf("[convai_bridge] PTT release ignored (not in PTT mode)\n");
        return;
    }
    if (!g_audio_src.ptt_pressed) {
        printf("[convai_bridge] PTT: not pressed\n");
        return;
    }
    /* Clearing ptt_pressed makes the capture loop exit; the thread then sends
     * commit (if it captured audio) and returns to idle poll. Async — release
     * returns immediately; commit is sent by the thread, AI response arrives
     * via the event callback. */
    g_audio_src.ptt_pressed = 0;
    printf("[convai_bridge] PTT: released (commit queued by capture thread)\n");
}

int bridge_uplink_ptt_is_pressed(void)
{
    return g_audio_src.ptt_pressed;
}

