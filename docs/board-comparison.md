# 海思 WS63 润和与乐鑫 ESP32-S3 产品差异对比

整理日期：2026-10-10。依据当前 `unified-app` 的源码、构建配置和已记录的实板日志。

海思与乐鑫主要在工具链、OS/BSP、驱动和固件流程上不同；两块乐鑫板的差异集中在存储、外设引脚、IO 扩展器、屏幕、触摸、音频声道和初始化时序。三款产品共用对话业务、云适配与页面设计，平台和硬件差异留在适配层。

以下以当前工程实际使用的配置和实现为准，不把原代码注释中提到的全部外设视为已适配功能。

## A. 阅读顺序：平台、板卡、应用实现

先比较海思与乐鑫的软件平台，再比较三款产品的板级硬件，最后比较独立应用的实现与复用。GUI、配网、缓冲大小属于当前项目的选择，不代表某个芯片只能采用这些方案。下文依据本地 BSP/源码与构建结果；未核实的硬件容量不作推断。

### A0. 先区分芯片平台、板型与运行环境

Hoperun（润和）是单板/BSP 提供方，WS63 是海思芯片平台，二者不是并列的芯片平台。下文的 GoldieOS、ST7789、AW9523B、音频布局和厂商打包工具均指当前 Hoperun 产品实现，不能代表所有 WS63 生态单板。

| 层次 | 海思侧 | 乐鑫侧 | 新产品应修改什么 |
|---|---|---|---|
| 芯片平台 | WS63、RISC-V 工具链、SDK ws63 库、海思 NV/Wi-Fi 接口 | ESP32-S3、Xtensa、ESP-IDF、SDK esp32s3 库 | 跨芯片/OS 改 ports；同芯片兼容后端尽量复用 |
| 当前软件后端 | src/platforms/goldieos：GoldieOS OSAL/服务 ABI、lwIP、mbedTLS | src/platforms/esp_idf：FreeRTOS/IDF 服务 | GoldieOS 是当前依赖，不是 WS63 的唯一软件栈 |
| 具体板级 | src/boards/ws63/hoperun：电源、显示传输、音频布局、BSP 路径 | src/boards/esp32/szpi、src/boards/esp32/lrd：引脚、Codec、显示和初始化 | 新增 src/boards/<vendor_board> 并实现板级接口 |
| 桌面运行环境 | src/platforms/goldieos/targets/windows：Windows SDK/HAL + Hoperun 虚拟设备 | 当前没有对应 ESP 桌面目标 | 模拟器是 Windows 原生程序，不执行 WS63 固件 |
| 共享应用 | core、services、src/application/ui/ai_lvgl.c、src/application/ui/web | 同一份源码 | 业务与界面功能优先修改共享层 |

WS63_BOARD=hoperun 选择 src/boards/ws63/hoperun/board.cmake；替换兼容 BSP 的生态单板主要修改 boards。板级接口位于 src/boards/ws63/include/ws_board.h，平台后端仍要求当前 GoldieOS AudioService/Wlan_Drv/WifiService ABI，服务获取不再属于 boards；不同 RTOS 或不兼容服务需要额外后端适配，不能只改板名。

### A1. 平台差异：海思与乐鑫

| 维度 | 海思 WS63：润和示例栈 | 乐鑫 ESP32-S3：两块板共用 |
|---|---|---|
| 目标指令集/ABI | BSP 编译选项 rv32imfc、ilp32f，RISC-V GCC 7.3 | Xtensa ESP32-S3，当前 ESP-IDF 6.1 工具链 GCC 15.2 |
| 操作系统与服务 | GoldieOS 的 goldie_osal/LiteOS 封装及 Audio/WiFi/RenderService | ESP-IDF/FreeRTOS 与组件接口 |
| GUI 栈 | 独立应用使用 LVGL 9.5.0 + 润和显示/触摸适配；旧设置页使用原生 GUI | 相同 LVGL 9.5.0、相同页面源码 + IDF 显示/触摸适配 |
| SDK 库 | SDK 26.8.0 的 libs/ws63/libconvai_sdk.a | 同版 libs/esp32s3/libconvai_sdk.a，两板共用 |
| SDK HAL | goldie 线程/信号量、lwIP socket、mbedTLS、NTP | FreeRTOS、ESP 网络/TLS 接口、SNTP |
| 时间与 TLS | NTP 同步，SDK CA 或私有配置 PEM；缺少 CA 拒绝连接 | SNTP，同步后连接；SDK CA 或 ESP 证书包 |
| 构建组织 | 使用工程内固定 GoldieOS 运行归档和预编译板级库，CMake/Ninja | IDF 工程，组件 manifest 与 lock 文件管理依赖 |
| 固件生产 | 该 BSP 的 Windows 链接/签名/打包工具生成 fwpkg，配合 BurnTool | idf.py/esptool，按 bootloader、分区表、应用偏移烧录 |
| 本项目配置 | config/ws63.local.h | 各自 build 目录的 sdkconfig/menuconfig |

