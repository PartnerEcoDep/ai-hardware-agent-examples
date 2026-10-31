# AI 硬件生态调研、产品对比与架构扩展建议

调研日期：2026-10-15。架构边界补充：2026-10-20。

本文面向云端 AI 对话 SDK 和示例应用的跨平台移植。资料来自芯片厂商、板卡伙伴的公开文档和代码仓，并结合当前工程已有的三板对比记录。新增平台尚未在本工程完成编译、烧写或功能验证；采购优先级是针对本项目适配价值的判断，不代表市场份额或量产稳定性排名。价格、库存和技术支持响应未作实测。

## 1. 阅读与比较边界

需要分别比较以下层次：

| 层次 | 内容 | 本工程对应位置 |
|---|---|---|
| 芯片能力 | CPU/ABI、内存接口、无线、音频外设、图形与 NPU | 平台选择与能力约束 |
| SDK/系统平台 | RTOS、驱动 API、通信栈、系统服务、工具链 | `src/platforms/`、构建适配、SDK HAL |
| 产品应用方案 | 云接入、音频算法、应用管理、页面与交互 | 参考方案；可复用部分进入共享层 |
| 具体板卡 | 引脚、Codec、屏幕、触摸、声道、电源与存储配置 | `src/boards/` 和目标配置 |
| 跨平台业务 | 会话、启停、重试、云事件、配网业务与页面设计 | `src/application/core/`、`src/application/services/`、`src/application/ui/` |

同芯片不等于同服务接口，同 RTOS 不等于同 SDK，同 RISC-V 不等于静态库 ABI 兼容。具备显示接口不代表已配置屏幕，支持录放音不代表具备有效回声参考或可用的全双工 AEC。

## 2. 芯片供应商与平台能力

| 供应商 | 代表芯片 | SDK/系统底座 | 上层方案及能力 | 本项目定位 |
|---|---|---|---|---|
| 乐鑫 | ESP32-S3 | ESP-IDF / FreeRTOS | ESP-Brookesia 提供服务、GUI 与应用组织能力；本项目当前没有直接依赖它 | 已验证基线 |
| 海思 | WS63/WS63E | FBB_WS63、LiteOS 等 | FBB 强调统一开发框架和 API；润和 GoldieOS 是当前具体后端 | 已验证基线；继续区分官方接口与伙伴服务 |
| 博通集成 Beken | BK7258 | Armino / BK_AVDK_SMP | 官方 AI Solution 提供语音、视觉、RTC、双屏及云模型接入方案 | 优先评估的新联网 MCU 平台 |
| 博流 Bouffalo Lab | BL616/BL618 | BouffaloSDK，包含 FreeRTOS 等组件 | 网络、TLS、文件系统、LVGL 和外设组件；伙伴提供音频与语音产品 | 验证另一套 RISC-V SDK 和伙伴产品适配 |
| 瑞昱 Realtek | RTL8721Dx、RTL8726E、RTL8730E 等 | Ameba RTOS / FreeRTOS | Wi-Fi、蓝牙、外设组件与开发工具；不同型号能力需分别确认 | 根据具体伙伴需求接入 |
| 思澈 SiFli | SF32LB52x 等 | 基于 RT-Thread 的 SiFli SDK | 图形、音频、低功耗及官方小智方案 | 验证 RT-Thread、图形和不同联网方式 |
| 瑞芯微 Rockchip | RV1106 等 | Linux BSP、媒体和 NPU SDK | 摄像头、ISP、编解码、本地视觉推理 | 后续专用视觉平台；暂缓采购 |

来源：[ESP-Brookesia][1]、[FBB_WS63][2]、[Beken AI Solution][3]、[BouffaloSDK][5]、[Ameba RTOS][6]、[SiFli SDK][7]、[思澈 AI 小智][8]、[RV1106 官方资料][10]。

### 2.1 Beken：底层 SDK 与 AI 方案分离

BK_AVDK_SMP 提供系统、驱动和无线通信基础，`bk_solution_ai` 在其上提供 AI 交互与云接入。官方方案包含多核通信/多媒体分工，支持音视频与双屏场景。它是应用层与底层平台分离的参考，但其 RTC/云链路不能直接视为本项目 ConvAI SDK 的替代接口。[3][4]

