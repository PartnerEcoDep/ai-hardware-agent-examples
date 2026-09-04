/**
 * @file convai_func_handlers.c
 * @brief Lightweight ESP32 handlers for the GoldieOS function-call set.
 *
 * These handlers intentionally perform no device business operations. They
 * only parse required parameters. A valid call leaves output_str unchanged so
 * the dispatcher sends the common default success response.
 */
#include "convai_func_handlers.h"

#include "convai_func_dispatch.h"
#include "cJSON.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "func_handlers";

static bool is_decimal_digit(char value) {
  return value >= '0' && value <= '9';
}

static const char *get_required_string(cJSON *args_json, const char *name) {
  cJSON *item = cJSON_GetObjectItem(args_json, name);
  return (item != NULL && cJSON_IsString(item)) ? item->valuestring : NULL;
}

static bool parse_alarm_time(cJSON *args_json, int *hour, int *minute) {
  cJSON *time_item = cJSON_GetObjectItem(args_json, "time");
  if (time_item != NULL && cJSON_IsString(time_item) &&
      time_item->valuestring != NULL) {
    const char *time = time_item->valuestring;
    if (strlen(time) == 5 && time[2] == ':' &&
        is_decimal_digit(time[0]) && is_decimal_digit(time[1]) &&
        is_decimal_digit(time[3]) && is_decimal_digit(time[4])) {
      *hour = (time[0] - '0') * 10 + (time[1] - '0');
      *minute = (time[3] - '0') * 10 + (time[4] - '0');
      return true;
    }
  }

  /* GoldieOS also accepts the legacy required pair hour + minute. */
  cJSON *hour_item = cJSON_GetObjectItem(args_json, "hour");
  cJSON *minute_item = cJSON_GetObjectItem(args_json, "minute");
  if (hour_item != NULL && cJSON_IsNumber(hour_item) &&
      minute_item != NULL && cJSON_IsNumber(minute_item)) {
    *hour = hour_item->valueint;
    *minute = minute_item->valueint;
    return true;
  }

  return false;
}

static bool handle_set_face(const char *call_id, cJSON *args_json,
                            char *output_buf, size_t buf_size,
                            const char **output_str) {
  (void)call_id;
  (void)output_buf;
  (void)buf_size;

  const char *face_expression =
      get_required_string(args_json, "face_expression");
  if (face_expression == NULL) {
    *output_str =
        "{\"result\":\"error\",\"message\":\"missing face_expression\"}";
    ESP_LOGW(TAG, "set_face: missing face_expression");
    return true;
  }

  ESP_LOGI(TAG, "set_face parsed: face_expression=%s", face_expression);
  return true;
}

static bool handle_set_alarm(const char *call_id, cJSON *args_json,
                             char *output_buf, size_t buf_size,
                             const char **output_str) {
  (void)call_id;
  (void)output_buf;
  (void)buf_size;

  int hour = 0;
  int minute = 0;
  if (!parse_alarm_time(args_json, &hour, &minute)) {
    *output_str =
        "{\"result\":\"error\",\"message\":\"缺少time参数,格式:\\\"HH:MM\\\"\"}";
    ESP_LOGW(TAG, "set_alarm: missing or invalid time");
    return true;
  }

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    *output_str = "{\"result\":\"error\",\"message\":\"时间范围无效\"}";
    ESP_LOGW(TAG, "set_alarm: time out of range: %d:%d", hour, minute);
    return true;
  }

  ESP_LOGI(TAG, "set_alarm parsed: %02d:%02d", hour, minute);
  return true;
}

static bool handle_get_weather(const char *call_id, cJSON *args_json,
                               char *output_buf, size_t buf_size,
                               const char **output_str) {
  (void)call_id;

  const char *location = get_required_string(args_json, "location");
  if (location == NULL) {
    *output_str = "{\"result\":\"error\",\"message\":\"缺少location参数\"}";
    ESP_LOGW(TAG, "get_weather: missing location");
    return true;
  }

  ESP_LOGI(TAG, "get_weather: location=%s", location);

  /* 回显 location 并返回占位"晴天"; 真实天气由 AI 在对话中给出.
   * location 直接拼入 JSON, 城市名通常无转义字符; 若放开为任意输入需做转义. */
  snprintf(output_buf, buf_size,
           "{\"result\":\"success\",\"message\":\"晴天\","
           "\"location\":\"%s\"}",
           location);
  *output_str = output_buf;
  return true;
}

void func_handlers_register(void) {
  func_dispatch_init();
  func_dispatch_register("set_face", handle_set_face);
  func_dispatch_register("set_alarm", handle_set_alarm);
  func_dispatch_register("get_weather", handle_get_weather);
}

void func_handlers_unregister(void) {
  func_dispatch_unregister();
}
