/**
 * @file convai_bridge.c
 * @brief Connects goldieos apps to the ConvAI SDK public API.
 *
 * Thin orchestration layer: engine lifecycle, SDK callback routing,
 * and public-API forwarders. Audio pipelines (uplink/downlink) and
 * config defaults live in their own modules.
 */
#include "convai_bridge.h"
#include "convai_bridge_defaults.h"
#include "convai_config_file.h"
#include "convai_audio_internal.h"
#include "convai_comfort.h"
#include "convai_memory_budget.h"
#include "service_manager.h"
#include "goldie_osal.h"

#include <stdio.h>
#include <string.h>

/* ---- internal state ---- */
static convai_engine_t          g_engine    = NULL;
static convai_status_e          g_status    = CONVAI_STATUS_IDLE;
static int                      g_started   = 0;
static convai_bridge_status_cb  g_status_cb  = NULL;
static convai_bridge_event_cb   g_event_cb   = NULL;
static convai_bridge_message_cb g_message_cb = NULL;

/* ---- startup config (set by settings UI, consumed by start) ---- */
static char g_startup_config[CONVAI_BUDGET_STARTUP_CONFIG_BYTES] = {0};

/* Device name injected by app layer (e.g. WiFi MAC). NULL → use default. */
static char g_device_name[CONVAI_BUDGET_DEVICE_NAME_BYTES] = {0};

static char g_json_copy_buf[CONVAI_BUDGET_JSON_COPY_BYTES]; /* 用于 on_message_data 回调 */

/* ---- Internal accessors (consumed by audio modules) ---- */
convai_engine_t bridge_get_engine(void) { return g_engine; }
int             bridge_is_started(void) { return g_started; }

/* ---- SDK callbacks ---- */
static void bridge_cleanup(void);

static void on_event(convai_engine_t e, convai_event_t *ev, void *ud)
{
    (void)e; (void)ud;
    const char *info = NULL;
    switch (ev->code) {
    case CONVAI_EV_CONNECTED:
        info = ev->data.details ? ev->data.details : "";
        printf("[convai_bridge] EVENT: CONNECTED (%s)\n", info);
        break;
    case CONVAI_EV_DISCONNECTED:
        info = ev->data.details ? ev->data.details : "";
        printf("[convai_bridge] EVENT: DISCONNECTED (reason=%s)\n", info);
        bridge_cleanup();
        break;
    case CONVAI_EV_FAILED:
        info = ev->data.details ? ev->data.details : "";
        printf("[convai_bridge] EVENT: FAILED %s\n", ev->data.details);
        break;
    default: break;
    }
    if (g_event_cb) g_event_cb(ev->code, info);
}

static void on_status(convai_engine_t e, convai_status_e s, void *ud)
{
    (void)e; (void)ud;
    g_status = s;
    if (g_status_cb) g_status_cb(s);
    const char *str = "?";
    switch (g_status) {
        case CONVAI_STATUS_IDLE:          str = "IDLE"; break;
        case CONVAI_STATUS_LISTENING:     str = "LISTENING"; break;
        case CONVAI_STATUS_THINKING:      str = "THINKING"; break;
        case CONVAI_STATUS_ANSWERING:     str = "ANSWERING"; break;
        case CONVAI_STATUS_INTERRUPTED:   str = "INTERRUPTED"; break;
        case CONVAI_STATUS_ANSWER_FINISHED: str = "ANSWER_FINISH"; break;
    }

    printf("[STATUS] %s\n", str);

    /* Forward to downlink module for playback state machine */
    bridge_downlink_on_status(s);

    /* Forward to comfort module for response-timeout arming */
    bridge_comfort_on_status(s);
}

static void on_audio(convai_engine_t e, const void *data, size_t len,
                     const convai_audio_frame_info_t *info, void *ud)
{
    (void)e; (void)ud;
    bridge_downlink_on_audio(data, len, info);

    /* TTS audio arrived — cancel any pending comfort timeout */
    bridge_comfort_on_audio(data, len, info);
}

