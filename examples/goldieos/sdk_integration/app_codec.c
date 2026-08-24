/**
 * @file app_codec.c
 * @brief App-layer codec dispatcher — routes encode/decode to per-codec modules.
 *
 * This file manages codec lifecycle (init/deinit) and dispatches encode/decode
 * calls to the appropriate codec implementation based on the active codec ID.
 * The actual encode/decode logic lives in separate files:
 *
 *   - app_codec_g711a.c   : G.711 A-law wrapper (adapts convai_codec_g711a.c)
 *   - app_codec_pcm16.c   : PCM16 passthrough (no compression)
 *   - app_codec_g711u.c   : G.711 mu-law (2:1 compression)
 *   - app_codec_adpcm.c   : IMA-ADPCM (4:1 compression, stateful)
 *   - app_codec_opus.c    : Opus (high quality, requires CONFIG_APP_ENABLE_OPUS)
 *   - convai_codec_g711a.c: G.711 A-law core implementation (reused from SDK)
 *
 * Codec IDs match the SDK's convai_audio_data_type_e:
 *   G711A=0, PCM=1, OPUS=2, G711U=3, IMA_ADPCM=4
 */
#include "app_codec.h"
#include "app_codec_internal.h"

#include <stdio.h>
#include <string.h>

/* ---- Current codec state ---- */

static int s_codec_id = -1;
static int s_sample_rate = 8000;

/* ---- Public API ---- */

int app_codec_init(app_codec_id_e codec_id)
{
    if (codec_id < 0 || codec_id >= APP_CODEC_MAX) {
        return APP_CODEC_ERR_INVALID_ARG;
    }

    /* Set sample rate based on codec */
    switch (codec_id) {
    case APP_CODEC_G711A:
    case APP_CODEC_PCM16:
    case APP_CODEC_G711U:
    case APP_CODEC_IMA_ADPCM:
        s_sample_rate = 8000;
        break;
    case APP_CODEC_OPUS:
#ifndef CONFIG_APP_ENABLE_OPUS
        return APP_CODEC_ERR_NOT_SUPPORTED;
#else
        s_sample_rate = 16000;
        break;
#endif
    default:
        return APP_CODEC_ERR_INVALID_ARG;
    }

    /* Initialize stateful codecs */
    if (codec_id == APP_CODEC_IMA_ADPCM) {
        app_codec_adpcm_init();
    }
    if (codec_id == APP_CODEC_OPUS) {
#ifdef CONFIG_APP_ENABLE_OPUS
        int ret = app_codec_opus_init();
        if (ret != APP_CODEC_OK) return ret;
#endif
    }

    s_codec_id = codec_id;
    return APP_CODEC_OK;
}

void app_codec_deinit(void)
{
    if (s_codec_id < 0) return;

    if (s_codec_id == APP_CODEC_OPUS) {
#ifdef CONFIG_APP_ENABLE_OPUS
        app_codec_opus_deinit();
#endif
    }

    s_codec_id = -1;
    s_sample_rate = 8000;
}

int app_codec_encode(const int16_t *pcm, int samples,
                     uint8_t *out, int cap, int *out_len)
{
    if (s_codec_id < 0) return APP_CODEC_ERR_NOT_INIT;
    if (pcm == NULL || out == NULL || out_len == NULL) return APP_CODEC_ERR_INVALID_ARG;
    if (samples <= 0 || cap <= 0) return APP_CODEC_ERR_INVALID_ARG;

    switch ((app_codec_id_e)s_codec_id) {
    case APP_CODEC_G711A:
        return app_codec_g711a_encode(pcm, samples, out, cap, out_len);
    case APP_CODEC_PCM16:
        return app_codec_pcm16_encode(pcm, samples, out, cap, out_len);
    case APP_CODEC_G711U:
        return app_codec_g711u_encode(pcm, samples, out, cap, out_len);
    case APP_CODEC_IMA_ADPCM:
        return app_codec_adpcm_encode(pcm, samples, out, cap, out_len);
    case APP_CODEC_OPUS:
#ifdef CONFIG_APP_ENABLE_OPUS
        return app_codec_opus_encode(pcm, samples, out, cap, out_len);
#else
        return APP_CODEC_ERR_NOT_SUPPORTED;
#endif
    default:
        return APP_CODEC_ERR_INVALID_ARG;
    }
}

int app_codec_decode(const uint8_t *buf, int len,
                     int16_t *pcm, int cap, int *out_samples)
{
    if (s_codec_id < 0) return APP_CODEC_ERR_NOT_INIT;
    if (buf == NULL || pcm == NULL || out_samples == NULL) return APP_CODEC_ERR_INVALID_ARG;
    if (len <= 0 || cap <= 0) return APP_CODEC_ERR_INVALID_ARG;

    switch ((app_codec_id_e)s_codec_id) {
    case APP_CODEC_G711A:
        return app_codec_g711a_decode(buf, len, pcm, cap, out_samples);
    case APP_CODEC_PCM16:
        return app_codec_pcm16_decode(buf, len, pcm, cap, out_samples);
    case APP_CODEC_G711U:
        return app_codec_g711u_decode(buf, len, pcm, cap, out_samples);
    case APP_CODEC_IMA_ADPCM:
        return app_codec_adpcm_decode(buf, len, pcm, cap, out_samples);
    case APP_CODEC_OPUS:
#ifdef CONFIG_APP_ENABLE_OPUS
        return app_codec_opus_decode(buf, len, pcm, cap, out_samples);
#else
        return APP_CODEC_ERR_NOT_SUPPORTED;
#endif
    default:
        return APP_CODEC_ERR_INVALID_ARG;
    }
}

int app_codec_get_id(void)
{
    return s_codec_id;
}

int app_codec_get_sample_rate(void)
{
    return (s_codec_id < 0) ? -1 : s_sample_rate;
}

const char *app_codec_get_name(void)
{
    if (s_codec_id < 0) return NULL;
    switch ((app_codec_id_e)s_codec_id) {
    case APP_CODEC_G711A:     return "g711a";
    case APP_CODEC_PCM16:     return "pcm16";
    case APP_CODEC_G711U:     return "g711u";
    case APP_CODEC_IMA_ADPCM: return "ima_adpcm";
    case APP_CODEC_OPUS:      return "opus";
    default:                  return "unknown";
    }
}

