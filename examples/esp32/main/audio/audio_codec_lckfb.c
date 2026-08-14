/**
 * @file audio_codec_lckfb.c
 * @brief 立创实战派板载音频初始化实现
 *
 * ES8311 (DAC 输出):
 *   I2C addr = 0x18, 连接到扬声器 (经 NS4150B 功放, PA_EN 由 PCA9557 控制)
 *
 * ES7210 (ADC 输入):
 *   I2C addr = 0x41 (7-bit), 3路 MIC: MIC1/MIC2 立体声 + MIC3 回采 DAC
 *
 * I2S 接口:
 *   MCLK = GPIO38 (主时钟, 由 ESP32 输出)
 *   BCLK = GPIO14 (位时钟)
 *   WS   = GPIO13 (左右声道时钟)
 *   DIN  = GPIO12 (数据输入 / 麦克风)
 *   DOUT = GPIO45 (数据输出 / 扬声器)
 *
 * 参考: xiaozhi-esp32 BoxAudioCodec
 */

#include "audio_codec_lckfb.h"
#include "board_lckfb_szpi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "audio_lckfb"

/* ===================================================================
 *  ES8311 寄存器定义 (DAC)
 * =================================================================== */
#define ES8311_RESET_REG00          0x00
#define ES8311_CLK_MANAGER_REG01    0x01
#define ES8311_CLK_MANAGER_REG02    0x02
#define ES8311_CLK_MANAGER_REG03    0x03
#define ES8311_CLK_MANAGER_REG04    0x04
#define ES8311_SYSTEM_REG0D         0x0D
#define ES8311_SYSTEM_REG0E         0x0E
#define ES8311_GPIO_REG44           0x44
#define ES8311_SDPIN_REG09          0x09
#define ES8311_DAC_REG31            0x31
#define ES8311_DAC_REG32            0x32
#define ES8311_DAC_REG33            0x33
#define ES8311_DAC_REG37            0x37
#define ES8311_GP_REG45             0x45

/* ===================================================================
 *  ES7210 寄存器定义 (ADC)
 * =================================================================== */
#define ES7210_RESET_REG00          0x00
#define ES7210_CLOCK_OFF2_REG01     0x01
#define ES7210_MICBIAS_REG02        0x02
#define ES7210_MIC1_GAIN_REG44      0x44  /* MIC1 增益 */
#define ES7210_MIC2_GAIN_REG45      0x45  /* MIC2 增益 */
#define ES7210_MIC3_GAIN_REG46      0x46  /* MIC3 增益 (AEC 参考通道) */
#define ES7210_ADC34_GAIN_REG22     0x22
#define ES7210_SDP_REG11            0x11
#define ES7210_SDP_REG12            0x12
#define ES7210_CLOCK_ON_REG40       0x40
#define ES7210_CLOCK_ON_REG41       0x41
#define ES7210_SYSTEM_REG42         0x42
#define ES7210_SYSTEM_REG43         0x43
#define ES7210_ADC_AUTO_CTL_REG71   0x71

/* ---- 内部辅助: I2C 写寄存器 ---- */
static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, 2, 100);
}

/* ===================================================================
 *  ES8311 初始化 (DAC 音频输出)
 * =================================================================== */
static int es8311_init(audio_lckfb_t *audio, i2c_master_bus_handle_t bus) {
    /* 添加 I2C 设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ES8311_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &audio->es8311_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 device add failed: %d", ret);
        return -1;
    }

    /* ---- 复位 ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_RESET_REG00, 0x1F);  /* 复位所有寄存器 */
    vTaskDelay(pdMS_TO_TICKS(30));
    i2c_write_reg(audio->es8311_dev, ES8311_RESET_REG00, 0x00);  /* 释放复位 */

    /* ---- 时钟配置 (MCLK=24.576MHz → 1024×Fs @ Fs=24000Hz?) ----
     *  实际配置根据采样率计算，这里使用 I2S 标准 256×Fs
     */
    i2c_write_reg(audio->es8311_dev, ES8311_CLK_MANAGER_REG01, 0x30);  /* MCLK/4 */
    i2c_write_reg(audio->es8311_dev, ES8311_CLK_MANAGER_REG02, 0x00);  /* BCLK/LRCK 分频 */
    i2c_write_reg(audio->es8311_dev, ES8311_CLK_MANAGER_REG03, 0x10);  /* 接 0x02=0x00: DIV=256 */
    i2c_write_reg(audio->es8311_dev, ES8311_CLK_MANAGER_REG04, 0x10);  /* LRCK divider */
    i2c_write_reg(audio->es8311_dev, ES8311_CLK_MANAGER_REG04, 0b00000000);  /* ADCDIV=0, DACDIV=0 */

    /* ---- I2S 格式: 16-bit, I2S standard ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_SDPIN_REG09, 0x00);  /* 16-bit, I2S */

    /* ---- 系统配置 ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_SYSTEM_REG0D, 0x01);  /* 启动从模式 */
    i2c_write_reg(audio->es8311_dev, ES8311_SYSTEM_REG0E, 0x02);  /* 使能 DAC */

    /* ---- DAC 配置 ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG31, 0x40);  /* ramp rate */
    i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG32, 0x00);  /* DAC 静音 */
    i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG37, 0x08);  /* SDOUT 使能 */

    /* ---- 音量设置 ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG32, 0x00);  /* 0dB */
    i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG33, 0x00);  /* L/R 同音量 */

    /* ---- 上电 ---- */
    i2c_write_reg(audio->es8311_dev, ES8311_GPIO_REG44, 0x00);
    i2c_write_reg(audio->es8311_dev, ES8311_GP_REG45, 0x00);

    ESP_LOGI(TAG, "ES8311 initialized");
    return 0;
}

