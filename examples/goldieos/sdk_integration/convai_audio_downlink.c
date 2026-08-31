/**
 * @file convai_audio_downlink.c
 * @brief Downlink audio pipeline: codec decode → ring buffer → DMA feedback playback.
 *
 * Owns the playback thread, ring buffer, DMA state machine, and downlink statistics.
 * Extracted from convai_bridge.c — behavior is bit-for-bit identical.
 */
#include "convai_audio_internal.h"
#include "convai_audio_dump.h"
#include "app_codec.h"
#include "convai_memory_budget.h"
#include "audio_service.h"
#include "goldie_osal.h"
#include "ringbuffer.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- Playback thread state ---- */
enum {
    PLAYBACK_IDLE,      /* HW stopped, waiting for next response */
    PLAYBACK_PLAYING,   /* DMA feedback-driven consumption */
};

#define DMA_TARGET           0x1800  /* 6144 bytes = 384ms target DMA fill level */
#define DMA_LOW              0x1000  /* 4096 bytes = 256ms, feed aggressively below this */
#define DMA_DRAINED          320     /* 20ms, considered drained for stop decision */
/* Pre-buffer threshold before starting playback: absorb arrival jitter so the DMA
 * doesn't underrun on the first burst. ~240ms of 16kHz-mono-16bit PCM. */
#define PLAYBACK_PREBUFFER   3840

#define AUDIO_DOWNLINK_DUMP_PATH         "audio_downlink_dump.wav"

typedef struct {
    int state;          /* current playback state */
    int running;        /* thread exit flag */
    int drain_to_stop;  /* set by on_status on ANSWER_FINISHED: drain then stop */
    void *thread_handle; /* goldie thread handle */
    goldie_sem exit_sem; /* semaphore for graceful exit */
    RingBuffer ring;    /* playback ring buffer */
    uint8_t ring_data[CONVAI_BUDGET_PLAYBACK_RING_BYTES];
    unsigned int dropped_bytes; /* PCM dropped because ring was full (overrun) */
} playback_ctrl_t;

static playback_ctrl_t g_playback_ctrl = {0};

static uint8_t g_pcm_decode_buf[CONVAI_BUDGET_PCM_DECODE_BYTES];

/* Keep the downlink dump on the playback thread: open, write, and close all
 * happen on one thread, and the WAV contains the PCM actually dequeued for
 * playback (including the final drain during session shutdown). */
static void playback_write(AudioService *audio, const void *data,
                           unsigned int len)
{
    bridge_dump_write(BRIDGE_AUDIO_DUMP_DOWNLINK, data, (size_t)len);
    if (audio && audio->audio_write) {
        audio->audio_write(data, len);
    }
}

/* ===================================================================
 *  Playback thread — DMA feedback-driven consumer.
 *
 *  Feeding: queries get_valid_length() to monitor audio hardware DMA
 *  water level.  When DMA is below DMA_TARGET (256 ms), data is pulled
 *  from the ring buffer and fed to the hardware.  When DMA is full,
 *  data stays in the ring buffer, absorbing network jitter.
 *
 *  Stopping: explicit signals from on_status.
 *    ANSWER_FINISHED → drain_to_stop flag → thread drains ring buffer
 *                      to DMA, waits for DMA to empty, then stops HW.
 *    INTERRUPTED     → immediate stop + ring buffer reset.
 *
 *  This eliminates the old fixed-10 ms-tick underrun by letting the
 *  hardware's actual consumption rate drive the feeding loop.
 * =================================================================== */

