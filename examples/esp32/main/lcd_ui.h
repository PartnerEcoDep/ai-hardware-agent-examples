/**
 * @file lcd_ui.h
 * @brief 基于 ST7789 framebuffer 的简易文字渲染模块
 *
 * 320×240 RGB565 全屏 framebuffer（PSRAM），内嵌 8×16 ASCII 字体。
 * 色值在 framebuffer 中存大端字节序，与现有 lcd_fill 一致。
 * 通过 extern 引用 main.c 中的 g_lcd_panel。
 */

#ifndef LCD_UI_H
#define LCD_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_UI_WIDTH   320
#define LCD_UI_HEIGHT  240

/**
 * @brief 初始化 UI 模块：分配 PSRAM framebuffer，初始化为黑色
 */
void lcd_ui_init(void);

/**
 * @brief 清屏为纯色
 * @param color RGB565 色值（主机字节序，内部转为大端）
 */
void lcd_ui_clear(uint16_t color);

/**
 * @brief 在指定坐标画一个 8×16 字符
 * @param x  左上角 X 坐标（0~319）
 * @param y  左上角 Y 坐标（0~239）
 * @param c  ASCII 字符（0x20-0x7F）
 * @param fg 前景色 RGB565
 * @param bg 背景色 RGB565
 */
void lcd_ui_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg);

/**
 * @brief 在指定坐标画一个字符串（不自动换行）
 * @param x    左上角 X 坐标
 * @param y    左上角 Y 坐标
 * @param str  ASCII 字符串
 * @param fg   前景色 RGB565
 * @param bg   背景色 RGB565
 */
void lcd_ui_draw_string(int x, int y, const char *str, uint16_t fg, uint16_t bg);

/**
 * @brief 画填充矩形
 * @param x     左上角 X
 * @param y     左上角 Y
 * @param w     宽度
 * @param h     高度
 * @param color RGB565 色值
 */
void lcd_ui_draw_rect(int x, int y, int w, int h, uint16_t color);

/**
 * @brief 在指定 Y 坐标水平居中画字符串
 * @param y    Y 坐标
 * @param str  ASCII 字符串
 * @param fg   前景色
 * @param bg   背景色
 */
void lcd_ui_center_text(int y, const char *str, uint16_t fg, uint16_t bg);

/**
 * @brief 将 framebuffer 一次性刷新到 LCD
 *
 * 调用 esp_lcd_panel_draw_bitmap 将整个 framebuffer 发送到 ST7789。
 * 内部处理大端字节序转换。
 */
void lcd_ui_flush(void);

/* ===================================================================
 *  增强渲染 API（新增）
 * =================================================================== */

/**
 * @brief 缩放绘制单个字符（8×16 字体按 scale 倍数放大）
 * @param x     左上角 X
 * @param y     左上角 Y
 * @param c     ASCII 字符
 * @param scale 缩放倍数 1~4（截断至合法范围）
 * @param fg    前景色 RGB565
 * @param bg    背景色 RGB565
 */
void lcd_ui_draw_char_scaled(int x, int y, char c, int scale,
                             uint16_t fg, uint16_t bg);

/**
 * @brief 缩放绘制字符串
 * @param x     左上角 X
 * @param y     左上角 Y
 * @param str   ASCII 字符串
 * @param scale 缩放倍数
 * @param fg    前景色
 * @param bg    背景色
 */
void lcd_ui_draw_string_scaled(int x, int y, const char *str, int scale,
                               uint16_t fg, uint16_t bg);

/**
 * @brief 水平居中缩放字符串
 * @param y     Y 坐标
 * @param str   ASCII 字符串
 * @param scale 缩放倍数
 * @param fg    前景色
 * @param bg    背景色
 */
void lcd_ui_center_text_scaled(int y, const char *str, int scale,
                               uint16_t fg, uint16_t bg);

/**
 * @brief 绘制简化 WiFi 信号图标（全部用 draw_rect 实现）
 *
 * 底部圆点（3×3）+ 上方 3 条横向色块，宽度递增模拟信号弧线。
 *
 * @param x            左上角 X
 * @param y            左上角 Y
 * @param size         图标尺寸（约 size×size 像素）
 * @param signal_level 0=无信号(灰), 1=弱(红), 2=中(黄), 3=强(绿)
 */
void lcd_ui_draw_wifi_icon(int x, int y, int size, int signal_level);

/**
 * @brief 画填充圆角矩形
 * @param x     左上角 X
 * @param y     左上角 Y
 * @param w     宽度
 * @param h     高度
 * @param r     圆角半径（像素）
 * @param color RGB565 色值
 */
void lcd_ui_draw_rounded_rect(int x, int y, int w, int h, int r, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* LCD_UI_H */