/* ===================================================================
 *  ES7210 初始化 (ADC 音频输入, 3路 MIC)
 * =================================================================== */
static int es7210_init(audio_lckfb_t *audio, i2c_master_bus_handle_t bus) {
    /* 添加 I2C 设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ES7210_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &audio->es7210_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ES7210 device add failed: %d", ret);
        return -1;
    }

    /* ---- 复位 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_RESET_REG00, 0xFF);
    vTaskDelay(pdMS_TO_TICKS(30));
    i2c_write_reg(audio->es7210_dev, ES7210_RESET_REG00, 0x00);

    /* 地址再检查 (0x41 或 0x40) */
    i2c_write_reg(audio->es7210_dev, ES7210_CLOCK_OFF2_REG01, 0x00);

    /* ---- 时钟设置 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_CLOCK_OFF2_REG01, 0b01100000);  /* MCC=0, OFF2=0b11 */

    /* ---- MIC 偏置 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_MICBIAS_REG02, 0b00100001);

    /* ---- MIC 增益 (24dB) ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_MIC1_GAIN_REG44, 0x18);  /* MIC1: 24dB */
    i2c_write_reg(audio->es7210_dev, ES7210_MIC2_GAIN_REG45, 0x18);  /* MIC2: 24dB */
    i2c_write_reg(audio->es7210_dev, ES7210_MIC3_GAIN_REG46, 0x00);  /* MIC3: 0dB (AEC参考) */

    /* ---- ADC 增益 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_ADC34_GAIN_REG22, 0x00);

    /* ---- I2S 格式: 24-bit, I2S standard ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_SDP_REG11, 0x02);
    i2c_write_reg(audio->es7210_dev, ES7210_SDP_REG12, 0x03);

    /* ---- 上电 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_SYSTEM_REG42, 0x00);
    i2c_write_reg(audio->es7210_dev, ES7210_SYSTEM_REG43, 0x20);
    i2c_write_reg(audio->es7210_dev, ES7210_ADC_AUTO_CTL_REG71, 0b01010101);  /* 所有通道自动控制 */

    /* ---- 使能所有通道 ---- */
    i2c_write_reg(audio->es7210_dev, ES7210_CLOCK_ON_REG40, 0x07);   /* 使能 MIC1/2/3 */
    i2c_write_reg(audio->es7210_dev, ES7210_CLOCK_ON_REG41, 0x00);

    ESP_LOGI(TAG, "ES7210 initialized");
    return 0;
}

/* ===================================================================
 *  I2S 初始化
 * =================================================================== */
static int i2s_init(audio_lckfb_t *audio) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    /* TX 通道 (扬声器) */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &audio->tx_chan, NULL));

    /* RX 通道 (麦克风) */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &audio->rx_chan));

    /* 标准 I2S 配置 (共用 BCLK/WS) */
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        AUDIO_BITS_PER_SAMPLE,
                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = AUDIO_I2S_MCLK_PIN,
            .bclk = AUDIO_I2S_BCLK_PIN,
            .ws   = AUDIO_I2S_WS_PIN,
            .dout = AUDIO_I2S_DOUT_PIN,
            .din  = AUDIO_I2S_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(audio->tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(audio->rx_chan, &std_cfg));

    ESP_LOGI(TAG, "I2S initialized (Fs=%d, %d-bit, MCLK/BCLK/WS/DIN/DOUT)",
             AUDIO_SAMPLE_RATE, AUDIO_BITS_PER_SAMPLE);
    return 0;
}

/* ===================================================================
 *  公共接口
 * =================================================================== */

