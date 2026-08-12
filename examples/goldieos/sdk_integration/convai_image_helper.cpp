#include "convai_image_helper.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "cJSON.h"
#include "convai/convai_api.h"
#include "utils/convai_base64.h"

#ifndef PLATFORM_TYPE_WS63

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

#else

typedef struct { void* dummy; } osFileHandle;

static int os_fopen(osFileHandle* f, const char* path, const char* mode) {
    (void)f; (void)path; (void)mode;
    return -1;
}

static int os_fclose(osFileHandle* f) {
    (void)f;
    return 0;
}

static uint32_t os_fsize(osFileHandle* f) {
    (void)f;
    return 0;
}

static uint32_t os_fread(void* buf, uint32_t elsz, uint32_t count, osFileHandle* f) {
    (void)buf; (void)elsz; (void)count; (void)f;
    return 0;
}

static void* os_malloc(size_t size) {
    (void)size;
    return NULL;
}

static void os_free(void* ptr) {
    (void)ptr;
}

#endif

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
    size_t out = convai_base64_encode(buf, cap, data, len);
    buf[out] = '\0';
    *out_base64 = buf;
    *out_len = out;
    return 0;
}

static int build_image_json(const char* base64_data, size_t base64_len, const char* format, char** out_json, size_t* out_len) {
    if (!base64_data || !format || !out_json || !out_len) return -1;

    cJSON* root = cJSON_CreateObject();
    if (!root) return -1;

    static uint32_t s_img_event_counter = 0;
    char event_id[32];
    snprintf(event_id, sizeof(event_id), "img_%u", s_img_event_counter++);
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

static int send_image_sync(void* sdk_handle, uint8_t* image_data, size_t image_len, const char* format) {
    if (!sdk_handle || !image_data || !format) {
        return -1;
    }

    char* base64_data = NULL;
    size_t base64_len = 0;
    if (encode_image_to_base64(image_data, image_len, &base64_data, &base64_len) != 0) {
        return -1;
    }

    char* json_str = NULL;
    size_t json_len = 0;
    if (build_image_json(base64_data, base64_len, format, &json_str, &json_len) != 0) {
        os_free(base64_data);
        return -1;
    }

    os_free(base64_data);

    int ret = convai_send_message(sdk_handle, json_str, json_len, NULL);

    cJSON_free(json_str);
    return ret;
}

int convai_image_init(ConvaiImageContext* ctx, const char* file_path) {
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

void convai_image_cleanup(ConvaiImageContext* ctx) {
    if (!ctx) return;
    if (ctx->data) {
        os_free(ctx->data);
        ctx->data = NULL;
    }
    ctx->len = 0;
    ctx->format = NULL;
    ctx->sent = false;
}

int convai_image_send_async(void* sdk_handle, ConvaiImageContext* ctx) {
    if (!sdk_handle || !ctx || !ctx->data || !ctx->format) {
        return -1;
    }
    if (ctx->sent) {
        return 0;
    }

    int ret = send_image_sync(sdk_handle, ctx->data, ctx->len, ctx->format);
    if (ret == 0) {
        ctx->sent = true;
    }
    return ret;
}

bool convai_image_has_pending(const ConvaiImageContext* ctx) {
    return (ctx && ctx->data && !ctx->sent);
}

void convai_image_mark_sent(ConvaiImageContext* ctx) {
    if (ctx) ctx->sent = true;
}

void convai_image_reset_sent(ConvaiImageContext* ctx) {
    if (ctx) ctx->sent = false;
}
