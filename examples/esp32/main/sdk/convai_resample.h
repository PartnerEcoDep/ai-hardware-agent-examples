/**
 * @file convai_resample.h
 * @brief 24kHz <-> 8kHz 整数倍重采样 (3:1 / 1:3), 单声道 16-bit PCM.
 *
 * ConvAI SDK/云端协议按 8kHz 单声道 16-bit 处理 G.711A (对齐 goldieos:
 * 上行/下行均为 8k)。而本板 ES7210 采集与 ES8311 播放硬件是 24kHz。
 * 故:
 *   上行: ES7210 采 24k → 降采样到 8k → G711A 编码 → 发送
 *   下行: SDK 下发 8k G711A → 解码成 8k PCM → 升采样到 24k → 播放
 * 采样率比为 3:1 (整数倍), 无需通用滤波器。
 */

#ifndef CONVAI_RESAMPLE_H
#define CONVAI_RESAMPLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 24kHz -> 8kHz 3:1 降采样 (单声道 16-bit).
 * @param in         输入 24k 单声道 PCM.
 * @param in_frames  输入帧数 (须为 3 的倍数, 尾部不足 3 帧被丢弃).
 * @param out        输出 8k 单声道 PCM (需 in_frames/3 帧容量).
 * @param out_cap    输出容量 (帧).
 * @param out_frames [out] 实际输出帧数.
 * @return 0 成功, -1 参数错误或容量不足.
 */
int convai_resample_down_3x(const int16_t *in, size_t in_frames,
                            int16_t *out, size_t out_cap,
                            size_t *out_frames);

/**
 * @brief 8kHz -> 24kHz 1:3 升采样 (单声道 16-bit, 线性插值).
 * @param in         输入 8k 单声道 PCM.
 * @param in_frames  输入帧数.
 * @param out        输出 24k 单声道 PCM (需 in_frames*3 帧容量).
 * @param out_cap    输出容量 (帧).
 * @param out_frames [out] 实际输出帧数.
 * @return 0 成功, -1 参数错误或容量不足.
 */
int convai_resample_up_3x(const int16_t *in, size_t in_frames,
                          int16_t *out, size_t out_cap,
                          size_t *out_frames);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_RESAMPLE_H */
