/**
 * @file app_codec.h
 * @brief App-layer audio codec interface.
 *
 * All encode/decode logic lives in the app layer (not in the SDK).
 * The SDK is a pure transport — it sends/receives encoded frames as-is.
 *
 * Codec IDs MUST match SDK's convai_audio_data_type_e:
 *   G711A=0, PCM=1, OPUS=2, G711U=3, IMA_ADPCM=4
 *
 * Typical usage:
 *   app_codec_init(APP_CODEC_G711A);          // after convai_create
 *   app_codec_encode(pcm, samples, buf, ...); // in uplink
 *   app_codec_decode(buf, len, pcm, ...);     // in downlink callback
 *   app_codec_deinit();                        // on cleanup
 */
#ifndef APP_CODEC_H
#define APP_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Codec IDs (must match convai_audio_data_type_e) ---- */

typedef enum {
    APP_CODEC_G711A     = 0,  /**< G.711 A-law, 8kHz, 2:1 (default). Stereo planar input. */
    APP_CODEC_PCM16     = 1,  /**< PCM 16-bit, 8kHz, no compression. Channel-agnostic (memcpy). */
    APP_CODEC_OPUS      = 2,  /**< Opus, 16kHz mono (requires CONFIG_APP_ENABLE_OPUS). */
    APP_CODEC_G711U     = 3,  /**< G.711 mu-law, 8kHz, 2:1. Channel-agnostic (per-sample). */
    APP_CODEC_IMA_ADPCM = 4,  /**< IMA-ADPCM, 8kHz, 4:1. Channel-agnostic (per-sample). */
    APP_CODEC_MAX
} app_codec_id_e;

/* ---- Error codes ---- */

typedef enum {
    APP_CODEC_OK              =  0,
    APP_CODEC_ERR_NOT_INIT    = -1,
    APP_CODEC_ERR_NOT_SUPPORTED = -2,
    APP_CODEC_ERR_INVALID_ARG = -3,
    APP_CODEC_ERR_BUF_TOO_SMALL = -4,
    APP_CODEC_ERR_ENCODE      = -5,
    APP_CODEC_ERR_DECODE      = -6,
} app_codec_err_e;

/* ---- Lifecycle ---- */

/**
 * Initialize codec. Call after convai_create(), before convai_start().
 * The codec_id must match the config JSON "audio.codec" field.
 */
int app_codec_init(app_codec_id_e codec_id);

/**
 * Deinitialize codec. Call on cleanup.
 * Must be called before free (Opus has internal heap).
 */
void app_codec_deinit(void);

/* ---- Encode / Decode ---- */

/**
 * Encode PCM16 samples to encoded frame.
 *
 * Input format: PCM16 samples in planar layout. For stereo codecs (e.g. G.711A),
 * the buffer contains [L0,L1,...,L(n-1), R0,R1,...,R(n-1)] and @p samples is
 * the total count (L+R combined). The actual channel interpretation is
 * codec-specific — see each codec implementation for details.
 *
 * @param pcm       Input PCM16 samples (planar layout, int16_t)
 * @param samples   Total number of input samples (all channels combined)
 * @param out       Output encoded buffer
 * @param cap       Output buffer capacity in bytes
 * @param out_len   [out] Actual encoded bytes written
 * @return APP_CODEC_OK or error code
 */
int app_codec_encode(const int16_t *pcm, int samples,
                     uint8_t *out, int cap, int *out_len);

/**
 * Decode encoded frame to PCM16 samples.
 *
 * Output format: PCM16 samples in planar layout. For stereo codecs (e.g. G.711A),
 * the output buffer contains [L0,L1,...,L(n-1), R0,R1,...,R(n-1)] and @p out_samples
 * is the total count (L+R combined). The actual channel interpretation is
 * codec-specific — see each codec implementation for details.
 *
 * @param buf        Input encoded data
 * @param len        Input data length in bytes
 * @param pcm        Output PCM16 buffer (planar layout)
 * @param cap        Output buffer capacity in samples (not bytes)
 * @param out_samples [out] Actual output samples (all channels combined)
 * @return APP_CODEC_OK or error code
 */
int app_codec_decode(const uint8_t *buf, int len,
                     int16_t *pcm, int cap, int *out_samples);

/* ---- Query ---- */

/** Get current codec ID, or -1 if not initialized. */
int app_codec_get_id(void);

/** Get current codec sample rate (8000 or 16000), or -1 if not initialized. */
int app_codec_get_sample_rate(void);

/** Get current codec name string, or NULL if not initialized. */
const char *app_codec_get_name(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CODEC_H */


