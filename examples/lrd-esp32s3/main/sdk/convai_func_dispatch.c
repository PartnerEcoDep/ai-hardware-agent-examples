/**
 * @file convai_func_dispatch.c
 * @brief ESP32 implementation of the generic ConvAI function-call pipeline.
 */
#include "convai_func_dispatch.h"

#include "convai_bridge.h"
#include "cJSON.h"
#include "esp_log.h"

#include <string.h>

#define FUNC_DISPATCH_MAX_HANDLERS 16
#define FUNC_DISPATCH_OUTPUT_BYTES 256

typedef struct {
  const char *name;
  convai_func_handler_t handler;
} func_dispatch_entry_t;

static const char *TAG = "func_dispatch";

static func_dispatch_entry_t s_registry[FUNC_DISPATCH_MAX_HANDLERS];
static int s_registry_count = 0;

static const char *const k_default_success =
    "{\"result\":\"success\",\"message\":\"成功\"}";

static const char *object_string(cJSON *object, const char *key) {
  cJSON *item = cJSON_GetObjectItem(object, key);
  return (item != NULL && cJSON_IsString(item)) ? item->valuestring : NULL;
}

static void log_function_call(const char *call_id, const char *name,
                              const char *arguments) {
  ESP_LOGI(TAG, "call_id=%s", call_id != NULL ? call_id : "(null)");
  ESP_LOGI(TAG, "name=%s", name != NULL ? name : "(null)");
  ESP_LOGI(TAG, "arguments=%s",
           arguments != NULL ? arguments : "(null)");
}

static convai_func_handler_t find_handler(const char *name) {
  for (int i = 0; i < s_registry_count; ++i) {
    if (strcmp(name, s_registry[i].name) == 0) {
      return s_registry[i].handler;
    }
  }
  return NULL;
}

static cJSON *create_output_item(const char *call_id, const char *output) {
  cJSON *item = cJSON_CreateObject();
  if (item == NULL) {
    ESP_LOGE(TAG, "cannot allocate function_call_output");
    return NULL;
  }

  if (cJSON_AddStringToObject(item, "type", "function_call_output") == NULL ||
      cJSON_AddStringToObject(item, "call_id",
                             call_id != NULL ? call_id : "") == NULL ||
      cJSON_AddStringToObject(item, "output", output) == NULL) {
    ESP_LOGE(TAG, "cannot populate function_call_output");
    cJSON_Delete(item);
    return NULL;
  }
  return item;
}

/* Parse and validate the event envelope. The returned root owns *calls_out. */
static cJSON *parse_function_call_event(const char *json_str,
                                        cJSON **calls_out) {
  *calls_out = NULL;

  cJSON *root = cJSON_Parse(json_str);
  if (root == NULL) {
    ESP_LOGW(TAG, "ignoring malformed message JSON");
    return NULL;
  }

  const char *type = object_string(root, "type");
  if (type == NULL ||
      strcmp(type, "response.function_call_arguments.done") != 0) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *calls = cJSON_GetObjectItem(root, "calls");
  if (calls == NULL || !cJSON_IsArray(calls)) {
    ESP_LOGW(TAG, "function-call event has no calls array");
    cJSON_Delete(root);
    return NULL;
  }

  *calls_out = calls;
  return root;
}

static cJSON *create_function_call_response(const char *call_id,
                                            const char *output) {
  cJSON *response = cJSON_CreateObject();
  if (response == NULL) {
    ESP_LOGE(TAG, "cannot allocate function-call response");
    return NULL;
  }

  cJSON *response_type =
      cJSON_AddStringToObject(response, "type", "conversation.item.create");
  if (response_type == NULL) {
    ESP_LOGE(TAG, "cannot populate function-call response");
    cJSON_Delete(response);
    return NULL;
  }

  cJSON *item = create_output_item(call_id, output);
  if (item == NULL) {
    cJSON_Delete(response);
    return NULL;
  }

  if (!cJSON_AddItemToObject(response, "item", item)) {
    ESP_LOGE(TAG, "cannot append function_call_output");
    cJSON_Delete(item);
    cJSON_Delete(response);
    return NULL;
  }
  return response;
}

