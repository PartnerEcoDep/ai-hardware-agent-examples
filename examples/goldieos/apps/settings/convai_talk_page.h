/**
 * @file convai_talk_page.h
 * @brief Conversation/emotion page: animated avatar display with PTT-aware
 *        lifecycle (init/deinit) and emotion switching.
 *
 * The talk page shows a full-screen animated avatar that reacts to AI speech
 * and emotion function-calls.  It runs its own flush thread (resident, started
 * in talk_page_init, stopped in talk_page_deinit).
 *
 * UI isolation: this module does NOT include main_ui.h.  All GUI control
 * access (FrameView_talk, LabelView_talk_*, Window_main, etc.) is performed
 * via callbacks registered by the settings app.  This avoids the C++
 * static-global-per-TU problem — main_ui.h declares controls as `static`,
 * so each translation unit that includes it gets its own (empty) copies;
 * a separate talk_page.cpp would access empty shared_ptrs and crash.
 * The callbacks run in the settings app's translation unit, where the real
 * control instances live.
 *
 * Usage (in the settings app lifecycle):
 *   - goldie_app_run:   talk_page_set_ui_callbacks(...) + talk_page_init()
 *   - cloud_status_callback:
 *       ANSWERING  → talk_page_show() + talk_page_play_animation()
 *       IDLE / INTERRUPTED / ANSWER_FINISHED → talk_page_stop_animation() + talk_page_hide()
 *   - goldie_app_suspend: talk_page_stop_animation()
 *   - goldie_app_resume:  if visible → talk_page_play_animation()
 *   - goldie_app_exit:    talk_page_deinit()
 *   - handle_emotion:     talk_page_set_emotion(emotion_id)
 */
#ifndef CONVAI_TALK_PAGE_H
#define CONVAI_TALK_PAGE_H

#include "goldie_osal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Emotion IDs (set by handle_emotion via function-call) ---- */
enum {
    EMOTION_NEUTRAL = 0,
    EMOTION_HAPPY,
    EMOTION_ANGRY,
    EMOTION_SAD,
    EMOTION_DOUBT,
};

/* ---- Play types (used by the update_avatar callback impl) ---- */
enum {
    TALK_PLAY_SILENCE = 0,
    TALK_PLAY_SPEAK,
    TALK_PLAY_SLEEP,
};

/**
 * UI callback table — implemented by the settings app (which owns the GUI
 * controls).  Registered via talk_page_set_ui_callbacks() before talk_page_init().
 */
typedef struct {
    int  (*is_visible)(void);       /* return FrameView_talk->isVisible() */
    void (*show)(void);             /* show talk page, hide cloud/config */
    void (*hide)(void);             /* hide talk page, show cloud, refresh avatar */
    void (*flush)(void);            /* FrameView_talk->flush(full screen) */
    void (*update_avatar)(int status, int emotion, int avatar_id);
                                    /* update eyes/tie/text per SDK status & emotion.
                                     * status is a convai_status_e value; the impl
                                     * maps it to a play type + status label text. */
} talk_page_ui_cb_t;

/**
 * Register the UI callbacks.  Must be called before talk_page_init().
 * The callbacks are invoked from the animation thread (flush, update_avatar)
 * and from the main thread (is_visible, show, hide) — implementations must
 * be thread-safe w.r.t. tiny_gui (the settings app ensures the talk page
 * animation thread is the only one flushing while talk page is visible).
 */
void talk_page_set_ui_callbacks(const talk_page_ui_cb_t *cb);

/**
 * Start the talk animation thread + init the exit semaphore.
 * Call once at app startup (goldie_app_run).
 */
void talk_page_init(void);

/**
 * Stop the animation thread and wait for it to exit cleanly via semaphore
 * before destroying the thread handle.  Call once at app shutdown
 * (goldie_app_exit), BEFORE convai_bridge_stop().
 */
void talk_page_deinit(void);

/** Show the talk page full-screen (hides cloud page). */
void talk_page_show(void);

/** Hide the talk page and return to the cloud config page. */
void talk_page_hide(void);

/**
 * Stop the animation and hide the talk page if it is currently visible.
 * No-op if the talk page is not visible.  Convenience wrapper for the
 * common "if visible { stop_animation; hide }" pattern used at multiple
 * return-from-conversation sites.
 * @return 1 if the page was visible (and is now hidden), 0 otherwise
 */
int talk_page_stop_and_hide(void);

/** Start the animation flush loop.  No-op if already running. */
void talk_page_play_animation(void);

/** Stop the animation flush loop.  The thread stays alive (idle). */
void talk_page_stop_animation(void);

/** Check whether the talk page is currently visible. */
int talk_page_is_visible(void);

/**
 * Set the current emotion for the avatar display.
 * The animation thread picks this up on the next frame.
 */
void talk_page_set_emotion(int emotion);

/**
 * Sync the avatar gender from the settings page.
 * @param avatar_id  0=female, 1=male (drives eye/tie animation set)
 */
void talk_page_set_avatar(int avatar_id);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_TALK_PAGE_H */