方案仓与底层 SDK 要使用相同发布标签。公开 GitHub/Gitee 与企业 GitLab 的访问范围不同，不能仅因企业 GitLab 受限就断言全部 SDK 不公开。官方同时提供 Windows 本地与 Docker 构建路径，仍需本工程实际验证。[3]

### 2.2 博流与瑞昱：按实际 SDK 接入

BouffaloSDK 集成 FreeRTOS、lwIP、TLS、文件系统和 LVGL 等组件；Ameba RTOS 提供基于 FreeRTOS 的开发框架。应以固定版本、目标芯片和具体开发板示例为准，不能把组件清单推广为所有芯片的完整功能。[5][6]

伙伴维护的 SDK 可能基于不同的上游版本。接入时先固定伙伴示例可用的 SDK，再评估向上游迁移，避免同时引入平台升级和应用移植两种变量。

### 2.3 思澈：联网方式与图形能力应独立描述

SiFli SDK 基于 RT-Thread，提供 HAL、设备驱动和中间件。官方小智方案包含唤醒、AEC/VAD、连续对话与动态表情；其公开入门资料使用手机蓝牙网络共享。[7][8][9]

这不意味着所有思澈产品只能蓝牙联网。实际板型存在不同扩展能力，应逐板确认无线配置；不能把 SF32LB52 主控能力与外接 Wi-Fi 能力混在一起。[17]

### 2.4 瑞芯微：Linux 与视觉扩展

RV1106 面向视觉和多媒体场景。伙伴提供的 Linux/Buildroot 系统、设备树和媒体组件，会引入系统服务及镜像管理边界。具备 NPU 不等于可以本地运行完整对话大模型；本项目可先采用云端对话，再按需求增加本地视觉处理。[10][12][16]

## 3. 生态伙伴产品与现有 Linux 板卡分析

### 3.1 安信可：AiPi-Audio 与语音产品扩展

AiPi-Audio 使用 Ai-M61-32S 模组，主控为 BL618，外接 ES8388 Codec。伙伴提供录放音示例及原理图、PCB/BOM、固件资源，可连接外部显示。[11]

| 差异归属 | 需要确认/实现的内容 |
|---|---|
| 平台级 | 博流/伙伴 SDK、线程同步、网络/TLS、时间、存储、SDK HAL、构建与烧写 |
| 板级 | ES8388 驱动和连线、音频通道、电源、按键，以及外接屏幕与触摸 |
| 媒体契约 | 采样率、PCM 格式、DMA 缓冲、回调时序、是否存在有效播放参考 |
| 共享业务 | 会话状态、云事件、启停重试、配网协议和网页、兼容显示条件下的页面 |

AiPi-Audio 适合暴露硬编码的音频槽位与外设假设，但不应默认其整机交互与现有带屏三板一致。

安信可 Ai-WV02-32S 使用 BL618 + VB6824 语音算法处理器，可用于检验外部语音处理器接入。AiPi-PalChatV2 是更接近对话产品的候选，但公开页面存在不同模组描述，采购前必须核对实际 PCB、模组、原理图和对应源码，不能仅按产品名下单。[13][14]

### 3.2 Luckfox：Pico Ultra W

按伙伴公开资料，Pico Ultra W 使用 RV1106G3，配置 256 MB DDR3L、8 GB eMMC、1 TOPS NPU，提供无线、以太网、摄像头和显示接口，配套 Buildroot 系统及 SDK。[12]

| 差异归属 | 需要确认/实现的内容 |
|---|---|
| 平台级 | Linux 线程/时钟、TLS、文件持久化、音频与网络服务、目标 libc/ABI |
| 板级/系统配置 | 设备树、无线模组驱动、音频链路、外接屏幕、触摸与摄像头 |
| 产品交付 | 应用更新、根文件系统与完整系统镜像的不同构建/更新路径 |
| 共享业务 | 对话协调、云事件、媒体公共处理、配网业务与页面设计 |

这块板用于检验跨操作系统边界。不能只增加一个 `boards` 目录就认为完成适配。屏幕、录音链路和热点支持均需实际选配与验证，不按接口清单推断开箱可用。

当前已有树莓派 2 Model B 和 3 Model B，优先复用树莓派 3 承担通用 Linux 架构适配，Pico Ultra W 保留为后续 RV1106 专用媒体、ISP/NPU 验证目标。

### 3.3 树莓派 2 Model B 与 3 Model B：复用现有设备