static const char *dispatch_to_handler(const char *call_id, const char *name,
                                       cJSON *args_json, char *output_buf,
                                       size_t output_buf_size) {
  const char *output = k_default_success;

  /* Match GoldieOS: malformed/unknown calls retain the default success
   * output. Registered handlers still validate their own required fields. */
  if (name == NULL || args_json == NULL) {
    return output;
  }

  convai_func_handler_t handler = find_handler(name);
  if (handler == NULL) {
    ESP_LOGW(TAG, "unhandled function: %s", name);
    return output;
  }

  bool handled = handler(call_id, args_json, output_buf, output_buf_size,
                          &output);
  if (!handled) {
    ESP_LOGW(TAG, "handler did not handle function: %s", name);
  }
  return output != NULL ? output : k_default_success;
}

/* Parse and dispatch one call, then build its standalone response. args_json
 * stays alive until the response has copied the handler output string. */
static cJSON *create_call_response(cJSON *call) {
  const char *call_id = NULL;
  const char *name = NULL;
  const char *arguments = NULL;

  if (call != NULL && cJSON_IsObject(call)) {
    call_id = object_string(call, "call_id");
    name = object_string(call, "name");
    arguments = object_string(call, "arguments");
  }
  log_function_call(call_id, name, arguments);

  cJSON *args_json = arguments != NULL ? cJSON_Parse(arguments) : NULL;
  char output_buf[FUNC_DISPATCH_OUTPUT_BYTES] = {0};
  const char *output = dispatch_to_handler(
      call_id, name, args_json, output_buf, sizeof(output_buf));
  cJSON *response = create_function_call_response(call_id, output);

  if (args_json != NULL) {
    cJSON_Delete(args_json);
  }
  return response;
}

static void send_function_call_response(cJSON *response) {
  char *response_str = cJSON_PrintUnformatted(response);
  if (response_str == NULL) {
    ESP_LOGE(TAG, "cannot serialize function-call response");
    return;
  }

  convai_engine_t engine = convai_bridge_get_engine();
  if (engine == NULL) {
    ESP_LOGW(TAG, "cannot send function-call response: engine unavailable");
    cJSON_free(response_str);
    return;
  }

  ESP_LOGI(TAG, "sending function call result: %s", response_str);
  int ret =
      convai_send_message(engine, response_str, strlen(response_str), NULL);
  if (ret != CONVAI_OK) {
    ESP_LOGE(TAG, "send function-call response failed: %d", ret);
  }
  cJSON_free(response_str);
}

static void send_all_call_responses(cJSON *calls) {
  const int call_count = cJSON_GetArraySize(calls);
  ESP_LOGI(TAG, "received %d function call(s)", call_count);

  for (int i = 0; i < call_count; ++i) {
    cJSON *call = cJSON_GetArrayItem(calls, i);
    cJSON *response = create_call_response(call);
    if (response != NULL) {
      send_function_call_response(response);
      cJSON_Delete(response);
    }
  }
}

static void func_dispatch_message_cb(const char *json_str) {
  if (json_str == NULL) {
    return;
  }

  cJSON *calls = NULL;
  cJSON *root = parse_function_call_event(json_str, &calls);
  if (root == NULL) {
    return;
  }

  send_all_call_responses(calls);
  cJSON_Delete(root);
}

void func_dispatch_init(void) {
  convai_bridge_on_message(func_dispatch_message_cb);
}

int func_dispatch_register(const char *name, convai_func_handler_t handler) {
  if (name == NULL || handler == NULL) {
    return -1;
  }

  for (int i = 0; i < s_registry_count; ++i) {
    if (strcmp(name, s_registry[i].name) == 0) {
      s_registry[i].handler = handler;
      return 0;
    }
  }

  if (s_registry_count >= FUNC_DISPATCH_MAX_HANDLERS) {
    ESP_LOGE(TAG, "handler registry is full; cannot register %s", name);
    return -1;
  }

  s_registry[s_registry_count].name = name;
  s_registry[s_registry_count].handler = handler;
  ++s_registry_count;
  return 0;
}

void func_dispatch_unregister(void) {
  convai_bridge_on_message(NULL);
  s_registry_count = 0;
}
