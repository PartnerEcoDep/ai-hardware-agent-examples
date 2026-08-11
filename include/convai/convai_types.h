/*!
 * Copyright (C) 2026 Huawei Cloud Computing Technologies Co., Ltd.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CONVAI_TYPES_H
#define CONVAI_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file convai_types.h
 * @brief Core types used across the ConvAI SDK public API.
 *
 * Contains: error codes, status codes, mode codes, data structures.
 */

/* ---- SDK version ---- */

#define CONVAI_VERSION_MAJOR 0
#define CONVAI_VERSION_MINOR 1
#define CONVAI_VERSION_PATCH 0

/* ---- Error Codes ---- */

/**
 * @brief ConvAI SDK error codes.
 *
 * All SDK functions return an error code from this enumeration.
 * Zero (CONVAI_OK) indicates success.
 */
typedef enum {
    CONVAI_OK                    =  0,
    CONVAI_ERR_UNKNOWN           = -1,
    CONVAI_ERR_INVALID_PARAM     = -2,
    CONVAI_ERR_OUT_OF_MEMORY     = -3,
    CONVAI_ERR_NOT_INITIALIZED   = -4,
    CONVAI_ERR_ALREADY_STARTED   = -5,
    CONVAI_ERR_NOT_STARTED       = -6,
    CONVAI_ERR_NETWORK           = -7,
    CONVAI_ERR_TIMEOUT           = -8,
    CONVAI_ERR_PROTOCOL          = -9,
    CONVAI_ERR_MEDIA             = -10,
    CONVAI_ERR_TLS               = -11,
    CONVAI_ERR_PLATFORM          = -12,
    CONVAI_ERR_NOT_SUPPORTED     = -13,
    CONVAI_ERR_INVALID_STATE     = -14,
    CONVAI_ERR_CONNECTION_LOST   = -15,
    CONVAI_ERR_INIT_FAILED       = -16,
    CONVAI_ERR_SESSION_NOT_READY = -17,
    CONVAI_ERR_CONFIG_INCOMPLETE = -18,
    CONVAI_ERR_INVALID_JSON      = -19
} convai_error_e;

/* ---- Status Codes ---- */

/**
 * @brief ConvAI agent status enumeration.
 *
 * Represents the application-visible state of the conversational AI agent.
 */
typedef enum {
    CONVAI_STATUS_IDLE = 0,
    CONVAI_STATUS_LISTENING,
    CONVAI_STATUS_THINKING,
    CONVAI_STATUS_ANSWERING,
    CONVAI_STATUS_INTERRUPTED,
    CONVAI_STATUS_ANSWER_FINISHED
} convai_status_e;

/* ---- Mode Codes ---- */

/**
 * @brief Engine session mode.
 */
typedef enum {
    CONVAI_MODE_WS = 0,
} convai_mode_e;

/* ---- Opaque Handle ---- */

/**
 * @brief Opaque engine instance handle.
 *
 * Created via convai_create() and destroyed via convai_destroy().
 */
typedef void* convai_engine_t;

/* ---- Data Types ---- */

typedef enum {
    CONVAI_AUDIO_DATA_TYPE_G711A   = 0,
} convai_audio_data_type_e;

typedef enum {
    CONVAI_DATA_TYPE_AUDIO   = 0,
    CONVAI_DATA_TYPE_MESSAGE = 1,
} convai_data_type_e;

/* ---- Data Structures ---- */

typedef struct {
    convai_audio_data_type_e data_type;
    int commit;  /* 0=streaming (append), 1=commit buffer (trigger AI) */
} convai_audio_frame_info_t;

typedef struct {
    bool is_binary;
} convai_message_info_t;

typedef struct {
    convai_data_type_e type;
    union {
        convai_audio_frame_info_t audio;
        convai_message_info_t     message;
    } info;
} convai_data_info_t;

typedef struct {
    convai_mode_e mode;
    const char *agent_id;
    const char *params;
} convai_opt_t;

/**
 * @brief Authentication information for device registration.
 */
typedef struct {
    const char *product_id;
    const char *product_key;
    const char *product_secret;
    const char *device_name;
} convai_info_t;

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_TYPES_H */