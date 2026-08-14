/**
 * @file state_viz.h
 * @brief State-visualization factory for the chat UI.
 *
 * Each chat state (idle, listening, thinking, speaking, disconnected, voice
 * select) registers a @c state_viz_t. @c ai_chat_ui_set_state() looks the viz
 * up by state and delegates object creation / reveal / animation to it.
 */

#ifndef STATE_VIZ_H
#define STATE_VIZ_H

#include "ai_chat_ui.h"  /* chat_state_t */
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Operations a state visualization must provide. */
typedef struct state_viz_s {
  chat_state_t state;   /**< Chat state this viz represents. */
  const char *name;     /**< Logging name. */
  void (*create)(void); /**< Build LVGL objects (once, at init). */
  void (*show)(void);   /**< Reveal this viz's objects. */
  void (*start_anims)(void); /**< (Re)start animations. */
} state_viz_t;

/**
 * @brief Get the viz registered for a chat state.
 * @param state Chat state.
 * @return Viz pointer, or NULL if none registered.
 */
const state_viz_t *state_viz_factory_get(chat_state_t state);

/**
 * @brief Register a state viz (replaces any previous registration).
 * @param viz Viz to register (must not be NULL, state must be valid).
 * @return ESP_OK on success.
 */
esp_err_t state_viz_factory_register(const state_viz_t *viz);

#ifdef __cplusplus
}
#endif

#endif /* STATE_VIZ_H */
