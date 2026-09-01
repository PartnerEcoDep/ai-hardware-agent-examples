/**
 * @file convai_tap.c
 * @brief Tap2talk (TAP) mode implementation.
 *
 * See convai_tap.h for the behavioral summary. This module owns the TAP
 * watchdog thread, TAP state globals, and the TAP turn_detection config.
 * The bridge wires it in from convai_bridge_tap_* forwarders, the
 * on_status SDK callback, and bridge_apply_turn_detection.
 *
 * Thread lifecycle follows the comfort module pattern (convai_comfort.c):
 * running is set before goldie_thread_create, and stop joins via exit_sem
 * (goldie_sem_wait) rather than a fixed sleep, so shutdown is deterministic.
 * The self-exit path (timeout fires) calls bridge_tap_stop() after NULLing
 * the thread handle, so tap_timeout_stop() inside bridge_tap_stop() sees
 * NULL and skips the join - the thread then destroys the sem itself.
 */
#include "convai_tap.h"
#include "convai_audio_internal.h"   /* bridge_get_engine(), bridge_uplink_tap_*, bridge_uplink_get_audio_mode */
#include "convai_memory_budget.h"
#include "goldie_osal.h"

#include <stdio.h>

/* ---- config ---- */
#define TAP_TIMEOUT_MS 5000  /* Client-side timeout matching server idle_timeout_ms */

/* ---- internal state ---- */
static void       *g_tap_timeout_thread = NULL;
static goldie_sem  g_tap_timeout_exit_sem;
static volatile int g_tap_timeout_running = 0;
static volatile int g_tap_speech_detected = 0;
static volatile int g_tap_post_answer = 0;  /* Phase 2: waiting for speech after AI answer finished */

static convai_bridge_tap_state_cb g_tap_state_cb = NULL;

/* ---- forward decls ---- */
static int  tap_timeout_watchdog_thread(void *arg);
static void tap_timeout_start(void);
static void tap_timeout_stop(void);

/* ================================================================
 * Public API
 * ================================================================ */

void bridge_tap_start(void)
{
    bridge_uplink_tap_start();

    tap_timeout_start();
    if (g_tap_state_cb) g_tap_state_cb(1);  /* Notify UI: tap recording started */
}

void bridge_tap_stop(void)
{
    /* Interrupt current response - status change will arrive via on_status()
     * from the SDK state machine. Do NOT modify g_status directly. */
    convai_interrupt(bridge_get_engine());
    tap_timeout_stop();
    bridge_uplink_tap_stop();
    if (g_tap_state_cb) g_tap_state_cb(0);  /* Notify UI: tap recording stopped */
}

int bridge_tap_is_active(void)
{
    return bridge_uplink_tap_is_active();
}

void bridge_tap_on_status(convai_status_e s)
{
    /* Cancel tap timeout when speech is detected (status -> LISTENING) */
    if (s == CONVAI_STATUS_LISTENING) {
        g_tap_speech_detected = 1;
    }

    /* Arm Phase 2: after AI answer finished, wait for new speech */
    if (s == CONVAI_STATUS_ANSWER_FINISHED &&
        bridge_uplink_get_audio_mode() == CONVAI_BRIDGE_AUDIO_TAP2TALK &&
        bridge_uplink_tap_is_active() && g_tap_timeout_running) {
        g_tap_post_answer = 1;
        g_tap_speech_detected = 0;
        printf("[convai_tap] answer finished, post-answer timeout armed (%dms)\n", TAP_TIMEOUT_MS);
    }
}

void bridge_tap_stop_watchdog(void)
{
    tap_timeout_stop();
}

void bridge_tap_set_state_cb(convai_bridge_tap_state_cb cb)
{
    g_tap_state_cb = cb;
}

void bridge_tap_apply_turn_detection(void)
{
    const char *session_update =
        "{\"session\":{\"audio\":{\"input\":{\"turn_detection\":"
        "{\"type\":\"server_vad\",\"threshold\":0.5,\"prefix_padding_ms\":300,"
        "\"silence_duration_ms\":200,\"create_response\":true,"
        "\"interrupt_response\":false,\"idle_timeout_ms\":5000}}}}}";
    int ret = convai_update(bridge_get_engine(), session_update);
    if (ret != CONVAI_OK) {
        printf("[convai_tap] ERROR: convai_update(turn_detection) failed: %s\n",
               convai_err_2_str(ret));
    } else {
        printf("[convai_tap] turn_detection applied for mode: TAP2TALK\n");
    }
}

/* ================================================================
 * Watchdog thread
 * ================================================================ */

