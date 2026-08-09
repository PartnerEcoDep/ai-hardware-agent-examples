/**
 * @file convai_func_dispatch.c
 * @brief Generic function-call dispatch framework implementation.
 *
 * App-agnostic: parses function-call messages, dispatches to registered
 * handlers, and sends replies.  No business logic lives here.
 */
#include "convai_func_dispatch.h"
#include "convai_bridge.h"
#include "convai/convai_api.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>

/* Max handlers an app may register.  Small fixed table — function calls are
 * a bounded set defined by the backend's tool list. */
#define FUNC_DISPATCH_MAX  16

typedef struct {
    const char            *name;
    convai_func_handler_t  handler;
} func_dispatch_entry_t;

static func_dispatch_entry_t s_registry[FUNC_DISPATCH_MAX];
static int s_count = 0;

/* Default success reply when a handler leaves *output_str unchanged. */
static const char *const kDefaultSuccess =
    "{\"result\":\"success\",\"message\":\"成功\"}";

/* ---- Message callback: parse + dispatch + reply ---- */
static void func_dispatch_message_cb(const char *json_str)
{
    if (!json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return;

    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        cJSON_Delete(root);
        return;
    }

    /* Only handle function-call argument completion events. */
    if (strcmp(type_item->valuestring,
               "response.function_call_arguments.done") != 0) {
        cJSON_Delete(root);
        return;
    }

    cJSON *calls = cJSON_GetObjectItem(root, "calls");
    if (!calls || !cJSON_IsArray(calls)) {
        printf("[FuncDispatch] WARNING: missing 'calls' array\n");
        cJSON_Delete(root);
        return;
    }

    int call_count = cJSON_GetArraySize(calls);
    printf("\n");
    printf("========================================\n");
    printf("FunctionCall Received (%d calls)\n", call_count);
    printf("========================================\n");

    /* Build the reply: a conversation.items.create with one
     * function_call_output per call. */
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", "conversation.items.create");
    cJSON *items = cJSON_AddArrayToObject(response, "items");

    for (int i = 0; i < call_count; i++) {
        cJSON *call = cJSON_GetArrayItem(calls, i);
        if (!call) continue;

        const char *call_id   = cJSON_GetStringValue(cJSON_GetObjectItem(call, "call_id"));
        const char *name      = cJSON_GetStringValue(cJSON_GetObjectItem(call, "name"));
        const char *arguments = cJSON_GetStringValue(cJSON_GetObjectItem(call, "arguments"));

        printf("\n");
        printf("call_id=%s\n",  call_id   ? call_id   : "(null)");
        printf("name=%s\n",     name      ? name      : "(null)");
        printf("arguments=%s\n", arguments ? arguments : "(null)");

        char output_buf[256];
        const char *output_str = kDefaultSuccess;

        cJSON *args_json = arguments ? cJSON_Parse(arguments) : NULL;

        if (name && args_json) {
            bool handled = false;
            for (int h = 0; h < s_count; h++) {
                if (strcmp(name, s_registry[h].name) == 0) {
                    handled = s_registry[h].handler(
                        call_id, args_json, output_buf,
                        sizeof(output_buf), &output_str);
                    break;
                }
            }
            if (!handled) {
                printf("[FuncDispatch] Unhandled function: %s\n", name);
            }
        }

        if (args_json) cJSON_Delete(args_json);

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "type", "function_call_output");
        cJSON_AddStringToObject(item, "call_id", call_id ? call_id : "");
        cJSON_AddStringToObject(item, "output", output_str);
        cJSON_AddItemToArray(items, item);
    }

    printf("\n");
    printf("========================================\n");

    char *response_str = cJSON_PrintUnformatted(response);
    if (response_str) {
        convai_engine_t engine = convai_bridge_get_engine();
        if (engine) {
            printf("[FuncDispatch] Sending function call result: %s\n", response_str);
            convai_send_message(engine, response_str, strlen(response_str), NULL);
        }
        cJSON_free(response_str);
    }
    cJSON_Delete(response);
    cJSON_Delete(root);
}

/* ---- Public API ---- */

void func_dispatch_init(void)
{
    convai_bridge_on_message(func_dispatch_message_cb);
}

int func_dispatch_register(const char *name, convai_func_handler_t handler)
{
    if (!name || !handler) return -1;

    /* If the name is already registered, replace the handler (last wins). */
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_registry[i].name, name) == 0) {
            s_registry[i].handler = handler;
            return 0;
        }
    }
    if (s_count >= FUNC_DISPATCH_MAX) {
        printf("[FuncDispatch] registry full (%d), cannot register %s\n",
               FUNC_DISPATCH_MAX, name);
        return -1;
    }
    s_registry[s_count].name    = name;
    s_registry[s_count].handler = handler;
    s_count++;
    return 0;
}

void func_dispatch_unregister(void)
{
    convai_bridge_on_message(NULL);
    /* Clear the table so a re-init starts clean (matters if the app is
     * re-entered without a full process restart). */
    s_count = 0;
}