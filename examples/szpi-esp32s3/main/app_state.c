/**
 * @file app_state.c
 * @brief Implementation of the global application state singleton.
 */

#include "app_state.h"

static app_state_t s_app_state = APP_STATE_BOOTING;

void app_state_set(app_state_t state) {
  s_app_state = state;
}

app_state_t app_state_get(void) {
  return s_app_state;
}