static void on_message_data(convai_engine_t e, const void *data, size_t len,
                           const convai_message_info_t *info, void *ud)
{
    (void)ud;

    printf("[convai_bridge] MESSAGE: %.*s (binary=%d)\n",
           (int)len, (const char *)data, info ? info->is_binary : 0);

    /* Pass raw JSON to the app layer via generic callback */
    if (g_message_cb && !(info && info->is_binary)) {
        size_t copy_len = len < sizeof(g_json_copy_buf) - 1 ? len : sizeof(g_json_copy_buf) - 1;
        memcpy(g_json_copy_buf, data, copy_len);
        g_json_copy_buf[copy_len] = '\0';
        g_message_cb(g_json_copy_buf);
    }
}

/* ===================================================================
 *  Public API
 * =================================================================== */

void convai_bridge_init(void)
{
    if (g_engine) {
        printf("[convai_bridge] already initialized\n");
        bridge_comfort_stop();
        return;
    }

    /* Load the optional runtime config file before building the config JSON:
     * values in convai.cfg (next to the executable) override the compiled-in
     * credential defaults. No-op on embedded targets without a filesystem
     * path to the executable (ws63 stub always fails silently). */
    if (convai_config_file_init() == 0) {
        printf("[convai_bridge] runtime config loaded from convai.cfg\n");
    }

    /* Platform init must be done by the app layer before calling this — bridge
     * is platform-agnostic and does not call any platform-specific init. */
    char config_json[CONVAI_BUDGET_STARTUP_CONFIG_BYTES];
    const char *dev_name = g_device_name[0] ? g_device_name : NULL;
    const char *cfg = bridge_build_config_json(config_json, sizeof(config_json), dev_name);

    printf("[convai_bridge] using config:\n%s\n", cfg);

    convai_event_handler_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.on_convai_event        = on_event;
    cb.on_convai_conversation_status = on_status;
    cb.on_convai_audio_data          = on_audio;
    cb.on_convai_message_data        = on_message_data;

    int ret = convai_create(&g_engine, cfg, &cb, NULL);
    if (ret != CONVAI_OK) {
        printf("[convai_bridge] ERROR: convai_create failed: %s\n", convai_err_2_str(ret));
        g_engine = NULL;
        return;
    }

    printf("[convai_bridge] engine created\n");
    printf("[convai_bridge] SDK version: %s\n", convai_get_version());

    register_service(CONVAI_BRIDGE_SERVICE_INDEX, g_engine);
    printf("[convai_bridge] registered at service index %d\n",
           CONVAI_BRIDGE_SERVICE_INDEX);
}

void convai_bridge_set_audio_source(convai_audio_source_t *src,
                                    int sr, int ch, int bits)
{
    bridge_uplink_set_audio_source(src, sr, ch, bits);
}

/* ---- startup config helpers ---- */

void convai_bridge_set_startup_config(const char *config)
{
    if (config) {
        strncpy(g_startup_config, config, sizeof(g_startup_config) - 1);
        g_startup_config[sizeof(g_startup_config) - 1] = '\0';
        printf("[convai_bridge] startup config saved (%zu bytes)\n", strlen(g_startup_config));
    }
}

void convai_bridge_set_device_name(const char *name)
{
    if (name) {
        strncpy(g_device_name, name, sizeof(g_device_name) - 1);
        g_device_name[sizeof(g_device_name) - 1] = '\0';
        printf("[convai_bridge] device name set: %s\n", g_device_name);
    } else {
        g_device_name[0] = '\0';
    }
}

const char *convai_bridge_get_startup_config(void)
{
    return g_startup_config[0] ? g_startup_config : NULL;
}

