/**
 * @file convai_audio_internal.h
 * @brief Internal shared state and cross-module entry points for the
 *        convai_bridge audio subsystem.
 *
 * Private header — used only by files in sdk_integration/.
 * NOT part of the public API; apps must not include this.
 */
#ifndef CONVAI_AUDIO_INTERNAL_H
#define CONVAI_AUDIO_INTERNAL_H

#include "convai/convai_api.h"
#include "convai_bridge.h"  /* convai_bridge_audio_mode_t (PTt mode enum) */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Engine accessors (owned by convai_bridge.c) ---- */

convai_engine_t bridge_get_engine(void);
int             bridge_is_started(void);

/* ---- Shared audio hardware info (owned by uplink, read by downlink) ---- */

typedef struct {
    void *audio_service;        /* AudioService* from goldieos */
    int   sample_rate;
    int   channels;
    int   bits_per_sample;
} audio_hw_info_t;

const audio_hw_info_t *bridge_get_audio_hw(void);

/* ---- Uplink module (convai_audio_uplink.c) ---- */

void bridge_uplink_start(void);
void bridge_uplink_stop(void);
void bridge_uplink_set_audio_source(void *src, int sr, int ch, int bits);
int  bridge_uplink_send(const uint8_t *data, size_t len,
                        const convai_audio_frame_info_t *info);
int  bridge_uplink_get_stats(unsigned int *sent, unsigned int *dropped);

/* ---- Uplink PTT (convai_audio_uplink.c) ---- */
/* Audio mode is owned by the uplink module (it drives the record thread).
 * Uses convai_bridge_audio_mode_t from convai_bridge.h (the public type). */

int  bridge_uplink_set_audio_mode(convai_bridge_audio_mode_t mode);
convai_bridge_audio_mode_t bridge_uplink_get_audio_mode(void);
void bridge_uplink_ptt_press(void);
void bridge_uplink_ptt_release(void);
int  bridge_uplink_ptt_is_pressed(void);

/* FE2026072900158: TAP2TALK control */
void bridge_uplink_tap_start(void);
void bridge_uplink_tap_stop(void);
int  bridge_uplink_tap_is_active(void);

/* FE2026072900158: TAP2TALK control */
void bridge_uplink_tap_start(void);
void bridge_uplink_tap_stop(void);
int  bridge_uplink_tap_is_active(void);

/* FE2026072900158: Apply turn_detection config to server based on current audio mode.
 * Called automatically after session starts. Can also be called manually if needed. */
void bridge_apply_turn_detection(void);

/* ---- Downlink module (convai_audio_downlink.c) ---- */

void bridge_downlink_start(void);
void bridge_downlink_stop(void);
void bridge_downlink_on_audio(const void *data, size_t len,
                              const convai_audio_frame_info_t *info);
void bridge_downlink_on_status(convai_status_e s);
int  bridge_downlink_get_stats(unsigned int *dropped_bytes);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_AUDIO_INTERNAL_H */

