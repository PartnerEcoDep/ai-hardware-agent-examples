/**
 * @file tpt29555a.c
 * @brief TPT29555A I2C 16-bit IO 扩展芯片驱动实现
 */

#include "tpt29555a.h"
#include "esp_log.h"

#define TAG "tpt29555a"

/* TPT29555A 寄存器 (端口 1 = 端口 0 寄存器 + 1) */
#define TPT29555A_REG_INPUT0    0x00
#define TPT29555A_REG_OUTPUT0   0x02
#define TPT29555A_REG_POLARITY0 0x04
#define TPT29555A_REG_CONFIG0   0x06

int tpt29555a_init(tpt29555a_t *dev, i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
    if (dev == NULL || bus == NULL) return -1;

    /* 1. 添加 I2C 设备 */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = i2c_addr,
        .scl_speed_hz    = 100000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &dev->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device: %d", ret);
        return -1;
    }

    /* 2. 初始状态: 极性正常, 全部输出高, 全部配置为输入 (bit=1) */
    dev->output[0] = 0xFF;
    dev->output[1] = 0xFF;
    dev->config[0] = 0xFF;
    dev->config[1] = 0xFF;

    uint8_t regs[] = {
        TPT29555A_REG_POLARITY0,     0x00,
        TPT29555A_REG_POLARITY0 + 1, 0x00,
        TPT29555A_REG_OUTPUT0,       dev->output[0],
        TPT29555A_REG_OUTPUT0 + 1,   dev->output[1],
        TPT29555A_REG_CONFIG0,       dev->config[0],
        TPT29555A_REG_CONFIG0 + 1,   dev->config[1],
    };
    for (int i = 0; i < (int)sizeof(regs); i += 2) {
        ret = i2c_master_transmit(dev->dev, &regs[i], 2, -1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Reg 0x%02X write failed: %d", regs[i], ret);
            i2c_master_bus_rm_device(dev->dev);
            return -1;
        }
    }

    dev->initialized = 1;
    ESP_LOGI(TAG, "TPT29555A initialized (addr=0x%02X)", i2c_addr);
    return 0;
}

int tpt29555a_set_direction(tpt29555a_t *dev, uint8_t port, uint8_t pin, bool output) {
    if (dev == NULL || !dev->initialized || port > 1 || pin > 7) return -1;

    if (output) {
        dev->config[port] &= (uint8_t)~(1U << pin);   /* bit=0 → 输出 */
    } else {
        dev->config[port] |= (uint8_t)(1U << pin);    /* bit=1 → 输入 */
    }
    esp_err_t ret = i2c_master_transmit(dev->dev,
        (uint8_t[]){TPT29555A_REG_CONFIG0 + port, dev->config[port]}, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Config port%d pin%d write failed: %d", port, pin, ret);
        return -1;
    }
    return 0;
}

int tpt29555a_set_output(tpt29555a_t *dev, uint8_t port, uint8_t pin, bool level) {
    if (dev == NULL || !dev->initialized || port > 1 || pin > 7) return -1;

    if (level) {
        dev->output[port] |= (uint8_t)(1U << pin);
    } else {
        dev->output[port] &= (uint8_t)~(1U << pin);
    }
    esp_err_t ret = i2c_master_transmit(dev->dev,
        (uint8_t[]){TPT29555A_REG_OUTPUT0 + port, dev->output[port]}, 2, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Output port%d pin%d write failed: %d", port, pin, ret);
        return -1;
    }
    return 0;
}