工具链名字包含 linux-musl 不表示 WS63 固件运行 Linux；本工程运行时接口来自 GoldieOS/LiteOS。WS63 的 Windows 打包要求是这份 BSP 的工具约束，不应扩展为海思所有平台的结论。两种 ABI 不兼容，不能互用静态库或直接互刷固件。

### A2. 三块板的硬件与驱动差异

| 项目 | 润和 WS63 | 实战派 ESP32-S3 | 利尔达 ESP32-S3 |
|---|---|---|---|
| 显示 | ST7789 SPI，应用横屏 320×240 | ST7789 SPI，320×240 | ST7789 SPI，320×240 |
| 触摸 | CST816D，0x15 | FT6336，0x38 | CST836U，0x15 |
| IO 扩展器 | AW9523B，0x5B | PCA9557，0x19 | TPT29555A，0x22 |
| 音频驱动 | ES8311，0x18，由 BSP AudioService 提供数据 | ES8311 + ES7210，ADC 地址 0x41 | ES8311 + ES7210，ADC 地址 0x40 |
| 采集布局 | 8 kHz PCM16 双通道，L=麦克风/R=播放参考 | 四槽 TDM，mic=slot0/ref=slot1 | 四槽 TDM，mic=slot2/ref=slot1 |
| 播放接口 | AudioService，8 kHz 单声道 PCM16 | 共用 Codec 驱动，双声道 PCM16 | 同实战派 |
| Flash | 当前材料未核实实际容量，不按镜像大小推断 | 配置 16 MB | 配置 8 MB |
| PSRAM | 当前材料未核实，不据此判断有无 | 8 MB Octal | 8 MB Quad |
| 电源/输入 | AW9523B 开关和电源保持、GPIO 键盘、触摸 | BOOT GPIO0 + 触摸 | 当前应用主要使用触摸 |

同为 0x15 的触摸芯片也不是同一型号，不能仅按 I2C 地址共用驱动。WS63 原 BSP 还包含 RTC、SLE、电池等支持；这些不等于当前最小 AI 应用已提供对应功能。ESP32 的具体引脚、复位顺序和存储分区见后面的 1–8 节。

### A3. 旧 WS63 示例与新独立应用

| 维度 | 旧 WS63 示例 | 新 WS63 独立应用 | 两块 ESP32 独立应用 |
|---|---|---|---|
| 入口 | 系统设置中的 AI 设置/对话页 | 开机直接 Unified AI | 开机直接 AI 页面 |
| 页面功能 | 设置体系及原 AI 功能 | 状态、Start/Stop、Wi-Fi | 状态、Start/Stop、Wi-Fi |
| 状态与 SDK 生命周期 | sdk_integration 旧 bridge | 公共 core + cloud 原文件 | 相同公共源文件 |
| UI 设计 | 设置页自己的布局 | 共享 ai_lvgl.c / ai_screen.h，LVGL 绘制 | 相同 ai_lvgl.c / ai_screen.h，LVGL 绘制 |
| 配网 | 原系统配置入口 | 热点 + 共用手机网页，内部 NV 双快照保存账号（不依赖 SD） | 相同热点 + 手机网页，NVS 保存账号 |
| 播放缓冲 | 原下行模块独立管理 | 16000 字节 G711A，约 2 秒 | 默认 256 KiB G711A，约 32 秒 |
| 音频能力 | 原程序有更多模式/诊断分支 | 只接入 G711A 最小对话链路 | 相同最小对话链路 |
| 验证阶段 | 原示例基础 | 用户确认清理/分层版显示、配网和对话正常 | 利尔达、实战派均已烧写，用户确认显示、配网和对话正常 |

