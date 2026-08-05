/**
 * @file convai_audio_diag.h
 * @brief Read-only capture diagnostics: per-frame signal metrics + vad_detect probe.
 *
 * PURE DIAGNOSTIC — never alters the capture/encode/send path. Computes a few
 * signal metrics each frame and logs a sparse summary (~1 line/sec) + state
 * changes, so an intermittent WS63 fault ("sometimes no audio" / "backend
 * receives white noise") can be traced back without on-device file storage.
 *
 * This module only REPORTS raw measured values. It does NOT classify frames as
 * voice/silent — measured thresholds differ between the Win simulator and WS63
 * hardware, so any fixed threshold would mislead on one of the two. Callers
 * read the logged values and judge against their own platform-specific norms.
 *
 * ---------------------------------------------------------------------------
 *  Metrics (all computed on int16 PCM samples)
 * ---------------------------------------------------------------------------
 *
 *  rmsL / rmsR  : per-channel mean(|sample|) over the frame.
 *                 Energy proxy (no sqrt). 0 = silence, hundreds = low/ambient,
 *                 thousands = speech, >10000 = clipping.
 *                 - Left channel (L): the mic signal sent to the cloud.
 *                 - Right channel (R): PLATFORM-DEPENDENT semantics:
 *                     * WS63 : speaker playback signal (AEC reference). The mic
 *                       hardware captures it so the cloud can do echo cancel.
 *                       A non-zero rmsR while silent locally = AI is talking.
 *                     * Win : forced to 0 by capture_one_frame (no AEC ref on
 *                       the simulator), so rmsR is always 0 on Win.
 *
 *  zeros       : count of exactly-0 samples in the left channel this frame.
 *                 Exposes "audio_read returned bytes but they're all zero" — a
 *                 hidden capture failure distinct from nodata (read returned
 *                 <=0). Reported as zeros/N in flip logs, and bumps `allzero`
 *                 in the summary when the entire left channel is zero.
 *
 *  dc          : signed mean of left-channel samples this frame. A DC-offset
 *                 probe. ~0 for healthy audio; a large positive/negative value
 *                 means the mic/hardware is biased — produces moderate RMS with
 *                 no real speech, a white-noise signature at the backend.
 *
 *  nodata      : (summary only) frames where audio_read returned <=0.
 *
 *  allzero     : (summary only) frames where audio_read returned >0 but the
 *                 entire left channel was 0.
 *
 *  dcwarn      : (summary only) frames where |dc| exceeded DIAG_DC_WARN_THRESH.
 *
 *  vad[y/n/err]: (summary only) tally of vad_detect() return values this
 *                 interval. vad_detect is the WS63 webrtc_vad wrapper; it takes
 *                 a 160-sample (20ms @ 8kHz) frame — a valid webrtc_vad length.
 *                   y   = returned 1 (vad says "voice")
 *                   n   = returned 0 (vad says "no voice")
 *                   err = returned -1 (invalid frame / sample-rate unset)
 *                 NULL/unassigned vad_detect → all three stay 0 (skipped, not
 *                 counted as err). Used to VERIFY whether vad_detect is usable:
 *                 err>0 or y constantly 25 (even in silence) = not trustworthy.
 *
 * ---------------------------------------------------------------------------
 *  What this module does NOT do
 * ---------------------------------------------------------------------------
 *  - No voice/silent classification of frames. There is no `voice=` / `silent=`
 *    field. The Win simulator and WS63 hardware have different ambient noise
 *    floors and mic gains, so any fixed RMS threshold would be wrong on one.
 *    Callers judge the raw values (rmsL/rmsR/dc/zeros/nodata) against their own
 *    platform's known-good baseline.
 *  - No per-frame flip logging (which would require a threshold to define a
 *    state change). Only periodic summaries.
 */
#ifndef CONVAI_AUDIO_DIAG_H
#define CONVAI_AUDIO_DIAG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — avoids pulling audio_service.h into this header. */
struct AudioService;

/**
 * Process one captured frame's diagnostics.
 *
 *   audio    : AudioService (used only to probe vad_detect; may be NULL →
 *              vad stats stay 0, RMS/zero/dc still computed).
 *   planar   : planar stereo buffer [L(n) .. R(n) ..] from capture_one_frame.
 *              R is read for rmsR. On Win R is all-zero by construction.
 *   n        : samples per channel (frame_count). Must be >= 1.
 *   read_ok  : 1 if audio_read returned >0 this frame, 0 if <=0 (nodata).
 *
 * Emits one summary line every DIAG_SUMMARY_EVERY frames. The wall-clock
 * interval depends on the real frame rate (differs Win vs WS63); see the
 * define in convai_audio_diag.c. Pure read-only: never modifies planar or any
 * encode/send state.
 */
void audio_diag_update(struct AudioService *audio,
                       const int16_t *planar, size_t n, int read_ok);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_AUDIO_DIAG_H */
