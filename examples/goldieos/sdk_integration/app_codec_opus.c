/**
 * @file app_codec_opus.c
 * @brief Opus codec — stub / real implementation.
 *
 * Opus is a high-quality, low-latency audio codec supporting both speech
 * (SILK, 8-12 kbps) and music (CELT, 32-128 kbps) in a single codec.
 * For the ConvAI voice-assistant use case we use CELT-only mode at 16 kHz
 * mono, ~32 kbps — a good quality/bitrate tradeoff for voice.
 *
 * Build configuration:
 *   - Without CONFIG_APP_ENABLE_OPUS: this file compiles to a stub that
 *     returns APP_CODEC_ERR_NOT_SUPPORTED for all operations. The codec
 *     is unavailable but the build still succeeds.
 *   - With    CONFIG_APP_ENABLE_OPUS: links against libopus (provided by
 *     third_party/opus-1.6.1/ via CMake). Encoder and decoder are created
 *     at init time and destroyed at deinit.
 *
 * Why a stub by default:
 *   - libopus adds ~100 KB of code + ~20 KB of BSS on the WS63 target.
 *   - Not all customers need Opus; G.711 and IMA-ADPCM cover most cases.
 *   - The stub keeps the Opus code path exercised (init/deinit/encode/decode
 *     are all reachable) so linking and the dispatch table stay correct.
 *
 * Implementation notes (CONFIG_APP_ENABLE_OPUS path):
 *   - OPUS_APPLICATION_VOIP is used (lower latency than AUDIO).
 *   - Fixed-point API (opus_encode_float / opus_decode_float are disabled
 *     via OPUS_DISABLE_FLOAT_API in CMake; we use opus_encode / opus_decode).
 *   - Frame size is 20 ms at 16 kHz = 320 samples per frame.
 *   - Bitrate is set to 32000 bps (opus_encoder_ctl(enc, OPUS_SET_BITRATE(32000))).
 *   - COMPLEXITY is set to 0 (lowest CPU, acceptable quality for voice).
 */
#include "app_codec.h"
#include "app_codec_internal.h"

#ifdef CONFIG_APP_ENABLE_OPUS
#include <opus.h>
#include <stdio.h>

/* Frame size: 20 ms at 16 kHz = 320 samples. */
#define OPUS_FRAME_SIZE   320
#define OPUS_SAMPLE_RATE  16000
#define OPUS_BITRATE      32000
#define OPUS_COMPLEXITY   0

static OpusEncoder *s_enc = NULL;
static OpusDecoder *s_dec = NULL;

int app_codec_opus_init(void)
{
    int err;

    if (s_enc == NULL) {
        s_enc = opus_encoder_create(OPUS_SAMPLE_RATE, 1 /* mono */,
                                    OPUS_APPLICATION_VOIP, &err);
        if (err != OPUS_OK || s_enc == NULL) return APP_CODEC_ERR_ENCODE;
        opus_encoder_ctl(s_enc, OPUS_SET_BITRATE(OPUS_BITRATE));
        opus_encoder_ctl(s_enc, OPUS_SET_COMPLEXITY(OPUS_COMPLEXITY));
    }

    if (s_dec == NULL) {
        s_dec = opus_decoder_create(OPUS_SAMPLE_RATE, 1 /* mono */, &err);
        if (err != OPUS_OK || s_dec == NULL) {
            opus_encoder_destroy(s_enc);
            s_enc = NULL;
            return APP_CODEC_ERR_DECODE;
        }
    }

    return APP_CODEC_OK;
}

void app_codec_opus_deinit(void)
{
    if (s_enc) { opus_encoder_destroy(s_enc); s_enc = NULL; }
    if (s_dec) { opus_decoder_destroy(s_dec); s_dec = NULL; }
}

int app_codec_opus_encode(const int16_t *pcm, int samples,
                          uint8_t *out, int cap, int *out_len)
{
    if (!s_enc) return APP_CODEC_ERR_NOT_INIT;
    int ret = opus_encode(s_enc, pcm, samples, out, (opus_int32)cap);
    if (ret < 0) {
        printf("[app_codec_opus] encode error: %d\n", ret);
        return APP_CODEC_ERR_ENCODE;
    }
    *out_len = ret;
    return APP_CODEC_OK;
}

int app_codec_opus_decode(const uint8_t *buf, int len,
                          int16_t *pcm, int cap, int *out_samples)
{
    if (!s_dec) return APP_CODEC_ERR_NOT_INIT;
    int ret = opus_decode(s_dec, buf, (opus_int32)len, pcm, cap, 0);
    if (ret < 0) {
        printf("[app_codec_opus] decode error: %d\n", ret);
        return APP_CODEC_ERR_DECODE;
    }
    *out_samples = ret;
    return APP_CODEC_OK;
}

#else /* !CONFIG_APP_ENABLE_OPUS — stub implementation */

int app_codec_opus_init(void)
{
    return APP_CODEC_ERR_NOT_SUPPORTED;
}

void app_codec_opus_deinit(void)
{
    /* no-op */
}

int app_codec_opus_encode(const int16_t *pcm, int samples,
                          uint8_t *out, int cap, int *out_len)
{
    (void)pcm; (void)samples; (void)out; (void)cap; (void)out_len;
    return APP_CODEC_ERR_NOT_SUPPORTED;
}

int app_codec_opus_decode(const uint8_t *buf, int len,
                          int16_t *pcm, int cap, int *out_samples)
{
    (void)buf; (void)len; (void)pcm; (void)cap; (void)out_samples;
    return APP_CODEC_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_APP_ENABLE_OPUS */