树莓派是具体单板及其系统生态，不能与 ESP-IDF、FBB 或 Linux 操作系统作为同层概念比较。采用 Raspberry Pi OS 时，共性后端按 Linux 接口组织，具体设备与外设差异由板级配置提供。[18][19][20]

| 对比项 | Raspberry Pi 2 Model B | Raspberry Pi 3 Model B |
|---|---|---|
| 当前设备状态 | 用户已有，未做本项目适配 | 用户已有，未做本项目适配 |
| CPU/系统位数 | 存在不同修订；早期 BCM2836，后续修订采用其他 SoC，需核对实板 | BCM2837，支持 64 位 CPU；操作系统位数按 SDK 兼容性选择 |
| 内存 | 1 GB | 1 GB |
| 网络 | 板载以太网；Wi-Fi 需要外接适配器 | 板载以太网及 Wi-Fi |
| 显示/输入起步方式 | HDMI 显示器和鼠标，之后按需接触摸屏 | 同左，优先验证共享 LVGL 页面 |
| 录放音起步方式 | USB 麦克风/声卡与扬声器；确认实际设备 | 同左；3.5 mm 接口不作为麦克风输入 |
| 本项目用途 | 第二个 Linux 板级目标，检验同后端复用 | 首个 Linux 适配目标，替代当前采购计划中的 Luckfox |

选择树莓派 3 的理由是已有硬件、网络接口和公开系统资料，适合先验证云端对话与共享应用的 Linux 后端；这是适配价值判断，不是已经通过性能或稳定性验收。Pi 2 的 USB Wi-Fi 网卡还涉及驱动和热点模式支持，第一阶段可先走有线网络。

树莓派可以替代 Luckfox 的通用 Linux 架构验证角色，但不能覆盖 RV1106 的专用 NPU、ISP、媒体 API 或相同功耗特性。两款树莓派均不默认具备麦克风、触摸屏、有效回声参考或可用 AEC。[18][19]

系统镜像和 ConvAI SDK 必须一起选择：不要仅因 Pi 3 CPU 支持 64 位就固定使用 64 位系统。Pi 2 不同修订也不能统一按 ARM64 处理。先核实 ARM Linux SDK 的指令集、浮点 ABI、libc 与链接方式；若只满足部分目标，应明确支持范围。

## 4. 与现有三板及 Windows 模拟器的对比

现有配置依据 [当前板卡对比](board-comparison.md) 和 [平台适配说明](../src/platforms/README.md)，不推广为芯片能力上限。

### 4.1 平台级差异

| 项目 | 实战派/利尔达 ESP32-S3 | 润和 WS63 | 润和 Windows 模拟器 | 安信可 BL618 | Luckfox RV1106 |
|---|---|---|---|---|---|
| 当前/候选后端 | ESP-IDF / FreeRTOS | 当前 GoldieOS/LiteOS 服务 | GoldieOS 虚拟设备与 Windows 后端 | 待选定博流/伙伴 SDK | Linux / Buildroot |
| ABI/库约束 | ESP32-S3 Xtensa 库 | WS63 RISC-V 库 | Windows 原生库 | 需目标 ABI 兼容 SDK | 需匹配 CPU、libc 和链接方式的 SDK |
| 网络接入 | 当前 Wi-Fi | 当前 Wi-Fi | 使用电脑现有网络 | 需移植 Wi-Fi 服务 | 系统网络后端 |
| 持久化 | 当前 NVS | 当前内部 NV | 不验证实板 NV | 需选定并适配 | 文件/系统存储 |
| 适配状态 | 已有实现及用户验证记录 | 已有实现及用户验证记录 | 已有实现及用户验证记录 | 未移植 | 未移植 |

已有验证范围以 [验证记录](validation.md) 为准；不能将历史镜像验证结论扩展为每次最新架构修改、压力测试或新增平台验收。

WS63 不等于 GoldieOS。另一块使用原生 FBB/LiteOS 接口的 WS63 板，需要实现相应后端；只有服务接口与 ABI 兼容时，才能主要通过 `boards` 接入。

树莓派 2/3 与 Luckfox 的平台级共性是 Linux：线程、socket、文件存储、时间和应用生命周期可通过同一后端组织。差异包括用户空间 ABI、libc、音频/显示接口、网络管理方式和厂商媒体服务；不能因同为 Linux 就直接共用所有二进制或系统配置。两款树莓派的板级和运行环境对比见 3.3 节。