原程序支持的更多模式不是芯片能力差异。统一架构保留 OS/媒体/UI 渲染适配，不强行把 WS63 改成 ESP-IDF 或让两者共用硬件驱动。新 WS63 仍保留 BSP 库与必要依赖，未声称消除了所有未使用的厂商预编译代码。

### A4. 差异应该放在哪里

| 改动类型 | 所属位置 | 是否跨海思/乐鑫复用 |
|---|---|---|
| 会话策略、启停、超时、云事件 | src/application/core/、src/application/services/cloud/ | 直接复用 |
| G711A、输入槽选择算法、播放环形缓冲 | src/application/services/media/ | 直接复用；参数/存储由平台提供 |
| 页面布局、配色、按钮/状态文案 | src/application/ui/include/ai_screen.h | 直接复用 |
| UI 控件与事件 | src/application/ui/ai_lvgl.c | 直接复用，平台间不复制页面 |
| 手机配网界面 | src/application/ui/web/ | 直接嵌入同一份资源，统一最小功能 |
| 配网输入校验 | src/application/services/network/ | 直接复用；HTTP/驱动/存储调用留在平台 |
| 触摸采样与显示刷新 | src/platforms/esp_idf/components/ui/ai_ui.c 与 src/platforms/goldieos/components/ui/ui.c | 当前 Hoperun 板级适配分块同步写屏，ESP 等待 DMA 回调 |
| OS、网络、TLS、采集/播放任务 | src/platforms/esp_idf/ 与 src/platforms/goldieos/targets/ws63/ | 接口语义一致，平台实现分开 |
| ESP 板型引脚、屏幕、Codec 时序 | src/boards/esp32/szpi/、src/boards/esp32/lrd/、src/boards/esp32/common/esp32s3/ | 同平台两板按实际硬件共用 |
| WS63 板级驱动和预编译系统库 | 工程内 GoldieOS 归档，构建时解压 | 保留厂商实现；需单独管理依赖版本 |

增加 ESP32-S3 伙伴板时优先扩展 boards 与配置；增加另一块兼容 WS63 板时在 boards 下提供 BSP/AudioService/显示和电源接口适配；增加其他芯片平台时实现相同的云媒体接口与事件投递约束。不要在 src/application/core/cloud 中加入厂商条件分支。

实现与构建入口见 [WS63 独立应用说明](../src/platforms/goldieos/README.md)，[组装代码](../src/platforms/goldieos/entry/app.c)，[媒体适配](../src/platforms/goldieos/components/media/media.c)，[公共页面设计](../src/application/ui/include/ai_screen.h)。当前运行依赖范围和逐文件校验见 [依赖说明](../third_party/goldieos/README.md)；构建脚本位于 src/platforms/goldieos/cmake，厂商驱动/服务头由固定归档提供。

### A5. Hoperun 实板与 Windows 模拟器

| 对比项 | Hoperun WS63 单板 | Hoperun Windows 模拟器 |
|---|---|---|
| 运行目标 | WS63 RISC-V 固件 | Windows x64 原生 EXE，MinGW 编译 |
| 构建入口 | src/platforms/goldieos/targets/ws63 | src/platforms/goldieos/targets/windows |
| 输出目录 | build-hoperun-app | build-hoperun-win |
| 交付产物 | out/unified_ai_hoperun.fwpkg；bin 不应推测地址裸烧 | unified_ai_simulator.exe |
| SDK 二进制 | libs/ws63/libconvai_sdk.a | libs/windows/libconvai_sdk.a |
| 界面复用 | ai_lvgl.c / ai_screen.h，物理屏幕与触摸 | 相同布局与状态机，BSP 虚拟屏幕/鼠标输入 |
| 配网入口 | 保留 Wi-Fi 按钮、自动热点、共用手机网页 | AI_UI_NO_PROVISION 隐藏 Wi-Fi；不启动热点/HTTP/DNS |
| 网络 | 实际无线扫描、连接、内部 NV 保存网络 | 直接使用 Windows 当前网络，不验证无线配网和 NV |
| 采集 | 当前板级布局：双通道 PCM16，mic=0/ref=1 | 默认系统录音设备，mic=0，参考为 AI_AUDIO_NO_REFERENCE |
| 播放 | BSP AudioService → 实际 Codec/扬声器 | Windows 虚拟音频服务 → 电脑输出设备 |
| 电源 | AW9523B 电源保持和 GPIO 键盘 | 不执行实板电源/GPIO 初始化 |
| 时间/TLS | NTP 时间接入 mbedTLS，强制证书校验 | Windows 时间与 Windows SDK HAL，仍校验证书 |
| 鉴权 | 编译时 WS63_LOCAL_CONFIG，默认 config/ws63.local.h | 默认复用同一私有头文件；convai.cfg 不用于本应用鉴权 |
| 可验证范围 | 显示/触摸、射频配网、门户、存储、音频和对话 | 界面、状态机、Windows 云连接与录放音 |
| 验证限制 | 需逐板实测，不能只凭编译通过 | 不验证单板引脚、无线热点、Codec、时序和硬件电源 |

