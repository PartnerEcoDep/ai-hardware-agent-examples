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
#include "convai/audio_mode.h"  /* FE2026072900158: turn_detection JSON generator */
#include "service_manager.h"
#include "goldie_osal.h"
#include "app_codec.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

    /* Initialize the app-layer codec before starting audio pipelines.
     * The codec ID is read from convai.cfg (same "codec" key used by
     * bridge_build_config_json) so the app codec matches what the SDK
     * negotiated with the server. Defaults to G711A(1).
     * This must be called here (not in convai_bridge_init) because
     * bridge_cleanup deinitializes the codec on disconnect, and the
     * codec must be re-initialized on every bridge start. */
    const char *codec_str = convai_config_file_get("codec");
    int codec_id = codec_str ? (int)strtol(codec_str, NULL, 10) : 0;  /* 默认 0=G.711A */
    int codec_ret = app_codec_init((app_codec_id_e)codec_id);
    if (codec_ret != APP_CODEC_OK) {
        printf("[convai_bridge] WARNING: app_codec_init(%d) failed: %d\n",
               codec_id, codec_ret);
    } else {
        printf("[convai_bridge] app codec initialized: %s (sr=%d)\n",
               app_codec_get_name(), app_codec_get_sample_rate());
    }

    bridge_uplink_start();
    bridge_downlink_start();
    bridge_comfort_start();

    g_started = 1;
    g_status  = CONVAI_STATUS_IDLE;
    if (g_status_cb) g_status_cb(g_status);

    printf("[convai_bridge] bridge setup done (IDLE)\n");

    /* FE2026072900158: 根据音频模式下发 turn_detection 配置到服务端。
     * PTT(push2talk) 发送 turn_detection: null；
     * TAP2TALK 发送 server_vad + idle_timeout_ms=5000 + interrupt_response=false；
     * AUTO 为旧模式（即全双工），不下发 turn_detection（向后兼容）。 */
    bridge_apply_turn_detection();
}

/* Clean up bridge-layer resources: audio threads, hardware, state.
 * Does NOT call convai_stop() — SDK handles itself on disconnect/failure. */
static void bridge_cleanup(void)
{
    if (!g_started) return;

    bridge_uplink_stop();
    bridge_comfort_stop();
    bridge_downlink_stop();

    /* Deinit app-layer codec after audio pipelines are stopped. */
    app_codec_deinit();

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

/* FE2026072900158: 根据当前音频模式生成 turn_detection JSON 并通过 convai_update
 * 下发到服务端。映射关系：
 *   PTT      → vad_enabled=0 → turn_detection: null（关闭VAD，客户端完全控制）
 *   TAP2TALK → vad_enabled=1, voice_interrupt=0, tap2talk_timeout_ms=5000
 *   AUTO     → vad_enabled=1, voice_interrupt=1 → server_vad + interrupt_response=true（全双工） */
void bridge_apply_turn_detection(void)
{
    if (!g_engine || !g_started) return;

    convai_bridge_audio_mode_t mode = bridge_uplink_get_audio_mode();

    convai_audio_mode_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    switch (mode) {
    case CONVAI_BRIDGE_AUDIO_PTT:
        cfg.vad_enabled         = 0;
        cfg.voice_interrupt     = 0;
        cfg.tap2talk_timeout_ms = 0;
        break;
    case CONVAI_BRIDGE_AUDIO_TAP2TALK:
        cfg.vad_enabled         = 1;
        cfg.voice_interrupt     = 0;
        cfg.tap2talk_timeout_ms = 5000;
        break;
    case CONVAI_BRIDGE_AUDIO_AUTO:
        cfg.vad_enabled         = 1;
        cfg.voice_interrupt     = 1;
        cfg.tap2talk_timeout_ms = 0;
        break;
    default:
        return;
    }

    char session_update[512];
    if (convai_audio_mode_generate_session_update(&cfg, session_update,
                                                  sizeof(session_update)) != 0) {
        printf("[convai_bridge] ERROR: generate turn_detection JSON failed\n");
        return;
    }

    int ret = convai_update(g_engine, session_update);
    if (ret != CONVAI_OK) {
        printf("[convai_bridge] ERROR: convai_update failed: %s\n", convai_err_2_str(ret));
        return;
    }

    const char *mode_name = "?";
    switch (mode) {
    case CONVAI_BRIDGE_AUDIO_PTT:      mode_name = "PTT"; break;
    case CONVAI_BRIDGE_AUDIO_TAP2TALK: mode_name = "TAP2TALK"; break;
    case CONVAI_BRIDGE_AUDIO_AUTO:     mode_name = "AUTO"; break;
    default: break;
    }
    printf("[convai_bridge] turn_detection applied for mode: %s\n", mode_name);
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
void convai_bridge_ptt_press(void)
{
    /* FE2026072900158: PTT打断 — AI说话时按住按钮先打断TTS播放。
     * 必须在此处（convai_bridge.c）处理，因为需要更新 g_status。
     * 若在 uplink 模块处理，g_status 仍为 ANSWERING，导致后续状态流转异常。 */
    if (g_status == CONVAI_STATUS_ANSWERING && g_engine) {
        int ret = convai_interrupt(g_engine);
        if (ret == CONVAI_OK) {
            printf("[convai_bridge] PTT: interrupted AI speech (ANSWERING → INTERRUPTED)\n");
            g_status = CONVAI_STATUS_INTERRUPTED;
            bridge_downlink_on_status(CONVAI_STATUS_INTERRUPTED);
        } else {
            printf("[convai_bridge] PTT: interrupt failed: %s\n", convai_err_2_str(ret));
        }
    }
    bridge_uplink_ptt_press();
}
void convai_bridge_ptt_release(void) { bridge_uplink_ptt_release(); }
int  convai_bridge_ptt_is_pressed(void) { return bridge_uplink_ptt_is_pressed(); }

/* FE2026072900158: TAP2TALK wrappers */
void convai_bridge_tap_start(void)
{
    /* TAP打断 — AI说话时第一次点击先打断TTS播放，再开始录音。
     * 必须在此处（convai_bridge.c）处理，因为需要更新 g_status。 */
    if (g_status == CONVAI_STATUS_ANSWERING && g_engine) {
        int ret = convai_interrupt(g_engine);
        if (ret == CONVAI_OK) {
            printf("[convai_bridge] TAP: interrupted AI speech (ANSWERING → INTERRUPTED)\n");
            g_status = CONVAI_STATUS_INTERRUPTED;
            bridge_downlink_on_status(CONVAI_STATUS_INTERRUPTED);
        } else {
            printf("[convai_bridge] TAP: interrupt failed: %s\n", convai_err_2_str(ret));
        }
    }
    bridge_uplink_tap_start();
}
void convai_bridge_tap_stop(void)   { bridge_uplink_tap_stop(); }
int  convai_bridge_tap_is_active(void) { return bridge_uplink_tap_is_active(); }

void convai_bridge_on_status(convai_bridge_status_cb cb)   { g_status_cb  = cb; }
void convai_bridge_on_event(convai_bridge_event_cb cb)     { g_event_cb   = cb; }
void convai_bridge_on_message(convai_bridge_message_cb cb) { g_message_cb = cb; }


