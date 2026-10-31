#ifndef AI_AUDIO_H
#define AI_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/* Use encoded silence when the capture device has no playback reference. */
#define AI_AUDIO_NO_REFERENCE UINT_MAX

/* PCM buffers use native int16_t samples. Uplink wire layout is planar:
 * G711A[microphone frames] followed by G711A[echo-reference frames], at 8 kHz.
 * Slot selection is board data, never an application-policy conditional. */
int ai_audio_encode_slots(const int16_t *pcm, size_t frames, unsigned slots,
                          unsigned mic, unsigned reference,
                          uint8_t *out, size_t capacity);
int ai_audio_decode_stereo(const uint8_t *data, size_t bytes,
                           int16_t *pcm, size_t sample_capacity);
uint8_t ai_alaw_encode(int16_t pcm);
int16_t ai_alaw_decode(uint8_t code);
#endif
