/**
 * @file lvgl_port.h
 * @brief LVGL 9.x 移植层 — ST7789 320x240, PSRAM 双缓冲, FT6336 触摸
 */
#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include <stdbool.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL：显示驱动、Tick 定时器、默认主题
 * @return 0 成功, -1 失败
 */
int lvgl_port_init(void);

/**
 * @brief 初始化 FT6336 触摸屏并注册 LVGL indev
 * @param i2c_bus  已初始化的 I2C 总线句柄 (I2C_NUM_1)
 * @return 0 成功, -1 失败
 */
int lvgl_port_touch_init(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief 加锁/解锁 LVGL (线程安全访问 LVGL API)
 */
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_PORT_H */