static int playback_thread_func(void *arg)
{
    (void)arg;
    playback_ctrl_t *ctrl = &g_playback_ctrl;
    const audio_hw_info_t *hw = bridge_get_audio_hw();
    AudioService *audio = (AudioService *)hw->audio_service;

    const int sr = hw->sample_rate > 0 ? hw->sample_rate : 8000;
    const int hw_reported_ch = hw->channels > 0 ? hw->channels : -1;

    /*
     * Playback data is intentionally kept MONO.
     *
     * Current DMA thresholds are all designed for:
     *   8000 Hz / mono / PCM16
     *
     * 20 ms  = 320 bytes
     * 256 ms = 4096 bytes
     * 384 ms = 6144 bytes
     */
    const int playback_ch = 1;

    int dump_ret = bridge_dump_open(BRIDGE_AUDIO_DUMP_DOWNLINK,
                                    AUDIO_DOWNLINK_DUMP_PATH,
                                    sr,
                                    playback_ch,
                                    16);
    if (dump_ret == 0) {
        printf("[convai_bridge] downlink audio dump file opened: %s\n",
               AUDIO_DOWNLINK_DUMP_PATH);
    } else {
        printf("[convai_bridge] WARNING: cannot open downlink dump file %s\n",
               AUDIO_DOWNLINK_DUMP_PATH);
    }

    printf(
        "[convai_bridge] playback thread started "
        "(sr=%d playback_ch=%d hw_reported_ch=%d, DMA feedback)\n",
        sr,
        playback_ch,
        hw_reported_ch
    );

    /* Static read buffer: the playback thread is a single instance (one
     * g_playback_ctrl, joined in bridge_downlink_stop before any restart),
     * so a file-scope buffer avoids a per-start malloc/free and the heap
     * fragmentation it causes on the memory-constrained WS63. */
    static uint8_t buf[CONVAI_BUDGET_PLAYBACK_READ_BYTES];

    int prev_state = PLAYBACK_IDLE;
    int hw_started = 0;

    while (ctrl->running) {

        switch (ctrl->state) {
        /* ---- HW stopped, waiting for next response ---- */
        case PLAYBACK_IDLE:
            if (hw_started && audio && audio->play_stop) {
                audio->play_stop();
                hw_started = 0;
                printf("[convai_bridge] playback HW stopped\n");
            }
            goldie_msleep(20);
            break;

        /* ---- DMA feedback-driven consumption ---- */
        case PLAYBACK_PLAYING: {
            /*
             * 1. Query hardware DMA water level.
             *    get_valid_length returns bytes still queued in the
             *    audio DMA buffer (0 = empty, large = full).
             */
            unsigned int dma_level = (audio && audio->get_valid_length)
                ? audio->get_valid_length(NULL) : 0;

            /*
             * 2. Feed DMA from the ring buffer.
             *    Before the HW is started, pre-buffer PLAYBACK_PREBUFFER bytes in the
             *    ring so the first burst of audio.delta has arrived before playback
             *    begins — this absorbs arrival jitter and avoids an immediate underrun.
             *    Once started, keep DMA topped up to DMA_TARGET.
             */
            if (!hw_started) {
                /* Pre-buffer phase: wait until enough PCM has accumulated, then
                 * start the HW and prime the DMA buffer up to DMA_TARGET in a loop. */
                if (ctrl->ring.count >= PLAYBACK_PREBUFFER) {
                    if (audio && audio->audio_output_config) {
                        audio->audio_output_config(sr);
                    }
                    if (audio && audio->play_start) {
                        audio->play_start();
                        hw_started = 1;
                    }
                    printf("[convai_bridge] playback HW started (sr=%d, prebuf=%u)\n",
                           sr, (unsigned)ctrl->ring.count);
                    /* Prime: drain ring into DMA until DMA is at target or ring empty. */
                    while (hw_started && dma_level < DMA_TARGET) {
                        int len = ring_buffer_bulk_read_noblock(&ctrl->ring,
                                                                buf, CONVAI_BUDGET_PLAYBACK_READ_BYTES);
                        if (len <= 0) break;
                        playback_write(audio, buf, (unsigned int)len);
                        dma_level = (audio && audio->get_valid_length)
                            ? audio->get_valid_length(NULL) : dma_level;
                    }
                }
            } else if (dma_level < DMA_TARGET) {
                /* Steady-state: keep draining the ring into the DMA until DMA is
                 * at target or the ring is empty. Previously this read only one
                 * 1024B chunk per loop iteration then slept, so during a downlink
                 * burst the IO thread filled the ring faster than this thread
                 * drained it → ring full → overrun (observed 45KB/turn). Draining
                 * in a loop keeps the ring low under burst load. */
                while (dma_level < DMA_TARGET) {
                    int len = ring_buffer_bulk_read_noblock(&ctrl->ring,
                                                            buf, CONVAI_BUDGET_PLAYBACK_READ_BYTES);
                    if (len <= 0) break;
                    playback_write(audio, buf, (unsigned int)len);
                    dma_level = (audio && audio->get_valid_length)
                        ? audio->get_valid_length(NULL) : dma_level;
                }
            }

            /*
             * 3. Check drain_to_stop: on_status sets this flag when
             *    ANSWER_FINISHED is received.  The thread drains the
             *    ring buffer to DMA, then waits for the DMA hardware
             *    to finish playing before stopping.
             */
            if (ctrl->drain_to_stop) {
                if (ctrl->ring.count == 0 && dma_level < DMA_DRAINED) {
                    if (hw_started && audio && audio->play_stop) {
                        audio->play_stop();
                        hw_started = 0;
                    }
                    ctrl->drain_to_stop = 0;
                    ctrl->state = PLAYBACK_IDLE;
                    printf("[convai_bridge] playback drained and stopped (dma=%u)\n",
                           dma_level);
                }
            }

            /*
             * 4. Dynamic sleep: sleep duration scales with DMA fill.
             *    High DMA → sleep longer (hardware has plenty).
             *    Low DMA  → sleep shorter (need to feed soon).
             *    This naturally follows the hardware consumption rate.
             */
            if (!hw_started)
                goldie_msleep(5);    /* waiting for first data */
            else if (dma_level > DMA_LOW)
                goldie_msleep(15);   /* plenty buffered, relax */
            else if (dma_level > DMA_DRAINED)
                goldie_msleep(5);    /* getting low, feed soon */
            else
                goldie_msleep(2);    /* nearly empty, urgent */
            break;
        }
        default:
            break;
        }
        prev_state = ctrl->state;
    }

    /* ---- Thread exiting ---- */
    ctrl->state = PLAYBACK_IDLE;

    /* Drain remaining ring-buffer data */
    {
        int d;
        while ((d = ring_buffer_bulk_read_noblock(&ctrl->ring,
                                                   buf, CONVAI_BUDGET_PLAYBACK_READ_BYTES)) > 0) {
            playback_write(audio, buf, (unsigned int)d);
        }
    }

    if (hw_started && audio && audio->play_stop) {
        audio->play_stop();
        hw_started = 0;
    }

    bridge_dump_close(BRIDGE_AUDIO_DUMP_DOWNLINK);
    printf("[convai_bridge] playback thread stopped\n");
    goldie_sem_post(&ctrl->exit_sem);

    return 0;
}

