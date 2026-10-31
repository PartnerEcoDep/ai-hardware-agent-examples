#include "ai_cloud.h"
#include "convai_api.h"
#include <assert.h>
#include <stdio.h>
static convai_event_handler_t handler;
static ai_event_t last;
static int creates, destroys, stops, posts, pushes, resets, finishes, create_result, start_result;
static bool active, sync_connect;
static int push_result;
static ai_uplink_fn send;
int convai_create(convai_engine_t *out, const char *json,
                  const convai_event_handler_t *cb, void *ctx) {
    (void)json; (void)ctx; ++creates; handler = *cb;
    if (create_result) return create_result;
    *out = (convai_engine_t)&handler; return 0;
}
void convai_destroy(convai_engine_t engine) { (void)engine; ++destroys; }
int convai_start(convai_engine_t engine, const convai_opt_t *opt) {
    assert(opt && opt->agent_id);
    if (sync_connect) {
        convai_event_t event = {.code = CONVAI_EV_CONNECTED};
        handler.on_convai_event(engine, &event, NULL);
    }
    return start_result;
}
int convai_stop(convai_engine_t engine) { (void)engine; ++stops; return 0; }
int convai_send_audio(convai_engine_t engine, const void *data, size_t size,
                      const convai_audio_frame_info_t *info) {
    assert(engine && data && size && info->data_type == CONVAI_AUDIO_DATA_TYPE_G711A); return 0;
}
static void post(void *ctx, const ai_event_t *event) { (void)ctx; last = *event; ++posts; }
static void enable(void *ctx, bool enabled, ai_uplink_fn fn, void *send_ctx) {
    (void)ctx; (void)send_ctx; active = enabled; send = fn;
}
static void reset(void *ctx) { (void)ctx; ++resets; }
static int push(void *ctx, const uint8_t *data, size_t size) {
    (void)ctx; assert(data && size); ++pushes; return push_result;
}
static void finish(void *ctx) { (void)ctx; ++finishes; }
static void status(convai_status_e s) { handler.on_convai_conversation_status(NULL, s, NULL); }
static void event(convai_event_code_e code) {
    convai_event_t e = {.code = code}; handler.on_convai_event(NULL, &e, NULL);
}
int main(void) {
    ai_cloud_config_t cfg = {.config_json = "{}", .agent_id = "test",
        .media = {.enable = enable, .reset = reset, .push = push, .finish = finish}, .post = post};
    assert(!ai_cloud_init(&cfg));
    create_result = -11;
    assert(ai_cloud_start(NULL, 1) == -11 && !active && !destroys);
    create_result = 0; start_result = -14; sync_connect = true;
    assert(ai_cloud_start(NULL, 2) == -14 && !active && destroys == 1);
    start_result = 0;
    assert(!ai_cloud_start(NULL, 3) && active && last.type == AI_CONNECTED && last.session == 3);
    assert(ai_cloud_start(NULL, 4) != 0);
    uint8_t audio[] = {0xd5};
    assert(!send(NULL, audio, sizeof(audio)));
    status(CONVAI_STATUS_ANSWERING);
    int before_resets = resets;
    status(CONVAI_STATUS_ANSWERING);
    assert(resets == before_resets); /* Duplicate status cannot erase queued audio. */
    handler.on_convai_audio_data(NULL, audio, sizeof(audio), NULL, NULL);
    assert(pushes == 1);
    status(CONVAI_STATUS_ANSWER_FINISHED);
    assert(finishes == 1 && last.type == AI_ANSWERING);
    status(CONVAI_STATUS_INTERRUPTED);
    handler.on_convai_audio_data(NULL, audio, sizeof(audio), NULL, NULL);
    assert(pushes == 1 && last.type == AI_HEARING);
    status(CONVAI_STATUS_ANSWERING);
    event(CONVAI_EV_DISCONNECTED);
    assert(last.type == AI_DISCONNECTED);
    int before = posts;
    event(CONVAI_EV_CONNECTED);
    assert(posts == before && send(NULL, audio, sizeof(audio)) != 0);
    assert(!ai_cloud_stop(NULL) && !active);
    before = posts;
    event(CONVAI_EV_CONNECTED); status(CONVAI_STATUS_ANSWERING);
    assert(posts == before);
    assert(!ai_cloud_stop(NULL) && destroys == 2 && stops == 2);
    assert(!ai_cloud_start(NULL, 5) && active);
    convai_audio_frame_info_t info = {.data_type = CONVAI_AUDIO_DATA_TYPE_OPUS};
    handler.on_convai_audio_data(NULL, audio, sizeof(audio), &info, NULL);
    assert(last.type == AI_FAILED && last.error == -3);
    ai_cloud_stop(NULL);
    assert(creates == 4 && destroys == 3 && resets > 0);
    assert(!ai_cloud_start(NULL, 6) && active);
    push_result = -1;
    info.data_type = CONVAI_AUDIO_DATA_TYPE_G711A;
    before = posts;
    handler.on_convai_audio_data(NULL, audio, sizeof(audio), &info, NULL);
    assert(posts == before + 1 && last.type == AI_FAILED && last.error == -3);
    int before_pushes = pushes;
    handler.on_convai_audio_data(NULL, audio, sizeof(audio), &info, NULL);
    assert(posts == before + 1 && pushes == before_pushes);
    assert(!ai_cloud_stop(NULL) && !active);
    puts("cloud failures, synchronous callback, drain, interrupt, disconnect, stop and format rejection passed");
}
