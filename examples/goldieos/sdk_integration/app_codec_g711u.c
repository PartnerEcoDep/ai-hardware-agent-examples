/**
 * @file app_codec_g711u.c
 * @brief G.711 mu-law (u-law) codec implementation.
 *
 * G.711 mu-law compresses 16-bit linear PCM to 8-bit logarithmic samples
 * at a 2:1 ratio. It is the North-American / Japanese telephone standard
 * (G.711 A-law is the European equivalent; both are 64 kbps @ 8 kHz).
 *
 * Algorithm overview:
 *   Encode: PCM16 → add bias → find segment → quantise → invert bits
 *   Decode: invert bits → extract segment + quant → reconstruct → subtract bias
 *
 * Reference: ITU-T G.711 (1988), Section 4 "Mu-law encoding/decoding".
 *
 * Use case: interoperability with North-American PSTN / SIP endpoints that
 * require mu-law rather than A-law.
 */
#include "app_codec.h"
#include "app_codec_internal.h"

/* ---- mu-law encode: PCM16 -> 8-bit mu-law ---- */

#define MULAW_BIAS  132   /* output bias for the "zero" level */
#define MULAW_CLIP  32635 /* maximum positive value before clipping */

/* Segment boundary table — the upper limit of each of the 8 segments. */
static const int16_t seg_uend[8] = {
    0x1F, 0x3F, 0x7F, 0xFF, 0x1FF, 0x3FF, 0x7FF, 0xFFF
};

static uint8_t pcm16_to_ulaw(int16_t pcm_val)
{
    int sign, seg, uval;

    /* Sign bit: 0x80 for negative samples, 0 for positive. */
    sign = (pcm_val < 0) ? 0x80 : 0;
    if (pcm_val < 0) pcm_val = (int16_t)(-pcm_val);

    /* Clip to mu-law maximum range and add the bias. */
    if (pcm_val > MULAW_CLIP) pcm_val = MULAW_CLIP;
    pcm_val = (int16_t)(pcm_val + MULAW_BIAS);

    /* Find the highest segment whose upper bound is <= pcm_val. */
    for (seg = 7; seg >= 0; seg--) {
        if (pcm_val >= seg_uend[seg]) break;
    }
    if (seg < 0) seg = 0;

    /* Quantise: extract 4 bits of mantissa from the appropriate bit position.
     * Segment 0 uses bit 1 (shift 1); segments 1-7 use bit (seg+2). */
    if (seg == 0) {
        uval = (pcm_val >> 1) & 0x0F;
    } else {
        uval = (pcm_val >> (seg + 2)) & 0x0F;
    }
    uval = (seg << 4) | uval;

    /* Invert all bits (G.711 convention) and combine with sign. */
    return (uint8_t)(~(uval | sign));
}

/* ---- mu-law decode: 8-bit mu-law -> PCM16 ---- */

static int16_t ulaw_to_pcm16(uint8_t ulaw)
{
    int sign, seg, uval;

    /* Undo the bit inversion applied at encode time. */
    ulaw = (uint8_t)(~ulaw);
    sign = (ulaw & 0x80) ? -1 : 1;
    ulaw &= 0x7F;

    /* Extract segment (3 bits) and quantisation (4 bits). */
    seg  = (ulaw >> 4) & 0x07;
    uval = ulaw & 0x0F;

    /* Reconstruct the linear value.
     * Segment 0: value = (uval << 1) + 1  (bias is implicit in the shift).
     * Segments 1-7: value = ((uval + 16) << (seg + 2)) - bias. */
    if (seg == 0) {
        uval = (uval << 1) + 1;
    } else {
        uval = ((uval + 16) << (seg + 2)) - MULAW_BIAS;
    }

    return (int16_t)(uval * sign);
}

/* ---- Public codec interface ---- */

int app_codec_g711u_encode(const int16_t *pcm, int samples,
                           uint8_t *out, int cap, int *out_len)
{
    if (samples > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;
    for (int i = 0; i < samples; i++) {
        out[i] = pcm16_to_ulaw(pcm[i]);
    }
    *out_len = samples;
    return APP_CODEC_OK;
}

int app_codec_g711u_decode(const uint8_t *buf, int len,
                           int16_t *pcm, int cap, int *out_samples)
{
    if (len > cap) return APP_CODEC_ERR_BUF_TOO_SMALL;
    for (int i = 0; i < len; i++) {
        pcm[i] = ulaw_to_pcm16(buf[i]);
    }
    *out_samples = len;
    return APP_CODEC_OK;
}
