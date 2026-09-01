/**
 * @file convai_ptt.c
 * @brief Push-to-talk (PTT) mode implementation.
 *
 * See convai_ptt.h for the behavioral summary. This module owns the PTT
 * press/release orchestration and the PTT turn_detection config. The bridge
 * wires it in from convai_bridge_ptt_* forwarders and bridge_apply_turn_detection.
 *
 * Press logic: dedup via uplink state, interrupt any in-progress response
 * (convai_interrupt sends response.cancel), then arm the uplink recorder.
 * The SDK state machine fires on_status(INTERRUPTED) asynchronously - this
 * module does NOT touch g_status or call bridge_downlink_on_status, that
 * would bypass the state machine and cause state confusion.
 */
#include "convai_ptt.h"
#include "convai_audio_internal.h"   /* bridge_get_engine(), bridge_uplink_ptt_* */

#include <stdio.h>

/* ---- public API ---- */

void bridge_ptt_press(void)
{
    if (bridge_uplink_ptt_is_pressed()) {
        printf("[convai_ptt] already pressed, ignoring duplicate press\n");
        return;
    }
    /* convai_interrupt sends response.cancel; the state machine will fire
     * on_status(INTERRUPTED) when the server confirms - do NOT touch
     * g_status or bridge_downlink_on_status here, that bypasses the
     * state machine and causes state confusion. */
    convai_interrupt(bridge_get_engine());
    bridge_uplink_ptt_press();
}

void bridge_ptt_release(void)
{
    bridge_uplink_ptt_release();
}

int bridge_ptt_is_pressed(void)
{
    return bridge_uplink_ptt_is_pressed();
}

void bridge_ptt_apply_turn_detection(void)
{
    const char *session_update =
        "{\"session\":{\"audio\":{\"input\":{\"turn_detection\":null}}}}";
    int ret = convai_update(bridge_get_engine(), session_update);
    if (ret != CONVAI_OK) {
        printf("[convai_ptt] ERROR: convai_update(turn_detection) failed: %s\n",
               convai_err_2_str(ret));
    } else {
        printf("[convai_ptt] turn_detection applied for mode: PTT\n");
    }
}

