/**
 * @file convai_func_handlers.c
 * @brief Settings-app business function-call handlers.
 *
 * Registers handlers for AI-initiated function calls specific to the settings
 * app: emotion display, alarm setting, weather query.  Uses the generic
 * dispatch framework (convai_func_dispatch) for message parsing and reply
 * delivery — this file only contains the per-function business logic.
 */
#include "convai_func_handlers.h"
#include "convai_func_dispatch.h"
#include "convai_talk_page.h"
#include "cJSON.h"
#include "alarm_service.h"
#include "service_manager.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ================================================================
 * Business handlers
 * ================================================================ */

/* handle_emotion: set the talk-page avatar emotion from a face_expression
 * string. Always returns true (handled) — set_face is always recognized and a
 * reply is sent either way. Missing face_expression yields an error reply.
 * Unsupported emotion values silently fall back to neutral with the default
 * success reply (device capability is not surfaced to the backend). */
static bool handle_emotion(const char *call_id, cJSON *args_json,
                           char *output_buf, size_t buf_size,
                           const char **output_str)
{
    (void)call_id;

    cJSON *emotion_item = cJSON_GetObjectItem(args_json, "face_expression");
    if (!emotion_item || !cJSON_IsString(emotion_item)) {
        *output_str = "{\"result\":\"error\",\"message\":\"missing face_expression\"}";
        return true;
    }

    const char *emotion = emotion_item->valuestring;
    printf("[AI Settings] EMOTION: %s\n", emotion);

    int new_emotion;
    if (strcmp(emotion, "neutral") == 0)      new_emotion = EMOTION_NEUTRAL;
    else if (strcmp(emotion, "happy") == 0)   new_emotion = EMOTION_HAPPY;
    else if (strcmp(emotion, "angry") == 0)   new_emotion = EMOTION_ANGRY;
    else if (strcmp(emotion, "sad") == 0)     new_emotion = EMOTION_SAD;
    else if (strcmp(emotion, "doubt") == 0)   new_emotion = EMOTION_DOUBT;
    else {
        /* 不支持的表情值: 静默兜底到 neutral, 回复默认成功(不暴露端侧能力) */
        printf("[AI Settings] EMOTION: unsupported '%s', falling back to neutral\n", emotion);
        new_emotion = EMOTION_NEUTRAL;
    }
    talk_page_set_emotion(new_emotion);
    return true;
}

/* handle_set_alarm: parse time/label/repeat and add an alarm via AlarmService.
 * Accepts two time formats: "time":"HH:MM" (standard) or "hour"/"minute"
 * numbers (legacy). */
