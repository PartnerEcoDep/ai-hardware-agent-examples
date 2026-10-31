#include "ws_board.h"
#include "driver_manager.h"
#ifndef _WIN32
#include "driver_core.h"
#endif
#include "goldie_osal.h"
#include "ai_audio.h"
extern void gpio_keyboard_init(void);
#ifndef _WIN32
static int power_fd = -1;
static int update_power_hold(void) {
    aw9523b_ioctl_arg_t arg = {.io_index = 13, .value = 0};
    if (goldie_ioctl(power_fd, AW9523B_IOCTL_GET_VAL, &arg) < 0) return -1;
    arg.io_index = 14;
    arg.value = arg.value ? 0 : 1;
    return goldie_ioctl(power_fd, AW9523B_IOCTL_SET_VAL, &arg);
}
static int power_task(void *ctx) {
    (void)ctx;
    for (;;) {
        /* Only control the board's power latch. If USB keeps the CPU alive,
         * the application remains usable regardless of switch position. */
        update_power_hold();
        goldie_msleep(100);
    }
    return 0;
}
#endif
void ws_board_prepare(void) {
#ifndef _WIN32
    gpio_keyboard_init();
#endif
}
int ws_board_init(void) {
#ifndef _WIN32
    power_fd = goldie_open(AW9523B_DRV_NAME, O_RDWR);
    if (power_fd < 0) return -1;
    aw9523b_ioctl_arg_t arg = {.io_index = 13, .value = 0};
    if (goldie_ioctl(power_fd, AW9523B_IOCTL_SET_DIR, &arg) < 0) return -1;
    arg.io_index = 14; arg.value = 1;
    if (goldie_ioctl(power_fd, AW9523B_IOCTL_SET_DIR, &arg) < 0) return -1;
    if (update_power_hold() < 0) return -1;
    return goldie_thread_create(power_task, NULL, "ai_power", 4096) ? 0 : -1;
#else
    return 0;
#endif
}

ws_board_capture_t ws_board_capture_format(void) {
#ifdef _WIN32
    return (ws_board_capture_t){2,0,AI_AUDIO_NO_REFERENCE};
#else
    return (ws_board_capture_t){2,0,1};
#endif
}

const char *ws_board_name(void) { return "Hoperun WS63"; }
