#include "convai_image_helper.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "cJSON.h"
#include "convai/convai_api.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
static uint64_t get_time_ms(void) {
    return GetTickCount64();
}

static void get_time_string(char* buf, size_t buf_size) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
#else
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void get_time_string(char* buf, size_t buf_size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm* tm_info = localtime(&ts.tv_sec);
    int ms = ts.tv_nsec / 1000000;
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
    size_t len = strlen(buf);
    snprintf(buf + len, buf_size - len, ".%03d", ms);
}
#endif

#ifndef PLATFORM_TYPE_WS63

const char* CONVAI_IMAGE_DEFAULT_PATH = "D:/test.png";

typedef FILE* osFileHandle;

static int os_fopen(osFileHandle* f, const char* path, const char* mode) {
    *f = fopen(path, mode);
    return (*f != NULL) ? 0 : -1;
}

static int os_fclose(osFileHandle* f) {
    return fclose(*f);
}

static uint32_t os_fsize(osFileHandle* f) {
    long pos = ftell(*f);
    fseek(*f, 0, SEEK_END);
    long size = ftell(*f);
    fseek(*f, pos, SEEK_SET);
    return (uint32_t)size;
}

static uint32_t os_fread(void* buf, uint32_t elsz, uint32_t count, osFileHandle* f) {
    return fread(buf, elsz, count, *f);
}

static void* os_malloc(size_t size) {
    return malloc(size);
}

static void os_free(void* ptr) {
    free(ptr);
}

static size_t local_base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t src_len) {
    static const char ENCODE_TABLE[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = ((src_len + 2) / 3) * 4;
    if (dst_cap < out_len + 1) return 0;

    size_t i, j;
    for (i = 0, j = 0; i < src_len; i += 3) {
        uint32_t octet_a = (i + 0 < src_len) ? (uint32_t)src[i + 0] : 0;
        uint32_t octet_b = (i + 1 < src_len) ? (uint32_t)src[i + 1] : 0;
        uint32_t octet_c = (i + 2 < src_len) ? (uint32_t)src[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        dst[j++] = ENCODE_TABLE[(triple >> 18) & 0x3F];
        dst[j++] = ENCODE_TABLE[(triple >> 12) & 0x3F];
        dst[j++] = (i + 1 < src_len) ? ENCODE_TABLE[(triple >> 6) & 0x3F] : '=';
        dst[j++] = (i + 2 < src_len) ? ENCODE_TABLE[triple & 0x3F] : '=';
    }
    dst[j] = '\0';
    return j;
}

static const char* detect_image_format(const uint8_t* data, size_t len) {
    if (len < 4) return NULL;
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return "jpeg";
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) return "png";
    if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46) return "gif";
    if (data[0] == 0x42 && data[1] == 0x4D) return "bmp";
    return NULL;
}

static int read_image_file(const char* path, uint8_t** out_data, size_t* out_len) {
    if (!path || !out_data || !out_len) return -1;
    *out_data = NULL;
    *out_len = 0;

    osFileHandle f;
    if (os_fopen(&f, path, "rb") != 0) {
        printf("[Image] Failed to open file: %s\n", path);
        return -1;
    }

    uint32_t size = os_fsize(&f);
    if (size == 0 || size > 1024) {
        printf("[Image] Invalid file size: %u (max: 1024 bytes)\n", size);
        os_fclose(&f);
        return -1;
    }

    uint8_t* data = (uint8_t*)os_malloc(size);
    if (!data) {
        printf("[Image] Failed to allocate memory\n");
        os_fclose(&f);
        return -1;
    }

    uint32_t read_bytes = os_fread(data, 1, size, &f);
    os_fclose(&f);

    if (read_bytes != size) {
        printf("[Image] Failed to read file: %u != %u\n", read_bytes, size);
        os_free(data);
        return -1;
    }

    *out_data = data;
    *out_len = size;
    return 0;
}

static int encode_image_to_base64(const uint8_t* data, size_t len, char** out_base64, size_t* out_len) {
    if (!data || !len || !out_base64 || !out_len) return -1;
    size_t cap = ((len + 2) / 3) * 4 + 1;
    char* buf = (char*)os_malloc(cap);
    if (!buf) return -1;
    size_t out = local_base64_encode(buf, cap, data, len);
    if (out == 0) {
        os_free(buf);
        return -1;
    }
    buf[out] = '\0';
    *out_base64 = buf;
    *out_len = out;
    return 0;
}

static int build_image_json(const char* base64_data, const char* format, char** out_json, size_t* out_len) {
    if (!base64_data || !format || !out_json || !out_len) return -1;

    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;

    static uint64_t s_img_event_counter = 0;
    char event_id[32];
    snprintf(event_id, sizeof(event_id), "img_%llu", (unsigned long long)s_img_event_counter++);
    cJSON_AddStringToObject(root, "event_id", event_id);
    cJSON_AddStringToObject(root, "type", "image");

    cJSON* image_obj = cJSON_CreateObject();
    if (!image_obj) {
        cJSON_Delete(root);
        return -1;
    }
    cJSON_AddStringToObject(image_obj, "data", base64_data);
    cJSON_AddStringToObject(image_obj, "format", format);
    cJSON_AddItemToObject(root, "image", image_obj);

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str) return -1;

    *out_json = json_str;
    *out_len = strlen(json_str);
    return 0;
}

