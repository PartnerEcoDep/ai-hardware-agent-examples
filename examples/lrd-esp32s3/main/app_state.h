/**
 * @file app_state.h
 * @brief Global application lifecycle state singleton.
 *
 * A single process-wide state used to gate behaviour before/after the
 * hardware and services are fully brought up.
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** High-level application lifecycle states. */
typedef enum {
  APP_STATE_BOOTING = 0,  /**< Bringing up hardware + services. */
  APP_STATE_RUNNING,      /**< Main loop active, accepting input. */
  APP_STATE_ERROR,        /**< Unrecoverable init failure. */
} app_state_t;

/**
 * @brief Set the global application state.
 * @param state New state value.
 */
void app_state_set(app_state_t state);

/**
 * @brief Get the current global application state.
 * @return Current state.
 */
app_state_t app_state_get(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_H */
