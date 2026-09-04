/**
 * @file convai_func_dispatch.h
 * @brief Generic ConvAI function-call message dispatcher for ESP32.
 *
 * The dispatcher owns the wire protocol only: it parses completed function
 * calls, invokes a registered handler, and sends function_call_output items
 * through convai_bridge. Function-specific parameter parsing belongs in the
 * handlers.
 */
#ifndef CONVAI_FUNC_DISPATCH_H
#define CONVAI_FUNC_DISPATCH_H

#include <stdbool.h>
#include <stddef.h>

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handler contract:
 * - args_json is owned by the dispatcher and is valid only during the call.
 * - output_str initially points to the common default success response. Leave
 *   it unchanged for that response, or repoint it to output_buf/a literal for
 *   a custom error.
 * - true means the registered function was handled, including parameter
 *   validation failures.
 */
typedef bool (*convai_func_handler_t)(const char *call_id,
                                      struct cJSON *args_json,
                                      char *output_buf,
                                      size_t buf_size,
                                      const char **output_str);

/** Install the dispatcher as the bridge message callback. */
void func_dispatch_init(void);

/** Register a handler. Registering the same name again replaces its handler. */
int func_dispatch_register(const char *name, convai_func_handler_t handler);

/** Remove the bridge callback and clear all registered handlers. */
void func_dispatch_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_FUNC_DISPATCH_H */