/* ---- Module entry points ---- */

void bridge_downlink_start(void)
{
    playback_ctrl_t *ctrl = &g_playback_ctrl;

    /* Init the ring buffer (mutex is initialised by ring_buffer_init) */
    ring_buffer_init(&ctrl->ring);
    ctrl->ring.buffer     = ctrl->ring_data;
    ctrl->ring.buffer_len = CONVAI_BUDGET_PLAYBACK_RING_BYTES;

    /* Init exit semaphore */
    goldie_sem_init(&ctrl->exit_sem);

    ctrl->state = PLAYBACK_IDLE;

    /* Set running=1 BEFORE creating the thread — the thread checks ctrl->running
     * at loop entry, so if we set it after thread_create the thread could run
     * with running==0 and exit immediately (silent playback failure). */
    ctrl->running = 1;

    goldie_thread_lock();
    ctrl->thread_handle = goldie_thread_create(
        playback_thread_func, NULL, "convai_playback",
        CONVAI_BUDGET_AUDIO_DOWNLINK_STACK_BYTES);
    if (ctrl->thread_handle) {
        goldie_thread_set_priority(ctrl->thread_handle, 21);
        printf("[convai_bridge] playback thread created\n");
    } else {
        /* Thread creation failed (task pool / heap exhausted). Clear running so
         * on_audio drops frames instead of flooding a ring nobody drains, and
         * destroy the sem to avoid leaking it. */
        ctrl->running = 0;
        goldie_sem_destroy(&ctrl->exit_sem);
        printf("[convai_bridge] ERROR: playback thread create failed — audio output disabled\n");
    }
    goldie_thread_unlock();
}

