/**
 * @file convai_resample.c
 * @brief 24kHz <-> 8kHz 整数倍重采样 (3:1 / 1:3), 单声道 16-bit PCM.
 */

#include "convai_resample.h"

/* 24kHz -> 8kHz: 3 输入帧 → 1 输出帧。
 * 取 3 点平均作为输出, 起到简单抗混叠(低通)作用, 避免降采样折叠噪声。 */
int convai_resample_down_3x(const int16_t *in, size_t in_frames,
                            int16_t *out, size_t out_cap,
                            size_t *out_frames)
{
    if (in == NULL || out == NULL || out_frames == NULL) return -1;
    size_t out_needed = in_frames / 3;
    if (out_cap < out_needed) return -1;

    size_t k = 0;
    for (size_t i = 0; i + 2 < in_frames; i += 3) {
        int32_t sum = (int32_t)in[i] + (int32_t)in[i + 1] + (int32_t)in[i + 2];
        /* 四舍五入到最近整数 */
        out[k++] = (int16_t)((sum + (sum >= 0 ? 1 : -1)) / 3);
    }
    *out_frames = k;
    return 0;
}

/* 8kHz -> 24kHz: 1 输入帧 → 3 输出帧 (线性插值)。
 * 相邻输入 a[i], a[i+1] 之间插 2 点 (1/3, 2/3), 使波形平滑。 */
int convai_resample_up_3x(const int16_t *in, size_t in_frames,
                          int16_t *out, size_t out_cap,
                          size_t *out_frames)
{
    if (in == NULL || out == NULL || out_frames == NULL) return -1;
    if (in_frames == 0) { *out_frames = 0; return 0; }
    size_t out_needed = in_frames * 3;
    if (out_cap < out_needed) return -1;

    size_t k = 0;
    for (size_t i = 0; i < in_frames; i++) {
        int32_t cur = in[i];
        int32_t nxt = (i + 1 < in_frames) ? in[i + 1] : in[i];
        out[k++] = (int16_t)cur;                       /* t=0   */
        out[k++] = (int16_t)((2 * cur + nxt) / 3);     /* t=1/3 */
        out[k++] = (int16_t)((cur + 2 * nxt) / 3);     /* t=2/3 */
    }
    *out_frames = k;
    return 0;
}
