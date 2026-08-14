/**
 * @file board_init.h
 * @brief Board-level hardware bring-up: I2C bus, PCA9557 IO expander, LED.
 */

#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#include "driver/i2c_master.h"
#include "pca9557.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared hardware singletons (defined in board_init.c). */
extern i2c_master_bus_handle_t g_i2c_bus;  /**< I2C master bus handle. */
extern pca9557_t g_pca9557;               /**< PCA9557 IO expander handle. */

/**
 * @brief Initialize I2C bus and PCA9557 IO expander.
 * @return ESP_OK on success, ESP_FAIL if either step fails.
 */
esp_err_t board_init_all(void);

/** Initialize the status LED GPIO (GPIO48). */
void board_led_init(void);

/** Set the status LED on/off. */
void board_led_set(int on);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_INIT_H */
