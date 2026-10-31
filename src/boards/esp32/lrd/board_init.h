#ifndef BOARD_INIT_H
#define BOARD_INIT_H
#include "driver/i2c_master.h"
#include "tpt29555a.h"
extern i2c_master_bus_handle_t g_i2c_bus;
extern tpt29555a_t g_tpt29555a;
esp_err_t board_init_all(void);
void board_pa_enable(int enabled, void *ctx);
void board_touch_reset(void);
#endif
