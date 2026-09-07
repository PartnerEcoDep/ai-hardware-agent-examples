/**
 * @file pca9557.h
 * @brief PCA9557 I2C 8-bit IO 扩展芯片驱动
 *
 * 仅实现本板需要的 3 个位:
 *   bit 0: LCD_CS
 *   bit 1: PA_EN (音频功放使能)
 *   bit 2: DVP_PWDN (摄像头休眠, 低有效)
 */

#ifndef PCA9557_H
#define PCA9557_H

#include <stdint.h>
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PCA9557 句柄
 */
typedef struct {
    i2c_master_dev_handle_t dev;   /* I2C 设备句柄 */
    uint8_t                  state; /* 当前输出寄存器值 */
    int                      initialized;
} pca9557_t;

/**
 * @brief 初始化 PCA9557
 *
 * @param dev      [out] 驱动句柄
 * @param bus      [in]  I2C 总线句柄
 * @param i2c_addr [in]  I2C 地址 (本板 0x19)
 * @return 0 成功, 非0 失败
 */
int pca9557_init(pca9557_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr);

/**
 * @brief 设置某个 IO 输出电平
 *
 * @param dev 驱动句柄
 * @param bit IO 位 (0-7)
 * @param level 0=低, 1=高
 * @return 0 成功
 */
int pca9557_set_output(pca9557_t *dev, uint8_t bit, uint8_t level);

/**
 * @brief 使能/禁用 LCD 显示 (CS 引脚)
 */
static inline int pca9557_lcd_cs(pca9557_t *dev, uint8_t enable) {
    /* CS 低有效 → bit0=0 选中, bit0=1 释放 */
    return pca9557_set_output(dev, 0, enable ? 0 : 1);
}

/**
 * @brief 使能/禁用音频功放
 */
static inline int pca9557_pa_en(pca9557_t *dev, uint8_t enable) {
    return pca9557_set_output(dev, 1, enable ? 1 : 0);
}

#ifdef __cplusplus
}
#endif

#endif /* PCA9557_H */
