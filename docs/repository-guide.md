# 上库清单与逐文件说明

更新日期：2026-10-20。以当前 unified-app 实际文件为准，路径相对于该目录；板级头文件已分别移入 src/boards/esp32/include 与 src/boards/ws63/include。本清单重整了此前分散的新增文件附录。当前媒体及 SDK 适配仍含故障定位用诊断输出，不能视为已完成诊断清理。尚未执行暂存、提交或推送。

## 当前验证状态与提交范围

| 目标 | 用户最新验收 |
|---|---|
| Hoperun WS63 单板 | 长回答仍有卡顿、Speaking 长停顿及满队列 Error -3。最新 TLS/socket/媒体交付诊断版已编译、14/14 主机测试通过；按用户要求暂缓烧写，尚未实机验证，21/22 的启动 set_rc 仍待补采 |
| 利尔达 ESP32-S3 | 用户确认最新 src 迁移版显示、配网、对话、重复启停和对话中进入配网五项正常 |
| 实战派 ESP32-S3 | 用户确认最新 src 迁移版显示、配网、对话、重复启停和对话中进入配网五项正常 |
| Hoperun Windows 模拟器 | 最新 src 迁移版本已启动，进程与窗口响应检查通过；界面和对话待用户验收，不提供 Wi-Fi 配网 |

最新 src 迁移版三款单板均已获用户五项功能验收。第四批公共邮箱代码保持已回退，历史构建、烧写及故障记录保留；润和历史偶发问题尚未完成根因修复与专项验证，不将本次验收扩大为长期稳定性结论。此结论不涵盖长期压力、异常断电、所有手机或其他 BSP；详见 [验证记录](validation.md)。

润和长回答专项结论与原始 GoldieOS 示例差异见 [音频定位记录](hoperun-audio-investigation.md)。播放循环间隔包含阻塞写入，不可直接当作线程饥饿时长；最新增加被动交付诊断，保留 21/22 优先级及其他播放、poll 和 TLS 行为，未认定根因修复。

提交下面列出的源码、配置模板、构建入口、测试、文档和必要依赖。SDK 三个平台库及 LVGL 源码归档为构建依赖；原始 SDK tar 可选归档。GoldieOS 运行归档、逐文件清单及构建适配上库，不再依赖外部示例工程。编译器和链接/签名工具包放忽略的 manifests/缓存，可本地导入或配置下载地址；详见 third_party/goldieos/README.md。

## 不提交的本地文件

- 构建目录 build*/（skills/build-* 工作流源码除外）：固件、EXE、合并镜像、生成头文件、CMake 缓存、烧写工具副本和日志。build-host 与 build-verification 已删除，再生成时仍忽略。
- managed_components/、__pycache__/：下载或运行缓存。
- config/local.defaults、config/ws63.local.h、sdkconfig、sdkconfig.old：实际鉴权与本地配置；只提交模板。
- 含本地鉴权的固件和 EXE 不作为普通源码上库。

目录迁移后 应用入口位于 src/platforms/esp_idf/entry、构建注册位于 src/platforms/esp_idf/cmake/components、平台实现位于 components，WS63 单板/模拟器共用代码位于 src/platforms/goldieos，说明文档位于 docs。2026-10-10 实机验收属于迁移前版本；第三批四目标构建及待实测项另见验证记录。

## 平台与板级边界

ws63 表示芯片平台、工具链及 SDK ABI；hoperun 表示润和板型。src/boards/ws63/hoperun 保存电源/引脚、显示传输、音频布局、BSP 路径；src/platforms/goldieos 保存当前 GoldieOS 后端运行时、网络、TLS/SDK、存储和应用接入。WS63_BOARD 选择板级实现；新板使用兼容服务 ABI 时主要扩展 boards，不同 RTOS/驱动体系仍需后端适配。

src/boards/esp32/common/esp32s3 为两块 ESP 板的共用实现，不代表 WS63 也使用它。Windows 模拟器使用 Windows SDK 与虚拟设备，复用公共应用和页面，不执行 WS63 固件。

## 行数统计口径与目录汇总

文本物理行数包含空行、注释、配置及文档；二进制/归档 .a、.tar、.tar.gz 不计行数。目录递归统计，父子目录有重叠，不可相加。只统计本清单候选文件。

