/**
 * @file convai_ptt.h
 * @brief Push-to-talk (PTT) mode: manual press/release to control recording.
 *
 * In PTT mode the user holds a button to capture speech and releases to
 * commit.  Press sends response.cancel (via convai_interrupt) so the agent
 * stops any in-progress answer before listening; release forwards to the
 * uplink module which stops recording and sends the commit.
 *
 * Private header - used only by files in sdk_integration/. NOT part of the
 * public API; apps must not include this. The bridge wires this module in
 * from convai_bridge_ptt_press/release forwarders and the
 * bridge_apply_turn_detection dispatcher, exactly as it does for the
 * comfort/uplink/downlink modules.
 */
#ifndef CONVAI_PTT_H
#define CONVAI_PTT_H

#include "convai/convai_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- PTT actions (called by convai_bridge_ptt_* forwarders) ---- */

/**
 * Start capturing: interrupt any in-progress response, then arm the uplink
 * recorder. Dedup-protected - no-op if already pressed.
 */
void bridge_ptt_press(void);

/** Stop capturing and commit - uplink sends the commit to trigger AI response. */
void bridge_ptt_release(void);

/** Non-zero if PTT button is currently pressed (recording in progress). */
int  bridge_ptt_is_pressed(void);

/* ---- Turn detection config (called by bridge_apply_turn_detection) ---- */

/** Apply PTT turn_detection: null (manual control, no server VAD). */
void bridge_ptt_apply_turn_detection(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_PTT_H */

