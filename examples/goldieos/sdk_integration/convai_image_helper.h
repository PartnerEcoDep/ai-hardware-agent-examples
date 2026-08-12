#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ConvaiImageContext {
    uint8_t*     data;
    size_t       len;
    const char*  format;
    bool         sent;
} ConvaiImageContext;

int convai_image_init(ConvaiImageContext* ctx, const char* file_path);
void convai_image_cleanup(ConvaiImageContext* ctx);

int convai_image_send_async(void* sdk_handle, ConvaiImageContext* ctx);

bool convai_image_has_pending(const ConvaiImageContext* ctx);
void convai_image_mark_sent(ConvaiImageContext* ctx);
void convai_image_reset_sent(ConvaiImageContext* ctx);

#ifdef __cplusplus
}
#endif