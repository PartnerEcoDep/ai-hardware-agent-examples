/**
 * @file pca9557.c
 * @brief PCA9557 I2C IO 扩展芯片驱动实现
 */

#include "pca9557.h"
#include "esp_log.h"

#define TAG "pca9557"

/* PCA9557 寄存器 */
#define PCA9557_REG_INPUT   0x00
#define PCA9557_REG_OUTPUT  0x01
#define PCA9557_REG_POL     0x02
#define PCA9557_REG_CONFIG  0x03

int pca9557_init(pca9557_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
    if (dev == NULL || bus == NULL) return -1;

    /* 1. 添加 I2C 设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = i2c_addr,
        .scl_speed_hz    = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &dev->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %d", ret);
        return -1;
    }

    /* 2. 配置: bit0/1/2 为输出 (写 0), bit3-7 为输入 (写 1)
     *    → CONFIG = 0b11111000 = 0xF8 */
    uint8_t config = 0xF8;
    ret = i2c_master_transmit(dev->dev, (uint8_t[]){PCA9557_REG_CONFIG, config}, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Config write failed: %d", ret);
        i2c_master_bus_rm_device(dev->dev);
        return -1;
    }

    /* 3. 初始输出状态: LCD_CS=高(释放), PA_EN=低(关闭), DVP_PWDN=低(工作)
     *    → OUTPUT = 0b00000011 = 0x03  (bit1=PA_EN=0, bit0=LCD_CS=1 → 实际是bit1=1, bit0=1? wait)
     *
     *    等等, 重新理清: 
     *    LCD_CS 低有效: 选中=bit0=0, 释放=bit0=1  → 初始释放: bit0=1
     *    PA_EN 高有效: 开启=bit1=1, 关闭=bit1=0   → 初始关闭: bit1=0
     *    综合: 0b00000011 = 0x03 (bit0=1, bit1=1) ← 不对!
     *    0b00000001 = 0x01 (bit0=1=LCD_CS释放, bit1=0=PA_EN关闭)
     */
    uint8_t init_out = 0x01;  /* bit0=1 (LCD_CS释放), bit1=0 (PA_EN关闭) */
    dev->state = init_out;
    ret = i2c_master_transmit(dev->dev, (uint8_t[]){PCA9557_REG_OUTPUT, init_out}, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Output write failed: %d", ret);
        i2c_master_bus_rm_device(dev->dev);
        return -1;
    }

    dev->initialized = 1;
    ESP_LOGI(TAG, "PCA9557 initialized (addr=0x%02X)", i2c_addr);
    return 0;
}

int pca9557_set_output(pca9557_t *dev, uint8_t bit, uint8_t level) {
    if (dev == NULL || !dev->initialized || bit > 7) return -1;

    if (level) dev->state |=  (1 << bit);
    else       dev->state &= ~(1 << bit);

    esp_err_t ret = i2c_master_transmit(dev->dev,
        (uint8_t[]){PCA9557_REG_OUTPUT, dev->state}, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set output bit%d=%d failed: %d", bit, level, ret);
        return -1;
    }
    return 0;
}
