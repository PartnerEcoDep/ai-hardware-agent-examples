/**
 * @file app_codec_internal.h
 * @brief Internal prototypes for per-codec implementation modules.
 *
 * Each codec lives in its own .c file so customers can read, audit, and
 * port the implementation independently. app_codec.c dispatches to these
 * based on the active codec ID — it contains no encode/decode logic itself.
 *
 * All codecs share the same encode/decode signature shape:
 *   encode: PCM16 mono samples  -> byte buffer
 *   decode: byte buffer         -> PCM16 mono samples
 *
 * Stateful codecs (IMA-ADPCM, Opus) expose init/deinit; stateless ones
 * (PCM16, G.711A, G.711U) do not.
 */
#ifndef APP_CODEC_INTERNAL_H
#define APP_CODEC_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- G.711 A-law (wrapper over convai_codec_g711a.c) ---- */

int app_codec_g711a_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len);
int app_codec_g711a_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples);

/* ---- PCM16 (no compression) ---- */

int app_codec_pcm16_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len);
int app_codec_pcm16_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples);

/* ---- G.711 mu-law ---- */

int app_codec_g711u_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len);
int app_codec_g711u_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples);

/* ---- IMA-ADPCM (stateful: predictor + step_index persist across calls) ---- */

/** Reset ADPCM state. Call once after app_codec_init(APP_CODEC_IMA_ADPCM). */
void app_codec_adpcm_init(void);

int app_codec_adpcm_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len);
int app_codec_adpcm_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples);

/* ---- Opus (stub; real implementation gated by CONFIG_APP_ENABLE_OPUS) ---- */

/**
 * Create Opus encoder + decoder instances.
 * Returns 0 on success, or APP_CODEC_ERR_NOT_SUPPORTED if Opus is disabled.
 */
int  app_codec_opus_init(void);

/** Destroy Opus encoder + decoder instances. Safe to call if never initialised. */
void app_codec_opus_deinit(void);

int app_codec_opus_encode(const int16_t *pcm, int samples,
                          uint8_t *out, int cap, int *out_len);
int app_codec_opus_decode(const uint8_t *buf, int len,
                          int16_t *pcm, int cap, int *out_samples);

#ifdef __cplusplus
}
#endif

#endif /* APP_CODEC_INTERNAL_H */