| 目录 | 文本文件数（递归） | 行数（递归） |
|---|---:|---:|
| `./（项目合计）` | 227 | 27161 |
| `config/` | 7 | 66 |
| `docs/` | 23 | 4701 |
| `docs/training/` | 9 | 1146 |
| `skills/` | 16 | 602 |
| `skills/_shared/` | 8 | 466 |
| `skills/_shared/manifests/` | 1 | 38 |
| `skills/_shared/references/` | 3 | 59 |
| `skills/_shared/scripts/` | 3 | 335 |
| `skills/_shared/tests/` | 1 | 34 |
| `skills/build-hoperun-simulator/` | 2 | 34 |
| `skills/build-hoperun-simulator/agents/` | 1 | 4 |
| `skills/build-hoperun-ws63/` | 2 | 34 |
| `skills/build-hoperun-ws63/agents/` | 1 | 4 |
| `skills/build-lrd/` | 2 | 34 |
| `skills/build-lrd/agents/` | 1 | 4 |
| `skills/build-szpi/` | 2 | 34 |
| `skills/build-szpi/agents/` | 1 | 4 |
| `src/` | 148 | 10803 |
| `src/application/` | 25 | 1041 |
| `src/application/core/` | 4 | 286 |
| `src/application/core/include/` | 2 | 109 |
| `src/application/services/` | 15 | 554 |
| `src/application/services/cloud/` | 2 | 170 |
| `src/application/services/cloud/include/` | 1 | 25 |
| `src/application/services/media/` | 5 | 144 |
| `src/application/services/media/include/` | 3 | 58 |
| `src/application/services/network/` | 8 | 240 |
| `src/application/services/network/include/` | 4 | 89 |
| `src/application/ui/` | 6 | 201 |
| `src/application/ui/include/` | 3 | 42 |
| `src/application/ui/web/` | 2 | 104 |
| `src/boards/` | 27 | 1539 |
| `src/boards/esp32/` | 22 | 1434 |
| `src/boards/esp32/common/` | 5 | 469 |
| `src/boards/esp32/common/esp32s3/` | 5 | 469 |
| `src/boards/esp32/common/esp32s3/include/` | 1 | 10 |
| `src/boards/esp32/include/` | 1 | 21 |
| `src/boards/esp32/lrd/` | 8 | 493 |
| `src/boards/esp32/szpi/` | 8 | 451 |
| `src/boards/ws63/` | 5 | 105 |
| `src/boards/ws63/hoperun/` | 4 | 93 |
| `src/boards/ws63/include/` | 1 | 12 |
| `src/platforms/` | 96 | 8223 |
| `src/platforms/esp_idf/` | 47 | 3864 |
| `src/platforms/esp_idf/cmake/` | 17 | 166 |
| `src/platforms/esp_idf/cmake/components/` | 16 | 163 |
| `src/platforms/esp_idf/cmake/components/ai_cloud/` | 1 | 3 |
| `src/platforms/esp_idf/cmake/components/ai_core/` | 1 | 2 |
| `src/platforms/esp_idf/cmake/components/ai_media/` | 1 | 3 |
| `src/platforms/esp_idf/cmake/components/ai_network/` | 1 | 2 |
| `src/platforms/esp_idf/cmake/components/ai_ui/` | 1 | 3 |
| `src/platforms/esp_idf/cmake/components/board/` | 1 | 15 |
| `src/platforms/esp_idf/cmake/components/convai_sdk/` | 1 | 12 |
| `src/platforms/esp_idf/cmake/components/esp-wifi-connect/` | 2 | 62 |
| `src/platforms/esp_idf/cmake/components/esp_config/` | 1 | 6 |
| `src/platforms/esp_idf/cmake/components/esp_media/` | 1 | 6 |
| `src/platforms/esp_idf/cmake/components/esp_network/` | 1 | 5 |
| `src/platforms/esp_idf/cmake/components/esp_sdk/` | 1 | 7 |
| `src/platforms/esp_idf/cmake/components/main/` | 3 | 37 |
| `src/platforms/esp_idf/components/` | 27 | 3567 |
| `src/platforms/esp_idf/components/config/` | 2 | 49 |
| `src/platforms/esp_idf/components/config/include/` | 1 | 11 |
| `src/platforms/esp_idf/components/convai/` | 8 | 1097 |
| `src/platforms/esp_idf/components/convai/include/` | 2 | 16 |
| `src/platforms/esp_idf/components/media/` | 2 | 188 |
| `src/platforms/esp_idf/components/media/include/` | 1 | 14 |
| `src/platforms/esp_idf/components/network/` | 14 | 2138 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/` | 12 | 2043 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/` | 6 | 375 |
| `src/platforms/esp_idf/components/network/include/` | 1 | 17 |
| `src/platforms/esp_idf/components/ui/` | 1 | 95 |
| `src/platforms/esp_idf/entry/` | 1 | 94 |
| `src/platforms/esp_idf/targets/` | 2 | 37 |
| `src/platforms/esp_idf/targets/esp32s3/` | 2 | 37 |
| `src/platforms/goldieos/` | 48 | 4328 |
| `src/platforms/goldieos/cmake/` | 13 | 1136 |
| `src/platforms/goldieos/cmake/toolchains/` | 2 | 115 |
| `src/platforms/goldieos/cmake/ws63-core/` | 1 | 23 |
| `src/platforms/goldieos/components/` | 29 | 2876 |
| `src/platforms/goldieos/components/config/` | 3 | 60 |
| `src/platforms/goldieos/components/config/include/` | 1 | 11 |
| `src/platforms/goldieos/components/convai/` | 5 | 1736 |
| `src/platforms/goldieos/components/convai/windows/` | 1 | 761 |
| `src/platforms/goldieos/components/convai/ws63/` | 3 | 968 |
| `src/platforms/goldieos/components/media/` | 3 | 316 |
| `src/platforms/goldieos/components/media/include/` | 1 | 15 |
| `src/platforms/goldieos/components/network/` | 10 | 572 |
| `src/platforms/goldieos/components/network/include/` | 1 | 16 |
| `src/platforms/goldieos/components/network/windows/` | 1 | 24 |
| `src/platforms/goldieos/components/network/ws63/` | 7 | 504 |
| `src/platforms/goldieos/components/runtime/` | 4 | 48 |
| `src/platforms/goldieos/components/runtime/include/` | 1 | 10 |
| `src/platforms/goldieos/components/runtime/windows/` | 1 | 17 |
| `src/platforms/goldieos/components/runtime/ws63/` | 1 | 18 |
| `src/platforms/goldieos/components/ui/` | 4 | 144 |
| `src/platforms/goldieos/components/ui/include/` | 1 | 13 |
| `src/platforms/goldieos/entry/` | 1 | 111 |
| `src/platforms/goldieos/include/` | 1 | 13 |
| `src/platforms/goldieos/targets/` | 3 | 104 |
| `src/platforms/goldieos/targets/windows/` | 2 | 54 |
| `src/platforms/goldieos/targets/ws63/` | 1 | 50 |
| `tests/` | 19 | 974 |
| `tests/stubs/` | 4 | 42 |
| `tests/stubs/goldie_media/` | 3 | 23 |
| `tests/stubs/goldie_media/lwip/` | 1 | 5 |
| `third_party/` | 8 | 9630 |
| `third_party/convai-sdk-26.8.0/` | 4 | 602 |
| `third_party/convai-sdk-26.8.0/include/` | 4 | 602 |
| `third_party/convai-sdk-26.8.0/include/convai/` | 4 | 602 |
| `third_party/goldieos/` | 3 | 9003 |
| `tools/` | 2 | 92 |

根目录直属文本文件：4 个，共 293 行；已包含在项目合计中。

