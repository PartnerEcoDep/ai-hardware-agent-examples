#include <assert.h>
#include <setjmp.h>
#include <string.h>
#define PLATFORM_TYPE_WS63 1
#include "../src/platforms/goldieos/components/media/media.c"

static uint32_t tick;
uint32_t sys_now(void) { return tick; }
static jmp_buf iteration;
static unsigned device_level, writes, completed;
static bool inject, write_fail;
static uint8_t samples[3200];
static bool scheduler_locked, create_fail;
static goldie_thread_handler created_fn;
static unsigned priorities_set;
void goldie_thread_lock(void) { assert(!scheduler_locked); scheduler_locked = true; }
void goldie_thread_unlock(void) { assert(scheduler_locked); scheduler_locked = false; }
int goldie_thread_set_priority(void *thread, unsigned priority) {
    assert(scheduler_locked && thread == &iteration);
    assert(priority == (created_fn == capture ? 22u : 21u));
    ++priorities_set;
    return 0;
}
void goldie_mutex_init(goldie_mutex *m) { m->held = 0; }
void goldie_mutex_lock(goldie_mutex *m) { assert(!m->held); m->held = 1; }
void goldie_mutex_unlock(goldie_mutex *m) { assert(m->held); m->held = 0; }
void goldie_msleep(int ms) { (void)ms; assert(!rx.held && !output.held); longjmp(iteration, 1); }
void *goldie_thread_create(goldie_thread_handler fn, void *arg, const char *name, unsigned int stack) {
    assert(scheduler_locked && !arg && name && stack == 8192);
    created_fn = fn;
    return create_fail ? NULL : &iteration;
}
void *wait_service(int index) { (void)index; return hw; }
ws_board_capture_t ws_board_capture_format(void) { return (ws_board_capture_t){2, 0, 1}; }
void ws_post(void *ctx, const ai_event_t *event) {
    (void)ctx;
    if (event->type == AI_TURN_FINISHED) ++completed;
    else assert(event->type == AI_FAILED && event->error == -6);
}
static void hw_start(void) { assert(output.held && !rx.held); }
static void hw_stop(void) { assert(output.held && !rx.held); }
static void hw_reset(void) { assert(output.held && !rx.held); device_level = 0; }
static unsigned hw_level(int *unused) { (void)unused; assert(!rx.held); return device_level; }
static int hw_write(const void *pcm, unsigned bytes) {
    assert(pcm && output.held && !rx.held);
    ++writes;
    tick += 25; /* Account for a slow BSP write without holding the queue lock. */
    /* SDK audio can enqueue even while a device write is in progress. */
    if (inject) { inject = false; assert(!push(NULL, samples, 240)); }
    if (write_fail) return -1;
    device_level += bytes; return (int)bytes;
}
static void run_iteration(void) { if (!setjmp(iteration)) playback(NULL); }
int main(void) {
    assert(media_thread_create(capture, "ai_capture", 22));
    assert(media_thread_create(playback, "ai_playback", 21));
    assert(!scheduler_locked && priorities_set == 2);
    create_fail = true;
    assert(!media_thread_create(capture, "ai_capture", 22));
    assert(!scheduler_locked && priorities_set == 2);
    create_fail = false;
    AudioService device = {.play_start = hw_start, .play_stop = hw_stop,
        .reset_playbuffer = hw_reset, .get_valid_length = hw_level, .audio_write = hw_write};
    hw = &device;
    goldie_mutex_init(&tx); goldie_mutex_init(&rx); goldie_mutex_init(&output);
    assert(!ai_playback_init(&queue, storage, sizeof(storage), 1600));
    memset(samples, 0xd5, sizeof(samples));
    assert(!push(NULL, samples, sizeof(samples)));
    inject = true; run_iteration();
    assert(writes == 4 && device_level == 4096 && queue.size == 1392);
    assert(diag.received == 3440 && diag.consumed == 2048);
    assert(diag.last_loop == 100 && diag.level == 4096);
    assert(push(NULL, samples, sizeof(storage)) == -1);
    assert(diag.received == 3440); /* Rejected data is not counted as accepted. */
    assert(diag.pushes == 2); /* A rejected packet is not an accepted handoff. */
    run_iteration(); assert(writes == 4); /* Respect device water mark. */
    reset(NULL); assert(!playing && !queue.size && !device_level);
    /* Reproduce queue=960, device=0 after playback has already started. */
    assert(!push(NULL, samples, 960));
    unsigned before = writes;
    run_iteration(); assert(writes == before && queue.size == 960);
    assert(!push(NULL, samples, 640));
    run_iteration(); assert(queue.size == 0 && device_level == 3200);
    device_level = 0;
    run_iteration(); /* An empty pull must preserve the started-answer state. */
    before = writes;
    assert(!push(NULL, samples, 960));
    run_iteration(); assert(writes == before + 2 && queue.size == 0 && device_level == 1920);
    reset(NULL);
    assert(!push(NULL, samples, 240)); finish(NULL);
    run_iteration(); assert(!queue.size && completed == 0 && device_level == 480);
    device_level = 0; run_iteration(); assert(completed == 1 && !playing);
    run_iteration(); assert(completed == 1); /* Completion is emitted only once. */
    assert(!push(NULL, samples, sizeof(samples))); write_fail = true;
    run_iteration(); assert(!queue.size && !playing && !device_level);
    reset(NULL);
    assert(!diag.push_seen && !diag.push_gap_max && !diag.pushes);
    tick = UINT32_MAX - 99;
    assert(!push(NULL, samples, 240));
    tick = 50;
    assert(!push(NULL, samples, 240));
    assert(diag.push_gap_max == 150); /* Wrap-safe callback gap. */
    tick += 69000;
    assert(!push(NULL, samples, 240));
    assert(diag.push_gap_max == 69000 && diag.pushes == 3);
    assert(queue.size == 720); /* Diagnostics do not drain or bypass buffering. */
    puts("GoldieOS refill, enqueue during write, reset, drain and write failure passed");
}
