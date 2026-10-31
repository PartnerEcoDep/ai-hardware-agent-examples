#include "ai_audio.h"
#include <limits.h>

uint8_t ai_alaw_encode(int16_t sample) {
    /* ITU G.711 A-law segment boundaries for 16-bit linear PCM. */
    static const int ends[] = {0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff};
    int value = sample;
    int mask = value >= 0 ? 0xd5 : 0x55;
    if (value < 0) value = -value - 1;
    unsigned seg = 0;
    while (seg < 8 && value > ends[seg]) ++seg;
    unsigned code = seg << 4;
    code |= ((unsigned)value >> (seg < 2 ? 4 : seg + 3)) & 15;
    return (uint8_t)(code ^ mask);
}

int16_t ai_alaw_decode(uint8_t code) {
    code ^= 0x55;
    int value = (code & 15) << 4;
    unsigned seg = (code >> 4) & 7;
    if (!seg) value += 8;
    else { value += 264; value <<= seg - 1; }
    return (int16_t)((code & 0x80) ? value : -value);
}

int ai_audio_encode_slots(const int16_t *pcm, size_t frames, unsigned slots,
                          unsigned mic, unsigned reference,
                          uint8_t *out, size_t capacity) {
    if (!pcm || !out || !slots || mic >= slots ||
        (reference != AI_AUDIO_NO_REFERENCE && reference >= slots) ||
        frames > capacity / 2 || frames > INT_MAX / 2 || frames > SIZE_MAX / slots)
        return -1;
    for (size_t i = 0; i < frames; ++i) {
        out[i] = ai_alaw_encode(pcm[i * slots + mic]);
        out[frames + i] = ai_alaw_encode(reference == AI_AUDIO_NO_REFERENCE ?
                                        0 : pcm[i * slots + reference]);
    }
    return (int)(frames * 2);
}

int ai_audio_decode_stereo(const uint8_t *data, size_t bytes,
                           int16_t *pcm, size_t sample_capacity) {
    if (!data || !pcm || bytes > sample_capacity / 2 || bytes > INT_MAX / 2) return -1;
    for (size_t i = 0; i < bytes; ++i) {
        pcm[2 * i] = pcm[2 * i + 1] = ai_alaw_decode(data[i]);
    }
    return (int)(bytes * 2);
}
