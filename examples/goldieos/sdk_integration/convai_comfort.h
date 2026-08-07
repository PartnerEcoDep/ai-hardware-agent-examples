/**
 * @file convai_comfort.h
 * @brief Response-timeout comfort word: inject a placeholder phrase when the
 *        agent is slow to start speaking.
 *
 * Arms a watchdog when the conversation enters THINKING; if no TTS audio
 * arrives within RESPONSE_TIMEOUT_MS, sends a fixed comfort text via
 * convai_send_message so the user isn't left in silence. Disarmed on any
 * status change away from THINKING and on the first TTS audio frame.
 *
 * Private header — used only by files in sdk_integration/. NOT part of the
 * public API; apps must not include this. The bridge wires this module in
 * from its setup/cleanup and on_status/on_audio callbacks, exactly as it
 * does for the uplink/downlink audio modules.
 */
#ifndef CONVAI_COMFORT_H
#define CONVAI_COMFORT_H

#include "convai/convai_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle (called by convai_bridge setup/cleanup) ---- */

/**
 * Start the watchdog thread. Idempotent — no-op if already running.
 * Spawns a low-priority polling thread that watches the armed flag.
 */
void bridge_comfort_start(void);

/**
 * Stop the watchdog thread and release resources. Idempotent — no-op if not
 * running. Joins the thread via exit_sem before destroying it.
 */
void bridge_comfort_stop(void);

/* ---- SDK callback hooks (called by convai_bridge on_status/on_audio) ---- */

/**
 * Arm the timeout on THINKING, disarm on any other status.
 */
void bridge_comfort_on_status(convai_status_e s);

/**
 * TTS audio arrived — disarm any pending timeout. The frame contents are not
 * inspected; only the arrival event matters. Signature mirrors
 * bridge_downlink_on_audio so the bridge forwards uniformly.
 */
void bridge_comfort_on_audio(const void *data, size_t len,
                             const convai_audio_frame_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_COMFORT_H */