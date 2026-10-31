# ConvAI SDK 26.8.0

本工程固定使用用户指定的 [ConvAI SDK 26.8.0 发布包](https://download.huaweicloud-koophone.com/convai-sdk/convai_sdk_26.8.0.tar)。

- 原始包：`convai_sdk_26.8.0.tar`
- SHA-256：`76ca0bf0e4ebbd352f9eb66c679746101165b0e1e343c8cbbd8ddde1636b1d7a`
- 解压目录：`convai-sdk-26.8.0/`，头文件和静态库必须成套更新。
- ESP32-S3 链接 `libs/esp32s3/libconvai_sdk.a`。
- WS63 独立固件链接 `libs/ws63/libconvai_sdk.a`；Windows 模拟器链接 `libs/windows/libconvai_sdk.a`。

二进制和头文件保留发布包原样。该版本内部已有 `sdk_cJSON_*` 命名空间，仍有部分 `cJSON_*` 外部引用；ESP32 显式依赖 IDF cJSON 组件，WS63 使用 BSP 提供的 cJSON，不使用全局 `--allow-multiple-definition`。

Wi-Fi 配网代码来自原示例中的 esp-wifi-connect 3.2.2，其上游地址、版本和 MIT 许可声明保留在组件的 `idf_component.yml` 中。ESP 板级驱动与 HAL 来自两份 ESP32-S3 示例；WS63 HAL 移植自 GoldieOS 示例，所需运行依赖已固定归档到本工程，不再要求完整外部 BSP。Brookesia 仅用作分层设计参考，没有引入其运行时或源码。

## LVGL 9.5.0 共用界面依赖

`lvgl-9.5.0-source.tar.gz` 是源码归档，来自本项目 IDF 锁定并下载的 `lvgl/lvgl 9.5.0` 组件，包含 src、lvgl.h、lvgl_private.h、lv_version.h 与原始 LICENCE.txt（MIT）。上游：https://github.com/lvgl/lvgl 。SHA-256：`acf605dc3b67f030eb3bd3f34967ff5ac115264b82fc03799f45231664387d9f`。

ESP32 继续通过 dependencies.lock 获取同版组件；WS63 CMake 校验并解压该归档，使用 src/platforms/goldieos/components/ui/lv_conf.h 构建。两平台编译同一份应用页面，使用 Montserrat 14 和一致主题参数。该源码归档是 WS63 构建必需文件，上库；压缩文件不计算物理行数。

WS63 开放热点 ABI 参考海思官方头文件：
https://gitee.com/HiSpark/fbb_ws63/blob/master/src/include/middleware/services/wifi/wifi_hotspot_config.h
以及同目录 wifi_device_config.h、wifi_hotspot.h。应用只声明所需的结构布局和两个函数；没有引入整套海思 Wi-Fi 源码。

GoldieOS 固定运行归档、逐文件校验与工具包边界见 [goldieos/README.md](goldieos/README.md)。
