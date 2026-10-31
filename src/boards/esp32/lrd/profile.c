#include "board.h"
#include "board_init.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const board_profile_t board_profile = {"LRD ESP32-S3", 2, 1, false};
void board_pa_enable(int enabled, void *ctx) {
    (void)ctx;
    tpt29555a_set_direction(&g_tpt29555a, IOEX_AMP_CTRL_PORT, IOEX_AMP_CTRL_PIN, true);
    tpt29555a_set_output(&g_tpt29555a, IOEX_AMP_CTRL_PORT, IOEX_AMP_CTRL_PIN, enabled != 0);
}
void board_touch_reset(void) {
    tpt29555a_set_direction(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, true);
    tpt29555a_set_output(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, false);
    vTaskDelay(pdMS_TO_TICKS(20));
    tpt29555a_set_output(&g_tpt29555a, IOEX_TP_RST_PORT, IOEX_TP_RST_PIN, true);
    vTaskDelay(pdMS_TO_TICKS(200));
}