### 4.2 板级与运行环境差异

| 项目 | 实战派 | 利尔达 | 润和 WS63 | Windows 模拟器 | AiPi-Audio | Pico Ultra W |
|---|---|---|---|---|---|---|
| 显示/输入 | 当前 320×240、FT6336 | 当前 320×240、CST836U | 当前 320×240、CST816D | 虚拟显示/鼠标 | 需确定外接显示和输入 | 需确定外接显示和输入 |
| 音频器件/入口 | ES8311 + ES7210 | ES8311 + ES7210 | BSP AudioService、ES8311 | 电脑录放音设备 | ES8388 | 所选系统音频链路待核实 |
| 当前 mic/ref | TDM slot 0/1 | TDM slot 2/1 | 双通道 0/1 | 无实板参考通道 | 待核实 | 待核实 |
| 存储配置 | 16 MB Flash、8 MB PSRAM | 8 MB Flash、8 MB PSRAM | 容量未核实 | 主机资源 | 需核实模组版本 | 256 MB DDR3L、8 GB eMMC |
| 实体板专属工作 | IO 扩展器、引脚和时序 | IO 扩展器、引脚和时序 | 电源、总线和 BSP 设备 | 不验证实体驱动/射频 | Codec、连线、外设初始化 | 设备树、外设选配及驱动 |

## 5. 架构扩展建议

以下为建议，不表示已经新增这些平台实现。

| 边界 | 应承担的内容 | 不应固定的假设 |
|---|---|---|
| `src/application/core` | 会话状态、启停、重试与事件协调 | 必须有触摸屏，必须经过热点配网 |
| `src/application/services/cloud` | ConvAI 会话、鉴权与云事件转换 | 厂商 AI 方案能够直接替代本项目云链路 |
| `src/application/services/media` | 音频格式、缓冲和处理契约 | 固定槽位/采样率，主 MCU 必须执行所有算法 |
| 网络共享服务与网页 | 状态与配置业务、输入校验、页面资源 | 所有平台使用相同 DHCP/HTTP 实现 |
| `ports` | SDK/OS API、任务、TLS、时间、存储、网络和媒体服务 | 同 RTOS 或同 CPU 架构就能复用整个实现 |
| `boards` | 器件、引脚、总线、音频映射、电源与初始化 | 同芯片的板子外设完全一致 |
| 构建与交付 | 固定工具链、BSP、SDK 与目标配置 | 所有目标输出相同类型的裸烧镜像 |

### 5.1 使用能力描述控制交互

按目标组合声明显示、触摸、物理按键、网络扫描、SoftAP、以太网、蓝牙共享网络、回声参考、外部语音处理器与摄像头能力。能力决定入口是否存在，业务决定有该能力时的行为。

相同显示能力的目标继续复用页面；无触摸目标可以将按键映射到相同命令。无配网能力的 Windows 目标继续使用主机网络。网络可用性与 Wi-Fi 配网状态分开建模。

### 5.2 固定媒体与异步事件语义

明确音频数据所有权、采样率、通道、帧时长、回声参考、时间戳、回调线程、停止顺序及缓冲背压。平台或外部 DSP 输出已处理音频时，允许绕过重复处理。

平台任务向公共协调入口投递事件，由指定 owner 串行处理业务，避免在网络/录音/云回调中直接操作 UI 或重建会话。具体约束以当前媒体接口和应用实现为准，扩展前先核对已有契约。

### 5.3 云 SDK 是新增平台的前置条件

购买与移植前确认 ConvAI SDK 是否提供兼容目标 CPU/ABI、OS、工具链、C 库和链接方式的发布包，或是否允许源码移植。现有 ESP32-S3/WS63/Windows 库不能直接视为支持新增平台。

SDK 评估先于显示移植：先完成最小联网/TLS/云会话和音频链路，再接入公共界面，最后完成配网体验与恢复路径。

### 5.4 构建路径继续区分两类用户

开发者脚本在已准备环境中构建；外部使用的 skill 负责安装/定位依赖，再调用共同构建入口。按目标锁定 SDK、BSP、工具链、校验值和产物，不复制业务代码。Linux BSP 构建若需要 Linux 环境，应明确 WSL/容器/虚拟机要求，不承诺原生 Windows 可直接构建。

### 5.5 Linux 平台与树莓派板级代码边界

