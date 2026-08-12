/**
 * @file convai_audio_dump.h
 * @brief WAV file dump for debugging (desktop only, no-op on embedded).
 *
 * Provides a simple interface to record PCM data to WAV files. Uplink and
 * downlink use independent streams so they can be dumped at the same time.
 * On embedded platforms, all functions are no-ops.
 */
#ifndef CONVAI_AUDIO_DUMP_H
#define CONVAI_AUDIO_DUMP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Independent WAV dump streams used by the audio pipelines. */
typedef enum {
    BRIDGE_AUDIO_DUMP_UPLINK = 0,
    BRIDGE_AUDIO_DUMP_DOWNLINK,
    BRIDGE_AUDIO_DUMP_COUNT
} bridge_audio_dump_stream_t;

/**
 * Open a WAV dump file and write the header.
 * On embedded platforms, this is a no-op.
 *
 * @param stream       Uplink or downlink dump stream
 * @param path         Output file path
 * @param sample_rate  Audio sample rate (e.g., 8000, 16000)
 * @param channels     Number of channels (1 = mono, 2 = stereo)
 * @param bits         Bits per sample (e.g., 16)
 * @return 0 on success, -1 on failure (or always 0 on embedded)
 */
int bridge_dump_open(bridge_audio_dump_stream_t stream, const char *path,
                     int sample_rate, int channels, int bits);

/**
 * Write PCM data to the dump file.
 * On embedded platforms, this is a no-op.
 *
 * @param stream  Uplink or downlink dump stream
 * @param data    PCM data buffer
 * @param len   Data length in bytes
 * @return 0 on success, -1 on failure (or always 0 on embedded)
 */
int bridge_dump_write(bridge_audio_dump_stream_t stream,
                      const void *data, size_t len);

/**
 * Finalize the WAV header and close the dump file.
 * On embedded platforms, this is a no-op.
 *
 * @param stream  Uplink or downlink dump stream
 * @return 0 on success, -1 on failure (or always 0 on embedded)
 */
int bridge_dump_close(bridge_audio_dump_stream_t stream);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_AUDIO_DUMP_H */
