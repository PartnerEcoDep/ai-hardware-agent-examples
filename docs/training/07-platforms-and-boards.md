# 第 7 步：平台对照与新硬件接入

[培训索引](README.md) · [上一步](06-network-provisioning.md) · [下一步](08-errors-and-testing.md)

## 1. 学习目标

用当前四个目标区分芯片、软件平台、运行环境和板型。面对一个新伙伴产品，先判断接口差异，再决定修改目录，不直接复制整个工程。

阅读前先打开[平台说明](../../src/platforms/README.md)、[板型对比](../board-comparison.md)和[GoldieOS 说明](../../src/platforms/goldieos/README.md)。

## 2. 四个维度不能互相替代

| 维度 | 回答的问题 | 当前例子 |
|---|---|---|
| 芯片/架构 | 程序在哪类处理器上执行 | ESP32-S3、WS63；模拟器在电脑处理器上执行 |
| 软件平台 | 使用哪套 OS、驱动和服务 API | ESP-IDF、GoldieOS |
| 运行环境 | 实机固件还是宿主模拟 | WS63 实机、Windows 模拟器 |
| 板型 | 引脚、屏幕、Codec、布局如何连接 | lrd、szpi、hoperun |

`boards/esp32` 当前是家族分组，实际已接入的是 ESP32-S3。不要把目录名理解成 ESP32-C3/P4 已支持。`boards/ws63` 也不表示所有 WS63 伙伴都兼容 GoldieOS。

模拟器仍沿用 Hoperun 配置和 GoldieOS 应用接口，但不执行 WS63 指令，也不测试 WS63 Wi-Fi 射频和真实音频外设。

## 3. 四目标的共用与替换

| 能力 | 利尔达 / 实战派 | 润和 WS63 | 润和 Windows 模拟器 |
|---|---|---|---|
| 状态机、controller | 同一公共源码 | 同左 | 同左 |
| ConvAI 业务接入 | 同一 ai_cloud.c | 同左 | 同左 |
| 编解码、播放队列算法 | 同一公共源码 | 同左 | 同左 |
| 对话页面 | 公共 LVGL 页面 | 同左 | 同左，禁用配网入口 |
| 手机网页 | 公共 HTML | 同一 HTML | 不启用 |
| 应用消息队列 | FreeRTOS queue | GoldieOS mutex/数组 | 共用 GoldieOS 入口机制 |
| 媒体任务 | ESP 媒体实现 | GoldieOS 媒体实现 | 共用媒体实现，设备服务由模拟 BSP 提供 |
| 网络 | ESP Wi-Fi 及事件 | WS63 WLAN/WifiService/lwIP | Windows 网络状态 |
| SDK HAL | ESP OS/socket/TLS | WS63 OS/socket/TLS | Windows OS/socket/TLS |
| 配网存储 | ESP NVS | WS63 内部 NV | 不提供单板配网存储功能 |

“同一公共源码”不意味着所有线程、缓冲容量和设备输出节奏一致，也不意味着四个目标同一时间都已经验收。最新状态以[验证记录](../validation.md)为准。

配网还有一项具体差异：公共网页包含按 request_id 查询提交结果的恢复流程，但当前只有 WS63 后端实现 `/connection/status`；ESP 尚未提供对应路由。共用页面与完整后端能力一致是两个不同结论，详见[第 6 步](06-network-provisioning.md)。

## 4. ESP 两块板为什么主要在 boards 区分

两板同用 ESP32-S3、ESP-IDF 和公共应用，差异集中于屏幕初始化、IO 扩展、触摸、功放及音频布局。

| 项目 | 利尔达 | 实战派 |
|---|---|---|
| 板级目录 | `boards/esp32/lrd` | `boards/esp32/szpi` |
| IO 扩展实现 | TPT29555A | PCA9557 |
| microphone / reference slot | 2 / 1 | 0 / 1 |
| 实体按键能力标志 | false | true |
| 共用板级部分 | `boards/esp32/common/esp32s3` | 同左 |

阅读 [lrd/profile.c](../../src/boards/esp32/lrd/profile.c) 和 [szpi/profile.c](../../src/boards/esp32/szpi/profile.c)，找到 board_profile、board_pa_enable、board_touch_reset。再阅读共用 [board.c](../../src/boards/esp32/common/esp32s3/board.c)，观察公共板级代码如何调用这些板型实现。