以下目录为未来实施建议，本文没有创建 Linux 实现或空目录。可按现有 `entry`、`components`、构建入口的职责组织 `src/platforms/linux`；树莓派板级名称可采用 `src/boards/raspberry_pi/{pi2_model_b,pi3_model_b}`，最终命名在实施时与仓库约定一致。

| 归属 | 具体内容 | Pi 2/3 复用规则 |
|---|---|---|
| `src/application/core`、`src/application/services` | 对话协调、云事件、媒体公共算法和配网业务 | 保持同一份逻辑，不加入树莓派型号判断 |
| `src/application/ui` | 页面布局、状态文案、控件事件和手机网页 | 复用兼容 LVGL 版本的页面；屏幕尺寸变化通过布局参数处理 |
| `src/platforms/linux/entry` | 进程启动、依赖组装、事件循环和退出清理 | 同一入口按目标配置组装 |
| `src/platforms/linux/components` | POSIX 线程/同步/时间、TLS、文件存储、SDK HAL、ALSA 等音频后端及显示/输入后端 | 按实际软件接口分后端，不能在每块板复制 ALSA 或网络管理实现 |
| Linux 网络适配 | 网络状态、扫描/连接、热点、HTTP/DNS 与所选系统网络管理服务的集成 | NetworkManager、wpa_supplicant/hostapd 等方案需选定并验证；差异属于软件后端 |
| 树莓派 `boards` | 目标能力、GPIO/外设连线、屏幕/触摸及音频设备选择、声道映射、板级初始化 | 新板优先改配置；使用稳定设备标识，避免固定声卡索引 |
| 板级系统配置资源 | 必需的设备树 overlay、GPIO/外设使能和安装清单 | 随具体板卡或外设组合维护，安装步骤由部署工具执行 |
| 构建目标与部署工具 | 32/64 位、sysroot、工具链、libc、依赖版本、服务安装、资源复制和运行权限 | 不把 ABI 选择藏进业务代码；区分应用构建、部署与完整系统镜像制作 |

ALSA、显示输入与网络管理是 Linux 软件后端，不应因首次在树莓派使用就归为树莓派私有业务。摄像头/NPU 等厂商专用 API 在未来作为可选平台组件接入；具体传感器、连线和硬件配置仍留板级。

推荐实施顺序：Pi 3 先使用以太网、USB 录放音、HDMI/鼠标完成云对话与公共 UI；再验证板载 Wi-Fi、热点、DHCP/DNS、自动弹窗与配置保存；最后接入 Pi 2 及触摸屏。热点与 STA 并发能力、回声消除和持续对话质量均需单独实测。

### 5.6 树莓派与 Luckfox：共用 Linux 平台，按实际接口选择后端

两者都适合采用“操作系统 + 用户态应用包”的模式。树莓派先选定 Raspberry Pi OS；本次调研的 Luckfox Pico Ultra W 配套 Linux/Buildroot SDK。[12][15][16][20] 从应用架构看，它们接近 Windows 原生应用：应用可以独立构建、部署和更新；完整系统镜像是另一层交付物。

以下是接入规划，不是现有 Linux 实现。当前工程仅支持 ESP-IDF 与 GoldieOS 目标，尚未构建或验证树莓派/Luckfox 应用。也不能把当前 GoldieOS Windows 模拟器当作独立 Linux 后端：它仍使用润和 Windows 虚拟设备与服务库。

#### 5.6.1 四类边界与复用规则

| 边界 | 共用内容 | 树莓派侧差异 | Luckfox 侧差异 | 组织规则 |
|---|---|---|---|---|
| 公共代码 `src/application` | 对话状态/协调、云事件、重试、媒体公共处理、配网业务及 LVGL 页面 | 输入来自鼠标/触摸等，映射为相同应用命令 | 同左；不能因增加视觉功能复制对话状态机 | 不加入板名条件分支；通过能力与回调适配 |
| Linux 平台后端 `src/platforms/linux` | 进程生命周期、POSIX 线程/同步/时钟、socket、文件持久化与 ConvAI HAL | 根据所选系统接入音频、显示和网络管理服务 | 根据裁剪系统和厂商 SDK 接入相应接口 | 相同软件接口共用实现；不同接口提供可选后端 |
| 板级配置 `src/boards` | 共用板级契约，而非共用所有驱动 | Pi 2/3 型号、GPIO、外设连接、设备选择与能力 | Pico Ultra W 型号、外设连接、传感器及设备选择 | 描述“接了什么、如何配置”，不复制网络管理器或媒体框架 |
| 构建与部署 | 构建入口、依赖校验和应用包组织方法 | 匹配 Pi 型号、系统位数、工具链与 sysroot | 匹配板卡 SDK、libc、sysroot 及媒体运行库 | 每个目标固定 ABI/依赖；区分应用包与完整系统镜像 |

