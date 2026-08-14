/**
 * @file audio_codec_lckfb.h
 * @brief 立创实战派板载音频初始化 (ES8311 DAC + ES7210 ADC + I2S)
 */

#ifndef AUDIO_CODEC_LCKFB_H
#define AUDIO_CODEC_LCKFB_H

#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "audio_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Name under which this board's codec is registered in the factory. */
#define AUDIO_CODEC_LCKFB_SZPI_NAME "lckfb-szpi"

/**
 * @brief 音频句柄
 *
 * 不透明结构, 字段全部在 audio_codec_lckfb.c 内定义.
 * 外部代码不能直接访问字段, 只能通过 audio_lckfb_*() 函数操作。
 */
typedef struct audio_lckfb_s audio_lckfb_t;

/* 音频回调: 数据就绪时调用 */
typedef int (*audio_playback_cb_t)(uint8_t *buf, size_t buf_size, size_t *filled);
typedef int (*audio_capture_cb_t)(const uint8_t *buf, size_t len);

/**
 * @brief 初始化 ES8311 + ES7210 + I2S
 *
 * @param audio     [out] 音频句柄
 * @param i2c_bus   [in]  I2C 总线
 * @param pa_enable [in] 功放使能回调 (通过 PCA9557), NULL 表示无 PA
 * @param pa_cb_ctx [in] 回调上下文
 * @return 0 成功
 */
int audio_lckfb_init(audio_lckfb_t *audio,
                     i2c_master_bus_handle_t i2c_bus,
                     void (*pa_enable)(int en, void *ctx),
                     void *pa_cb_ctx);

/**
 * @brief 从 I2S 读取麦克风数据 (非阻塞)
 *
 * @param audio 音频句柄
 * @param buf   缓冲区
 * @param len   请求长度 (字节)
 * @param received [out] 实际接收长度
 * @return 0 成功, <0 错误
 */
int audio_lckfb_capture(audio_lckfb_t *audio, uint8_t *buf, size_t len, size_t *received);

/**
 * @brief 向 I2S 写入扬声器数据 (非阻塞)
 *
 * @param audio 音频句柄
 * @param buf   数据
 * @param len   长度 (字节)
 * @param sent  [out] 实际发送长度
 * @return 0 成功, <0 错误
 */
int audio_lckfb_playback(audio_lckfb_t *audio, const uint8_t *buf, size_t len, size_t *sent);

/**
 * @brief 设置扬声器音量
 *
 * @param audio 音频句柄
 * @param vol 0-100
 * @return 0 成功
 */
int audio_lckfb_set_volume(audio_lckfb_t *audio, int vol);

/**
 * @brief 设置麦克风增益
 *
 * @param audio 音频句柄
 * @param gain_db 增益 (dB), 范围 -20 到 +30
 * @return 0 成功
 */
int audio_lckfb_set_mic_gain(audio_lckfb_t *audio, int gain_db);

/**
 * @brief 本板 codec 的抽象实例 (audio_codec_t 适配层)
 *
 * 硬件句柄由实现文件内部持有, 调用方只通过 audio_codec_t 的函数指针访问。
 */
extern audio_codec_t audio_codec_lckfb_szpi;

/**
 * @brief 将本板 codec 注册到 audio_codec 工厂
 * @return ESP_OK 成功
 */
esp_err_t audio_codec_lckfb_register(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_CODEC_LCKFB_H */