int audio_lckfb_init(audio_lckfb_t *audio,
                     i2c_master_bus_handle_t i2c_bus,
                     void (*pa_enable)(int en, void *ctx),
                     void *pa_cb_ctx) {
    if (audio == NULL || i2c_bus == NULL) return -1;
    memset(audio, 0, sizeof(*audio));

    /* 1. I2S 先初始化 (提供 MCLK 给 Codec) */
    if (i2s_init(audio) != 0) goto fail_i2s;

    /* 2. ES8311 DAC */
    if (es8311_init(audio, i2c_bus) != 0) goto fail_es8311;

    /* 3. ES7210 ADC */
    if (es7210_init(audio, i2c_bus) != 0) goto fail_es7210;

    /* 4. 使能 I2S 通道 */
    ESP_ERROR_CHECK(i2s_channel_enable(audio->tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(audio->rx_chan));

    /* 5. 开启功放 */
    if (pa_enable) pa_enable(1, pa_cb_ctx);

    ESP_LOGI(TAG, "Audio subsystem initialized");
    return 0;

fail_es7210:
    if (audio->es8311_dev) i2c_master_bus_rm_device(audio->es8311_dev);
fail_es8311:
    if (audio->tx_chan) i2s_del_channel(audio->tx_chan);
    if (audio->rx_chan) i2s_del_channel(audio->rx_chan);
fail_i2s:
    return -1;
}

int audio_lckfb_capture(audio_lckfb_t *audio, uint8_t *buf, size_t len, size_t *received) {
    if (audio == NULL || buf == NULL) return -1;
    esp_err_t ret = i2s_channel_read(audio->rx_chan, buf, len, received, 0);
    if (ret == ESP_ERR_TIMEOUT) { if (received) *received = 0; return 0; }
    return (ret == ESP_OK) ? 0 : -1;
}

int audio_lckfb_playback(audio_lckfb_t *audio, const uint8_t *buf, size_t len, size_t *sent) {
    if (audio == NULL || buf == NULL) return -1;
    esp_err_t ret = i2s_channel_write(audio->tx_chan, buf, len, sent, 0);
    if (ret == ESP_ERR_TIMEOUT) { if (sent) *sent = 0; return 0; }
    return (ret == ESP_OK) ? 0 : -1;
}

int audio_lckfb_set_volume(audio_lckfb_t *audio, int vol) {
    if (audio == NULL || audio->es8311_dev == NULL) return -1;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;

    /* ES8311 音量范围 0(静音) ~ 32(最大), 映射 0-100 → 0-32 */
    uint8_t reg_val = (uint8_t)(vol * 32 / 100);
    return i2c_write_reg(audio->es8311_dev, ES8311_DAC_REG32, reg_val);
}

int audio_lckfb_set_mic_gain(audio_lckfb_t *audio, int gain_db) {
    if (audio == NULL || audio->es7210_dev == NULL) return -1;
    /* gain_db 映射到 ES7210 增益寄存器 (0x00=0dB ~ 0x2A=42dB) */
    if (gain_db < 0) gain_db = 0;
    if (gain_db > 42) gain_db = 42;
    uint8_t reg_val = (uint8_t)gain_db;
    int ret = i2c_write_reg(audio->es7210_dev, ES7210_MIC1_GAIN_REG44, reg_val);
    if (ret != ESP_OK) return -1;
    return i2c_write_reg(audio->es7210_dev, ES7210_MIC2_GAIN_REG45, reg_val);
}

/* ===================================================================
 *  audio_codec_t 适配层 (工厂模式)
 *
 *  上层只持有 audio_codec_t*, 硬件句柄 s_hw 由本文件独占,
 *  换板时新增一个同样实现 audio_codec_t 的文件即可。
 * =================================================================== */

/* 本 codec 独占的硬件句柄 */
static audio_lckfb_t s_hw;

static esp_err_t lckfb_codec_init(audio_codec_t *self,
                                  i2c_master_bus_handle_t bus,
                                  void (*pa_enable)(int en, void *ctx),
                                  void *pa_ctx) {
    (void)self;
    return (audio_lckfb_init(&s_hw, bus, pa_enable, pa_ctx) == 0) ? ESP_OK
                                                                  : ESP_FAIL;
}

static esp_err_t lckfb_codec_set_volume(audio_codec_t *self, uint8_t pct) {
    (void)self;
    return (audio_lckfb_set_volume(&s_hw, (int)pct) == 0) ? ESP_OK : ESP_FAIL;
}

static int lckfb_codec_read(audio_codec_t *self, void *buf, int samples) {
    (void)self;
    if (buf == NULL || samples <= 0) return -1;
    size_t received = 0;
    if (audio_lckfb_capture(&s_hw, (uint8_t *)buf, (size_t)samples,
                            &received) != 0) {
        return -1;
    }
    return (int)received;
}

static int lckfb_codec_write(audio_codec_t *self, const void *buf,
                             int samples) {
    (void)self;
    if (buf == NULL || samples <= 0) return -1;
    size_t sent = 0;
    if (audio_lckfb_playback(&s_hw, (const uint8_t *)buf, (size_t)samples,
                             &sent) != 0) {
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
