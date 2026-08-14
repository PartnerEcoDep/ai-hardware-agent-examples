/**
 * @file audio_codec_lckfb.c
 * @brief 立创实战派板载音频初始化 (基于 esp_codec_dev 官方组件)
 *
 * 驱动链:
 *   I2S0 (std TX 立体声) → ES8311 (DAC) → LOUT/ROUT → NS4150B (PA) → 喇叭
 *   MIC1/MIC2/MIC3/MIC4 → ES7210 (ADC TDM) → I2S0 TDM RX
 *
 * PA_EN 由 PCA9557 bit1 控制 (走 pa_enable_cb), 不走 es8311 pa_pin。
 * ES7210 MIC3 是 ES8311 输出的回采 (AEC 参考), 跟 ES8311 同步。
 *
 * 之前手写寄存器, 调了多次都不出声。改用乐鑫官方 esp_codec_dev 组件
 * (xiaozhi-esp32 验证过的同一套驱动), 避免寄存器值/时序的微妙差异。
 */

#include "audio_codec_lckfb.h"
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/* I2S TDM 模式 (RX 用 4-slot TDM 采 ES7210) */
#include "driver/i2s_tdm.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "es8311_codec.h"
#include "es7210_adc.h"

#define TAG "audio_lckfb"

/* ---- 内部硬件句柄 (本文件私有, 外部只能通过 audio_lckfb_*() 访问) ---- */
struct audio_lckfb_s {
    i2s_chan_handle_t tx_chan;     /* 立体声标准 I2S (→ ES8311) */
    i2s_chan_handle_t rx_chan;     /* TDM 4-slot (← ES7210) */
    const audio_codec_data_if_t *data_if;
    esp_codec_dev_handle_t       spk;     /* ES8311 OUT */
    esp_codec_dev_handle_t       mic;     /* ES7210 IN */
    void (*pa_enable)(int en, void *ctx);
    void *pa_ctx;
};

static struct audio_lckfb_s s_hw;

/* ===================================================================
 *  I2S 初始化 — TX std (立体声 16-bit) + RX TDM 4-slot 16-bit
 *  共用 I2S0, 一次 i2s_new_channel 同时创建 TX+RX
 * =================================================================== */
static int i2s_init(struct audio_lckfb_s *audio) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    chan_cfg.dma_desc_num  = 6;
    chan_cfg.dma_frame_num = 240;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &audio->tx_chan, &audio->rx_chan));

    /* TX: 标准立体声 (ES8311 DAC) */
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        AUDIO_BITS_PER_SAMPLE, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK_PIN,
            .bclk = AUDIO_I2S_BCLK_PIN,
            .ws   = AUDIO_I2S_WS_PIN,
            .dout = AUDIO_I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(audio->tx_chan, &std_cfg));

    /* RX: TDM 4-slot (ES7210 ADC, MIC1/MIC2/MIC3/MIC4)
     * 4 slot 保证 MCLK_256 整除 (256/64=4), BCLK=1.536MHz, MCLK=6.144MHz
     * ES7210 TDM 时隙顺序: MIC1, MIC3, MIC2, MIC4 (实测播放时 slot1 幅值飙升) */
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz  = AUDIO_SAMPLE_RATE,
            .clk_src         = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple   = I2S_MCLK_MULTIPLE_256,
            .bclk_div        = 8,
        },
        .slot_cfg = {
            .data_bit_width = AUDIO_BITS_PER_SAMPLE,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 |
                                               I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width       = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol         = false,
            .bit_shift      = true,
            .left_align     = false,
            .big_endian     = false,
            .bit_order_lsb  = false,
            .skip_mask      = false,
            .total_slot     = 4,
        },
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK_PIN,
            .bclk = AUDIO_I2S_BCLK_PIN,
            .ws   = AUDIO_I2S_WS_PIN,
            .dout = I2S_GPIO_UNUSED,
            .din  = AUDIO_I2S_DIN_PIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(audio->rx_chan, &tdm_cfg));

    ESP_LOGI(TAG, "I2S0: TX=std stereo, RX=TDM 4-slot, Fs=%d, %d-bit",
             AUDIO_SAMPLE_RATE, AUDIO_BITS_PER_SAMPLE);
    return 0;
}

/* ===================================================================
 *  esp_codec_dev 初始化: ES8311 (DAC) + ES7210 (ADC) 共用 I2S0
 * =================================================================== */
