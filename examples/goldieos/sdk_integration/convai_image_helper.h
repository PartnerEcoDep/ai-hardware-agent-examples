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

typedef struct ConvaiImageState {
    ConvaiImageContext* ctx;
    const char*         image_path;
    bool                image_sent_this_turn;
    int64_t             listening_end_time;
} ConvaiImageState;

int convai_image_init(ConvaiImageContext* ctx, const char* file_path);
void convai_image_cleanup(ConvaiImageContext* ctx);

int convai_image_send(void* sdk_handle, ConvaiImageContext* ctx);

bool convai_image_has_pending(const ConvaiImageContext* ctx);
void convai_image_mark_sent(ConvaiImageContext* ctx);
void convai_image_reset_sent(ConvaiImageContext* ctx);

void convai_image_state_init(ConvaiImageState* state, const char* image_path);
void convai_image_state_cleanup(ConvaiImageState* state);

void convai_image_state_on_idle(ConvaiImageState* state);
int convai_image_state_on_listening(void* sdk_handle, ConvaiImageState* state);
void convai_image_state_on_thinking(ConvaiImageState* state);
void convai_image_state_on_answering(ConvaiImageState* state);

#ifdef __cplusplus
}
#endif