## 根目录、文档与 ESP 应用入口

生态调研文档按现有工作区内容纳入清单；本轮不重新验证其外部调研结论。

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `.gitignore` | 24 | 排除构建目录、下载缓存和本地鉴权配置 |
| `docs/architecture.md` | 362 | 说明当前依赖方向及未改变范围 |
| `docs/architecture-walkthrough.md` | 314 | 基于源码梳理整体架构、四张流程/复用图、验证与规划边界及 14 页分享提纲 |
| `docs/hoperun-audio-investigation.md` | 184 | 保存单次预缓冲复测及原始 GoldieOS 对照，区分报错路径、优先级/播放/SDK 差异与待验证根因 |
| `docs/ai-hardware-ecosystem.md` | 338 | AI 硬件生态调研、产品对比与扩展建议 |
| `docs/changes-src-layout-2026-10-15.md` | 191 | src、application/core、platforms 与 convai 迁移的逐文件记录及验证 |
| `src/platforms/esp_idf/cmake/paths.cmake` | 3 | 为 ESP 组件扫描和配置提供工程根及源码根路径 |
| `docs/changes-shared-layout-2026-10-15.md` | 81 | 公共目录与平台入口迁移的逐文件修改、构建证据及产物哈希 |
| `docs/stage0-contracts.md` | 247 | 阶段 0 四目标能力基线、会话与测试协议草案、首批验收用例、待确认项和退出条件 |
| `docs/board-comparison.md` | 310 | 芯片平台、三款板级实现和 Hoperun Windows 模拟器对比及当前验收状态 |
| `CMakeLists.txt` | 20 | 轻量构建分发；默认 ESP32-S3，支持 GoldieOS WS63/Windows，阻止跨目标复用缓存 |
| `README.md` | 209 | 更新媒体契约归属 |
| `docs/repository-guide.md` | 550 | 同步文件用途及行数 |
| `docs/validation.md` | 831 | 记录测试、四目标构建及产物哈希 |
| `dependencies.lock` | 40 | 锁定组件版本和内容校验值；应与 manifest 一起提交 |
| `src/platforms/esp_idf/cmake/components/main/CMakeLists.txt` | 3 | 注册应用入口组件及其依赖 |
| `src/platforms/esp_idf/cmake/components/main/Kconfig.projbuild` | 28 | menuconfig 中的 Agent、鉴权、服务地址、音量与播放缓冲选项；默认鉴权值为空 |
| `src/platforms/esp_idf/entry/app_main.c` | 94 | ESP 启动及事件循环，使用 FreeRTOS 队列调用公共协调层 |
| `src/platforms/esp_idf/cmake/components/main/idf_component.yml` | 6 | 声明 ESP-IDF 版本范围及 cJSON、esp_codec_dev、LVGL 依赖 |

## 配置模板与板型配置

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `config/common.defaults` | 21 | 两块 ESP32-S3 共用的 IDF、内存、网络、LVGL 等默认配置 |
| `config/local.defaults.example` | 8 | 本地鉴权配置模板；复制为 `local.defaults` 后填写，真实文件不提交 |
| `config/lrd.defaults` | 3 | 利尔达的 8 MB Flash、Quad PSRAM 和分区表选择 |
| `config/lrd.partitions.csv` | 10 | 利尔达分区布局 |
| `config/szpi.defaults` | 3 | 实战派的 16 MB Flash、Octal PSRAM 和分区表选择 |
| `config/szpi.partitions.csv` | 10 | 实战派 NVS、OTA 数据、两个应用槽和 assets 分区布局 |
| `config/ws63.local.h.example` | 11 | WS63 私有鉴权、服务地址、CA 和播放缓冲模板 |

## 公共业务与服务

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/application/core/ai_app.c` | 96 | 对话状态机：启停、联网/断线、连接超时、错误及旧会话事件过滤 |
| `src/application/core/include/ai_app.h` | 62 | 对话状态、事件、界面快照、应用实例与注入操作的公共定义 |
| `src/application/services/cloud/ai_cloud.c` | 145 | ConvAI SDK 生命周期、状态/音频回调翻译、打断处理与停止同步 |
| `src/application/services/cloud/include/ai_cloud.h` | 25 | 引用媒体契约，删除重复归属的定义 |
| `src/application/services/media/ai_audio.c` | 48 | 音频格式转换与不同板型的 TDM 麦克风/参考声道选择 |
| `src/application/services/media/ai_playback.c` | 38 | 移除统计累加，保留缓冲和边界处理 |
| `src/application/services/media/include/ai_audio.h` | 20 | G711A 编解码、输入声道映射和输出双声道转换接口 |
| `src/application/services/media/include/ai_playback.h` | 19 | 移除队列统计类型及字段 |
| `src/application/services/network/ai_portal.c` | 29 | 三板共用的 DNS 报文解析、边界校验与应答生成。 |
| `src/application/services/network/ai_provision.c` | 60 | 纯 C 配网参数/索引、HTTP 请求头和存储 CRC/边界校验 |
| `src/application/services/network/include/ai_portal.h` | 19 | 无平台依赖的 C/C++ 接口，以及 HTTP 跳转状态和缓存策略。 |
| `src/application/services/network/include/ai_provision.h` | 27 | 跨平台配网校验 API 与保存网络快照结构 |

## ESP 板级接口及共用驱动

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/boards/esp32/common/esp32s3/audio_codec.h` | 15 | ESP 音频 Codec 操作接口；属于 ESP 板级代码，并非 WS63 公共抽象 |
| `src/boards/esp32/common/esp32s3/audio_codec_lckfb.c` | 372 | I2S、ES8311 DAC、ES7210 ADC 初始化、音量及 PCM 读写；名称沿用原代码，实际两块板共用 |
| `src/boards/esp32/common/esp32s3/audio_codec_lckfb.h` | 9 | 共用 ES8311/ES7210 Codec 实例声明 |
| `src/boards/esp32/common/esp32s3/board.c` | 63 | 公共板级组装、Codec 调用、触摸读取/坐标映射和按键读取 |
| `src/boards/esp32/include/board.h` | 21 | ESP 板型描述、初始化、音频读写及输入接口；不是跨平台统一 HAL |
| `src/boards/esp32/common/esp32s3/include/board_display.h` | 10 | ESP 屏幕句柄及显示分辨率接口，供 LVGL 端口使用 |

