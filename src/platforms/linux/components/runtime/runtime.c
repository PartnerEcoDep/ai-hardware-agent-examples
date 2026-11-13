#define _POSIX_C_SOURCE 200809L
#include "linux_runtime.h"
#include <errno.h>
#include <time.h>
uint32_t linux_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}
void linux_sleep_ms(unsigned ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) && errno == EINTR) {}
}
