/**
 * @file app_codec_adpcm.c
 * @brief IMA-ADPCM codec implementation (4:1 compression).
 *
 * IMA-ADPCM (Adaptive Differential PCM) compresses 16-bit PCM to 4 bits per
 * sample (32 kbps @ 8 kHz, 2:1 ratio vs PCM, same bitrate as G.711 but with
 * different quality characteristics — generally better SNR on voice).
 *
 * Encoding:
 *   - Maintain a predictor (last reconstructed value) and a step-index into
 *     a fixed 89-entry step-size table.
 *   - For each sample: diff = sample - predictor; quantise diff to 4 bits
 *     using the current step size; update predictor and step-index.
 *   - Pack two 4-bit codes into one byte (low nibble first).
 *
 * Decoding:
 *   - Inverse of encoding: expand each 4-bit code using the current step
 *     size, update predictor and step-index.
 *
 * State: predictor (int16_t) + step_index (int8_t) must persist across
 * calls within a session. Call app_codec_adpcm_init() at session start to
 * reset to zero.
 *
 * Reference: IMA Recommended Practices for ADPCM, Version 4.0 (1992).
 *
 * Use case: bandwidth-constrained links where G.711 quality is not required,
 * or where the 2:1 compression of G.711 is insufficient.
 */
#include "app_codec.h"
#include "app_codec_internal.h"

/* ---- IMA-ADPCM step-size table (89 entries, ITU-T G.726 / IMA standard) ---- */

static const int16_t ima_step_table[89] = {
       7,    8,    9,   10,   11,   12,   13,   14,
      16,   17,   19,   21,   23,   25,   28,   31,
      34,   37,   41,   45,   50,   55,   60,   66,
      73,   80,   88,   97,  107,  118,  130,  143,
     157,  173,  190,  209,  230,  253,  279,  307,
     337,  371,  408,  449,  494,  544,  598,  658,
     724,  796,  876,  963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493,10442,11487,12635,13899,
   15289,16818,18500,20350,22385,24623,27086,29794,
   32767
};

/* ---- IMA-ADPCM index-adjustment table (per 4-bit code) ---- */

static const int8_t ima_index_table[16] = {
    -1, -1, -1, -1,  2,  4,  6,  8,
    -1, -1, -1, -1,  2,  4,  6,  8
};

/* ---- Codec state (module-private) ---- */

typedef struct {
    int16_t predictor;      /* reconstructed value of the last sample */
    int8_t  step_index;     /* index into ima_step_table */
} ima_state_t;

static ima_state_t s_state;

void app_codec_adpcm_init(void)
{
    s_state.predictor   = 0;
    s_state.step_index  = 0;
}

/* ---- Encode one PCM16 sample to a 4-bit ADPCM code ---- */

static uint8_t adpcm_encode_sample(int16_t sample, ima_state_t *st)
{
    int16_t diff   = (int16_t)(sample - st->predictor);
    int16_t step   = ima_step_table[st->step_index];
    int16_t vpdiff = 0;   /* accumulated quantised difference */
    uint8_t code   = 0;

    /* Sign bit: 1 = negative difference. */
    if (diff < 0) {
        code  = 0x08;
        diff  = (int16_t)(-diff);
    }

    /* Quantise |diff| into 3 magnitude bits using successive approximation. */
    int16_t mask = step >> 1;
    for (int i = 2; i >= 0; i--) {
        if (diff >= step) {
            code   |= (uint8_t)(1 << i);
            vpdiff = (int16_t)(vpdiff + step);
            diff    = (int16_t)(diff - step);
        }
        step >>= 1;
    }
    /* Final bit (LSB): compare against half-step threshold. */
    if (diff >= mask) {
        code   |= 0x01;
        vpdiff  = (int16_t)(vpdiff + mask);
    }

    /* Update predictor (add or subtract depending on sign bit). */
    if (code & 0x08) {
        st->predictor = (int16_t)(st->predictor - vpdiff);
    } else {
        st->predictor = (int16_t)(st->predictor + vpdiff);
    }

    /* Clip predictor to int16_t range. */
    if (st->predictor >  32767) st->predictor =  32767;
    if (st->predictor < -32768) st->predictor = -32768;

    /* Update step-index (clamped to 0..88). */
    st->step_index += ima_index_table[code];
    if (st->step_index <  0) st->step_index = 0;
    if (st->step_index > 88) st->step_index = 88;

    return code & 0x0F;
}

/* ---- Decode one 4-bit ADPCM code to a PCM16 sample ---- */

static int16_t adpcm_decode_sample(uint8_t code, ima_state_t *st)
{
    int16_t step   = ima_step_table[st->step_index];
    int16_t vpdiff = 0;

    /* Reconstruct the quantised difference from the 3 magnitude bits. */
    int16_t mask = step >> 1;
    for (int i = 0; i < 3; i++) {
        if (code & (uint8_t)(1 << i)) vpdiff = (int16_t)(vpdiff + step);
        step >>= 1;
    }
    /* Sign bit (bit 3): subtract instead of add. */
    if (code & 0x08) {
        st->predictor = (int16_t)(st->predictor - vpdiff);
    } else {
        st->predictor = (int16_t)(st->predictor + vpdiff);
    }

    /* Clip predictor. */
    if (st->predictor >  32767) st->predictor =  32767;
    if (st->predictor < -32768) st->predictor = -32768;

    /* Update step-index. */
    st->step_index += ima_index_table[code];
    if (st->step_index <  0) st->step_index = 0;
    if (st->step_index > 88) st->step_index = 88;

    return st->predictor;
}

/* ---- Public codec interface ---- */

/**
 * Encode PCM16 mono samples to IMA-ADPCM.
 *
 * Output: 4:1 compression — `samples / 2` bytes written.
 * Packing: two 4-bit codes per byte, low nibble first (IMA convention).
 */
int app_codec_adpcm_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len)
{
    int out_bytes = samples / 2;
    if (out_bytes > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;

    uint8_t *p = out;
    for (int i = 0; i + 1 < samples + 1; i += 2) {
        uint8_t lo = adpcm_encode_sample(pcm[i],     &s_state);
        uint8_t hi = adpcm_encode_sample(pcm[i + 1], &s_state);
        *p++ = (uint8_t)((hi << 4) | lo);
    }
    *out_len = out_bytes;
    return APP_CODEC_OK;
}

/**
 * Decode IMA-ADPCM bytes to PCM16 mono samples.
 *
 * Output: 4:1 expansion — `len * 2` samples written.
 * Packing: low nibble first (matches the encode order).
 */
int app_codec_adpcm_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples)
{
    int temp_samples = len * 2;
    if (temp_samples > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;

    int16_t *p = pcm;
    for (int i = 0; i < len; i++) {
        uint8_t byte = buf[i];
        *p++ = adpcm_decode_sample(byte & 0x0F,        &s_state);
        *p++ = adpcm_decode_sample((byte >> 4) & 0x0F, &s_state);
    }
    *out_samples = temp_samples;
    return APP_CODEC_OK;
}