static int esp_codec_dev_init(struct audio_lckfb_s *audio, i2c_master_bus_handle_t i2c_bus) {
    /* 1. I2S → data_if (esp_codec_dev 包装 I2S channel handles) */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_NUM_0,
        .rx_handle = audio->rx_chan,
        .tx_handle = audio->tx_chan,
    };
    audio->data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (audio->data_if == NULL) {
        ESP_LOGE(TAG, "audio_codec_new_i2s_data failed");
        return -1;
    }

    /* 2. I2C bus → ctrl_if for ES8311 (8-bit 地址: 7-bit << 1) */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = BOARD_I2C_PORT,
        .addr       = (ES8311_I2C_ADDR << 1),
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (ctrl_if == NULL) {
        ESP_LOGE(TAG, "audio_codec_new_i2c_ctrl(ES8311) failed");
        return -1;
    }

    /* 2b. I2C ctrl_if for ES7210 (I2C addr 0x41) */
    audio_codec_i2c_cfg_t es7210_i2c_cfg = {
        .port       = BOARD_I2C_PORT,
        .addr       = (ES7210_I2C_ADDR << 1),
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *es7210_ctrl_if = audio_codec_new_i2c_ctrl(&es7210_i2c_cfg);
    if (es7210_ctrl_if == NULL) {
        ESP_LOGE(TAG, "audio_codec_new_i2c_ctrl(ES7210) failed");
        return -1;
    }

    /* 3. ES8311 codec_if (DAC) — pa_pin = -1 跳过 es8311 内部 PA 控制 */
    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if       = ctrl_if,
        .gpio_if       = audio_codec_new_gpio(),
        .codec_mode    = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin        = -1,
        .pa_reverted   = false,
        .master_mode   = false,
        .use_mclk      = true,
        .digital_mic   = false,
        .invert_mclk   = false,
        .invert_sclk   = false,
        .hw_gain       = { .pa_voltage = 0, .codec_dac_voltage = 0 },
        .no_dac_ref    = false,
        .mclk_div      = 256,
    };
    const audio_codec_if_t *es8311_if = es8311_codec_new(&es8311_cfg);
    if (es8311_if == NULL) {
        ESP_LOGE(TAG, "es8311_codec_new failed");
        return -1;
    }

    /* 4. 包装成 OUT handle (DAC) */
    esp_codec_dev_cfg_t out_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = es8311_if,
        .data_if  = audio->data_if,
    };
    audio->spk = esp_codec_dev_new(&out_cfg);
    if (audio->spk == NULL) {
        ESP_LOGE(TAG, "esp_codec_dev_new(OUT) failed");
        return -1;
    }

    /* 5. 打开 ES8311 DAC: 配置采样率 / 位宽 / 通道, 启用 DAC 输出 */
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
        .channel         = 2,
        .channel_mask    = 0,
        .sample_rate     = AUDIO_SAMPLE_RATE,
        .mclk_multiple   = 0,
    };
    if (esp_codec_dev_open(audio->spk, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_open(spk) failed");
        return -1;
    }
    esp_codec_dev_set_out_vol(audio->spk, 70);
    ESP_LOGI(TAG, "esp_codec_dev: ES8311 DAC open ok, Fs=%d ch=%d",
             AUDIO_SAMPLE_RATE, 2);

    /* 6. ES7210 codec_if (ADC) — 独立 I2C 地址 0x41 */
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if      = es7210_ctrl_if,
        .master_mode  = false,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
        .mclk_src     = ES7210_MCLK_FROM_PAD,
        .mclk_div     = 256,
    };
    const audio_codec_if_t *es7210_if = es7210_codec_new(&es7210_cfg);
    if (es7210_if == NULL) {
        ESP_LOGE(TAG, "es7210_codec_new failed");
        return -1;
    }

    /* 7. 包装成 IN handle (ADC) */
    esp_codec_dev_cfg_t in_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = es7210_if,
        .data_if  = audio->data_if,
    };
    audio->mic = esp_codec_dev_new(&in_cfg);
    if (audio->mic == NULL) {
        ESP_LOGE(TAG, "esp_codec_dev_new(IN) failed");
        return -1;
    }

    /* 8. 打开 ES7210 MIC: 配置采样率 */
    esp_codec_dev_sample_info_t mic_fs = {
        .bits_per_sample = AUDIO_BITS_PER_SAMPLE,
        .channel         = 4,
        .channel_mask    = 0,
        .sample_rate     = AUDIO_SAMPLE_RATE,
        .mclk_multiple   = 0,
    };
    if (esp_codec_dev_open(audio->mic, &mic_fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "esp_codec_dev_open(mic) failed");
        return -1;
    }

    /* 9. MIC 增益 (dB)。原 24dB 远距离人声幅值过低(日志 slot0(MIC1)≈763/32767)，
     *    提高到 36dB 增强远场拾音; 若出现削波/底噪过大可回调到 30dB。 */
    esp_codec_dev_set_in_gain(audio->mic, 36.0);

    ESP_LOGI(TAG, "esp_codec_dev: ES8311 (DAC) + ES7210 (ADC 4-mic) ready");
    return 0;
}

/* ===================================================================
 *  公共接口
 * =================================================================== */
