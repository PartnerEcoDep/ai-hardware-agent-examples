#include "ai_cloud.h"
#include "convai_api.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>

static ai_cloud_config_t cfg;
static convai_engine_t engine;
static uint32_t session_id;
static atomic_bool accepting;
static atomic_bool connected;
static atomic_bool playback_allowed;
static atomic_bool answering;

static void post(ai_event_type_t type, int error) {
    ai_event_t event = {.type = type, .session = session_id, .error = error};
    cfg.post(cfg.event_ctx, &event);
}

static int send_audio(void *unused, const uint8_t *data, size_t size) {
    (void)unused;
    if (!atomic_load(&accepting) || !atomic_load(&connected)) return 1;
    convai_audio_frame_info_t info = {.data_type = CONVAI_AUDIO_DATA_TYPE_G711A};
    return convai_send_audio(engine, data, size, &info);
}

static void on_event(convai_engine_t handle, convai_event_t *event, void *ctx) {
    (void)handle; (void)ctx;
    if (!event || !atomic_load(&accepting)) return;
    switch (event->code) {
    case CONVAI_EV_CONNECTED:
        atomic_store(&connected, true);
        post(AI_CONNECTED, 0);
        break;
    case CONVAI_EV_DISCONNECTED:
    case CONVAI_EV_FAILED:
        atomic_store(&accepting, false);
        atomic_store(&connected, false);
        atomic_store(&playback_allowed, false);
        /* App owner performs the uplink barrier and SDK stop, never the SDK's
         * own callback thread (which may hold internal SDK locks). */
        cfg.media.reset(cfg.media.ctx);
        post(event->code == CONVAI_EV_FAILED ? AI_FAILED : AI_DISCONNECTED, -1);
        break;
    default: break;
    }
}

static void on_status(convai_engine_t handle, convai_status_e status, void *ctx) {
    (void)handle; (void)ctx;
    if (!atomic_load(&accepting) || !atomic_load(&connected)) return;
    switch (status) {
    case CONVAI_STATUS_LISTENING: post(AI_HEARING, 0); break;
    case CONVAI_STATUS_THINKING: post(AI_PROCESSING, 0); break;
    case CONVAI_STATUS_ANSWERING:
        if (!atomic_load(&answering)) cfg.media.reset(cfg.media.ctx);
        atomic_store(&answering, true);
        atomic_store(&playback_allowed, true);
        post(AI_ANSWERING, 0);
        break;
    case CONVAI_STATUS_INTERRUPTED:
        atomic_store(&answering, false);
        atomic_store(&playback_allowed, false);
        cfg.media.reset(cfg.media.ctx);
        post(AI_HEARING, 0);
        break;
    case CONVAI_STATUS_ANSWER_FINISHED:
        atomic_store(&answering, false);
        cfg.media.finish(cfg.media.ctx);
        /* UI transitions on actual playback drain, not the cloud's last packet. */
        break;
    default: break;
    }
}

static void on_audio(convai_engine_t handle, const void *data, size_t size,
                      const convai_audio_frame_info_t *info, void *ctx) {
    (void)handle; (void)ctx;
    if (!atomic_load(&accepting) || !atomic_load(&connected) ||
        !atomic_load(&playback_allowed) || !data || !size) return;
    bool unsupported = info && info->data_type != CONVAI_AUDIO_DATA_TYPE_G711A;
    int push_result = unsupported ? 0 : cfg.media.push(cfg.media.ctx, data, size);
    if (unsupported || push_result != 0) {
        printf("[ai_cloud] downlink rejected: reason=%s type=%d bytes=%lu push_rc=%d\n",
               unsupported ? "format" : "media_push", info ? (int)info->data_type : -1,
               (unsigned long)size, push_result);
        atomic_store(&playback_allowed, false);
        cfg.media.reset(cfg.media.ctx);
        post(AI_FAILED, -3); /* Explicit failure on overflow/unsupported format. */
    }
}

int ai_cloud_init(const ai_cloud_config_t *config) {
    if (!config || !config->config_json || !config->agent_id ||
        !config->agent_id[0] || !config->post || !config->media.enable ||
        !config->media.reset || !config->media.push || !config->media.finish || engine)
        return -1;
    cfg = *config;
    return 0;
}

int ai_cloud_start(void *unused, uint32_t session) {
    (void)unused;
    if (engine || !cfg.post) return -1;
    session_id = session;
    atomic_store(&connected, false);
    atomic_store(&playback_allowed, true);
    atomic_store(&answering, false);
    cfg.media.reset(cfg.media.ctx);
    static const convai_event_handler_t handler = {
        .on_convai_event = on_event,
        .on_convai_conversation_status = on_status,
        .on_convai_audio_data = on_audio,
    };
    int rc = convai_create(&engine, cfg.config_json, &handler, NULL);
    if (rc) { if (engine) convai_destroy(engine); engine = NULL; return rc; }
    atomic_store(&accepting, true);
    cfg.media.enable(cfg.media.ctx, true, send_audio, NULL);
    convai_opt_t opt = {.mode = CONVAI_MODE_WS, .agent_id = cfg.agent_id,
                        .params = cfg.startup_json};
    rc = convai_start(engine, &opt);
    if (rc) ai_cloud_stop(NULL);
    return rc;
}

int ai_cloud_stop(void *unused) {
    (void)unused;
    if (!cfg.post) return 0;
    atomic_store(&accepting, false);
    atomic_store(&connected, false);
    atomic_store(&playback_allowed, false);
    cfg.media.enable(cfg.media.ctx, false, NULL, NULL);
    cfg.media.reset(cfg.media.ctx);
    int rc = 0;
    if (engine) {
        rc = convai_stop(engine);
        convai_destroy(engine);
        engine = NULL;
    }
    /* A callback that was already executing is now joined. Close its media
     * side effects too before allowing another session to start. */
    cfg.media.enable(cfg.media.ctx, false, NULL, NULL);
    cfg.media.reset(cfg.media.ctx);
    return rc;
}
