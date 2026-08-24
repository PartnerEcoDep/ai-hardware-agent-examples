/**
 * @file app_codec_g711a.c
 * @brief G.711 A-law codec — thin wrapper over convai_codec_g711a.c.
 *
 * The underlying G.711A implementation (convai_codec_g711a.c) uses a byte-
 * oriented API that takes raw uint8_t* buffers and explicit channel counts.
 * This file adapts it to the common app_codec interface (int16_t* PCM,
 * sample-count oriented) so the dispatcher in app_codec.c can call all
 * codecs uniformly.
 *
 * The actual A-law encode/decode tables and algorithms live in
 * convai_codec_g711a.c — see that file for the implementation details.
 *
 * Use case: the default ConvAI codec. 8 kHz, 64 kbps, 2:1 compression.
 * Standard in European and Chinese telephony systems.
 */
#include "app_codec.h"
#include "app_codec_internal.h"
#include "convai_codec_g711a.h"   /* convai_g711a_encode / convai_g711a_decode */

#include <stddef.h>

int app_codec_g711a_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len)
{
    size_t pcm_len = (size_t)samples * 2;  /* int16_t -> byte count */
    size_t enc_len = 0;

    /* convai_g711a_encode expects planar layout:
     *   channels=2 means stereo planar: [L0, L1, ..., L(n-1), R0, R1, ..., R(n-1)]
     * The uplink already deinterleaved to planar_buf, so we pass channels=2. */
    int ret = convai_g711a_encode((const uint8_t *)pcm, pcm_len, 2,
                                  out, (size_t)cap, &enc_len);
    if (ret != 0) return APP_CODEC_ERR_ENCODE;
    *out_len = (int)enc_len;
    return APP_CODEC_OK;
}

int app_codec_g711a_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples)
{
    size_t pcm_cap = (size_t)cap * 2;   /* sample count -> byte capacity */
    size_t pcm_len = 0;

    int ret = convai_g711a_decode(buf, (size_t)len,
                                  (uint8_t *)pcm, pcm_cap, &pcm_len);
    if (ret != 0) return APP_CODEC_ERR_DECODE;
    *out_samples = (int)(pcm_len / 2);  /* byte count -> sample count */
    return APP_CODEC_OK;
}

