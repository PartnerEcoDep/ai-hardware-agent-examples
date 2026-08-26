/**
 * @file convai_talk_page.cpp
 * @brief Conversation/emotion page implementation.
 *
 * UI-isolated: does NOT include main_ui.h.  All control access goes through
 * callbacks registered by the settings app.  This avoids the C++ static-
 * global-per-TU trap (main_ui.h's `static` controls would be empty copies
 * in this translation unit → null shared_ptr dereference).
 */
#include "convai_talk_page.h"
#include "convai_bridge.h"

#include <stdio.h>
#include <string.h>

/* ---- module state (file-scope) ---- */
static int talk_current_emotion = EMOTION_NEUTRAL;
static int talk_avatar_id      = 0;       /* 0=female, 1=male */

static int talk_init_flag       = 0;      /* thread exit gate */
static int talk_running_flag    = 0;      /* animation running gate */
static int talk_thread_running  = 0;      /* thread alive indicator */
static void *talk_thread_handle = NULL;   /* goldie_thread_create handle */
static goldie_sem talk_exit_sem;          /* thread exit notification */

/* UI callbacks (registered by settings app) */
static talk_page_ui_cb_t talk_ui;

/* ---- forward declarations (internal) ---- */
static int talk_play_task(void *param);

/* ================================================================
 * Public API
 * ================================================================ */

void talk_page_set_ui_callbacks(const talk_page_ui_cb_t *cb)
{
    if (cb) {
        talk_ui = *cb;
    } else {
        memset(&talk_ui, 0, sizeof(talk_ui));
    }
}

void talk_page_init(void)
{
    talk_init_flag = 1;
    goldie_sem_init(&talk_exit_sem);
    talk_thread_handle = goldie_thread_create(talk_play_task, NULL,
                                              "talk_anim", 0x1000);
    /* Only mark running if the thread was actually created — otherwise
     * deinit's sem_wait would block forever (no thread to post the sem). */
    talk_thread_running = (talk_thread_handle != NULL) ? 1 : 0;
    if (!talk_thread_handle) {
        printf("[Talk] WARNING: animation thread create failed\n");
    }
}

void talk_page_deinit(void)
{
    talk_running_flag = 0;
    talk_init_flag = 0;

    /* Wait for talk_play_task to finish via sem.
     * goldie_thread_destroy() directly kills the underlying OS thread
     * without waiting — we must ensure the task function has fully
     * returned before destroying the handle. */
    if (talk_thread_running) {
        goldie_sem_wait(&talk_exit_sem);
    }
    goldie_sem_destroy(&talk_exit_sem);

    if (talk_thread_handle) {
        goldie_thread_destroy(talk_thread_handle);
        talk_thread_handle = NULL;
    }
}

void talk_page_show(void)
{
    if (talk_ui.show) talk_ui.show();
}

void talk_page_hide(void)
{
    if (talk_ui.hide) talk_ui.hide();
}

int talk_page_stop_and_hide(void)
{
    if (!talk_page_is_visible()) return 0;
    talk_running_flag = 0;
    if (talk_ui.hide) talk_ui.hide();
    return 1;
}

void talk_page_play_animation(void)
{
    talk_running_flag = 1;
}

void talk_page_stop_animation(void)
{
    talk_running_flag = 0;
}

int talk_page_is_visible(void)
{
    return talk_ui.is_visible ? talk_ui.is_visible() : 0;
}

void talk_page_set_emotion(int emotion)
{
    /* FE2026072900158: PTT 和 TAP2TALK 模式下屏蔽表情，始终显示 NEUTRAL。
     * 这两种模式下用户通过按钮主动交互，不需要表情反馈。 */
    convai_bridge_audio_mode_t mode = convai_bridge_get_audio_mode();
    if (mode == CONVAI_BRIDGE_AUDIO_PTT || mode == CONVAI_BRIDGE_AUDIO_TAP2TALK) {
        talk_current_emotion = EMOTION_NEUTRAL;
        return;
    }
    talk_current_emotion = emotion;
}

void talk_page_set_avatar(int avatar_id)
{
    talk_avatar_id = avatar_id;
}

/* ================================================================
 * Animation thread
 * ================================================================ */

static int talk_play_task(void *param)
{
    (void)param;
    while (talk_init_flag) {
        while (talk_running_flag) {
            /* read status from bridge; the update_avatar callback maps it
             * to a play type + status label text. */
            int st = (int)convai_bridge_get_status();

            /* update avatar UI (eyes/tie/text) via callback */
            if (talk_ui.update_avatar) {
                talk_ui.update_avatar(st, talk_current_emotion, talk_avatar_id);
            }
            /* flush the talk page via callback */
            if (talk_ui.flush) {
                talk_ui.flush();
            }
            goldie_msleep(200);
        }
        goldie_msleep(200);
    }
    talk_thread_running = 0;
    goldie_sem_post(&talk_exit_sem);
    return 0;
}