## 实战派板级

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/boards/esp32/szpi/board_init.c` | 49 | 实战派 I2C 与 PCA9557 初始化 |
| `src/boards/esp32/szpi/board_init.h` | 10 | 实战派初始化入口与板级设备句柄声明 |
| `src/boards/esp32/szpi/board_pins.h` | 114 | 实战派引脚、I2C 地址、音频及屏幕硬件常量 |
| `src/boards/esp32/szpi/lcd_init.c` | 112 | 实战派 ST7789 SPI 初始化、寄存器序列、方向和背光控制 |
| `src/boards/esp32/szpi/lcd_init.h` | 9 | 实战派屏幕初始化、背光和句柄声明 |
| `src/boards/esp32/szpi/pca9557.c` | 78 | PCA9557 寄存器与输出控制实现 |
| `src/boards/esp32/szpi/pca9557.h` | 69 | PCA9557 类型、引脚操作及控制接口声明 |
| `src/boards/esp32/szpi/profile.c` | 10 | 实战派名称、麦克风/参考 slot0/slot1、BOOT 按键能力和功放控制 |

## 利尔达板级

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/boards/esp32/lrd/board_init.c` | 49 | 利尔达 I2C 与 TPT29555A 初始化 |
| `src/boards/esp32/lrd/board_init.h` | 10 | 利尔达初始化入口与板级设备句柄声明 |
| `src/boards/esp32/lrd/board_pins.h` | 121 | 利尔达引脚、I2C 地址、音频及屏幕硬件常量 |
| `src/boards/esp32/lrd/lcd_init.c` | 119 | 利尔达 ST7789 初始化、寄存器序列、方向和背光控制 |
| `src/boards/esp32/lrd/lcd_init.h` | 9 | 利尔达屏幕初始化、背光和句柄声明 |
| `src/boards/esp32/lrd/profile.c` | 19 | 利尔达名称、麦克风/参考 slot2/slot1、功放控制和触摸复位 |
| `src/boards/esp32/lrd/tpt29555a.c` | 92 | TPT29555A 寄存器、方向与输出控制实现 |
| `src/boards/esp32/lrd/tpt29555a.h` | 74 | TPT29555A 类型、端口与控制接口声明 |

## Hoperun 板级及 WS63 板级接口

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/boards/ws63/hoperun/README.md` | 14 | 新增同类 WS63 板的步骤与 BSP 兼容边界。 |
| `src/boards/ws63/hoperun/board.c` | 57 | Hoperun 键盘、电源保持及音频布局；服务获取已归平台组件。 |
| `src/boards/ws63/hoperun/board.cmake` | 2 | Hoperun BSP 默认路径和板级源文件清单。 |
| `src/boards/ws63/hoperun/display.c` | 20 | 显示字节序与分块写入。 |
| `src/boards/ws63/include/ws_board.h` | 12 | WS63 板级接口、板名和采集布局；服务句柄须兼容当前 GoldieOS 后端 |

## ESP-IDF 平台端口

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/platforms/esp_idf/components/config/config.c` | 38 | 从构建配置读取鉴权参数，构造 SDK JSON；API Key 优先，否则使用产品三元组 |
| `src/platforms/esp_idf/components/media/media.c` | 174 | 移除累计统计，保留故障处理和原有低频日志 |
| `src/platforms/esp_idf/components/network/network.cpp` | 78 | 状态查询限定为文件内部函数 |
| `src/platforms/esp_idf/components/convai/convai_platform_esp32.c` | 84 | 将 ESP 平台函数组装为 ConvAI HAL 并注册 |
| `src/platforms/esp_idf/components/convai/include/convai_platform_esp32.h` | 5 | SDK 平台注册入口声明 |
| `src/platforms/esp_idf/components/convai/convai_platform_esp32_internal.h` | 136 | HAL 私有结构和各适配函数声明 |
| `src/platforms/esp_idf/components/convai/esp32_misc.c` | 141 | 日志、设备标识等辅助 HAL 实现 |
| `src/platforms/esp_idf/components/convai/esp32_netal.c` | 251 | Socket 网络适配，非阻塞检查与 TCP_NODELAY |
| `src/platforms/esp_idf/components/convai/esp32_osal.c` | 223 | 内存、时钟、线程创建/join/destroy、互斥、随机数及字符串接口 |
| `src/platforms/esp_idf/components/convai/esp32_tlsal.c` | 246 | mbedTLS 连接、收发和证书校验适配 |

