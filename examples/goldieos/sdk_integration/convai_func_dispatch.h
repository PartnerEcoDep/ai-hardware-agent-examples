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
 *   - Returns true if the call was recognized and handled (a success or error
 *     reply is placed in *output_str).
 *   - Returns false only if the args were not recognized at all. The dispatcher
 *     sends *output_str either way; false just adds an "unhandled" diagnostic
 *     log. Success vs. failure is conveyed by the JSON "result" field in the
 *     reply, NOT by this return value — so a handled-but-failed call still
 *     returns true.
 */
#ifndef CONVAI_FUNC_DISPATCH_H
#define CONVAI_FUNC_DISPATCH_H

#include <stddef.h>
#include <stdbool.h>

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
 * @return true if the call was recognized and handled (success or error reply
 *         placed in *output_str); false only if the args were not recognized.
 */
typedef bool (*convai_func_handler_t)(const char *call_id,
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