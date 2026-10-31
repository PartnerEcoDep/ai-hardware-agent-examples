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

#ifndef CONVAI_API_H
#define CONVAI_API_H

#include "convai_types.h"
#include "convai_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file convai_api.h
 * @brief Public API of the ConvAI SDK.
 *
 * Typical lifecycle:
 *   1. convai_create()   – allocate and initialise an engine instance
 *   2. convai_start()    – connect to the service and begin the session
 *   3. convai_send_message() / convai_send_audio() – send input
 *   4. convai_stop()     – gracefully end the session
 *   5. convai_destroy()  – release all resources
 */

/* ---- Lifecycle ---- */

/**
 * @brief Create a new engine instance.
 *
 * @param handle      [out] Engine handle pointer (set on success).
 * @param config_json [in]  JSON configuration string.
 * @param handler     [in]  Event handler callbacks.
 * @param user_data   [in]  Opaque pointer forwarded to every callback.
 * @return CONVAI_OK on success, or a negative error code.
 */
int convai_create(convai_engine_t *handle,
                  const char *config_json,
                  const convai_event_handler_t *handler,
                  void *user_data);

/**
 * @brief Destroy an engine instance and release all resources.
 *
 * If the engine is still running it will be stopped first.
 *
 * @param engine [in] Engine to destroy (NULL-safe).
 */
void convai_destroy(convai_engine_t handle);

/* ---- Session control ---- */

/**
 * @brief Start the engine session (connect to server, begin processing).
 *
 * @param handle [in] Engine instance.
 * @param opt    [in] Per-start options (agent_id, params). May be NULL.
 * @return CONVAI_OK if the start sequence was initiated successfully.
 */
int convai_start(convai_engine_t handle, const convai_opt_t *opt);

/**
 * @brief Stop the engine session gracefully.
 *
 * @param engine [in] Engine instance.
 * @return CONVAI_OK on success.
 */
int convai_stop(convai_engine_t handle);

/**
 * @brief Dynamically update engine configuration at runtime.
 *
 * @param handle                [in] Engine instance.
 * @param session_update_json   [in] JSON configuration string.
 * @return CONVAI_OK on success, or a negative error code.
 */
int convai_update(convai_engine_t handle, const char *session_update_json);

/* ---- Data input ---- */

/**
 * @brief Send an audio frame to the agent.
 *
 * @param handle    [in] Engine instance.
 * @param data_ptr  [in] Audio data buffer (G.711 A-law encoded).
 * @param data_len  [in] Length of audio data in bytes.
 * @param info_ptr  [in] Audio format descriptor.
 * @return CONVAI_OK on success, or a negative error code.
 */
int convai_send_audio(convai_engine_t           handle,
                      const void               *data_ptr,
                      size_t                    data_len,
                      const convai_audio_frame_info_t *info_ptr);

/**
 * @brief Send a text message to the agent.
 *
 * @param handle    [in] Engine instance.
 * @param data_ptr  [in] Text data buffer (UTF-8).
 * @param data_len  [in] Length of text data in bytes.
 * @param info_ptr  [in] Message descriptor (may be NULL for default).
 * @return CONVAI_OK on success, or a negative error code.
 */
int convai_send_message(convai_engine_t           handle,
                        const void               *data_ptr,
                        size_t                    data_len,
                        const convai_message_info_t *info_ptr);

/**
 * @brief Interrupt TTS playback immediately (client-side interrupt).
 *
 * Sends output_audio_buffer.clear to the server, which stops the current
 * TTS audio stream and interrupts the ongoing response generation.
 *
 * Use case: user presses a "stop" button during TTS playback in any mode.
 *
 * @param handle [in] Engine instance.
 * @return CONVAI_OK on success, or a negative error code.
 */
int convai_interrupt(convai_engine_t handle);
/* ---- Utilities ---- */

/**
 * @brief Get the SDK version string.
 *
 * @return Statically-allocated version string, e.g. "0.1.0".
 */
const char *convai_get_version(void);

/**
 * @brief Convert an error code to a human-readable string.
 *
 * @param err_code [in] An error code from convai_error_e.
 * @return Statically-allocated string (do not free).
 */
const char *convai_err_2_str(int err_code);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_API_H */