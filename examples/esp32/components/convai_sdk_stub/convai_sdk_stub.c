/**
 * @file convai_sdk_stub.c
 * @brief AI Hardware Agent SDK 桩实现 (Mock)
 *
 * 在正式 SDK (libconvai_sdk.a) 到位之前, 提供所有 API 符号的 mock 实现,
 * 使项目可以独立编译烧录到 ESP32-S3 硬件。
 *
 * 换正式 SDK 时: 从 main/CMakeLists.txt 将 convai_sdk_stub 改为 convai_sdk。
 */

#include "convai_api.h"
#include "convai_platform.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>

#define TAG "sdk_stub"

/* ===================================================================
 *  平台注册
 * =================================================================== */

static const convai_platform_t *g_platform = NULL;

int convai_platform_init(const convai_platform_t *platform) {
    if (platform == NULL) return -1;
    if (platform->abi_version != CONVAI_ABI_VERSION) {
        printf("[%s] WARNING: ABI version mismatch (got 0x%04x, expected 0x%04x)\n",
               TAG, platform->abi_version, CONVAI_ABI_VERSION);
    }
    g_platform = platform;
    printf("[%s] Platform registered (ABI 0x%04x)\n", TAG, platform->abi_version);
    return 0;
}

int convai_platform_is_registered(void) {
    return g_platform != NULL ? 1 : 0;
}

/* ===================================================================
 *  引擎生命周期
 * =================================================================== */

static convai_event_handler_t g_handler;
static void            *g_user_data;

int convai_create(convai_engine_t *handle,
                  const char *config_json,
                  const convai_event_handler_t *handler,
                  void *user_data) {
    printf("[%s] ========================================\n", TAG);
    printf("[%s]   SDK STUB — Mock Mode\n", TAG);
    printf("[%s]   正式 SDK 就位后替换\n", TAG);
    printf("[%s] ========================================\n", TAG);
    printf("[%s] convai_create(config=%s)\n", TAG,
           config_json ? config_json : "(null)");

    if (handler == NULL) return CONVAI_ERR_INVALID_PARAM;
    g_handler   = *handler;
    g_user_data = user_data;

    *handle = (convai_engine_t)0xCAFE0001;
    return CONVAI_OK;
}

void convai_destroy(convai_engine_t handle) {
    printf("[%s] convai_destroy(handle=%p)\n", TAG, handle);
    memset(&g_handler, 0, sizeof(g_handler));
}

int convai_start(convai_engine_t handle, const convai_opt_t *opt) {
    printf("[%s] convai_start(handle=%p, agent_id=%s)\n", TAG,
           handle, (opt && opt->agent_id) ? opt->agent_id : "(null)");

    if (handle != (convai_engine_t)0xCAFE0001) return CONVAI_ERR_INVALID_PARAM;
    if (!g_platform) return CONVAI_ERR_NOT_INITIALIZED;

    /* 模拟状态变化 */
    if (g_handler.on_convai_conversation_status) {
        g_handler.on_convai_conversation_status(handle, CONVAI_STATUS_LISTENING, g_user_data);
    }

    /* 模拟发送欢迎消息 */
    if (g_handler.on_convai_message_data) {
        const char *welcome = "{\"type\":\"stub\",\"msg\":\"SDK stub running. Hardware OK.\"}";
        convai_message_info_t info = { .is_binary = false };
        g_handler.on_convai_message_data(handle, welcome, strlen(welcome), &info, g_user_data);
    }

    /* 模拟发送静音音频帧 (验证音频播放链路) */
    if (g_handler.on_convai_audio_data) {
        static const uint8_t silence[320] = {0};
        convai_audio_frame_info_t info = { .data_type = CONVAI_AUDIO_DATA_TYPE_G711A };
        g_handler.on_convai_audio_data(handle, silence, sizeof(silence), &info, g_user_data);
    }

    return CONVAI_OK;
}

int convai_stop(convai_engine_t handle) {
    printf("[%s] convai_stop(handle=%p)\n", TAG, handle);

    if (g_handler.on_convai_conversation_status) {
        g_handler.on_convai_conversation_status(handle, CONVAI_STATUS_IDLE, g_user_data);
    }
    return CONVAI_OK;
}

int convai_update(convai_engine_t handle, const char *session_update_json) {
    printf("[%s] convai_update(json=%s)\n", TAG,
           session_update_json ? session_update_json : "(null)");
    return CONVAI_OK;
}

/* ===================================================================
 *  数据输入
 * =================================================================== */

int convai_send_audio(convai_engine_t handle,
                      const void *data_ptr, size_t data_len,
                      const convai_audio_frame_info_t *info_ptr) {
    static int audio_count = 0;
    audio_count++;
    if (audio_count == 1) {
        ESP_LOGI(TAG, "convai_send_audio() — streaming active (%zu bytes/frame)", data_len);
    }
    /* Throttle to every 500 frames (~5s @ 20ms frames) to avoid spamming
     * the shared stdout mutex and starving ESP_LOGI output from other tasks
     * (which previously made the UI look "frozen" when logs went silent). */
    if (audio_count % 500 == 0) {
        ESP_LOGI(TAG, "convai_send_audio() — %d frames sent", audio_count);
    }
    return CONVAI_OK;
}

int convai_send_message(convai_engine_t handle,
                        const void *data_ptr, size_t data_len,
                        const convai_message_info_t *info_ptr) {
    printf("[%s] convai_send_message(len=%zu): %.*s\n", TAG,
           data_len, (int)data_len, (const char *)data_ptr);
    return CONVAI_OK;
}

/* ===================================================================
 *  工具函数
 * =================================================================== */

const char *convai_get_version(void) {
    return "STUB-1.0.0";
}

const char *convai_err_2_str(int err_code) {
    static char buf[32];
    static const char *names[] = {
        "OK", "UNKNOWN", "INVALID_PARAM", "OUT_OF_MEMORY",
        "NOT_INITIALIZED", "ALREADY_STARTED", "NOT_STARTED", "NETWORK",
        "TIMEOUT", "PROTOCOL", "MEDIA", "TLS", "PLATFORM",
        "NOT_SUPPORTED", "INVALID_STATE", "CONNECTION_LOST",
        "INIT_FAILED", "SESSION_NOT_READY", "CONFIG_INCOMPLETE", "INVALID_JSON"
    };
    int idx = -err_code;
    if (idx >= 0 && idx < (int)(sizeof(names)/sizeof(names[0]))) {
        snprintf(buf, sizeof(buf), "STUB_%s", names[idx]);
    } else {
        snprintf(buf, sizeof(buf), "STUB_ERR_%d", err_code);
    }
    return buf;
}