## GoldieOS 平台、组件、选板及构建入口

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt` | 23 | 可选 WS63 公共静态库构建入口 |
| `src/platforms/goldieos/README.md` | 88 | WS63 独立应用架构、BSP 依赖、配置、固件构建及验收边界 |
| `src/platforms/goldieos/cmake/board.cmake` | 16 | WS63_BOARD 选择、合法性校验及禁止跨板复用构建目录。 |
| `src/platforms/goldieos/cmake/common.cmake` | 56 | GoldieOS 公共源码及组件构建组装 |
| `src/platforms/goldieos/targets/ws63/CMakeLists.txt` | 50 | WS63 固件构建入口；选板、BSP 裁剪、TLS/网页/网关适配链接，按 WS63_BOARD 导出镜像 |
| `src/platforms/goldieos/entry/app.c` | 111 | GoldieOS 启动及事件循环，使用本地受锁保护队列 |
| `src/platforms/goldieos/components/config/config.c` | 39 | 从私有头文件读取鉴权，构造并校验 SDK JSON |
| `src/platforms/goldieos/components/network/ws63/dhcp_gateway.c` | 41 | 当前 Hoperun BSP 缺少 DHCP 网关选项的兼容处理，校验边界并重算 UDP 校验和 |
| `src/platforms/goldieos/components/network/ws63/dhcp_gateway.h` | 8 | 报文补全接口与长度约束。 |
| `src/platforms/goldieos/components/ui/lv_conf.h` | 22 | WS63 的 LVGL 9.5.0 配置，统一字体、主题、色深及刷新参数 |
| `src/platforms/goldieos/components/media/media.c` | 298 | 移除累计统计和专用辅助变量，保留录放音行为 |
| `src/platforms/goldieos/components/network/ws63/network.c` | 341 | 状态查询限定为文件内部函数，配网逻辑不变 |
| `src/platforms/goldieos/components/network/ws63/portal_gateway.c` | 42 | AP IPv4 输出适配；复制 DHCP 报文后补全，退出配网恢复原输出函数。 |
| `src/platforms/goldieos/components/runtime/ws63/runtime.c` | 18 | 委托通用板级初始化，提供 NTP、SDK 初始化与设备标识适配 |
| `src/platforms/goldieos/components/runtime/include/runtime.h` | 10 | BSP 就绪、运行环境初始化、时钟、设备标识和 SDK 适配接口 |
| `src/platforms/goldieos/components/convai/ws63/sdk_platform.c` | 872 | WS63 SDK HAL；Goldie OSAL、网络、TLS/CA、线程退出等待、日志与设备信息 |
| `src/platforms/goldieos/components/convai/ws63/delivery_diag.h` | 54 | 临时 TLS 交付统计、读后明文快照和限频窗口，不改变 socket 就绪 |
| `src/platforms/goldieos/components/ui/ui.c` | 91 | 共用 LVGL 页面、BSP 输入事件、快照邮箱及 UI 任务；显示传输委托板级接口 |
| `src/platforms/goldieos/components/network/ws63/wifi_store.c` | 40 | 内部 NV 读取、校验回退、写入读回与外来键保护；不依赖 SD |
| `src/platforms/goldieos/components/network/ws63/wifi_store.h` | 10 | NV 应用键预留及双快照存储接口 |
| `src/platforms/goldieos/components/network/ws63/ws_wifi_abi.h` | 22 | 当前润和 BSP Wi-Fi 热点最小 ABI，包含结构尺寸和偏移断言 |

## Windows 模拟器端口

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/platforms/goldieos/targets/windows/CMakeLists.txt` | 26 | MinGW 模拟器目标、Windows SDK/HAL 及厂商虚拟设备链接 |
| `src/platforms/goldieos/targets/windows/README.md` | 28 | 模拟器依赖、构建运行、配置来源与验证边界 |
| `src/platforms/goldieos/components/runtime/windows/runtime.c` | 17 | Windows 时间、SDK 初始化及板级启动适配；网络已独立到 network 组件 |
| `src/platforms/goldieos/components/convai/windows/sdk_platform.c` | 761 | Windows SDK 平台实现，包含线程、网络、TLS 证书校验与日志 |

## 公共页面及 UI 端口

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/application/ui/ai_lvgl.c` | 55 | 三板唯一 LVGL 页面实现，创建控件、处理按钮和渲染状态快照 |
| `src/platforms/esp_idf/components/ui/ai_ui.c` | 95 | ESP LVGL 运行端口、触摸任务及显示 DMA 完成通知；页面布局在 ai_lvgl.c |
| `src/application/ui/include/ai_lvgl.h` | 8 | 公共 LVGL 页面创建、对话状态和网络文本接口 |
| `src/application/ui/include/ai_screen.h` | 23 | ESP32 与 WS63 共用的页面尺寸、配色、按钮文案及状态格式 |
| `src/application/ui/include/ai_ui.h` | 11 | UI 初始化、状态快照、网络文字及启停/配网命令接口 |
| `src/application/ui/web/wifi_configuration.html` | 101 | 三板共用最小配网页面：扫描、连接保存、删除、默认及退出；安全显示 SSID |
| `src/application/ui/web/wifi_configuration_done.html` | 3 | 共用配网完成提示页面，兼容 done.html 入口 |

## ESP 组件注册及 Wi-Fi 组件

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/platforms/esp_idf/cmake/components/ai_cloud/CMakeLists.txt` | 3 | 云组件公开依赖媒体契约所在组件 |
| `src/platforms/esp_idf/cmake/components/ai_core/CMakeLists.txt` | 2 | 注册公共状态机和协调层 |
| `src/platforms/esp_idf/cmake/components/ai_media/CMakeLists.txt` | 3 | 注册公共音频转换和播放缓冲组件 |
| `src/platforms/esp_idf/cmake/components/ai_network/CMakeLists.txt` | 2 | 注册公共网络状态及列表操作源码 |
| `src/platforms/esp_idf/cmake/components/ai_ui/CMakeLists.txt` | 3 | 注册 UI 组件及 LVGL、显示、系统依赖 |
| `src/platforms/esp_idf/cmake/components/board/CMakeLists.txt` | 15 | 按 UNIFIED_BOARD 选择板型源码并加入公共驱动 |
| `src/platforms/esp_idf/cmake/components/convai_sdk/CMakeLists.txt` | 12 | 检查 SDK 路径并导入 ESP32-S3 预编译库，公开头文件和 cJSON 链接依赖 |
| `src/platforms/esp_idf/cmake/components/esp-wifi-connect/CMakeLists.txt` | 44 | 注册 NVS 后端源码 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/dns_server.cc` | 103 | ESP UDP socket/任务适配；调用共享 ai_portal DNS 应答逻辑 |
| `src/platforms/esp_idf/cmake/components/esp-wifi-connect/idf_component.yml` | 18 | 组件版本、来源、MIT 许可标识及依赖声明 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/dns_server.h` | 27 | 配网 DNS 服务声明 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/ssid_manager.h` | 21 | 移除闲置 Clear 声明 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_configuration_ap.h` | 85 | Save 返回持久化结果 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_manager.h` | 128 | Wi-Fi 管理器接口、配置和事件定义 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/wifi_station.h` | 100 | Station 扫描、连接与状态接口 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/ssid_manager.cc` | 26 | 移除闲置 Clear 包装，不改存储加载或快照 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_configuration_ap.cc` | 755 | 向网页反馈保存失败，SmartConfig 字符串补终止符并去除密码日志；本轮补充 HTTP 任务栈 8 KB |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_manager.cc` | 313 | Station 与配网 AP 的生命周期管理和事件转发 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/wifi_station.cc` | 401 | 已保存网络扫描、连接、重连与获取 IP |

