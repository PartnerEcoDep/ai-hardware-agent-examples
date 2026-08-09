# ESP32-S3 AI Hardware Agent — 主组件 CMakeLists.txt
# 板卡: 立创·实战派 LCKFB SZPI-ESP32-S3

idf_component_register(
    SRCS
        "main.c"
        "convai_platform_esp32.c"
        "pca9557.c"
        "audio_codec_lckfb.c"
        "wifi_provisioning.cpp"
        "lcd_ui.c"
        "wifi_prov_ui.c"
        "ai_chat_ui.c"
        "lvgl_port.c"
        "voice_config.c"
    INCLUDE_DIRS
        "."
        "${CMAKE_SOURCE_DIR}/../../include/convai"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        esp_http_server
        esp_https_ota
        driver
        esp_driver_i2s          # I2S 音频接口
        esp_driver_ledc         # LEDC PWM (背光)
        esp_driver_spi          # SPI (LCD)
        esp_driver_i2c          # I2C (Codec / PCA9557)
        esp_lcd                 # ST7789 LCD 驱动
        lvgl                    # LVGL 图形库
        esp_lvgl_port           # LVGL ESP-IDF 移植层
        mbedtls
        lwip
        esp-wifi-connect
        convai_sdk_stub          # ★ STUB — 正式 SDK 到位后改回 convai_sdk
)
