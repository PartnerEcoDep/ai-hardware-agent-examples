/**
 * @file convai_audio_diag.c
 * @brief Read-only capture diagnostics implementation. See convai_audio_diag.h
 *        for the metric definitions and design rationale.
 *
 * Pure signal metrics + vad_detect probe. No voice/silent classification, no
 * thresholds that judge content — only raw measured values + a DC-bias flag
 * (a hardware-fault signature, not a content judgment).
 */
#include "convai_audio_diag.h"
#include "audio_service.h"   /* AudioService, vad_detect */

#include <stdio.h>

/* Print a summary every N captured frames. Actual wall-clock interval depends
 * on the real frame rate (set by audio_read's return size + sample rate), which
 * differs between Win and WS63 and isn't known precisely here. At ~50fps this
 * is ~0.5s; at ~25fps ~1s. Tune up if too chatty, down if you need finer detail. */
#define DIAG_SUMMARY_EVERY  25
#define DIAG_DC_WARN_THRESH 2000  /* |mean sample| above this = DC bias (hardware fault) */
#define DIAG_VAD_SAMPLES    160   /* 20ms @ 8kHz — a valid webrtc_vad frame length
                                   * (10/20/30ms only; the full 40ms frame is rejected) */

/* vad_detect return interpretation (webrtc_vad convention). */
enum { DIAG_VAD_ERR = -1, DIAG_VAD_NO = 0, DIAG_VAD_YES = 1 };

void audio_diag_update(struct AudioService *audio,
                       const int16_t *planar, size_t n, int read_ok)
{
    static unsigned int since_summary = 0;
    /* raw counts over the current interval */
    static unsigned int nodata = 0, allzero = 0, dcwarn = 0;
    static unsigned int rmsL_min = ~0u, rmsL_max = 0;
    static unsigned int rmsR_min = ~0u, rmsR_max = 0;
    /* vad_detect return tally (only counts frames where vad was actually called) */
    static unsigned int vad_yes = 0, vad_no = 0, vad_err = 0;

    if (!read_ok) {
        nodata++;
    } else if (n > 0 && planar != NULL) {
        /* Single pass over both channels: accumulate L abs-sum (rmsL), L signed-sum
         * (dc), L zero-count, and R abs-sum (rmsR) in one loop — previously three
         * separate helper calls each walking L, plus one for R (4 passes total). */
        const int16_t *L = planar;
        const int16_t *R = planar + n;   /* planar layout: [L(n).. R(n)..] */
        unsigned long absL = 0, absR = 0;
        long          sumL = 0;
        unsigned int  zeros = 0;
        for (size_t i = 0; i < n; i++) {
            int16_t sl = L[i];
            int16_t sr = R[i];
            long vl = sl;
            long vr = sr;
            absL  += (unsigned long)(vl < 0 ? -vl : vl);
            absR  += (unsigned long)(vr < 0 ? -vr : vr);
            sumL  += vl;
            if (sl == 0) zeros++;
        }
        unsigned int rL = (unsigned int)(absL / n);
        unsigned int rR = (unsigned int)(absR / n);
        if (rL < rmsL_min) rmsL_min = rL;  if (rL > rmsL_max) rmsL_max = rL;
        if (rR < rmsR_min) rmsR_min = rR;  if (rR > rmsR_max) rmsR_max = rR;

        if (zeros == n) allzero++;          /* entire L channel zero */

        int dc = (int)(sumL / (long)n);
        if (dc < -DIAG_DC_WARN_THRESH || dc > DIAG_DC_WARN_THRESH) dcwarn++;

        /* vad_detect probe: 160-sample (20ms @ 8kHz) left-channel frame.
         * NULL/unassigned → skip (no err tally, so err reflects ONLY real
         * calls returning -1, not "interface absent"). */
        if (audio && audio->vad_detect && n >= DIAG_VAD_SAMPLES) {
            int r = audio->vad_detect((short *)L, DIAG_VAD_SAMPLES);
            if (r == DIAG_VAD_YES)      vad_yes++;
            else if (r == DIAG_VAD_NO)  vad_no++;
            else                        vad_err++;
        }
    }

    if (++since_summary >= DIAG_SUMMARY_EVERY) {
        printf("[convai_diag] summary: nodata=%u allzero=%u dcwarn=%u "
               "rmsL[%u..%u] rmsR[%u..%u] vad[y=%u n=%u err=%u]\n",
               nodata, allzero, dcwarn,
               rmsL_min == ~0u ? 0 : rmsL_min, rmsL_max,
               rmsR_min == ~0u ? 0 : rmsR_min, rmsR_max,
               vad_yes, vad_no, vad_err);
        nodata = allzero = dcwarn = 0;
        vad_yes = vad_no = vad_err = 0;
        rmsL_min = rmsR_min = ~0u;  rmsL_max = rmsR_max = 0;
        since_summary = 0;
    }
}