## 测试与维护工具

| 文件 | 行数 | 说明 |
|---|---:|---|
| `tests/test_goldie_media.c` | 108 | GoldieOS 播放补充、锁边界、清理与排空回归测试 |
| `tests/test_ws_delivery_diag.c` | 35 | TLS 缓存与 socket 就绪诊断计数、静默窗口和计时回绕回归 |
| `tests/stubs/goldie_media/goldie_osal.h` | 13 | 媒体测试专用互斥和线程接口替身 |
| `tests/stubs/goldie_media/service_manager.h` | 5 | 媒体测试专用服务查询声明 |
| `tests/stubs/goldie_media/lwip/sys.h` | 5 | WS63 媒体耗时诊断测试的 lwIP 公共单调时钟声明替身 |

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `tests/CMakeLists.txt` | 86 | 云/配置测试声明直接使用的媒体头目录 |
| `tests/test_audio.c` | 70 | 删除统计断言，保留音频算法/缓冲行为测试 |
| `tests/test_cloud.c` | 96 | 使用假 SDK 测试云适配的创建/启动失败、回调、打断和停止行为 |
| `tests/test_core.c` | 61 | 状态机启停、断线、迟到事件、错误、超时和重复会话测试 |
| `tests/test_dhcp_gateway.c` | 48 | 网关补全、重复选项、异常/截断报文、容量和 UDP 校验和测试。 |
| `tests/test_portal.c` | 31 | DNS A/AAAA、截断报文、容量限制和附加记录处理回归测试。 |
| `tests/test_provision.c` | 44 | 异常 HTTP 长度/请求、网络参数与索引、损坏存储快照测试 |
| `tests/test_wifi_web.cjs` | 75 | 执行真实共用页面脚本，验证扫描去重、特殊名称、保存/默认/删除、退出及错误；需 Node |
| `tests/test_ws_config.c` | 26 | 测试真实云配置生成、设备标识失败、内存失败恢复及 JSON 数字类型 |
| `tests/test_ws_wifi_store.c` | 40 | 模拟 NV 的重启加载、读写失败、冲突保护、校验回退和删除回归 |
| `tools/check_architecture.py` | 54 | 防止媒体模块直接引入云或 SDK 头 |
| `tools/update_repository_guide.py` | 38 | 从 Git 候选文件集合检查清单覆盖率、更新逐文件及目录行数；与执行目录无关，不读取鉴权内容。 |

## 第三方依赖

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `third_party/README.md` | 25 | SDK 下载来源、版本、SHA-256 和依赖说明；提交 |
| `third_party/convai-sdk-26.8.0/include/convai/convai_api.h` | 162 | 引擎创建、启停、收发等公共 API；提交 |
| `third_party/convai-sdk-26.8.0/include/convai/convai_event.h` | 124 | 事件类型与回调定义；提交 |
| `third_party/convai-sdk-26.8.0/include/convai/convai_platform.h` | 148 | 平台 HAL ABI 与接口；提交 |
| `third_party/convai-sdk-26.8.0/include/convai/convai_types.h` | 168 | SDK 通用类型、音频格式与选项；提交 |
| `third_party/convai-sdk-26.8.0/libs/esp32s3/libconvai_sdk.a` | — | 两块 ESP32-S3 必需的真实 SDK 静态库；提交 |
| `third_party/convai-sdk-26.8.0/libs/windows/libconvai_sdk.a` | — | Windows 模拟器必需的 SDK 静态库；提交，不计算行数 |
| `third_party/convai-sdk-26.8.0/libs/ws63/libconvai_sdk.a` | — | WS63 目标 SDK 库；独立固件必须提供 |
| `third_party/convai_sdk_26.8.0.tar` | — | 原始 SDK 发布包，可选归档；不要只提交它而漏掉构建实际使用的解压目录 |
| `third_party/lvgl-9.5.0-source.tar.gz` | — | LVGL 9.5.0 源码及 MIT 许可归档；WS63 构建必需；提交，不计压缩文件行数 |

## 新手源码培训文档

以下培训文件随源码上库，内容以当前实现为基准，规划和未验证事项单独标注。

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `docs/training/README.md` | 65 | 八步培训索引、术语、阅读方法和验证边界 |
| `docs/training/01-directory-and-layers.md` | 145 | 目录、三层职责、依赖注入与源码阅读练习 |
| `docs/training/02-build-and-startup.md` | 120 | 四目标构建选择、平台入口、初始化及主循环 |
| `docs/training/03-core-and-controller.md` | 131 | 公共状态机、事件命令、会话隔离及协调规则 |
| `docs/training/04-session-and-ui.md` | 129 | Start/Stop、SDK 回调、跨线程消息与界面快照 |
| `docs/training/05-audio-pipeline.md` | 139 | 采集编码、上下行、缓存、锁、排空及诊断边界 |
| `docs/training/06-network-provisioning.md` | 131 | 扫描连接、DHCP/DNS/HTTP 门户、保存和失败核对 |
| `docs/training/07-platforms-and-boards.md` | 120 | 四目标对照、新 WS63 伙伴和未来 Linux 接入边界 |
| `docs/training/08-errors-and-testing.md` | 166 | 异常路径、现有测试、实机证据、定位与结业练习 |

## 当前验证产物（不上库）

