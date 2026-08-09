/**
 * @file convai_func_dispatch.h
 * @brief Generic function-call dispatch framework.
 *
 * Receives function-call messages from the AI backend (via the bridge message
 * callback), parses them, dispatches each call to a registered handler, and
 * sends the handler's reply back to the backend as a function_call_output.
 *
 * App-agnostic: knows nothing about which functions exist or what they do.
 * Apps register their own handlers via func_dispatch_register().
 *
 * Usage:
 *   func_dispatch_init();                          // install message callback
 *   func_dispatch_register("set_face", my_handler); // register business handlers
 *   ...
 *   func_dispatch_unregister();                    // at shutdown
 *
 * Handler contract:
 *   - Returns true if the call was handled (reply sent), false if unrecognized
 *     args (still sends a reply with the error).
 *   - On success, may set *output_str to a custom JSON reply (default success
 *     JSON is used if left unchanged).
 */
#ifndef CONVAI_FUNC_DISPATCH_H
#define CONVAI_FUNC_DISPATCH_H

#include <stddef.h>

/* Forward decl — avoids pulling cJSON.h into this header. */
struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function-call handler signature.
 *
 * @param call_id     function call id (for reply correlation)
 * @param args_json   parsed arguments JSON object (owned by dispatcher; do not free)
 * @param output_buf  scratch buffer for building a custom reply (256 bytes)
 * @param buf_size    size of output_buf
 * @param output_str  IN/OUT: defaults to a success JSON string; handler may
 *                    repoint it to output_buf (or a literal) to customize
 * @return true = handled (reply sent), false = args error (error reply sent)
 */
typedef int (*convai_func_handler_t)(const char *call_id,
                                     struct cJSON *args_json,
                                     char *output_buf, size_t buf_size,
                                     const char **output_str);

/**
 * Install the message callback on the bridge and prepare the dispatch table.
 * Call once at app startup, before registering handlers.
 */
void func_dispatch_init(void);

/**
 * Register a handler for a function-call name.
 * Multiple handlers for the same name: the last registered wins.
 * @param name     function name (must match the backend's "name" field)
 * @param handler  handler function
 * @return 0 on success, -1 if table full
 */
int func_dispatch_register(const char *name, convai_func_handler_t handler);

/**
 * Unregister the message callback from the bridge.
 * Call at app shutdown. The dispatch table is cleared.
 */
void func_dispatch_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_FUNC_DISPATCH_H */