void bridge_downlink_stop(void)
{
    playback_ctrl_t *ctrl = &g_playback_ctrl;

    if (!ctrl->running) return;

    ctrl->running = 0;

    if (ctrl->thread_handle) {
        goldie_sem_wait(&ctrl->exit_sem);
        goldie_thread_destroy(ctrl->thread_handle);
        ctrl->thread_handle = NULL;
        goldie_sem_destroy(&ctrl->exit_sem);
    }

    /* Reset ring buffer (thread is gone, no contention) */
    ring_buffer_reset(&ctrl->ring);

    printf("[convai_bridge] playback thread destroyed\n");
}

void bridge_downlink_on_audio(const void *data, size_t len,
                              const convai_audio_frame_info_t *info)
{
    static unsigned int s_downlink_diag_count = 0;

    if (s_downlink_diag_count < 24) {
        const uint8_t *bytes = (const uint8_t *)data;
        size_t head_len = (bytes != NULL)
                          ? (len < 8 ? len : 8)
                          : 0;
        size_t i;

        printf("[convai_bridge] downlink frame: type=%d len=%u commit=%d head=",
               info != NULL ? (int)info->data_type : -1,
               (unsigned int)len,
               info != NULL ? info->commit : -1);

        for (i = 0; i < head_len; ++i) {
            printf("%02X", bytes[i]);
        }

        printf("\n");
        s_downlink_diag_count++;
    }

    int pcm_samples = 0;
    int dec_ret = app_codec_decode((const uint8_t *)data, (int)len,
                                   (int16_t *)g_pcm_decode_buf,
                                   (int)(sizeof(g_pcm_decode_buf) / sizeof(int16_t)),
                                   &pcm_samples);
    if (dec_ret != APP_CODEC_OK || pcm_samples == 0) {
        printf("[convai_bridge] WARNING: codec decode failed (ret=%d samples=%d)\n",
               dec_ret, pcm_samples);
        return;
    }
    static unsigned int s_opus_pcm_diag_count = 0;

    if (app_codec_get_id() == APP_CODEC_OPUS &&
        s_opus_pcm_diag_count < 24) {

        int codec_sr = app_codec_get_sample_rate();

        printf(
            "[OPUS-DL] packet=%u bytes "
            "decoded=%d samples sr=%d duration=%dms\n",
            (unsigned int)len,
            pcm_samples,
            codec_sr,
            codec_sr > 0
                ? pcm_samples * 1000 / codec_sr
                : -1
        );

        s_opus_pcm_diag_count++;
    }
    /* Opus decodes to 16kHz mono. Downsample to hw rate if needed. */
    const audio_hw_info_t *hw = bridge_get_audio_hw();
    int hw_sr = hw->sample_rate > 0 ? hw->sample_rate : 8000;
    int hw_reported_ch = hw->channels > 0 ? hw->channels : -1;

    if (app_codec_get_id() == APP_CODEC_OPUS) {
        int codec_sr = app_codec_get_sample_rate();  /* 16000 for Opus */
        if (codec_sr > hw_sr && hw_sr > 0) {
            int16_t *src = (int16_t *)g_pcm_decode_buf;
            int out_samples = (int)((long)pcm_samples * hw_sr / codec_sr);
            double ratio = (double)codec_sr / hw_sr;
            for (int i = 0; i < out_samples; i++) {
                double src_idx = i * ratio;
                int idx0 = (int)src_idx;
                int idx1 = idx0 + 1;
                if (idx0 >= pcm_samples) idx0 = pcm_samples - 1;
                if (idx1 >= pcm_samples) idx1 = pcm_samples - 1;
                double frac = src_idx - idx0;
                src[i] = (int16_t)(src[idx0] * (1.0 - frac) + src[idx1] * frac);
            }
            pcm_samples = out_samples;
        }
    }
    size_t pcm_len = (size_t)pcm_samples * 2;
    static unsigned int s_play_fmt_diag_count = 0;

    if (s_play_fmt_diag_count < 24) {
        printf(
            "[PLAY-FMT] sr=%d playback_ch=1 hw_reported_ch=%d "
            "samples=%d bytes=%u duration=%dms\n",
            hw_sr,
            hw_reported_ch,
            pcm_samples,
            (unsigned int)pcm_len,
            hw_sr > 0 ? pcm_samples * 1000 / hw_sr : -1
        );

        s_play_fmt_diag_count++;
    }

    /* If the playback thread isn't running (e.g. LOS_TaskCreate failed under
     * task-pool pressure), don't dump PCM into a ring nobody is draining — that
     * just accumulates overrun bytes (observed 336KB in one turn) and masks the
     * real problem. Drop the whole frame early instead. */
    if (!g_playback_ctrl.running || g_playback_ctrl.thread_handle == NULL) {
        g_playback_ctrl.dropped_bytes += pcm_len;
        return;
    }

    /*
     * Push decoded PCM into the ring buffer (non-blocking).
     * ring_buffer_bulk_write_noblock returns 0 on success (full frame written),
     * -1 when the ring is full and the entire frame is rejected (all-or-nothing).
     * Count rejected frames so we can tell underrun (no drops, DMA runs dry)
     * from overrun (drops, ring full). NOTE: the previous code compared the
     * return value to pcm_len, which miscounted every successful write (0 <
     * pcm_len) as a drop and inflated overrun_bytes hugely (44KB reported with
     * no audible loss).
     */
    int wr_ret = ring_buffer_bulk_write_noblock(&g_playback_ctrl.ring,
                                                  g_pcm_decode_buf,
                                                  (unsigned int)pcm_len);
    if (wr_ret != 0) {
        g_playback_ctrl.dropped_bytes += pcm_len;
    }
}

void bridge_downlink_on_status(convai_status_e s)
{
    /*
     * State-driven playback (DMA feedback mode):
     *   ANSWERING       → start playback (DMA feedback loop drives feeding)
     *   ANSWER_FINISHED  → signal drain-to-stop (thread drains ring buffer
     *                       to DMA, waits for DMA to empty, then stops HW)
     *   INTERRUPTED     → stop HW immediately, drop all buffered data
     */
    if (s == CONVAI_STATUS_ANSWERING) {
        g_playback_ctrl.drain_to_stop = 0;
        g_playback_ctrl.state = PLAYBACK_PLAYING;
    } else if (s == CONVAI_STATUS_ANSWER_FINISHED) {
        g_playback_ctrl.drain_to_stop = 1;
        /* state stays PLAYING — thread will drain then stop */
    } else if (s == CONVAI_STATUS_INTERRUPTED) {
        g_playback_ctrl.drain_to_stop = 0;
        g_playback_ctrl.state = PLAYBACK_IDLE;
        ring_buffer_reset(&g_playback_ctrl.ring);
    }
}

int bridge_downlink_get_stats(unsigned int *dropped_bytes)
{
    *dropped_bytes = g_playback_ctrl.dropped_bytes;
    return 0;
}



