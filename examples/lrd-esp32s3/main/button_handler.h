/**
 * @file button_handler.h
 * @brief BOOT-button polling state machine.
 *
 * The button is polled from the main loop (not interrupt-driven) to avoid
 * priority-inversion issues with the LVGL lock. Short press toggles the
 * conversation; long press is reserved for future use.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the BOOT-button GPIO for polling. */
void button_handler_init(void);

/**
 * @brief Poll the button once and apply short/long-press transitions.
 *        Call from the main loop. Performs chat-UI state transitions.
 */
void button_handler_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_HANDLER_H */