树莓派 2 Model B 与 3 Model B 的具体硬件条件见 3.3 节。Pi 3 成为首个 Linux 目标，Pi 2 用来验证同后端跨板复用；Luckfox 保留为后续瑞芯微媒体/视觉扩展目标。

#### 5.6.2 常见差异应放在哪里

| 改动实例 | 所属位置 | 判断依据 |
|---|---|---|
| 两块板都通过 ALSA 录放音 | 共用 Linux ALSA 后端 | 使用相同软件接口，无需按板复制 |
| 声卡设备名、采样通道与回声参考不同 | 板级/目标配置 | 是具体设备选择与数据布局差异；避免固定声卡序号 |
| 需要瑞芯微专用音频/视频 API | Linux 可选媒体后端 | 属于厂商软件接口差异，不放入板级业务代码 |
| 桌面窗口、DRM/KMS 或其他显示接口不同 | Linux UI 后端 | 按实际显示接口组织；不将所有显示实现绑定树莓派或 Luckfox 品牌 |
| 屏幕型号、分辨率、旋转、触摸校准不同 | 板级/目标配置；必要时增加设备驱动 | 不改变公共按钮语义；保留逻辑布局并配置缩放/居中 |
| NetworkManager 与 wpa_supplicant/hostapd 等接入不同 | Linux 网络后端 | 差异来自系统服务，不是 GPIO 或板型 |
| 无线网卡、网口选择、热点能力不同 | 板级/目标能力配置 | 能力需实测；不能因板上有 Wi-Fi 就默认支持热点或 AP/STA 并发 |
| 摄像头传感器、连线、设备树 overlay | 板级配置与安装资源 | 硬件描述随具体外设组合维护 |
| RKNN、ISP 或厂商媒体处理接口 | 可选 Linux 平台组件 | 厂商 API 与具体传感器配置分离，公共应用通过能力接口调用 |
| 动态库、设备权限、服务安装及自启动 | 部署工具与部署配置 | 不在业务运行时临时安装依赖或修改系统配置 |

ALSA、网络管理器和具体显示驱动均为候选接入方式，尚未确认在每一套目标系统中可用。Linux 后端能复用多少，要根据实际系统和外设验证，不能承诺接入 Luckfox 时只修改 `boards`。

#### 5.6.3 建议目录与依赖方向

下面仅表达职责；实施时按真实需求创建目录，不提前添加空平台或空组件。

```text
src/application/                       # 共用应用、服务和 LVGL 页面
src/platforms/linux/
├── entry/                             # 启动、组装、事件循环与退出
├── components/
│   ├── runtime/                       # Linux 基础运行能力
│   ├── convai/                        # Linux SDK HAL
│   ├── media/                         # ALSA/按需增加的厂商后端
│   ├── network/                       # 实际系统网络管理后端
│   └── ui/                            # 显示与输入后端
└── targets/                           # 工具链、ABI 与依赖组装
src/boards/
├── raspberry_pi/{pi2_model_b,pi3_model_b}/
└── rockchip/luckfox_pico_ultra_w/
tools/                                 # 共用构建、安装和镜像制作工具
```

入口按目标选择板级配置和平台后端，并将回调交给公共协调模块。公共应用不直接调用 ALSA、网络管理器、RKNN 或 GPIO；板级文件不承担云会话与配网业务。更换后端不改变 Start/Stop 等公共命令语义。

#### 5.6.4 应用包、系统镜像与验证

默认构建应用包：Linux ELF 可执行文件、必要运行库、资源、配置模板及安装说明/脚本。应用包安装在已有系统中；需要整机交付时，再通过单独镜像流程加入 OS、驱动/设备树配置、应用和开机启动服务。制作应用包不等于重建内核，也不要求每次更新应用都重新写整张系统卡。

