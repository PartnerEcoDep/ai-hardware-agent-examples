#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct convai_image_context_s {
    uint8_t*     data;
    size_t       len;
    const char*  format;
    bool         sent;
} convai_image_context_t;

typedef struct convai_image_state_s {
    convai_image_context_t ctx;
    const char*            image_path;
    bool                   image_sent_this_turn;
    int64_t                listening_end_time;
} convai_image_state_t;

#ifndef PLATFORM_TYPE_WS63
extern const char* CONVAI_IMAGE_DEFAULT_PATH;
#endif

int convai_image_init(convai_image_context_t* ctx, const char* file_path);
void convai_image_cleanup(convai_image_context_t* ctx);

int convai_image_send(void* sdk_handle, convai_image_context_t* ctx);

bool convai_image_has_pending(const convai_image_context_t* ctx);
void convai_image_mark_sent(convai_image_context_t* ctx);
void convai_image_reset_sent(convai_image_context_t* ctx);

void convai_image_state_init(convai_image_state_t* state, const char* image_path);
void convai_image_state_cleanup(convai_image_state_t* state);

void convai_image_state_on_idle(convai_image_state_t* state);
int convai_image_state_on_listening(void* sdk_handle, convai_image_state_t* state);
void convai_image_state_on_thinking(convai_image_state_t* state);
void convai_image_state_on_answering(convai_image_state_t* state);

#ifdef __cplusplus
}
#endif