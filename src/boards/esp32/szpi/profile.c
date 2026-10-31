#include "board.h"
#include "board_init.h"
#include "board_pins.h"

const board_profile_t board_profile = {"SZPI ESP32-S3", 0, 1, true};
void board_pa_enable(int enabled, void *ctx) {
    (void)ctx;
    pca9557_pa_en(&g_pca9557, enabled);
}
void board_touch_reset(void) { /* FT6336 uses the board's power-on reset. */ }
