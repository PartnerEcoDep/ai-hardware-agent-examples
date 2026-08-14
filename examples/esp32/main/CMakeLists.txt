# ESP32-S3 AI Hardware Agent — 主组件 CMakeLists.txt
# 板卡: 立创·实战派 LCKFB SZPI-ESP32-S3
#
# 源码按功能模块分目录存放:
#   board/   板级硬件 (I2C/ SPI/ LCD/ PCA9557/ 板载定义)
#   audio/   音频编解码抽象 + 板载实现 + 初始化
#   sdk/     SDK 对接 (引擎创建/事件回调 + 平台 HAL 分层)
#   sdk/platform/ ConvAI 平台 HAL (vtable 装配 + OSAL/NetAL/TLSAL/Misc)
#   wifi/    Wi-Fi 配网 (SoftAP+BLE 配网 + UI)
#   ui/      图形与 UI (LVGL 移植 / 旧 LCD UI / 聊天 UI / 面板注册表)
#   ui/widgets/ 聊天 UI 的 LVGL 控件 (状态可视化 / 顶栏 / 浮球)
#   voice/   音色描述 (配置 + 工厂接口)

idf_component_register(
    SRCS
        # ---- 应用入口与启动流程 ----
        "main.c"
        "app_state.c"
        "sntp_init.c"
        "button_handler.c"

        # ---- 板级硬件 (board/) ----
        "board/board_init.c"
        "board/lcd_init.c"
        "board/lcd_boot_diag.c"
        "board/pca9557.c"

        # ---- 音频 (audio/) ----
        "audio/audio_codec.c"
        "audio/audio_codec_lckfb.c"
        "audio/audio_init.c"

        # ---- SDK 对接 (sdk/) ----
        "sdk/sdk_init.c"

        # ---- SDK 平台 HAL (sdk/platform/) ----
        "sdk/platform/convai_platform_esp32.c"
        "sdk/platform/esp32_osal.c"
        "sdk/platform/esp32_netal.c"
        "sdk/platform/esp32_tlsal.c"
        "sdk/platform/esp32_misc.c"

        # ---- 网络配网 (wifi/) ----
        "wifi/wifi_provisioning.cpp"
        "wifi/wifi_prov_ui.c"

        # ---- 图形与 UI (ui/) ----
        "ui/lvgl_port.c"
        "ui/lcd_ui.c"
        "ui/ai_chat_ui.c"
        "ui/ui_panel.c"

        # ---- UI 控件 (ui/widgets/) ----
        "ui/widgets/top_bar.c"
        "ui/widgets/float_ball.c"
        "ui/widgets/state_viz.c"

        # ---- 音色 (voice/) ----
        "voice/voice_config.c"
        "voice/voice_factory.c"

        # ---- 自动生成的字体（勿手工编辑）----
        "lv_font_custom_cjk_14.c"
        "lv_font_custom_cjk_16.c"
    INCLUDE_DIRS
        "."
        "audio"
        "board"
        "sdk"
        "sdk/platform"
        "ui"
        "ui/widgets"
        "voice"
        "wifi"
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