两种运行环境都使用 src/boards/ws63/hoperun 接入对应 BSP 服务，但 Windows 分支不操作实体板硬件。应用和公共媒体算法复用不等于两个目标的驱动、ABI 或固件可互换。三块单板的显示、配网、对话及模拟器的界面、对话均已由用户逐一确认正常；不扩大为压力或全硬件验收。

## B. 同属乐鑫平台的两款产品：详细差异

以下保留实战派与利尔达的逐项对比。

## 1. 主控、存储与构建配置

| 项目 | 立创实战派 `szpi` | 利尔达 `lrd` | 对软件的影响 |
|---|---|---|---|
| 主控 | ESP32-S3 | ESP32-S3 | 共用 ESP-IDF 平台代码和 SDK 库 |
| Flash | 16 MB | 8 MB | 使用不同的 Flash 配置和分区表 |
| PSRAM 容量 | 8 MB | 8 MB | 两者均可用于播放缓冲等内存分配 |
| PSRAM 接口 | Octal，八线 | Quad，四线 | 启动配置不同，不能直接互刷镜像 |
| PSRAM 配置频率 | 80 MHz | 80 MHz | 当前相同 |
| 构建选择 | `UNIFIED_BOARD=szpi` | `UNIFIED_BOARD=lrd` | 决定编译哪套板级驱动 |
| 构建目录 | `build-szpi/` | `build-lrd/` | 分别保存 sdkconfig、缓存和产物 |

对应配置：[实战派默认配置](../config/szpi.defaults)、[利尔达默认配置](../config/lrd.defaults)。

分区上的主要区别：

| 分区 | 实战派 | 利尔达 |
|---|---|---|
| `nvs` | 起始 `0x9000`，大小 `0x4000` | 相同 |
| `otadata` | 起始 `0xD000`，大小 `0x2000` | 相同 |
| `phy_init` | 起始 `0xF000`，大小 `0x1000` | 相同 |
| `ota_0` | 起始 `0x20000`，大小 `0x3F0000` | 起始 `0x20000`，大小 `0x370000` |
| `ota_1` | 起始 `0x410000`，大小 `0x3F0000` | 起始 `0x390000`，大小 `0x370000` |
| `assets` | 起始 `0x800000`，大小 8 MiB | 起始 `0x700000`，大小 1 MiB |

这些分区是当前保留的存储布局，并不表示最小应用已实现 OTA 或资源管理功能。

分区定义：[实战派分区表](../config/szpi.partitions.csv)、[利尔达分区表](../config/lrd.partitions.csv)。

## 2. I2C 与 IO 扩展器

| 项目 | 实战派 | 利尔达 |
|---|---|---|
| I2C 控制器 | I2C1 | I2C0 |
| SDA | GPIO1 | GPIO48 |
| SCL | GPIO2 | GPIO47 |
| 板级 I2C 速率配置 | 400 kHz | 100 kHz |
| IO 扩展器 | PCA9557 | TPT29555A |
| 扩展器地址，7 位 | `0x19` | `0x22` |
| 扩展器组织方式 | 单组 8 位 IO | 两组 8 位端口 |
| 功放使能 | PCA9557 bit1 | TPT29555A P1_0 |
| LCD 片选 | PCA9557 bit0 | 直接连接 GPIO10 |
| LCD 硬件复位 | 当前未通过扩展器控制 | TPT29555A P1_5 |
| 触摸复位 | 依赖板上上电复位 | TPT29555A P1_3 |
| 触摸中断 | 当前统一实现不使用 | 定义了 P1_4，当前统一实现不使用 |

板级速率用于扩展器和触摸等设备配置；音频 Codec 的 I2C 访问由 `esp_codec_dev` 适配层管理，不能简单理解为所有设备都强制使用同一速率。

两个扩展器的寄存器布局和端口控制方式也不同，因此保留独立驱动：

