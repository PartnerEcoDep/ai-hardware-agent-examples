/**
 * @file app_codec_pcm16.c
 * @brief PCM16 (uncompressed) codec — passthrough copy.
 *
 * No compression: 1 sample = 2 bytes. The "encode" is just a memcpy from
 * the int16_t sample buffer to the byte output; "decode" is the reverse.
 *
 * Use case: reference / debug (no quality loss), or when the transport
 * bandwidth is not a constraint (e.g. USB tethering).
 */
#include "app_codec.h"
#include "app_codec_internal.h"

#include <string.h>

int app_codec_pcm16_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len)
{
    int bytes = samples * 2;
    if (bytes > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;
    memcpy(out, pcm, (size_t)bytes);
    *out_len = bytes;
    return APP_CODEC_OK;
}

int app_codec_pcm16_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples)
{
    int samples = len / 2;
    if (samples > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;
    memcpy(pcm, buf, (size_t)(samples * 2));
    *out_samples = samples;
    return APP_CODEC_OK;
}