static int send_image_core(void* sdk_handle, uint8_t* image_data, size_t image_len, const char* format) {
    if (!sdk_handle || !image_data || !format) {
        return -1;
    }

    char* base64_data = NULL;
    size_t base64_len = 0;
    (void)base64_len;
    if (encode_image_to_base64(image_data, image_len, &base64_data, &base64_len) != 0) {
        return -1;
    }

    char* json_str = NULL;
    size_t json_len = 0;
    if (build_image_json(base64_data, format, &json_str, &json_len) != 0) {
        os_free(base64_data);
        return -1;
    }

    os_free(base64_data);

    int ret = convai_send_message(sdk_handle, json_str, json_len, NULL);

    cJSON_free(json_str);
    return ret;
}

int convai_image_init(convai_image_context_t* ctx, const char* file_path) {
    if (!ctx || !file_path) return -1;

    memset(ctx, 0, sizeof(*ctx));

    if (read_image_file(file_path, &ctx->data, &ctx->len) != 0) {
        return -1;
    }

    ctx->format = detect_image_format(ctx->data, ctx->len);
    if (!ctx->format) {
        os_free(ctx->data);
        ctx->data = NULL;
        ctx->len = 0;
        return -1;
    }

    return 0;
}

void convai_image_cleanup(convai_image_context_t* ctx) {
    if (!ctx) return;
    if (ctx->data) {
        os_free(ctx->data);
        ctx->data = NULL;
    }
    ctx->len = 0;
    ctx->format = NULL;
    ctx->sent = false;
}

int convai_image_send(void* sdk_handle, convai_image_context_t* ctx) {
    if (!sdk_handle || !ctx || !ctx->data || !ctx->format) {
        return -1;
    }
    if (ctx->sent) {
        return 0;
    }

    int ret = send_image_core(sdk_handle, ctx->data, ctx->len, ctx->format);
    if (ret == 0) {
        ctx->sent = true;
    }
    return ret;
}

bool convai_image_has_pending(const convai_image_context_t* ctx) {
    return (ctx && ctx->data && !ctx->sent);
}

void convai_image_mark_sent(convai_image_context_t* ctx) {
    if (ctx) ctx->sent = true;
}

void convai_image_reset_sent(convai_image_context_t* ctx) {
    if (ctx) ctx->sent = false;
}

void convai_image_state_init(convai_image_state_t* state, const char* image_path) {
    if (!state || !image_path) return;
    memset(state, 0, sizeof(*state));
    state->image_path = image_path;
    convai_image_init(&state->ctx, image_path);
}

void convai_image_state_cleanup(convai_image_state_t* state) {
    if (!state) return;
    convai_image_cleanup(&state->ctx);
    state->image_path = NULL;
    state->image_sent_this_turn = false;
    state->listening_end_time = 0;
}

void convai_image_state_on_idle(convai_image_state_t* state) {
    if (!state) return;
    state->image_sent_this_turn = false;
    convai_image_cleanup(&state->ctx);
    convai_image_init(&state->ctx, state->image_path);
}

int convai_image_state_on_listening(void* sdk_handle, convai_image_state_t* state) {
    if (!sdk_handle || !state) return -1;
    if (!state->image_sent_this_turn && convai_image_has_pending(&state->ctx)) {
        state->image_sent_this_turn = true;
        return convai_image_send(sdk_handle, &state->ctx);
    }
    return 0;
}

void convai_image_state_on_thinking(convai_image_state_t* state) {
    if (!state) return;
    state->listening_end_time = get_time_ms();
    char time_str[64];
    get_time_string(time_str, sizeof(time_str));
    printf("[Image] LISTENING ended at: %s\n", time_str);
}

void convai_image_state_on_answering(convai_image_state_t* state) {
    if (!state) return;
    if (state->listening_end_time > 0) {
        uint64_t answer_start_time = get_time_ms();
        char time_str[64];
        get_time_string(time_str, sizeof(time_str));
        uint64_t delta = answer_start_time - state->listening_end_time;
        printf("[Image] ANSWERING started at: %s\n", time_str);
        printf("[Image] Processing time (LISTENING->ANSWERING): %llu ms\n", (unsigned long long)delta);
        state->listening_end_time = 0;
    }
}

#else

const char* CONVAI_IMAGE_DEFAULT_PATH = NULL;

int convai_image_init(convai_image_context_t* ctx, const char* file_path) {
    (void)ctx; (void)file_path;
    return -1;
}

void convai_image_cleanup(convai_image_context_t* ctx) {
    if (!ctx) return;
    ctx->data = NULL;
    ctx->len = 0;
    ctx->format = NULL;
    ctx->sent = false;
}

int convai_image_send(void* sdk_handle, convai_image_context_t* ctx) {
    (void)sdk_handle; (void)ctx;
    return -1;
}

void convai_image_state_init(convai_image_state_t* state, const char* image_path) {
    (void)state; (void)image_path;
}

void convai_image_state_cleanup(convai_image_state_t* state) {
    (void)state;
}

void convai_image_state_on_idle(convai_image_state_t* state) {
    (void)state;
}

int convai_image_state_on_listening(void* sdk_handle, convai_image_state_t* state) {
    (void)sdk_handle; (void)state;
    return -1;
}

void convai_image_state_on_thinking(convai_image_state_t* state) {
    (void)state;
}

void convai_image_state_on_answering(convai_image_state_t* state) {
    (void)state;
}

#endif