static bool handle_set_alarm(const char *call_id, cJSON *args_json,
                            char *output_buf, size_t buf_size,
                            const char **output_str)
{
    (void)call_id;

    int hour   = 0;
    int minute = 0;
    bool time_parsed = false;

    /* ---- 格式 A: "time": "HH:MM" (AI 下发的标准格式) ---- */
    cJSON *time_item = cJSON_GetObjectItem(args_json, "time");
    if (time_item && cJSON_IsString(time_item)) {
        const char *time_str = time_item->valuestring;
        if (strlen(time_str) == 5 && time_str[2] == ':') {
            hour   = (time_str[0] - '0') * 10 + (time_str[1] - '0');
            minute = (time_str[3] - '0') * 10 + (time_str[4] - '0');
            time_parsed = true;
        }
    }

    /* ---- 格式 B: "hour" / "minute" 数字 (兼容旧格式) ---- */
    if (!time_parsed) {
        cJSON *hour_item   = cJSON_GetObjectItem(args_json, "hour");
        cJSON *minute_item = cJSON_GetObjectItem(args_json, "minute");
        if (hour_item && cJSON_IsNumber(hour_item) &&
            minute_item && cJSON_IsNumber(minute_item)) {
            hour   = hour_item->valueint;
            minute = minute_item->valueint;
            time_parsed = true;
        }
    }

    if (!time_parsed) {
        *output_str = "{\"result\":\"error\",\"message\":\"缺少time参数,格式:\\\"HH:MM\\\"\"}";
        printf("[AI Settings] SET_ALARM ERROR: missing time\n");
        return true;
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        *output_str = "{\"result\":\"error\",\"message\":\"时间范围无效\"}";
        printf("[AI Settings] SET_ALARM ERROR: invalid time %d:%d\n", hour, minute);
        return true;
    }

    AlarmService *alarm_svc = (AlarmService*)get_service(ALARM_SERVICE_INDEX);
    if (!alarm_svc) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟服务不可用\"}";
        printf("[AI Settings] SET_ALARM ERROR: service not available\n");
        return true;
    }

    AlarmInfo alarm;
    memset(&alarm, 0, sizeof(AlarmInfo));
    alarm.m_hour     = (char)hour;
    alarm.m_min      = (char)minute;
    alarm.enabled    = true;
    alarm.ring_index = 0;

    /* ---- 解析 repeat / weekdays ---- */
    cJSON *weekdays_item = cJSON_GetObjectItem(args_json, "weekdays");
    cJSON *repeat_item   = cJSON_GetObjectItem(args_json, "repeat");

    if (weekdays_item && cJSON_IsArray(weekdays_item)) {
        int sz = cJSON_GetArraySize(weekdays_item);
        for (int w = 0; w < 7 && w < sz; w++) {
            cJSON *d = cJSON_GetArrayItem(weekdays_item, w);
            alarm.weekdays[w] = (d && cJSON_IsTrue(d));
        }
    } else if (repeat_item && cJSON_IsString(repeat_item)) {
        const char *repeat = repeat_item->valuestring;
        if (strcmp(repeat, "none") == 0) {
            /* 一次性闹钟: 全部不选 */
        } else if (strcmp(repeat, "daily") == 0) {
            for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
        } else if (strcmp(repeat, "weekdays") == 0) {
            for (int w = 0; w < 5; w++) alarm.weekdays[w] = true;
        } else {
            for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
        }
    } else {
        for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
    }

    /* enabled（可选，默认 true） */
    cJSON *enabled_item = cJSON_GetObjectItem(args_json, "enabled");
    if (enabled_item && cJSON_IsBool(enabled_item))
        alarm.enabled = cJSON_IsTrue(enabled_item) ? true : false;

    printf("[AI Settings] SET_ALARM: %02d:%02d, enabled=%d\n",
           alarm.m_hour, alarm.m_min, alarm.enabled);

    int ret = alarm_svc->add_alarm(&alarm);
    if (ret >= 0) {
        snprintf(output_buf, buf_size,
                 "{\"result\":\"success\",\"message\":\"闹钟已设置\",\"index\":%d}", ret);
        *output_str = output_buf;
        printf("[AI Settings] SET_ALARM OK: index=%d\n", ret);
    } else if (ret == -2) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟已满,最多10个\"}";
        printf("[AI Settings] SET_ALARM ERROR: max alarms reached\n");
    } else {
        snprintf(output_buf, buf_size,
                 "{\"result\":\"error\",\"message\":\"添加失败,错误码:%d\"}", ret);
        *output_str = output_buf;
        printf("[AI Settings] SET_ALARM ERROR: ret=%d\n", ret);
    }

    return true;
}

/* handle_get_weather: device has no HTTP capability; just echo the location
 * and return a placeholder. Real weather data is delivered by the AI in the
 * conversation. */
static bool handle_get_weather(const char *call_id, cJSON *args_json,
                              char *output_buf, size_t buf_size,
                              const char **output_str)
{
    (void)call_id;

    cJSON *loc_item = cJSON_GetObjectItem(args_json, "location");
    const char *location = (loc_item && cJSON_IsString(loc_item))
                           ? loc_item->valuestring : NULL;

    if (!location) {
        *output_str = "{\"result\":\"error\",\"message\":\"缺少location参数\"}";
        printf("[AI Settings] GET_WEATHER ERROR: missing location\n");
        return true;
    }

    printf("[AI Settings] GET_WEATHER: location=%s\n", location);

    snprintf(output_buf, buf_size,
             "{\"result\":\"success\",\"message\":\"晴天\","
             "\"location\":\"%s\"}", location);
    *output_str = output_buf;

    return true;
}

/* ================================================================
 * Public API
 * ================================================================ */

void func_handlers_register(void)
{
    /* Install the generic dispatch message callback, then register this
     * app's business handlers. */
    func_dispatch_init();
    func_dispatch_register("set_face",     handle_emotion);
    func_dispatch_register("set_alarm",    handle_set_alarm);
    func_dispatch_register("get_weather",  handle_get_weather);
}

void func_handlers_unregister(void)
{
    func_dispatch_unregister();
}
