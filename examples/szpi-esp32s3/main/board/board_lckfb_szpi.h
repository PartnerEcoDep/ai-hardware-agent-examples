/**
 * @file board_lckfb_szpi.h
 * @brief 立创·实战派 ESP32-S3 开发板 GPIO 引脚定义
 *
 * 主控: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB Octal PSRAM)
 *
 * 外设清单:
 *   - 音频: ES8311 (DAC) + ES7210 (ADC, 3路MIC, 支持AEC)
 *   - IO扩展: PCA9557 @ 0x19 (控制 LCD_CS / PA_EN / DVP_PWDN)
 *   - LCD: ST7789 320x240 SPI (CS 经 PCA9557)
 *   - 触摸: FT6336 @ I2C (用户板为 FT6336, 非旧参考的 FT5x06)
 *   - 摄像头: GC0308 (DVP 接口)
 *   - 姿态: QMI8658 @ 0x6A (I2C)
 *   - LED: GPIO48
 *   - 按键: GPIO0 (BOOT)
 */

#ifndef BOARD_LCKFB_SZPI_H
#define BOARD_LCKFB_SZPI_H

#include "driver/gpio.h"

/* ===================================================================
 *  I2C 总线 (音频 Codec + 触摸屏 + PCA9557 + QMI8658)
 * =================================================================== */
#define BOARD_I2C_PORT           I2C_NUM_1
#define BOARD_I2C_SDA_PIN        GPIO_NUM_1
#define BOARD_I2C_SCL_PIN        GPIO_NUM_2
#define BOARD_I2C_CLK_SPEED      400000

/* ===================================================================
 *  PCA9557 IO 扩展器 (I2C 地址 0x19)
 *
 *  映射:
 *    bit 0 → LCD_CS    (0=选中, 1=释放)
 *    bit 1 → PA_EN     (0=关闭, 1=开启功放)
 *    bit 2 → DVP_PWDN  (0=摄像头工作, 1=摄像头休眠)
 *    bit 3-7 → NC
 * =================================================================== */
#define PCA9557_I2C_ADDR        0x19
#define PCA9557_BIT_LCD_CS      0
#define PCA9557_BIT_PA_EN       1
#define PCA9557_BIT_DVP_PWDN    2

/* ===================================================================
 *  I2S 音频接口
 *
 *  采样率: 输入 8000Hz / 输出 8000Hz (与 ConvAI G.711A 内容 1:1 对齐)
 *  ES7210: 3路 MIC 输入 (MIC1/MIC2 用于立体声, MIC3 回采 DAC 做 AEC)
 *  ES8311: DAC 音频输出
 * =================================================================== */
#define AUDIO_I2S_MCLK_PIN       GPIO_NUM_38
#define AUDIO_I2S_WS_PIN         GPIO_NUM_13
#define AUDIO_I2S_BCLK_PIN       GPIO_NUM_14
#define AUDIO_I2S_DIN_PIN        GPIO_NUM_12     /* I2S 数据输入 (麦克风) */
#define AUDIO_I2S_DOUT_PIN       GPIO_NUM_45     /* I2S 数据输出 (扬声器) */

#define AUDIO_SAMPLE_RATE        8000
#define AUDIO_BITS_PER_SAMPLE    16
#define AUDIO_CHANNELS           1               /* 单声道 */

/* ES8311 Codec I2C 地址 */
#define ES8311_I2C_ADDR          0x18

/* ES7210 ADC I2C 地址 */
#define ES7210_I2C_ADDR          0x41            /* 7-bit: 0x82 >> 1 = 0x41 */

/* ===================================================================
 *  LCD 显示 (ST7789 320x240, SPI3)
 *
 *  CS 通过 PCA9557 bit0 控制, DC 直连 GPIO39
 *  背光: GPIO42 (LEDC PWM)
 * =================================================================== */
#define LCD_SPI_HOST             SPI3_HOST
#define LCD_MOSI_PIN             GPIO_NUM_40
#define LCD_CLK_PIN              GPIO_NUM_41
#define LCD_DC_PIN               GPIO_NUM_39
/* CS 不直连 GPIO, 通过 PCA9557 bit0 */
#define LCD_CS_VIA_PCA9557       1

#define LCD_WIDTH                320
#define LCD_HEIGHT               240
#define LCD_PIXEL_CLOCK_HZ       (80 * 1000 * 1000)  /* 80 MHz, 与官方 LCKFB 例程一致 */
#define LCD_BACKLIGHT_PIN        GPIO_NUM_42
#define LCD_BACKLIGHT_INVERT     true
#define LCD_SPI_MODE             2                  /* ST7789 Mode 2 (CPOL=1, CPHA=0), 与官方例程一致 */

/* ===================================================================
 *  触摸屏 (FT5x06, I2C)
 * =================================================================== */
#define TOUCH_I2C_ADDR           0x38

/* ===================================================================
 *  LED & 按键
 * =================================================================== */
#define BOARD_LED_GPIO           GPIO_NUM_48     /* 蓝色 LED */
#define BOARD_BOOT_BUTTON_GPIO   GPIO_NUM_0      /* BOOT 按键 (低有效) */

/* ===================================================================
 *  音频功放 (NS4150B, 由 PCA9557 bit1 控制)
 * =================================================================== */
#define AUDIO_PA_CONTROL_METHOD  2               /* 0=无 PA, 1=GPIO直连, 2=PCA9557 */

/* ===================================================================
 *  调试串口
 * =================================================================== */
#define BOARD_UART_PORT          UART_NUM_0
#define BOARD_UART_TX_PIN        GPIO_NUM_43
#define BOARD_UART_RX_PIN        GPIO_NUM_44

#endif /* BOARD_LCKFB_SZPI_H */
