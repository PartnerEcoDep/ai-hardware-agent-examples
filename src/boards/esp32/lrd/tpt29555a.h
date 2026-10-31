/**
 * @file tpt29555a.h
 * @brief TPT29555A I2C 16-bit IO 扩展芯片驱动 (两个 8-bit 端口)
 *
 * 寄存器映射:
 *   INPUT0/1     0x00/0x01
 *   OUTPUT0/1    0x02/0x03
 *   POLARITY0/1  0x04/0x05
 *   CONFIG0/1    0x06/0x07   (bit=0 输出, bit=1 输入, 复位默认全输入)
 *
 * 仅实现本板需要的接口:
 *   - AMP_CTRL  (P1_0, 音频功放使能)
 *   - LCD_RST   (P1_5, LCD 复位, 低有效)
 *   - TP_RST    (P1_3, 触摸复位, 低有效)
 */

#ifndef TPT29555A_H
#define TPT29555A_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TPT29555A 句柄
 */
typedef struct {
    i2c_master_dev_handle_t dev;    /* I2C 设备句柄 */
    uint8_t                  output[2]; /* OUTPUT0/1 寄存器缓存 */
    uint8_t                  config[2]; /* CONFIG0/1 寄存器缓存 */
    int                      initialized;
} tpt29555a_t;

/**
 * @brief 初始化 TPT29555A
 *
 * @param dev      [out] 驱动句柄
 * @param bus      [in]  I2C 总线句柄
 * @param i2c_addr [in]  I2C 地址 (本板 0x22)
 * @return 0 成功, 非0 失败
 */
int tpt29555a_init(tpt29555a_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr);

/**
 * @brief 设置某个 IO 方向
 *
 * @param dev    驱动句柄
 * @param port   IO 端口 (0 或 1)
 * @param pin    IO 位 (0-7)
 * @param output true=输出, false=输入
 * @return 0 成功
 */
int tpt29555a_set_direction(tpt29555a_t *dev, uint8_t port, uint8_t pin, bool output);

/**
 * @brief 设置某个 IO 输出电平 (需先设为输出方向)
 *
 * @param dev   驱动句柄
 * @param port  IO 端口 (0 或 1)
 * @param pin   IO 位 (0-7)
 * @param level false=低, true=高
 * @return 0 成功
 */
int tpt29555a_set_output(tpt29555a_t *dev, uint8_t port, uint8_t pin, bool level);

#ifdef __cplusplus
}
#endif

#endif /* TPT29555A_H */
