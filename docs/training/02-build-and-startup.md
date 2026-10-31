# 第 2 步：构建选择与应用启动

[培训索引](README.md) · [上一步](01-directory-and-layers.md) · [下一步](03-core-and-controller.md)

## 1. 学习目标

解释“同一份源码”如何生成四个不同程序；找到每个程序的启动入口；区分构建时选择、运行时初始化及应用循环。

构建是电脑把源码编译并链接成程序；启动是单板/模拟器开始执行该程序。这两条链路不能混为一谈。

## 2. 当前目标身份

| 产品 | 根入口 `UNIFIED_TARGET` | 板型参数 | 平台入口 |
|---|---|---|---|
| 利尔达 ESP32-S3 | `esp32s3` | `UNIFIED_BOARD=lrd` | `esp_idf/entry/app_main.c` |
| 立创实战派 ESP32-S3 | `esp32s3` | `UNIFIED_BOARD=szpi` | 同上 |
| 润和 WS63 实机 | `goldieos_ws63` | `WS63_BOARD=hoperun` | `goldieos/entry/app.c` |
| 润和 Windows 模拟器 | `goldieos_windows` | `WS63_BOARD=hoperun` | 同上 |

根入口默认 ESP32-S3，ESP 板型默认 szpi。教学时应显式说明选择，避免把默认值当成当前接入设备的自动识别结果。构建不会通过串口自动探测板型。

## 3. ESP 构建链路

按下面顺序阅读：

1. [根 CMakeLists.txt](../../CMakeLists.txt)：定义工程根和源码根，并分发目标。
2. [ESP project.cmake](../../src/platforms/esp_idf/targets/esp32s3/project.cmake)：校验 lrd/szpi，固定 `IDF_TARGET=esp32s3`，选择默认配置和 SDK 路径。
3. [组件注册目录](../../src/platforms/esp_idf/cmake/components)：描述 ai_core、ai_cloud、esp_media、board、main 等源码与依赖。
4. [board 组件](../../src/platforms/esp_idf/cmake/components/board/CMakeLists.txt)：选择共用代码和对应板级实现。
5. [main 组件](../../src/platforms/esp_idf/cmake/components/main/CMakeLists.txt)：接入平台入口。

`application/core` 虽然已重命名，构建组件仍叫 `ai_core`；平台目录叫 `convai`，部分兼容构建组件仍叫 `esp_sdk`。目录名、CMake 组件名、C 函数名不必同步改名。

ESP 配置组合通常为 `config/common.defaults`、板型 defaults 和可选本地 defaults。实际生成的 sdkconfig 位于构建目录。修改 defaults 后，既有缓存不一定按直觉覆盖已有配置，应核对实际生成配置，而不是只看模板。

## 4. GoldieOS 构建链路

依次阅读 [板级选择](../../src/platforms/goldieos/cmake/board.cmake)、[润和 board.cmake](../../src/boards/ws63/hoperun/board.cmake)、[common.cmake](../../src/platforms/goldieos/cmake/common.cmake)。

板级选择载入 `WS63_BOARD` 对应的文件；它提供板级源文件和 BSP 位置。common 组装公共 core、cloud、media、network、板级源码，再调用各组件的注册函数。

各组件按后端选择实现。例如 [network/component.cmake](../../src/platforms/goldieos/components/network/component.cmake) 在 Windows 下接入电脑网络；在 WS63 下加入 NV、DHCP 和门户兼容代码，并把公共 HTML 嵌入固件。

[WS63 目标](../../src/platforms/goldieos/targets/ws63/CMakeLists.txt) 使用嵌入式工具链和厂商打包流程；[Windows 目标](../../src/platforms/goldieos/targets/windows/CMakeLists.txt) 使用模拟器 BSP、Windows SDK 库并定义 `AI_UI_NO_PROVISION`。

