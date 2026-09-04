/**
 * @file board_lckfb_szpi.h
 * @brief 板级 GPIO 引脚定义 (移植自 LCKFB 实战派 → laiwfs300 板)
 *
 * 主控: ESP32-S3 (8MB Flash, 8MB Quad PSRAM)
 *
 * 外设清单:
 *   - 音频: ES8311 (DAC) + ES7210 (ADC, 4路MIC, 支持AEC)
 *   - IO扩展: TPT29555A @ 0x22 (AMP_CTRL / LCD_RST / TP_RST / TP_INT)
 *   - LCD: ST7789 240x320 SPI (CS 直连 GPIO10, RST 经 IOEX)
 *   - 触摸: CST836U @ I2C 0x15
 *   - LED: GPIO46 (RGB LED, 可能不可用)
 *   - 按键: 无物理按键, 对话启停走触摸屏按钮
 */

#ifndef BOARD_LCKFB_SZPI_H
#define BOARD_LCKFB_SZPI_H

#include "driver/gpio.h"

/* ===================================================================
 *  I2C 总线 (音频 Codec + 触摸屏 + TPT29555A)
 * =================================================================== */
#define BOARD_I2C_PORT           I2C_NUM_0
#define BOARD_I2C_SDA_PIN        GPIO_NUM_48
#define BOARD_I2C_SCL_PIN        GPIO_NUM_47
#define BOARD_I2C_CLK_SPEED      100000

/* ===================================================================
 *  TPT29555A IO 扩展器 (I2C 地址 0x22, 双 8-bit 端口)
 *
 *  映射:
 *    P1_0 → AMP_CTRL (音频功放使能, 高有效)
 *    P1_3 → TP_RST   (触摸复位, 低有效)
 *    P1_4 → TP_INT   (触摸中断)
 *    P1_5 → LCD_RST  (LCD 复位, 低有效)
 * =================================================================== */
#define IOEX_I2C_ADDR           0x22
#define IOEX_AMP_CTRL_PORT      1
#define IOEX_AMP_CTRL_PIN       0
#define IOEX_TP_RST_PORT        1
#define IOEX_TP_RST_PIN         3
#define IOEX_TP_INT_PORT        1
#define IOEX_TP_INT_PIN         4
#define IOEX_LCD_RST_PORT       1
#define IOEX_LCD_RST_PIN        5

/* ===================================================================
 *  I2S 音频接口
 *
 *  采样率: 输入 8000Hz / 输出 8000Hz (与 ConvAI G.711A 内容 1:1 对齐)
 *  ES7210: 4路 MIC 输入 (TDM 4-slot)
 *  ES8311: DAC 音频输出
 * =================================================================== */
#define AUDIO_I2S_MCLK_PIN       GPIO_NUM_42
#define AUDIO_I2S_WS_PIN         GPIO_NUM_39
#define AUDIO_I2S_BCLK_PIN       GPIO_NUM_41
#define AUDIO_I2S_DIN_PIN        GPIO_NUM_40     /* I2S 数据输入 (麦克风) */
#define AUDIO_I2S_DOUT_PIN       GPIO_NUM_38     /* I2S 数据输出 (扬声器) */

#define AUDIO_SAMPLE_RATE        8000
#define AUDIO_BITS_PER_SAMPLE    16
#define AUDIO_CHANNELS           1               /* 单声道 */

/* ES8311 Codec I2C 地址 */
#define ES8311_I2C_ADDR          0x18

/* ES7210 ADC I2C 地址 (7-bit) */
#define ES7210_I2C_ADDR          0x40

/* ===================================================================
 *  LCD 显示 (ST7789 240x320, SPI2)
 *
 *  CS 直连 GPIO10, DC 直连 GPIO16, RST 经 TPT29555A P1_5
 *  背光: GPIO17 (LEDC PWM, 高有效)
 *  面板原生 240x320, 初始化时 swap_xy + mirror 旋转为 320x240 横屏
 * =================================================================== */
#define LCD_SPI_HOST             SPI2_HOST
#define LCD_MOSI_PIN             GPIO_NUM_13
#define LCD_CLK_PIN              GPIO_NUM_12
#define LCD_DC_PIN               GPIO_NUM_16
#define LCD_CS_PIN               GPIO_NUM_10
#define LCD_CS_VIA_PCA9557       0

#define LCD_WIDTH                320
#define LCD_HEIGHT               240
/* SPI 时钟 10MHz → 40MHz: 全屏传输从 ~123ms 降到 ~31ms, 4 倍带宽。
 * 解决动画密集时 SPI 队列排空慢、draw_bitmap 阻塞拖死 lv_timer_handler
 * (LVGL 锁被长期持有 → UI 卡死) 的根因。ST7789 支持 40MHz+。 */
#define LCD_PIXEL_CLOCK_HZ       (40 * 1000 * 1000)  /* 40 MHz */
#define LCD_BACKLIGHT_PIN        GPIO_NUM_17
#define LCD_BACKLIGHT_INVERT     false
#define LCD_SPI_MODE             0                  /* ST7789 Mode 0 */

/* ===================================================================
 *  触摸屏 (CST836U, I2C)
 * =================================================================== */
#define TOUCH_I2C_ADDR           0x15

/* ===================================================================
 *  LED & 按键
 * =================================================================== */
#define BOARD_LED_GPIO           GPIO_NUM_46     /* RGB LED (可能不可用) */
#define BOARD_BOOT_BUTTON_GPIO   GPIO_NUM_0      /* 未使用 (laiwfs300 无物理按键) */

/* ===================================================================
 *  音频功放 (由 TPT29555A AMP_CTRL P1_0 控制)
 * =================================================================== */
#define AUDIO_PA_CONTROL_METHOD  3               /* 3=TPT29555A IOEX */

/* ===================================================================
 *  调试串口
 * =================================================================== */
#define BOARD_UART_PORT          UART_NUM_0
#define BOARD_UART_TX_PIN        GPIO_NUM_43
#define BOARD_UART_RX_PIN        GPIO_NUM_44

#endif /* BOARD_LCKFB_SZPI_H */