板型配置是构建时选择。当前不是运行时根据设备 ID 自动装载不同驱动，也没有通用插件发现系统。

## 5. 润和的 GoldieOS 边界

当前 GoldieOS 媒体从平台服务管理器获取 AudioService；网络使用 WifiService/Wlan_Drv；UI 通过 GoldieOS 应用管理注册独立应用。BSP 已经提供了大量硬件接入。

板级 [board.c](../../src/boards/ws63/hoperun/board.c) 保存电源与采集布局，[display.c](../../src/boards/ws63/hoperun/display.c) 保存显示接入，[board.cmake](../../src/boards/ws63/hoperun/board.cmake) 指定板级源文件和 BSP 路径。

[ws_board.h](../../src/boards/ws63/include/ws_board.h) 不暴露 AudioService 或 WifiService。服务查找留在平台层，避免每个伙伴板文件复制相同的服务管理逻辑。

但是板级实现本身可以包含 GoldieOS 驱动/OSAL 头文件。接口隔离不是要求驱动代码完全不知道平台；它是控制平台细节传播范围。

## 6. 新增 WS63 伙伴：先问接口是否兼容

### 情况 A：沿用兼容的 GoldieOS 服务

建议步骤：

1. 明确板型唯一名称和 BSP 位置，新增 `boards/ws63/<board>`。
2. 实现现有 ws_board 契约：生命周期、名称、采集布局、显示初始化和写入。
3. 提供 board.cmake，检查服务 ABI、音频读写单位和通道布局。
4. 使用独立构建目录选择 `WS63_BOARD`，验证编译和启动。
5. 依次验证显示触摸、配网保存、对话、重复启停、长回答与异常恢复。

只有服务和设备能力契约确实兼容时，才能主要修改 boards；不能仅凭芯片型号相同就承诺平台零修改。

### 情况 B：直接使用海思 SDK/LiteOS 或另一套 OS

需要提供对应软件后端：入口、线程与队列、媒体、网络、配置、UI 运行环境及 ConvAI HAL。公共 application 可以继续复用，GoldieOS 的服务类型不应被复制成所有 WS63 板的强制标准。

目前没有实施迁移到 fbb_ws63；本教材不把它列为已经存在的目录。后端命名与拆分需在实际适配时依据接口确认。

## 7. 树莓派的扩展示例：规划，不是现成支持

如果产品运行 Linux，通常新增 `platforms/linux`，具体树莓派型号和音频/显示硬件放入 boards。适配步骤可复用当前模块职责，而不复制整套状态机。

| 需要实现/核实 | 目的 |
|---|---|
| 应用 owner 和消息传递 | 串行驱动同一个 controller |
| 音频设备后端 | 将实际音频格式映射为当前媒体契约 |
| SDK OS/socket/TLS HAL | 提供 Linux 能力，确认 SDK 对应 CPU ABI 库可用 |
| LVGL 显示/输入后端 | 复用公共页面，接入实际屏幕和输入设备 |
| 网络管理和存储 | 根据产品需要提供配网或仅在线状态 |
| 板级布局 | 处理具体声卡、麦克风参考、功放和按键 |

尤其先确认 ConvAI SDK 是否提供目标 Linux/CPU 的可用库或构建方式；当前仓库的三个静态库不能任意跨 ABI 复用。

## 8. 两套接口的当前限制

ESP 使用 board.h，GoldieOS 使用 ws_board.h；平台网络接口也分别叫 esp_network 与 ws_network。当前通过公共控制契约复用行为，没有把所有平台 API 强行统一成同一名字。

新增硬件应先实现已经明确的公共契约。遇到确实共同的接口再提炼，不预先创建空平台、空组件或为猜测中的硬件加入大量分支。

## 9. 练习与参考答案

场景一：另一块 ESP32-S3 板只换了触摸复位引脚。优先板级，不改 ai_app。

场景二：另一块 WS63 板没有 GoldieOS AudioService。需要新的媒体平台适配，不能只在 board.c 改宏。

场景三：树莓派产品不需要手机配网。可借鉴模拟器不注册 provision 的能力选择，但网络实现仍应属于 Linux，不复制 Windows API。

场景四：所有板都要改变“网络恢复后是否自动重开对话”。属于公共产品规则，应先修改 core/controller 设计和测试，再验证各平台。

本节完成标准：面对新板，能先列“哪些接口相同、哪些不同”，再给出文件改动范围。
