/**
 * @file convai_comfort.c
 * @brief Response-timeout comfort word module.
 *
 * See convai_comfort.h for the behavioral summary. This file owns the
 * watchdog thread and the comfort-message construction. The bridge wires it
 * in from bridge_setup/bridge_cleanup and the on_status/on_audio SDK
 * callbacks, exactly as it does for the uplink/downlink audio modules.
 *
 * Thread lifecycle follows the downlink pattern (convai_audio_downlink.c):
 * running is set before goldie_thread_create, and stop joins via exit_sem
 * (goldie_sem_wait) rather than a fixed sleep, so shutdown is deterministic.
 */
#include "convai_comfort.h"
#include "convai_audio_internal.h"   /* bridge_get_engine() */
#include "convai_memory_budget.h"
#include "goldie_osal.h"

#include <stdio.h>
#include <string.h>

/* ---- config ---- */
#define RESPONSE_TIMEOUT_MS     10000
#define COMFORT_TEXT            "别着急，我正在想呢"

/* ---- internal state ---- */
typedef struct {
    volatile int armed;      /* set by on_status/on_audio (SDK cb thread), read by watchdog */
    int          running;    /* set by start/stop (bridge thread), read by watchdog */
    long         start_ms;
    void        *thread_handle;
    goldie_sem   exit_sem;
} comfort_ctrl_t;

static comfort_ctrl_t g_comfort_ctrl = {0};

/* ---- forward decls ---- */
static int comfort_watchdog_thread(void *arg);

/* ---- comfort message ---- */

static void send_comfort_message(void)
{
    if (!bridge_get_engine()) return;

    printf("[convai_comfort] RESPONSE TIMEOUT - sending comfort message\n");
    char json[CONVAI_BUDGET_COMFORT_JSON_BYTES];
    int n = snprintf(json, sizeof(json),
        "{\"type\":\"conversation.item.create\",\"item\":{\"type\":\"ExternalTextToSpeech\",\"text\":\"%s\"}}",
        COMFORT_TEXT);

    if (n < 0 || (size_t)n >= sizeof(json)) {
        printf("[convai_comfort] comfort message truncated (n=%d)\n", n);
        return;
    }
    convai_send_message(bridge_get_engine(), json, (size_t)n, NULL);
}

/* ---- watchdog thread ---- */

static int comfort_watchdog_thread(void *arg)
{
    (void)arg;
    comfort_ctrl_t *ctrl = &g_comfort_ctrl;

    while (ctrl->running) {
        if (ctrl->armed) {
            goldie_timeval tv;
            goldie_gettimeofday(&tv);
            long now_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            if (now_ms - ctrl->start_ms >= RESPONSE_TIMEOUT_MS) {
                ctrl->armed = 0;
                send_comfort_message();
            }
        }
        goldie_msleep(50);
    }

    printf("[convai_comfort] watchdog thread stopped\n");
    goldie_sem_post(&ctrl->exit_sem);
    return 0;
}

/* ---- public API ---- */

void bridge_comfort_start(void)
{
    comfort_ctrl_t *ctrl = &g_comfort_ctrl;
    if (ctrl->running) return;

    goldie_sem_init(&ctrl->exit_sem);
    ctrl->armed = 0;

    /* Set running=1 BEFORE creating the thread: the thread checks running at
     * loop entry, so if we set it after thread_create the thread could exit
     * immediately (silent watchdog failure). */
    ctrl->running = 1;

    goldie_thread_lock();
    ctrl->thread_handle = goldie_thread_create(
            comfort_watchdog_thread, NULL, "convai_comfort",
            CONVAI_BUDGET_COMFORT_STACK_BYTES);
    if (ctrl->thread_handle) {
        goldie_thread_set_priority(ctrl->thread_handle, 20);
    } else {
        ctrl->running = 0;
        goldie_sem_destroy(&ctrl->exit_sem);
    }
    goldie_thread_unlock();

    if (ctrl->running) {
        printf("[convai_comfort] watchdog thread created\n");
    }
}

void bridge_comfort_stop(void)
{
    comfort_ctrl_t *ctrl = &g_comfort_ctrl;
    if (!ctrl->running) return;

    ctrl->armed = 0;
    ctrl->running = 0;

    if (ctrl->thread_handle) {
        goldie_sem_wait(&ctrl->exit_sem);      /* join: block until thread posts */
        goldie_thread_destroy(ctrl->thread_handle);
        ctrl->thread_handle = NULL;
        goldie_sem_destroy(&ctrl->exit_sem);
    }

    printf("[convai_comfort] watchdog thread destroyed\n");
}

void bridge_comfort_on_status(convai_status_e s)
{
    comfort_ctrl_t *ctrl = &g_comfort_ctrl;

    /* Arm on THINKING, disarm on any other state */
    if (s == CONVAI_STATUS_THINKING) {
        goldie_timeval tv;
        goldie_gettimeofday(&tv);
        ctrl->start_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        ctrl->armed = 1;
    } else {
        ctrl->armed = 0;
    }
}

void bridge_comfort_on_audio(const void *data, size_t len,
                             const convai_audio_frame_info_t *info)
{
    (void)data; (void)len; (void)info;

    /* TTS audio arrived — cancel any pending timeout */
    g_comfort_ctrl.armed = 0;
}