共用 Linux 源码不保证同一个二进制可运行。树莓派和 Luckfox 都需固定 CPU 指令集、32/64 位、浮点 ABI、libc、动态加载器和依赖版本，取得匹配的 ARM Linux ConvAI SDK。Pi 2 不同修订还需先确认实际 CPU，不能将 Pi 3 的 ARM64 程序直接用于所有 Pi 2。

验证顺序：先在 Pi 3 验证程序加载、共享页面、USB 录放音和真实云对话；再验证 Wi-Fi 配网、保存恢复、自启动与日志；用 Pi 2 检验板级复用；接入 Luckfox 时先复用通用 Linux 能力，再单独验证厂商媒体、摄像头与 NPU。UI 替身、编译成功和完整功能验收分别记录。

## 6. 生态可用性与采购建议

评价维度为公开源码、文档和硬件资料、版本管理、构建/烧写路径、完整录放音及显示配置、对本项目架构的覆盖。没有实测厂商支持响应，因此不作绝对生态优劣或量产可靠性排名。

| 候选 | 已有生态证据 | 本项目优先级与理由 | 下单前核实 |
|---|---|---|---|
| Beken BK7258 官方 AI Toy/AIDK 套件 | 官方底层 SDK 与 AI 方案公开，固定标签、硬件资源、构建和烧写路径明确 | 第一批：云端语音最接近，可检验多核、音频和 SDK 边界 | 必须与当前开源方案对应；双屏形态与当前横屏 UI 不同；确认 ConvAI SDK |
| 已有 Raspberry Pi 3 Model B | 官方硬件和系统文档公开，已有实体板 | Linux 第一优先：替代 Luckfox 的通用 Linux 适配角色，无需新增主板采购 | ARM Linux SDK、系统位数、microSD/电源、USB 音频和显示输入配套 |
| 已有 Raspberry Pi 2 Model B | 与 Pi 3 共用 Linux 接口体系，已有实体板 | Pi 3 跑通后的第二板验证，不新增主板采购 | 实板修订及 ABI；可先用有线网络，USB Wi-Fi 需核对驱动/热点支持 |
| SiFli SF32LB52-DevKit-LCD 套件 | SDK、版本记录、板卡资料和语音方案公开 | 第一批可选：检验 RT-Thread、图形和低功耗产品；若只做同等 Wi-Fi 对话则需先确认联网配置 | LCD/触摸/喇叭是否含在套件；具体模组、Flash/PSRAM 和 Wi-Fi/蓝牙联网方式 |
| Luckfox Pico Ultra W | 板卡 Wiki、SDK、镜像和 BoardConfig 公开 | 暂缓采购；出现 RV1106 专用视觉、ISP/NPU 或媒体需求时再评估 | 选择 W 型；配齐显示、输入及录音；接受 Linux 构建环境；确认 SDK libc/ABI |
| 安信可 AiPi-Audio / 对应 BL618 语音套件 | 上游 SDK 与伙伴硬件、录放音资料公开 | 第二批；若近期有博流生态合作可提前 | 不按旧产品名推断新 PCB；核对源码、外接显示和回声参考；语音处理器接口能否开放 |
| Realtek Ameba 具体板卡 | 官方 RTOS 仓与工具链路径公开 | 有明确伙伴产品时再采购 | 本轮未锁定带屏完整音频板，不仅凭芯片或 Wi-Fi 模组选择 |

采购组合建议：

1. 只买一套，且重点是现有云端对话：优先 BK7258 官方 AI 套件，先确认云 SDK 可支持。
2. MCU + Linux 组合：购买 BK7258，复用已有 Pi 3 Model B；仅补齐缺少的电源、microSD、录放音和显示输入设备，暂不购买 Luckfox。
3. 需要新增两类 MCU 时：BK7258 + SF32LB52-DevKit-LCD 套件；若必须独立 Wi-Fi 联网，先核对思澈实际套件配置，否则可改选资料齐全的 BL618 语音套件。
4. 同平台第二板验证：先复用已有 Pi 2 Model B，检验 Linux 后端复用和板级差异；不为这一目标重复采购 Linux 主板。
5. 后续明确需要瑞芯微专用视觉和媒体链路时，再采购 Luckfox。Pi 3 的验证不能替代这部分硬件验收。

不把官方开发套件算作第三方生态伙伴产品；本次第三方伙伴分析对象是安信可和 Luckfox。公开资源充足只说明具备评估基础，不等于已验证从干净 Windows 完成全链路构建。