- [PCA9557 驱动](../src/boards/esp32/szpi/pca9557.c)
- [TPT29555A 驱动](../src/boards/esp32/lrd/tpt29555a.c)

## 3. LCD 显示

两块板都使用 ST7789，统一应用最终都按 **320×240 横屏、RGB565** 绘制，因此页面布局共用。底层连接和初始化存在以下差异：

| 项目 | 实战派 | 利尔达 |
|---|---|---|
| SPI 控制器 | SPI3 | SPI2 |
| MOSI | GPIO40 | GPIO13 |
| SCLK | GPIO41 | GPIO12 |
| DC | GPIO39 | GPIO16 |
| CS | 经 PCA9557 bit0 控制 | GPIO10，由 SPI 驱动控制 |
| SPI Mode | Mode 2 | Mode 0 |
| SPI 时钟配置 | 80 MHz | 40 MHz |
| SPI 事务队列深度 | 3 | 5 |
| 背光引脚 | GPIO42 | GPIO17 |
| 背光有效电平 | 低电平点亮 | 高电平点亮 |
| LCD 复位方式 | 调用面板驱动复位接口，再通过扩展器选中 LCD | 扩展器拉低复位 20 ms，拉高后等待 50 ms |

目前两者的 ST7789 调整寄存器序列基本相同，均设置横屏方向和颜色反转；主要差异是总线、片选、复位与背光。

当前背光实现是 GPIO 开关，尚未使用 PWM 调光。80 MHz 与 40 MHz 是总线配置差异，不能直接推断实际界面帧率相差一倍。

对应代码：[实战派 LCD 初始化](../src/boards/esp32/szpi/lcd_init.c)、[利尔达 LCD 初始化](../src/boards/esp32/lrd/lcd_init.c)。

## 4. 触摸与按键交互

| 项目 | 实战派 | 利尔达 |
|---|---|---|
| 触摸芯片 | FT6336 | CST836U |
| I2C 地址，7 位 | `0x38` | `0x15` |
| 触摸复位 | `board_touch_reset()` 无额外操作 | 经 TPT29555A 主动复位 |
| 复位时序 | 使用上电复位 | 拉低 20 ms，释放后等待 200 ms |
| 当前代码允许的触点计数上限 | 5 | 2 |
| 应用实际读取触点 | 第一个触点 | 第一个触点 |
| 物理启停按键 | GPIO0 BOOT，低有效 | 当前适配不启用物理按键 |
| 对话启停方式 | 触摸按钮或 BOOT 短按 | 触摸按钮 |

这里的“触点计数上限”是当前代码的过滤条件，不是对芯片实际多点触摸能力的声明。最小应用没有多点手势功能。

两块板当前共用触摸读取逻辑：

- 从寄存器 `0x02` 开始读取触点信息。
- 通过轮询获取状态。
- 共用横屏坐标变换：`x = raw_y`，`y = 239 - raw_x`。
- 由独立触摸任务向 UI 传递状态。

实现位于 [公共板级代码](../src/boards/esp32/common/esp32s3/board.c)。不同芯片目前可使用这套最小读取方式，并不意味着后续手势、中断或休眠功能也能完全共用。

## 5. 音频硬件与引脚

两块板都使用 **ES8311 输出 + ES7210 输入**，因此共用 Codec 驱动。但接线和输入数据选择不同。

| 项目 | 实战派 | 利尔达 |
|---|---|---|
| I2S 控制器 | I2S0 | I2S0 |
| MCLK | GPIO38 | GPIO42 |
| BCLK | GPIO14 | GPIO41 |
| WS/LRCK | GPIO13 | GPIO39 |
| DIN，麦克风数据输入 | GPIO12 | GPIO40 |
| DOUT，扬声器数据输出 | GPIO45 | GPIO38 |
| ES8311 地址，7 位 | `0x18` | `0x18` |
| ES7210 地址，7 位 | `0x41` | `0x40` |
| 功放控制 | PCA9557 bit1，高有效 | TPT29555A P1_0，高有效 |
| 输入采集布局 | 四个 TDM slot | 四个 TDM slot |
| 上行人声选择 | **slot0** | **slot2** |
| 上行播放参考选择 | **slot1** | **slot1** |

麦克风 slot0/slot2 的区别是关键音频适配项。如果直接复制另一块板的映射，即使 I2S 读取成功，也可能上传错误的输入通道。

