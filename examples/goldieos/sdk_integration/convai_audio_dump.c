/**
 * @file convai_audio_dump.c
 * @brief WAV file dump implementation (desktop only, no-op on embedded).
 *
 * Writes PCM audio data to independent WAV files with proper RIFF headers.
 * Used for debugging audio capture and playback on desktop platforms.
 */
#include "convai_audio_dump.h"

#include <stdio.h>
#include <stdint.h>

#ifndef __EMBEDDED__

typedef struct {
    FILE *file;
    long  data_bytes;
} audio_dump_state_t;

static audio_dump_state_t g_dump_states[BRIDGE_AUDIO_DUMP_COUNT];

static audio_dump_state_t *dump_get_state(bridge_audio_dump_stream_t stream)
{
    if (stream < BRIDGE_AUDIO_DUMP_UPLINK ||
        stream >= BRIDGE_AUDIO_DUMP_COUNT) {
        return NULL;
    }
    return &g_dump_states[stream];
}

/**
 * Write a placeholder WAV header; sizes will be patched on close.
 */
static int dump_wav_header(FILE *f, int sample_rate, int channels, int bits)
{
    /* RIFF header */
    fwrite("RIFF", 1, 4, f);
    uint32_t riff_size = 0;  /* placeholder */
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1; /* PCM */
    fwrite(&audio_format, 2, 1, f);
    uint16_t ch = (uint16_t)channels;
    fwrite(&ch, 2, 1, f);
    uint32_t sr = (uint32_t)sample_rate;
    fwrite(&sr, 4, 1, f);
    uint32_t byte_rate = (uint32_t)(sample_rate * channels * bits / 8);
    fwrite(&byte_rate, 4, 1, f);
    uint16_t block_align = (uint16_t)(channels * bits / 8);
    fwrite(&block_align, 2, 1, f);
    uint16_t bps = (uint16_t)bits;
    fwrite(&bps, 2, 1, f);

    /* data chunk header */
    fwrite("data", 1, 4, f);
    uint32_t data_size = 0;  /* placeholder */
    fwrite(&data_size, 4, 1, f);

    fflush(f);
    return 0;
}

/**
 * Patch the WAV header with actual data size.
 */
static void dump_wav_finalize(FILE *f, long total_data_bytes)
{
    if (!f) return;

    /* RIFF chunk size = 4 + 24 + 8 + data_size = 36 + data_size */
    uint32_t riff_size = (uint32_t)(36 + total_data_bytes);
    fseek(f, 4, SEEK_SET);
    fwrite(&riff_size, 4, 1, f);

    /* data chunk size */
    fseek(f, 40, SEEK_SET);
    uint32_t data_size = (uint32_t)total_data_bytes;
    fwrite(&data_size, 4, 1, f);

    fclose(f);
}

int bridge_dump_open(bridge_audio_dump_stream_t stream, const char *path,
                     int sample_rate, int channels, int bits)
{
    audio_dump_state_t *state = dump_get_state(stream);
    if (state == NULL) return -1;

    if (state->file) {
        dump_wav_finalize(state->file, state->data_bytes);
        state->file = NULL;
    }

    state->file = fopen(path, "wb");
    if (!state->file) return -1;

    dump_wav_header(state->file, sample_rate, channels, bits);
    state->data_bytes = 0;
    return 0;
}

int bridge_dump_write(bridge_audio_dump_stream_t stream,
                      const void *data, size_t len)
{
    audio_dump_state_t *state = dump_get_state(stream);
    if (state == NULL) return -1;
    if (!state->file) return 0;

    size_t written = fwrite(data, 1, len, state->file);
    state->data_bytes += (long)written;
    return (written == len) ? 0 : -1;
}

int bridge_dump_close(bridge_audio_dump_stream_t stream)
{
    audio_dump_state_t *state = dump_get_state(stream);
    if (state == NULL) return -1;
    if (!state->file) return 0;

    dump_wav_finalize(state->file, state->data_bytes);
    state->file = NULL;
    state->data_bytes = 0;
    return 0;
}

#else /* __EMBEDDED__ */

int bridge_dump_open(bridge_audio_dump_stream_t stream, const char *path,
                     int sample_rate, int channels, int bits)
{
    (void)stream; (void)path; (void)sample_rate; (void)channels; (void)bits;
    return -1;  /* not supported on embedded — no filesystem */
}

int bridge_dump_write(bridge_audio_dump_stream_t stream,
                      const void *data, size_t len)
{
    (void)stream; (void)data; (void)len;
    return 0;
}

int bridge_dump_close(bridge_audio_dump_stream_t stream)
{
    (void)stream;
    return 0;
}

#endif /* __EMBEDDED__ */