## 7. 新平台验收顺序

1. 固定板卡版本、SDK/BSP、工具链及 ConvAI SDK 支持方式。
2. 从干净环境复现厂商示例构建与烧写，记录额外二进制依赖。
3. 完成网络、时间同步、证书校验和最小云连接。
4. 验证录音、播放、声道、回声参考及停止恢复。
5. 接入共享协调层和 UI；按实际能力接入配网。
6. 实测首次配网、断网恢复、会话失败重试、连续对话与重启后配置保存。
7. 同 SDK 后端适配第二块板，检验是否主要修改 `boards`；若仍改动 `ports`，记录真实接口差异后再抽取共性。

## 8. 参考资料

链接按调研时公开资料保留；SDK/板卡版本持续演进，实施时应固定标签或提交，不直接以开发分支作为可复现依赖。

1. [ESP-Brookesia 官方仓库][1]
2. [HiSpark FBB_WS63 官方 README][2]；[海思 WS63 官方文档](https://docs.hisilicon.com/repos/fbb_ws63/zh-CN/master/)
3. [Beken BK7258 AI Solution 官方仓库、套件入口与版本约束][3]
4. [Beken AI Solution 架构说明][4]；[底层 BK_AVDK_SMP 仓库](https://github.com/bekencorp/bk_avdk_smp)
5. [BouffaloSDK 官方仓库][5]
6. [Realtek Ameba RTOS 官方仓库][6]
7. [SiFli SDK 官方仓库][7]；[版本发布](https://github.com/OpenSiFli/SiFli-SDK/releases)
8. [思澈 AI 小智能力说明][8]
9. [思澈小智快速入门与蓝牙共享网络说明][9]
10. [Rockchip RV1106 官方简版数据手册][10]
11. [安信可 AiPi-Audio 产品与开发资源][11]；[官方测试固件指南](https://docs.ai-thinker.com/_media/open_hard/voice-audio/aipi-audio_development_board_test_firmware_user_guide.pdf)
12. [Luckfox Pico Ultra 系列参数][12]
13. [安信可 Ai-WV02-32S 模组][13]
14. [安信可 AiPi-PalChatV2 产品页][14]；[中文说明](https://docs.ai-thinker.com/aipi-palchatv2/instructions.html)
15. [Luckfox Pico SDK 官方仓库][15]
16. [Luckfox SDK 构建与 BoardConfig 说明][16]
17. [SiFli 开发板型号与配置指南][17]
18. [Raspberry Pi 3 Model B 官方产品资料][18]
19. [Raspberry Pi 官方硬件与板卡修订说明][19]
20. [Raspberry Pi OS 官方文档][20]；[入门与音频设备说明][21]

[1]: https://github.com/espressif/esp-brookesia
[2]: https://gitee.com/HiSpark/fbb_ws63/blob/master/README.md
[3]: https://github.com/bekencorp/bk_solution_ai
[4]: https://docs.riselink.ai/arminodoc/bk_ai_smp/bk7258/en/v3.1.1/intro/index.html
[5]: https://github.com/bouffalolab/bouffalo_sdk
[6]: https://github.com/Ameba-AIoT/ameba-rtos
[7]: https://github.com/OpenSiFli/SiFli-SDK
[8]: https://wiki.sifli.com/application/xiaozhi.html
[9]: https://docs.sifli.com/projects/xiaozhi/get-started/
[10]: https://www.rock-chips.com/uploads/pdf/2022.8.26/191/RV1106%20Brief%20Datasheet.pdf
[11]: https://wxkf.ai-thinker.com/en/voice-audio/
[12]: https://wiki.luckfox.com/Luckfox-Pico-Ultra/
[13]: https://docs.ai-thinker.com/en/ai-wv02/index.html
[14]: https://docs.ai-thinker.com/en/aipi-palchatv2/index.html
[15]: https://github.com/LuckfoxTECH/luckfox-pico
[16]: https://wiki.luckfox.com/Luckfox-Pico-Pi/SDK/
[17]: https://wiki.sifli.com/en/board/devkit_guide.html
[18]: https://www.raspberrypi.com/products/raspberry-pi-3-model-b/
[19]: https://www.raspberrypi.com/documentation/computers/raspberry-pi.html
[20]: https://www.raspberrypi.com/documentation/computers/os.html
[21]: https://www.raspberrypi.com/documentation/computers/getting-started.html