两个目标复用同一个 GoldieOS 应用入口，不代表它们链接相同的机器码。WS63 与 Windows 的指令集、库和操作系统接口不同。

## 5. 从系统进入应用

ESP 的启动路径：

```text
ESP-IDF 系统启动
  → app_main()
  → 创建应用事件队列
  → board_init()
  → ai_ui_init()
  → esp_media_init()
  → convai_platform_esp32_init()
  → prepare()，尝试准备云配置
  → ai_controller_init()
  → esp_network_init()
  → 应用事件循环
```

在 [app_main.c](../../src/platforms/esp_idf/entry/app_main.c) 中，硬件/资源初始化失败经 `require_ok` 进入 abort；云配置准备失败只记录并保留后续 Start 重试机会。不要把两种错误策略讲成完全相同。

GoldieOS 的启动路径：

```text
GoldieOS 初始化注册机制
  → GOLDIE_INIT_CALL_(standalone_entry)
  → 创建 unified_ai 应用线程
  → app_task()
  → 初始化邮箱锁
  → ws_runtime_prepare() / ws_media_prepare()
  → ws_ui_init() / ws_runtime_init() / ws_media_init()
  → ws_runtime_sdk_init()
  → prepare() / ai_controller_init()
  → 开启网络并创建校时线程
  → 应用事件循环
```

在 [GoldieOS app.c](../../src/platforms/goldieos/entry/app.c) 中，`ws_ui_init` 安装并进入独立 Unified AI 应用；当前不嵌在原“设置—AI 设置”页面。初始化失败经 `require` 显示失败步骤并停留，不继续执行后面的初始化。

## 6. 主循环到底做什么

共同点：先读取网络状态，再消费应用消息，再执行 controller tick。SDK 回调不直接调用状态机，而是投递消息，交给这个 owner 串行处理。

ESP 使用 FreeRTOS 队列，深度 32，队列接收带等待；还检查实体按键。GoldieOS 使用 mutex 保护的 32 项环形消息数组，每轮最多取一项并休眠。它们使用同样的 `ai_message_t`，没有共用一份队列实现。

tick 不是“每次发送一包音频”。它负责应用超时、网络提示更新等；录音和播放有自己的线程。

时间同步也不是 SDK 音频任务：ESP 网络模块启动 SNTP；GoldieOS 校时线程通过 runtime 接口执行同步。网络可用和时间可用于 TLS 是两个条件。

## 7. 产物与配置目录

| 本地产物 | 含义 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 利尔达多个烧写分区内容按地址合并的镜像 |
| `build-szpi/unified_ai_szpi_merged.bin` | 实战派的对应合并镜像 |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 润和厂商烧写包 |
| `build-hoperun-win/unified_ai_simulator.exe` | Windows 模拟器程序 |

“合并镜像”是分区打包含义，不是把两块板的程序合在一起。文件名不保证内容最新，交付时应核对构建记录、时间和哈希。

不同目标、板型和构建入口使用独立构建目录。不要复制其他板子的 CMakeCache 当作新目标配置；不要把 build 目录作为产品源码上库。

本节只走读，不要求执行烧写。实际准备工具链和构建步骤见[项目 README](../../README.md)及各目标的 [skills 目录](../../skills)。

## 8. 练习与参考答案

1. 找到禁用模拟器 Wi-Fi 页面的定义位置。
2. 为什么公共 core 不需要为利尔达和实战派各复制一份？
3. 云配置启动准备失败后，能否在同一次运行中再尝试？
4. 应用主循环暂停，录音线程是否必然也停止？

答案：Windows 目标定义 `AI_UI_NO_PROVISION`；两板链接同一公共源码并选择不同板级实现；Start 会通过 prepare 再准备配置；录音线程独立运行，是否停止上传取决于媒体 enable 和会话状态，不能仅从主循环推断。

讲解检查：能画出“构建目标选择 → 平台入口 → 初始化 → 消息循环”，就可以进入下一节。