| 目标 | 产物 | 使用方式 |
|---|---|---|
| 利尔达 | build-lrd/unified_ai_lrd_merged.bin | ESP32-S3，8 MB Flash，合并镜像烧写到 0x0 |
| 实战派 | build-szpi/unified_ai_szpi_merged.bin | ESP32-S3，16 MB Flash，合并镜像烧写到 0x0 |
| Hoperun 单板 | build-hoperun-app/out/unified_ai_hoperun.fwpkg | 使用匹配板卡的厂商工具，不推测裸 BIN 烧写地址 |
| Hoperun 模拟器 | build-hoperun-win/unified_ai_simulator.exe | 在所在目录运行，使用 Windows 网络和音频 |


## 平台整改新增文件

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `src/platforms/README.md` | 31 | 平台模块职责、GoldieOS/WS63 边界、未来平台扩展约定 |
| `src/platforms/goldieos/components/network/windows/network.c` | 24 | 状态查询限定为文件内部函数 |
| `src/platforms/goldieos/components/config/component.cmake` | 10 | GoldieOS config 组件源文件与依赖注册；按需要选择后端 |
| `src/platforms/goldieos/components/media/component.cmake` | 3 | GoldieOS media 组件源文件与依赖注册；按需要选择后端 |
| `src/platforms/goldieos/components/network/component.cmake` | 28 | GoldieOS network 组件源文件与依赖注册；按需要选择后端 |
| `src/platforms/goldieos/components/runtime/component.cmake` | 3 | GoldieOS runtime 组件源文件与依赖注册；按需要选择后端 |
| `src/platforms/goldieos/components/convai/component.cmake` | 7 | GoldieOS sdk 组件源文件与依赖注册；按需要选择后端 |
| `src/platforms/goldieos/components/ui/component.cmake` | 18 | GoldieOS ui 组件源文件与依赖注册；按需要选择后端 |

## 提交前检查

在 unified-app 仓库根目录运行：

```sh
python tools/update_repository_guide.py
python tools/check_architecture.py
git status --short
git diff --check
git add --dry-run -- .
```

确认新增文件、删除旧网页文件的变更和忽略范围后再执行暂存/提交。不要只提交已跟踪文件而遗漏 src/boards/ws63/hoperun、src/application/services/network、WS63/Windows 源码及测试。实际内容以当前 Git 差异为准，模板中的鉴权保持为空。

## Windows 构建 skills（2026-10-13）

四个独立入口和共用资源都位于 skills。_shared 不放 SKILL.md，不注册为第五个 skill。以下文本文件上库；真实二进制归档、manifests/cache、local.json 与 build/skills/* 忽略。共用实现按最新 GoldieOS 组件架构工作。

| 文件 | 行数 | 用途及提交要求 |
|---|---:|---|
| `skills/build-lrd/SKILL.md` | 30 | 独立目标入口、调用条件、参数、产物与验收边界。 |
| `skills/build-lrd/agents/openai.yaml` | 4 | 该目标的显示名称及调用提示。 |
| `skills/build-szpi/SKILL.md` | 30 | 独立目标入口、调用条件、参数、产物与验收边界。 |
| `skills/build-szpi/agents/openai.yaml` | 4 | 该目标的显示名称及调用提示。 |
| `skills/build-hoperun-ws63/SKILL.md` | 30 | 独立目标入口、调用条件、参数、产物与验收边界。 |
| `skills/build-hoperun-ws63/agents/openai.yaml` | 4 | 该目标的显示名称及调用提示。 |
| `skills/build-hoperun-simulator/SKILL.md` | 30 | 独立目标入口、调用条件、参数、产物与验收边界。 |
| `skills/build-hoperun-simulator/agents/openai.yaml` | 4 | 该目标的显示名称及调用提示。 |
| `skills/_shared/scripts/build.ps1` | 143 | 共用构建引擎、输出隔离与摘要交付。 |
| `skills/_shared/scripts/dependencies.ps1` | 90 | 本地/缓存/下载归档校验与解压。 |
| `skills/_shared/scripts/esp-idf.ps1` | 102 | ESP-IDF 安装、EIM 环境复用和合并镜像。 |
| `skills/_shared/manifests/dependencies.json` | 38 | 版本、可信地址与 SHA-256；WS63 工具包支持本地路径/HTTPS 与校验值。 |
| `skills/_shared/references/hoperun.md` | 14 | 独立 Hoperun 单板/模拟器、运行归档与工具包要求。 |
| `skills/_shared/references/esp-idf.md` | 20 | ESP 两板安装、离线和 EIM 使用。 |
| `skills/_shared/references/dependencies.md` | 25 | 清单、本地包、来源和验证边界。 |
| `skills/_shared/tests/dependencies.tests.ps1` | 34 | 本地包/缓存/坏校验值/离线/目录边界测试。 |

## 第一批组件依赖优化新增文件（2026-10-14）

已修正 build*/ 对 skills/build-* 工作流源码的误排除；这些源码列入上库清单，skills 下载缓存仍忽略。

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/platforms/esp_idf/cmake/components/esp_config/CMakeLists.txt` | 6 | 独立注册 ESP config 组件，声明公开接口与私有构建依赖 |
| `src/platforms/esp_idf/cmake/components/esp_media/CMakeLists.txt` | 6 | 移除云组件依赖，显式声明应用及媒体依赖 |
| `src/platforms/esp_idf/cmake/components/esp_network/CMakeLists.txt` | 5 | ai_network 收为实现私有依赖 |
| `src/platforms/esp_idf/cmake/components/esp_sdk/CMakeLists.txt` | 7 | 独立注册 ESP sdk 组件，声明公开接口与私有构建依赖 |
| `src/platforms/esp_idf/components/config/include/esp_config.h` | 11 | 云鉴权配置组装接口 |
| `src/platforms/esp_idf/components/media/include/esp_media.h` | 14 | 显式引用应用事件及媒体契约，不再引用云头 |
| `src/platforms/esp_idf/components/network/include/esp_network.h` | 17 | 移除内部网络状态类型和查询声明 |
| `src/platforms/esp_idf/components/convai/include/esp_device.h` | 11 | SDK 设备 ID 查询窄接口，配置模块无需依赖 HAL 内部类型 |
| `src/platforms/goldieos/components/config/include/ws_config.h` | 11 | 云鉴权配置组装接口 |
| `src/platforms/goldieos/components/media/include/ws_media.h` | 15 | 仅引用媒体契约 |
| `src/platforms/goldieos/components/network/include/ws_network.h` | 16 | 移除内部网络状态类型和查询声明 |
| `src/platforms/goldieos/components/ui/include/ws_ui.h` | 13 | 平台界面初始化和状态呈现接口 |
| `src/platforms/goldieos/include/ws_app_events.h` | 13 | 应用事件邮箱投递与界面命令入口，供媒体和 UI 使用 |

## 平台构建入口新增文件（2026-10-14）

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/platforms/esp_idf/targets/esp32s3/CMakeLists.txt` | 11 | ESP32-S3 直接构建入口，经根分发保留统一资源根 |
| `src/platforms/esp_idf/targets/esp32s3/project.cmake` | 26 | ESP 专属板型校验、SDK 路径、默认配置及 IDF 组件组装 |