static int tap_timeout_watchdog_thread(void *arg)
{
    (void)arg;
    /* Local phase tracker: 0 = Phase 1 (waiting for first speech after tap),
     * 1 = Phase 2 (waiting for new speech after AI answer finished).
     * Cannot reuse g_tap_post_answer for this because that flag is consumed
     * (cleared) when arming Phase 2 - if left set, the inner wait loop skips
     * and the thread busy-loops printing "armed" forever. */
    int in_phase2 = 0;
    goldie_timeval start_tv;
    goldie_gettimeofday(&start_tv);
    long start_ms = start_tv.tv_sec * 1000 + start_tv.tv_usec / 1000;

    while (g_tap_timeout_running) {
        /* Wait for speech or timeout. */
        while (g_tap_timeout_running && !g_tap_speech_detected) {
            goldie_timeval now_tv;
            goldie_gettimeofday(&now_tv);
            long now_ms = now_tv.tv_sec * 1000 + now_tv.tv_usec / 1000;
            if (now_ms - start_ms >= TAP_TIMEOUT_MS) {
                if (!in_phase2) {
                    /* Phase 1 timeout: no speech after tap_start */
                    printf("[convai_tap] client timeout (%dms) - no speech detected, auto-stopping\n", TAP_TIMEOUT_MS);
                } else {
                    /* Phase 2 timeout: no speech after AI answer finished */
                    printf("[convai_tap] post-answer timeout (%dms) - no new speech, auto-exiting tap mode\n", TAP_TIMEOUT_MS);
                }
                g_tap_timeout_running = 0;
                g_tap_timeout_thread = NULL;
                bridge_tap_stop();
                /* self-exit: tap_timeout_stop saw NULL and skipped cleanup,
                 * so the thread destroys the sem itself before returning */
                goldie_sem_destroy(&g_tap_timeout_exit_sem);
                return 0;
            }
            goldie_msleep(50);
        }

        if (g_tap_speech_detected && g_tap_timeout_running) {
            g_tap_speech_detected = 0;
            if (!in_phase2) {
                /* Phase 1: speech detected, wait for ANSWER_FINISHED */
                printf("[convai_tap] speech detected, phase 1 done, waiting for answer\n");
            } else {
                /* Phase 2: new speech after answer, reset for next cycle */
                printf("[convai_tap] new speech detected after answer, resetting watchdog\n");
                in_phase2 = 0;
            }
        }

        /* Wait for ANSWER_FINISHED to arm Phase 2 */
        while (g_tap_timeout_running && !g_tap_post_answer) {
            goldie_msleep(50);
        }

        /* Phase 2 armed: consume the flag, reset timer, start waiting for new speech */
        if (g_tap_post_answer && g_tap_timeout_running) {
            g_tap_post_answer = 0;
            in_phase2 = 1;
            g_tap_speech_detected = 0;
            goldie_timeval arm_tv;
            goldie_gettimeofday(&arm_tv);
            start_ms = arm_tv.tv_sec * 1000 + arm_tv.tv_usec / 1000;
            printf("[convai_tap] post-answer watchdog armed (%dms)\n", TAP_TIMEOUT_MS);
        }
    }

    g_tap_timeout_running = 0;
    goldie_sem_post(&g_tap_timeout_exit_sem);
    return 0;
}

static void tap_timeout_start(void)
{
    if (g_tap_timeout_running) return;

    g_tap_speech_detected = 0;
    g_tap_timeout_running = 1;
    goldie_sem_init(&g_tap_timeout_exit_sem);

    goldie_thread_lock();
    g_tap_timeout_thread = goldie_thread_create(
        tap_timeout_watchdog_thread, NULL, "convai_tap_timeout",
        CONVAI_BUDGET_TAP_TIMEOUT_STACK_BYTES);
    if (g_tap_timeout_thread) {
        goldie_thread_set_priority(g_tap_timeout_thread, 20);
        printf("[convai_tap] timeout watchdog started (%dms)\n", TAP_TIMEOUT_MS);
    } else {
        g_tap_timeout_running = 0;
        goldie_sem_destroy(&g_tap_timeout_exit_sem);
        printf("[convai_tap] WARNING - failed to start timeout thread\n");
    }
    goldie_thread_unlock();
}

static void tap_timeout_stop(void)
{
    g_tap_timeout_running = 0;
    g_tap_speech_detected = 0;
    g_tap_post_answer = 0;
    if (g_tap_timeout_thread) {
        /* Wait for the thread to finish before destroying.
         * goldie_thread_destroy() kills the OS thread directly - calling it
         * while the task is still running leaks resources.  The sem is posted
         * by the thread just before it returns, so sem_wait guarantees the
         * task has fully completed.  (Pattern: convai_talk_page.cpp) */
        goldie_sem_wait(&g_tap_timeout_exit_sem);
        goldie_thread_destroy(g_tap_timeout_thread);
        g_tap_timeout_thread = NULL;
        goldie_sem_destroy(&g_tap_timeout_exit_sem);
        printf("[convai_tap] timeout watchdog stopped\n");
    }
}