static void bridge_setup(void);
int convai_bridge_start(void)
{
    if (!g_engine) {
        printf("[convai_bridge] ERROR: not initialized\n");
        return -1;
    }

    convai_opt_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.mode     = CONVAI_MODE_WS;
    opt.agent_id = bridge_get_default_agent_id();
    opt.params   = g_startup_config[0] ? g_startup_config : bridge_get_default_startup_config();

    printf("[convai_bridge] START: agent_id=%s, params=%s\n",
           opt.agent_id, opt.params);

    int ret = convai_start(g_engine, &opt);
    if (ret == CONVAI_OK) {
        bridge_setup();
    } else {
        printf("[convai_bridge] start FAILED: %s\n", convai_err_2_str(ret));
    }
    return ret;
}

/* Bridge-layer startup: audio recording + playback threads, state.
 * Does NOT call convai_start() — caller handles SDK initialization. */
static void bridge_setup(void)
{
    if (g_started) return;

    bridge_uplink_start();
    bridge_downlink_start();
    bridge_comfort_start();

    g_started = 1;
    g_status  = CONVAI_STATUS_IDLE;
    if (g_status_cb) g_status_cb(g_status);

    printf("[convai_bridge] bridge setup done (IDLE)\n");
}

/* Clean up bridge-layer resources: audio threads, hardware, state.
 * Does NOT call convai_stop() — SDK handles itself on disconnect/failure. */
static void bridge_cleanup(void)
{
    if (!g_started) return;

    bridge_uplink_stop();
    bridge_comfort_stop();
    bridge_downlink_stop();

    g_started = 0;
    g_status  = CONVAI_STATUS_IDLE;
    if (g_status_cb) g_status_cb(g_status);

    printf("[convai_bridge] bridge cleanup done\n");
}

int convai_bridge_stop(void)
{
    if (!g_engine) return -1;

    bridge_cleanup();

    printf("[convai_bridge] STOP\n");
    int ret = convai_stop(g_engine);
    if (ret != CONVAI_OK) {
        printf("[convai_bridge] stop FAILED: %s\n", convai_err_2_str(ret));
    }
    return ret;
}

int convai_bridge_restart(void)
{
    printf("[convai_bridge] RESTART\n");
    convai_bridge_stop();
    goldie_msleep(100);
    return convai_bridge_start();
}

convai_engine_t convai_bridge_get_engine(void)     { return g_engine; }
convai_status_e convai_bridge_get_status(void)     { return g_status; }
int convai_bridge_is_speaking(void)                { return (g_status == CONVAI_STATUS_ANSWERING); }
int convai_bridge_is_started(void)                 { return g_started; }

int convai_bridge_get_uplink_stats(unsigned int *frames_sent,
                                   unsigned int *frames_dropped)
{
    if (frames_sent == NULL || frames_dropped == NULL) return -1;
    return bridge_uplink_get_stats(frames_sent, frames_dropped);
}

int convai_bridge_get_downlink_stats(unsigned int *dropped_bytes)
{
    if (dropped_bytes == NULL) return -1;
    return bridge_downlink_get_stats(dropped_bytes);
}

int convai_bridge_send_audio(const uint8_t *data, size_t len,
                             const convai_audio_frame_info_t *info)
{
    return bridge_uplink_send(data, len, info);
}

/* ---- Audio mode / PTT (forwarded to uplink module) ---- */
int convai_bridge_set_audio_mode(convai_bridge_audio_mode_t mode)
{
    return bridge_uplink_set_audio_mode(mode);
}
convai_bridge_audio_mode_t convai_bridge_get_audio_mode(void)
{
    return bridge_uplink_get_audio_mode();
}
void convai_bridge_ptt_press(void)   { bridge_uplink_ptt_press(); }
void convai_bridge_ptt_release(void) { bridge_uplink_ptt_release(); }
int  convai_bridge_ptt_is_pressed(void) { return bridge_uplink_ptt_is_pressed(); }

void convai_bridge_on_status(convai_bridge_status_cb cb)   { g_status_cb  = cb; }
void convai_bridge_on_event(convai_bridge_event_cb cb)     { g_event_cb   = cb; }
void convai_bridge_on_message(convai_bridge_message_cb cb) { g_message_cb = cb; }