## 公共应用协调新增文件（2026-10-14）

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/application/core/ai_controller.c` | 81 | 公共应用协调：启动门禁、配置重试、命令分发、网络状态、溢出与提示刷新 |
| `src/application/core/include/ai_controller.h` | 47 | 平台无关的命令、消息、协调实例与回调契约 |
| `tests/test_controller.c` | 109 | 验证幂等启停、配置重试、配网停止顺序、旧事件、溢出、超时及无配网目标 |

## 第三批新增文件（2026-10-15）

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/application/services/network/include/ai_network_status.h` | 22 | 公共网络状态、地址及热点信息契约，不包含密码 |
| `src/application/services/network/ai_network_status.c` | 15 | 公共网络状态文案格式化，保留当前界面提示 |
| `src/application/services/network/include/ai_wifi_store.h` | 21 | 移除清空接口声明，保留保存/删除/默认操作 |
| `src/application/services/network/ai_wifi_store.c` | 47 | 移除清空实现，保留公共列表及保存结果逻辑 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/include/esp_wifi_store.h` | 14 | ESP NVS 快照加载与保存接口 |
| `src/platforms/esp_idf/components/network/esp-wifi-connect/esp_wifi_store.c` | 70 | NVS 双快照、CRC、旧键兼容读取及提交回读 |
| `tests/stubs/nvs.h` | 19 | 主机测试用 NVS 最小接口替身，不参与固件构建 |
| `tests/test_esp_wifi_store.c` | 66 | 旧键迁移、存储失败、CRC 回退、空列表和键所有权测试 |
| `tests/test_network_contract.c` | 37 | 改为逐个删除至空并验证空列表删除失败 |
| `docs/changes-2026-10-15.md` | 58 | 原始第三批记录标注后续精简及已删除文件 |

## 第四批历史记录（代码已回退）

| 文件 | 行数 | 说明 |
|---|---:|---|
| `docs/changes-batch4-2026-10-15.md` | 34 | 第四批实施及整体回退的逐文件历史记录 |

## 第三批精简记录

| 文件 | 行数 | 说明 |
|---|---:|---|
| `docs/changes-simplify-2026-10-15.md` | 34 | 本轮逐文件修改记录 |

## 媒体契约归属新增文件

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/application/services/media/include/ai_media.h` | 19 | 承接既有媒体操作及上行回调声明，无新增接口 |
| `docs/changes-media-contract-2026-10-15.md` | 21 | 本轮逐文件变更记录 |


## GoldieOS 独立工程新增依赖（2026-10-20）

归档内部清单见 third_party/goldieos/files.json，不将解压缓存重复计入源码清单。工具包与编译器缓存不提交。

| 文件 | 行数 | 说明 |
|---|---:|---|
| `src/platforms/goldieos/cmake/convai-artifacts.cmake` | 102 | 本工程产物命名、版本副本与文件复制构建函数。 |
| `src/platforms/goldieos/cmake/convai-build-options.cmake` | 89 | 本工程通用警告及 WS63 ABI 编译选项。 |
| `src/platforms/goldieos/cmake/goldieos-common.cmake` | 130 | 组装保留的公共运行源码与 Opus；不包含旧应用或 AI bridge。 |
| `src/platforms/goldieos/cmake/goldieos-win.cmake` | 103 | Windows 虚拟设备、核心库和系统链接配置。 |
| `src/platforms/goldieos/cmake/goldieos-ws63.cmake` | 132 | WS63 运行源码、驱动与平台编译配置。 |
| `src/platforms/goldieos/cmake/runtime.cmake` | 23 | 校验工程内 GoldieOS 归档并解压至独立构建目录。 |
| `src/platforms/goldieos/cmake/toolchains/ws63-compiler-launcher.bat` | 19 | 确保指定 RISC-V 编译器及运行库可被子进程找到。 |
| `src/platforms/goldieos/cmake/toolchains/ws63-riscv-gcc.cmake` | 96 | 从配置工具根选择编译器，不推导外部示例目录。 |
| `src/platforms/goldieos/cmake/ws63-firmware.cmake` | 158 | 暂存运行库和链接配置，调用独立工具包生成固件。 |
| `src/platforms/goldieos/cmake/ws63-mbedtls.cmake` | 189 | 从固定运行包重建 mbedTLS 与海思加固适配。 |
| `src/platforms/goldieos/components/convai/ws63/convai_memory_budget.h` | 42 | 厂商 TLS 配置依赖的内存预算常量，脱离旧 AI bridge。 |
| `third_party/goldieos/checksums.cmake` | 2 | 固定运行归档 SHA-256。 |
| `third_party/goldieos/files.json` | 8972 | 归档逐文件路径、大小和 SHA-256；不含本地绝对路径。 |
| `third_party/goldieos/README.md` | 29 | 依赖范围、来源、工具包与上库边界说明。 |
| `third_party/goldieos/runtime.tar.gz` | — | GoldieOS 必要运行源码、头文件、成套库与打包配置归档；二进制不计行数。 |
