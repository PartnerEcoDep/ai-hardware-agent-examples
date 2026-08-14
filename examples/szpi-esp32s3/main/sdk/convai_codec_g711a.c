/**
 * @file convai_codec_g711a.c
 * @brief G.711 A-law codec implementation (ITU-T G.711 standard).
 *
 * Encoder: 16-bit PCM → 8-bit A-law (13-bit linear → log companding)
 * Decoder: 8-bit A-law → 16-bit PCM
 *
 * Ported verbatim from goldieos' convai_codec_g711a.c. The ESP32 codec
 * driver delivers raw 16-bit PCM, so we encode on the uplink and decode
 * on the downlink to satisfy CONVAI_AUDIO_DATA_TYPE_G711A.
 */

#include "convai_codec_g711a.h"

/* ------------------------------------------------------------------ */
/*  A-law encode: 16-bit PCM → 8-bit A-law                            */
/* ------------------------------------------------------------------ */

static uint8_t pcm16_to_alaw(int16_t pcm_val)
{
    int mask;
    int seg;

    /* Sign + magnitude with offset to avoid -32768 overflow */
    if (pcm_val >= 0) {
        mask = 0xD5;            /* sign-bit = 1 for positive (post-XOR convention) */
    } else {
        mask = 0x55;            /* sign-bit = 0 for negative */
        pcm_val = -pcm_val - 8; /* offset avoids overflow at -32768 */
    }

    /* Convert 16-bit linear → 13-bit linear */
    pcm_val >>= 3;

    /* Clip 13-bit value to maximum */
    if (pcm_val > 4095) pcm_val = 4095;

    /* Segment determination: sequential shift-left search.
     * Segment boundaries (13-bit): 0-31(seg0), 32-63(seg1), 64-127(seg2),
     * 128-255(seg3), 256-511(seg4), 512-1023(seg5), 1024-2047(seg6),
     * 2048-4095(seg7) */
    seg = 0;
    if (pcm_val >= 32)  { seg = 1; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 2; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 3; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 4; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 5; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 6; pcm_val >>= 1;
    if (pcm_val >= 32)  { seg = 7; pcm_val >>= 1; }}}}}}}

    /* Build pre-inversion byte: seg(3 bits) | quant(4 bits) | sign handled by mask XOR */
    uint8_t aval = (uint8_t)((seg << 4) | ((pcm_val >> 1) & 0x0F));

    /* Apply even-bit inversion and sign via mask XOR */
    return (uint8_t)(aval ^ mask);
}

/* ------------------------------------------------------------------ */
/*  A-law decode: 8-bit A-law → 16-bit PCM                            */
/* ------------------------------------------------------------------ */

static int16_t alaw_to_pcm16(uint8_t alaw_val)
{
    int16_t t;

    /* Undo even-bit inversion */
    alaw_val ^= 0x55;

    /* Extract step (low 4 bits) and segment (bits 6-4, after inversion) */
    t   = (int16_t)((alaw_val & 0x0F) << 4);
    int16_t seg = (int16_t)((alaw_val & 0x70) >> 4);

    /* Expand based on segment */
    switch (seg) {
    case 0:
        t += 8;
        break;
    case 1:
        t += 0x108;     /* 264 */
        break;
    default:
        t += 0x108;     /* 264 */
        t <<= (int16_t)(seg - 1);
        break;
    }

    /* Apply sign: after XOR 0x55, bit7=1 means positive, bit7=0 means negative */
    return (int16_t)((alaw_val & 0x80) ? t : -t);
}

/* ------------------------------------------------------------------ */
/*  Public interface                                                  */
/* ------------------------------------------------------------------ */

int convai_g711a_encode(const uint8_t *pcm, size_t pcm_len, int channels,
                        uint8_t *out, size_t out_cap,
                        size_t *out_len)
{
    if ((pcm == NULL) || (out == NULL) || (out_len == NULL)) return -1;
    if (pcm_len % 2 != 0) return -1;
    if (channels < 1) channels = 1;

    size_t total_samples = pcm_len / 2;
    size_t frames = total_samples / (size_t)channels;
    size_t out_needed = frames * (size_t)channels;
    if (out_cap < out_needed) return -1;

    const int16_t *planar = (const int16_t *)pcm;
    for (int ch = 0; ch < channels; ch++) {
        for (size_t f = 0; f < frames; f++) {
            out[ch * frames + f] = pcm16_to_alaw(planar[ch * frames + f]);
        }
    }

    *out_len = out_needed;
    return 0;
}

int convai_g711a_decode(const uint8_t *encoded, size_t enc_len,
                        uint8_t *pcm, size_t pcm_cap,
                        size_t *pcm_len)
{
    if ((encoded == NULL) || (pcm == NULL) || (pcm_len == NULL)) return -1;

    size_t need = enc_len * 2;
    if (pcm_cap < need) return -1;

    for (size_t i = 0; i < enc_len; i++) {
        int16_t sample = alaw_to_pcm16(encoded[i]);
        /* Little-endian output */
        pcm[i * 2]     = (uint8_t)(sample & 0xFF);
        pcm[i * 2 + 1] = (uint8_t)((sample >> 8) & 0xFF);
    }

    *pcm_len = need;
    return 0;
}
