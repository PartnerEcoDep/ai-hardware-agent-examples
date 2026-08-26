/**
 * @file convai_bridge.h
 * @brief Thin integration layer: goldieos apps -> ConvAI SDK public API.
 *
 * Thin integration layer: goldieos apps -> ConvAI SDK public API.
 * Transport/protocol are stubs; key API calls log to stdout.
 */
#ifndef CONVAI_BRIDGE_H
#define CONVAI_BRIDGE_H

#include "convai/convai_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Service index (CONVAI_SERVICE_INDEX = 4) ---- */
#define CONVAI_BRIDGE_SERVICE_INDEX  4

/* ---- Lifecycle ---- */

/**
 * Create an engine with default config and logging callbacks.
 * Registers itself as CONVAI_BRIDGE_SERVICE_INDEX in the service manager.
 */
void convai_bridge_init(void);

/**
 * Start the engine session (wake-up / begin conversation).
 * Logs the call; transport connect is stubbed.
 */
int  convai_bridge_start(void);

/**
 * Stop the engine session (sleep / end conversation).
 */
int  convai_bridge_stop(void);

/**
 * Restart: stop + start.
 */
int  convai_bridge_restart(void);

/* ---- Accessors ---- */

/** Get the engine handle for direct SDK calls. */
convai_engine_t convai_bridge_get_engine(void);

/** Get current agent status (mapped from on_convai_conversation_status). */
convai_status_e convai_bridge_get_status(void);

/** Non-zero if agent is currently speaking. */
int convai_bridge_is_speaking(void);

/** Non-zero if a session is active (between convai_bridge_start and stop).
 * Audio mode can only be changed when this returns 0. */
int convai_bridge_is_started(void);

/** Uplink (mic) audio send statistics since the recording thread started.
 *  frames_sent: mic frames successfully enqueued to the SDK send queue.
 *  frames_dropped: mic frames dropped because send failed (queue full / OOM /
 *  not connected). A high drop rate (>10%) can degrade upstream ASR quality.
 *  Returns 0 on success, -1 if no recording has run. */
int convai_bridge_get_uplink_stats(unsigned int *frames_sent,
                                   unsigned int *frames_dropped);

/** Downlink (playback) audio statistics since playback started.
 *  dropped_bytes: decoded PCM dropped because the playback ring was full
 *  (overrun). Non-zero means the playback thread can't keep up with arrivals
 *  (CPU/scheduling), distinct from underrun (DMA runs dry, no drops).
 *  Returns 0 on success, -1 if no playback has run. */
int convai_bridge_get_downlink_stats(unsigned int *dropped_bytes);
/** Opaque audio source handle (AudioService from goldieos). */
typedef void convai_audio_source_t;

/**
 * Set the audio source for automatic mic recording during sessions.
 * Must be called before convai_bridge_start(). Pass NULL to disable.
 * When set, start() spawns a background thread that reads PCM from
 * the source and feeds it to convai_send_audio_data().
 */
void convai_bridge_set_audio_source(convai_audio_source_t *src,
                                    int sample_rate, int channels, int bits);

/**
 * Send a PCM audio frame to the agent.
 * Logs the call; actual network send is stubbed.
 */
int convai_bridge_send_audio(const uint8_t *data, size_t len,
                             const convai_audio_frame_info_t *info);

/* ---- Audio Mode / PTT ---- */

/** Audio recording mode (FE2026072900158: extended with tap2talk) */
typedef enum {
    CONVAI_BRIDGE_AUDIO_AUTO     = 0,  /**< Continuous recording, server-side VAD (duplex, legacy) */
    CONVAI_BRIDGE_AUDIO_PTT      = 1,  /**< Push-to-talk: manual press/release (push2talk mode) */
    CONVAI_BRIDGE_AUDIO_TAP2TALK = 2,  /**< FE2026072900158: VAD + idle_timeout_ms=5000, tap to start */
} convai_bridge_audio_mode_t;

/**
 * Set audio recording mode (AUTO or PTT). Default is AUTO.
 *
 * Mode is bound to a session: only allowed when the engine is NOT started.
 * Calling this during an active session returns -1 (caller must stop the
 * session first, then switch, then restart). This keeps the recording thread
 * model simple — one mode per session, no mid-session reconfiguration of the
 * capture loop.
 *
 * @return 0 on success, -1 if a session is active (mode unchanged).
 */
int convai_bridge_set_audio_mode(convai_bridge_audio_mode_t mode);

/** Get current audio recording mode. */
convai_bridge_audio_mode_t convai_bridge_get_audio_mode(void);

/**
 * PTT: start capturing mic audio (call on button press).
 * Only effective in PTT mode and when engine is started.
 */
void convai_bridge_ptt_press(void);

/**
 * PTT: stop capturing and send commit to trigger AI response (call on release).
 * Only effective in PTT mode.
 */
void convai_bridge_ptt_release(void);

/** Non-zero if PTT button is currently pressed (recording in progress). */
int convai_bridge_ptt_is_pressed(void);

/**
 * FE2026072900158: TAP2TALK: start capturing mic audio (first tap).
 * Only effective in TAP2TALK mode and when engine is started.
 */
void convai_bridge_tap_start(void);

/**
 * FE2026072900158: TAP2TALK: stop capturing mic audio (second tap).
 * No commit sent — server VAD handles end of speech.
 */
void convai_bridge_tap_stop(void);

/** Non-zero if TAP2TALK is currently recording. */
int convai_bridge_tap_is_active(void);

/* ---- Callback types (for apps that need status notifications) ---- */
typedef void (*convai_bridge_status_cb)(convai_status_e status);
typedef void (*convai_bridge_event_cb)(convai_event_code_e event_type, const char *info);
typedef void (*convai_bridge_message_cb)(const char *message);

/** Register callbacks for status changes, connection events, and emotion updates. */
void convai_bridge_on_status(convai_bridge_status_cb cb);
void convai_bridge_on_event(convai_bridge_event_cb cb);
void convai_bridge_on_message(convai_bridge_message_cb cb);

/**
 * Set / get the startup config JSON passed to convai_start() via
 * opt.config_json.  Call set() before start() to supply the AI
 * personality/voice configuration generated by the settings UI.
 * Returns NULL if no config has been set.
 */
void convai_bridge_set_startup_config(const char *config);
const char *convai_bridge_get_startup_config(void);

/**
 * Set the device name used in the create-time config (e.g. WiFi MAC).
 * Call before convai_bridge_init(). If not set or NULL, a hardcoded default
 * is used. The app layer is responsible for obtaining a platform-specific
 * unique ID — bridge does not depend on any platform's device_id function.
 */
void convai_bridge_set_device_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_BRIDGE_H */

