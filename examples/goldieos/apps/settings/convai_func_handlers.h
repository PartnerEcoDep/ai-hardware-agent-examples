/**
 * @file convai_func_handlers.h
 * @brief Function-call handler registry for AI-initiated actions.
 *
 * Registers handlers for function-calls received from the AI backend
 * (emotion, alarm, weather, etc.) and dispatches incoming messages
 * to the appropriate handler.
 *
 * Usage:
 *   - Call func_handlers_register() once at startup (goldie_app_run).
 *   - Incoming function-call messages are dispatched automatically via
 *     the bridge message callback.
 */
#ifndef CONVAI_FUNC_HANDLERS_H
#define CONVAI_FUNC_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register all function-call handlers with the bridge.
 * This sets up the message callback that dispatches to the handler registry.
 * Call once at app startup after convai_bridge_on_event/on_status.
 */
void func_handlers_register(void);

/**
 * Unregister the function-call message callback.
 * Call at app shutdown before unregistering other callbacks.
 */
void func_handlers_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_FUNC_HANDLERS_H */