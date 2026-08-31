/**
 * @file convai_tap.h
 * @brief Tap2talk (TAP) mode: tap to start recording, auto-stop on timeout.
 *
 * In TAP mode the user taps once to start recording. A client-side watchdog
 * enforces a two-phase timeout:
 *   Phase 1 - wait for speech after tap_start (TAP_TIMEOUT_MS).
 *   Phase 2 - after AI answer finished, wait for new speech (TAP_TIMEOUT_MS).
 * If either phase times out, tap mode auto-stops (interrupt + uplink stop).
 * Speech detection (status -> LISTENING) cancels Phase 1; ANSWER_FINISHED
 * arms Phase 2.
 *
 * Private header - used only by files in sdk_integration/. NOT part of the
 * public API; apps must not include this. The bridge wires this module in
 * from convai_bridge_tap_* forwarders, the on_status callback, and the
 * bridge_apply_turn_detection dispatcher, exactly as it does for the
 * comfort/uplink/downlink modules.
 */
#ifndef CONVAI_TAP_H
#define CONVAI_TAP_H

#include "convai/convai_api.h"
#include "convai_bridge.h"  /* convai_bridge_tap_state_cb typedef */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle (called by convai_bridge_tap_* forwarders) ---- */

/** Start tap recording: arm uplink, start watchdog, notify UI. */
void bridge_tap_start(void);

/** Stop tap recording: interrupt response, stop watchdog, stop uplink, notify UI. */
void bridge_tap_stop(void);

/** Non-zero if TAP2TALK is currently recording. */
int  bridge_tap_is_active(void);

/* ---- SDK callback hooks (called by convai_bridge on_status) ---- */

/**
 * Drive the watchdog state machine from SDK status changes:
 *   LISTENING       -> speech detected (cancels Phase 1 timeout)
 *   ANSWER_FINISHED -> arm Phase 2 timeout (TAP2TALK mode only)
 */
void bridge_tap_on_status(convai_status_e s);

/* ---- Cleanup (called by bridge_cleanup) ---- */

/** Stop the watchdog thread only - for the bridge_cleanup path. */
void bridge_tap_stop_watchdog(void);

/* ---- Callback registration ---- */

/** Register the tap-state callback (notifies UI of tap start/stop). */
void bridge_tap_set_state_cb(convai_bridge_tap_state_cb cb);

/* ---- Turn detection config (called by bridge_apply_turn_detection) ---- */

/** Apply TAP2TALK turn_detection: server_vad + idle_timeout_ms=5000. */
void bridge_tap_apply_turn_detection(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_TAP_H */

