/**
 * @file convai_codec_g711a.h
 * @brief G.711 A-law codec — protocol-required for ConvAI SDK audio.
 *
 * The SDK expects audio as 8-bit A-law packed bytes (per
 * CONVAI_AUDIO_DATA_TYPE_G711A). The ESP32 codec driver delivers raw
 * 16-bit PCM, so we must encode on the uplink and decode on the
 * downlink. Algorithm is the standard ITU-T G.711 A-law, ported from
 * goldieos' convai_codec_g711a.c.
 */
#ifndef CONVAI_CODEC_G711A_H
#define CONVAI_CODEC_G711A_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encode 16-bit PCM to 8-bit A-law.
 * @param pcm       Interleaved PCM (channels samples per frame).
 * @param pcm_len   Total bytes of PCM input.
 * @param channels  Number of channels (1=mono, 2=stereo). Output preserves
 *                  channel order: ch0[0..N-1], ch1[0..N-1], ...
 * @param out       Output buffer for A-law bytes.
 * @param out_cap   Capacity of @p out.
 * @param out_len   [out] Number of A-law bytes written.
 * @return 0 on success, -1 on bad args or insufficient output space.
 */
int convai_g711a_encode(const uint8_t *pcm, size_t pcm_len, int channels,
                        uint8_t *out, size_t out_cap,
                        size_t *out_len);

/**
 * Decode 8-bit A-law to 16-bit PCM (little-endian).
 * @param encoded   A-law bytes.
 * @param enc_len   Number of A-law bytes.
 * @param pcm       Output buffer (must hold at least enc_len * 2 bytes).
 * @param pcm_cap   Capacity of @p pcm.
 * @param pcm_len   [out] Bytes written to @p pcm.
 * @return 0 on success, -1 on bad args or insufficient output space.
 */
int convai_g711a_decode(const uint8_t *encoded, size_t enc_len,
                        uint8_t *pcm, size_t pcm_cap,
                        size_t *pcm_len);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_CODEC_G711A_H */