int audio_lckfb_init(audio_lckfb_t *audio_pub,
                     i2c_master_bus_handle_t i2c_bus,
                     void (*pa_enable)(int en, void *ctx),
                     void *pa_cb_ctx) {
    if (audio_pub == NULL || i2c_bus == NULL) return -1;
    struct audio_lckfb_s *audio = audio_pub;
    memset(audio, 0, sizeof(*audio));
    audio->pa_enable = pa_enable;
    audio->pa_ctx    = pa_cb_ctx;

    if (i2s_init(audio) != 0) {
        ESP_LOGE(TAG, "I2S init failed");
        return -1;
    }
    if (esp_codec_dev_init(audio, i2c_bus) != 0) {
        ESP_LOGE(TAG, "esp_codec_dev init failed");
        i2s_del_channel(audio->tx_chan);
        i2s_del_channel(audio->rx_chan);
        return -1;
    }
    if (audio->pa_enable) audio->pa_enable(1, audio->pa_ctx);

    ESP_LOGI(TAG, "Audio subsystem initialized (esp_codec_dev, spk+mic ready)");
    return 0;
}

int audio_lckfb_capture(audio_lckfb_t *audio_pub, uint8_t *buf, size_t len, size_t *received) {
    if (audio_pub == NULL || buf == NULL) {
        if (received) *received = 0;
        return -1;
    }
    struct audio_lckfb_s *audio = audio_pub;
    if (audio->mic == NULL) {
        if (received) *received = 0;
        return 0;
    }
    /* esp_codec_dev_read 返回错误码 (0=成功), 成功即请求长度被填满。 */
    int r = esp_codec_dev_read(audio->mic, buf, (int)len);
    if (r != ESP_CODEC_DEV_OK) {
        if (received) *received = 0;
        return -1;
    }
    if (received) *received = len;
    return 0;
}

int audio_lckfb_playback(audio_lckfb_t *audio_pub, const uint8_t *buf, size_t len, size_t *sent) {
    if (audio_pub == NULL || buf == NULL) {
        if (sent) *sent = 0;
        return -1;
    }
    struct audio_lckfb_s *audio = audio_pub;
    if (audio->spk == NULL) {
        if (sent) *sent = 0;
        return 0;
    }
    /* esp_codec_dev_write 返回错误码 (0=成功), 成功即请求长度被写满。 */
    int w = esp_codec_dev_write(audio->spk, (void *)buf, (int)len);
    if (w != ESP_CODEC_DEV_OK) {
        if (sent) *sent = 0;
        return -1;
    }
    if (sent) *sent = len;
    return 0;
}

int audio_lckfb_set_volume(audio_lckfb_t *audio_pub, int vol) {
    if (audio_pub == NULL) return -1;
    struct audio_lckfb_s *audio = audio_pub;
    if (audio->spk == NULL) return -1;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    return (esp_codec_dev_set_out_vol(audio->spk, vol) == ESP_CODEC_DEV_OK) ? 0 : -1;
}

int audio_lckfb_set_mic_gain(audio_lckfb_t *audio_pub, int gain_db) {
    if (audio_pub == NULL) return -1;
    struct audio_lckfb_s *audio = audio_pub;
    if (audio->mic == NULL) return -1;
    if (gain_db < 0) gain_db = 0;
    if (gain_db > 45) gain_db = 45;
    return (esp_codec_dev_set_in_gain(audio->mic, (float)gain_db) == ESP_CODEC_DEV_OK) ? 0 : -1;
}

/* ===================================================================
 *  audio_codec_t 适配层 (工厂模式)
 * =================================================================== */
static esp_err_t lckfb_codec_init(audio_codec_t *self,
                                  i2c_master_bus_handle_t bus,
                                  void (*pa_enable)(int en, void *ctx),
                                  void *pa_ctx) {
    (void)self;
    return (audio_lckfb_init(&s_hw, bus, pa_enable, pa_ctx) == 0) ? ESP_OK : ESP_FAIL;
}

static esp_err_t lckfb_codec_set_volume(audio_codec_t *self, uint8_t pct) {
    (void)self;
    return (audio_lckfb_set_volume(&s_hw, (int)pct) == 0) ? ESP_OK : ESP_FAIL;
}

static int lckfb_codec_read(audio_codec_t *self, void *buf, int samples) {
    (void)self;
    if (buf == NULL || samples <= 0) return -1;
    size_t received = 0;
    if (audio_lckfb_capture(&s_hw, (uint8_t *)buf, (size_t)samples, &received) != 0) {
        return -1;
    }
    return (int)received;
}

static int lckfb_codec_write(audio_codec_t *self, const void *buf, int samples) {
    (void)self;
    if (buf == NULL || samples <= 0) return -1;
    size_t sent = 0;
    if (audio_lckfb_playback(&s_hw, (const uint8_t *)buf, (size_t)samples, &sent) != 0) {
        return -1;
    }
    return (int)sent;
}

audio_codec_t audio_codec_lckfb_szpi = {
    .name       = AUDIO_CODEC_LCKFB_SZPI_NAME,
    .init       = lckfb_codec_init,
    .set_volume = lckfb_codec_set_volume,
    .read       = lckfb_codec_read,
    .write      = lckfb_codec_write,
};

esp_err_t audio_codec_lckfb_register(void) {
    return audio_codec_factory_register(&audio_codec_lckfb_szpi);
}