当前数据处理路径为：

```text
四个 TDM slot
    ↓ 按 board_profile 选择
人声通道 + 播放参考通道
    ↓ 共用音频转换
G711A 平面声道数据
    ↓
ConvAI SDK
```

两块板共用的音频设置包括：

- 8 kHz 采样率、16 位有效 PCM 数据。
- ES7210 四 slot 采集，ES8311 标准 I2S 双声道输出。
- G711A 编解码与上行数据组织。
- 下行缓冲、预缓冲、打断清理和播放结束处理。
- 当前驱动设置的输入增益。

通道选择分别定义在 [实战派 profile](../src/boards/esp32/szpi/profile.c) 和 [利尔达 profile](../src/boards/esp32/lrd/profile.c)。这里保留播放参考通道，但应用没有新增本地 AEC 算法。

## 6. 音频初始化时序

| 项目 | 实战派 | 利尔达 |
|---|---|---|
| `BOARD_CODEC_PREENABLE` | `0` | `1` |
| 创建并配置 I2S 后 | 不提前使能 TX/RX | 先使能 TX/RX |
| 后续 Codec 打开过程 | 交由 Codec 适配过程继续管理 | 在预使能基础上打开 Codec |

这是当前保留的板级初始化策略，代码位于 [共用音频驱动](../src/boards/esp32/common/esp32s3/audio_codec_lckfb.c)。

此前实战派启动日志出现过两条“尚未启用的 I2S 通道被 disable”的提示，随后 Codec 打开和音频初始化成功；利尔达对应日志没有这两条提示。需要区分“初始化时序提示”和“录放音失败”，不能仅凭这两条日志判断对话功能不可用。

## 7. 应用界面与业务层

| 内容 | 当前差异 |
|---|---|
| 页面布局 | 无差异，共用一份 LVGL 页面 |
| 板型名称 | 分别显示 `SZPI ESP32-S3`、`LRD ESP32-S3` |
| Start/Stop、Wi-Fi 按钮 | 相同 |
| 状态机与错误处理 | 相同 |
| 配网与时间同步 | 相同 |
| ConvAI SDK | 同一版本 26.8.0、同一个 ESP32-S3 静态库 |
| 鉴权逻辑 | 相同；两套构建目录各自保存配置值 |
| 音频缓冲和云端打断策略 | 相同 |
| 额外输入 | 实战派支持 BOOT 启停 |

两次烧写时的 COM6、COM8 等端口编号属于电脑的设备枚举结果，不是应用架构或板型的固定参数。

## 8. 差异在代码中的分布

| 差异类型 | 主要文件 |
|---|---|
| 引脚、地址、SPI 参数、板级开关 | `src/boards/<板型>/board_pins.h` |
| I2C 与扩展器初始化 | `src/boards/<板型>/board_init.c` |
| 扩展器寄存器操作 | `pca9557.c/.h` 或 `tpt29555a.c/.h` |
| LCD 连接、复位、背光 | `src/boards/<板型>/lcd_init.c` |
| 人声/参考通道、按键能力、功放与触摸复位 | `src/boards/<板型>/profile.c` |
| Flash、PSRAM | `config/<板型>.defaults` |
| Flash 分区布局 | `config/<板型>.partitions.csv` |
| 构建时选择上述实现 | [board 组件注册文件](../src/platforms/esp_idf/cmake/components/board/CMakeLists.txt) |

当前每块板各有 **8 个专属板级文件**，另加默认配置和分区表。`src/application/core/`、`src/application/services/`、`src/platforms/esp_idf/`、`src/application/ui/` 和 `src/platforms/esp_idf/entry/` 共用。

对于后续新增 ESP32-S3 伙伴板，主要工作是新增板级适配和配置；对于 WS63，本次已在 src/platforms/goldieos/targets/ws63 中实现独立入口及平台适配，复用对话状态机、云接入、公共音频处理和页面设计；当前 Hoperun 基础功能已通过用户实板验收。

## 统一业务启动体验（2026-10-10）

三款单板上电后直接进入配网和对话初始化；润和 WS63 不再以“仅充电模式”阻塞业务，也不因拨动开关而在业务层关闭 Wi-Fi。电源锁存仍由 src/boards/ws63/hoperun/board.c 的板级任务处理，物理断电属于板级行为。Windows 模拟器沿用同一应用流程，但使用电脑网络且不提供配网入口。
