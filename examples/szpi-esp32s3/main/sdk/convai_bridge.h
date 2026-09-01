/**
 * @file convai_bridge.h
 * @brief Thin integration layer: ESP32 apps -> ConvAI SDK public API.
 *
 * Kept API-compatible with goldieos' convai_bridge where it makes sense:
 * init / start / stop / restart / getter / callback registration.
 * The audio uplink (mic capture thread) is owned here, so DISCONNECTED /
 * FAILED events can stop it cleanly.
 *
 * Differences vs goldieos (intentional, out of scope for now):
 *   - No PTT / audio-mode switching (continuous mic capture).
 *   - Downlink uses a ring buffer + playback task to smooth network jitter;
 *     INTERRUPTED flushes queued TTS so barge-in stops the speaker quickly.
 *   - No comfort timeout or service_manager. Function-call dispatch lives in
 *     the separate convai_func_dispatch / convai_func_handlers modules.
 *   - No convai_bridge_set_audio_source (mic is the only source).
 *   - convai_bridge_set_startup_config feeds the voice selector's config.
 */
#ifndef CONVAI_BRIDGE_H
#define CONVAI_BRIDGE_H

#include "convai_api.h"
#include "convai_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Lifecycle ---- */

/** Register the platform HAL, create the SDK engine. Does NOT start the
 *  session yet — call convai_bridge_start() to connect. */
void convai_bridge_init(void);

/** Start the session (connect to server, begin processing). Spawns the
 *  mic-capture task. Idempotent: returns 0 if already started. */
int  convai_bridge_start(void);

/** Stop the session gracefully. Stops the mic-capture task. Idempotent. */
int  convai_bridge_stop(void);

/** stop + 100ms delay + start. */
int  convai_bridge_restart(void);

/* ---- Accessors ---- */

convai_engine_t convai_bridge_get_engine(void);
convai_status_e convai_bridge_get_status(void);
int  convai_bridge_is_speaking(void);
int  convai_bridge_is_started(void);

/** Uplink (mic) send stats since capture task started.
 *  frames_sent: mic frames enqueued to the SDK.
 *  frames_dropped: frames where codec->read failed or convai_send_audio
 *  returned non-OK (no engine, queue full, etc.).
 *  Returns 0 on success, -1 if capture never ran. */
int  convai_bridge_get_uplink_stats(unsigned int *frames_sent,
                                    unsigned int *frames_dropped);

/* ---- Callback types ---- */
typedef void (*convai_bridge_status_cb)(convai_status_e status);
typedef void (*convai_bridge_event_cb)(convai_event_code_e event_type,
                                       const char *info);
typedef void (*convai_bridge_message_cb)(const char *message);

/** Register callbacks (pass NULL to clear). The bridge itself only logs
 *  events internally; UI/UI-side modules subscribe through these setters
 *  so page registration can swap handlers (mirrors goldieos). */
void convai_bridge_on_status(convai_bridge_status_cb cb);
void convai_bridge_on_event(convai_bridge_event_cb cb);
void convai_bridge_on_message(convai_bridge_message_cb cb);

/* ---- Config injection (mirrors goldieos) ---- */

/** Startup config JSON passed to convai_start() via opt.params.
 *  May be called before start(); survives across restart. */
void convai_bridge_set_startup_config(const char *config);
const char *convai_bridge_get_startup_config(void);

/** Device name used in the create-time config (e.g. WiFi MAC).
 *  Call before convai_bridge_init(). NULL restores the hardcoded default. */
void convai_bridge_set_device_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_BRIDGE_H */
