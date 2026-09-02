# AI Hardware Agent Examples 最佳实践

本文档基于华为 AI Hardware Agent SDK，指导开发者完成各平台的编译、烧录与运行。

---

## 目录

1. [概述](#概述)
2. [仓库结构](#仓库结构)
3. [SDK 与 Examples 获取](#sdk-与-examples-获取)
4. [工程集成](#工程集成)
5. [设备凭证配置](#设备凭证配置)
6. [平台编译指南](#平台编译指南)
7. [开发文档](#开发文档)

---

## 概述

AI Hardware Agent SDK 采用 **SDK 与 Examples 分离发布** 的模式：

| 组件                            | 说明                                         | 发布形式                                     |
| ------------------------------- | -------------------------------------------- | -------------------------------------------- |
| **AI Hardware Agent SDK** | 核心库（含`libconvai_sdk.a` + 公共头文件） | `ai-hardware-agent-sdk-<version>.tar`      |
| **Examples**              | 平台示例代码（含 GoldieOS demo 应用）        | `ai-hardware-agent-examples-<version>.tar` |

SDK 的 `include/` 和 `libs/` 放在工程根目录，供各平台共用。

> **平台归属说明：** 本仓库中的 **WS63** 平台与 **模拟器（WIN）** 平台均属于 **`examples/goldieos`（GoldieOS demo）** 的两种目标平台；ESP32-S3 则对应 `examples/szpi-esp32s3`。

---

## 仓库结构

```
ai-hardware-agent-examples/
├── CMakeLists.txt             # 顶层构建入口（ws63 / goldieos 模拟器）
├── CMakePresets.json          # 两个平台的对称构建预设
├── CMakeUserPresets.json      # 可选的本机覆盖配置（不提交）
├── cmake/
│   ├── convai-version.cmake   # 版本信息
│   ├── convai-sdk.cmake       # SDK 源码 / 预编译库解析
│   ├── convai-build-options.cmake  # 公共编译策略
│   ├── convai-artifacts.cmake      # 版本化产物规则
│   └── toolchains/
│       ├── ws63-riscv-gcc.cmake
│       └── goldieos-mingw-gcc.cmake
├── include/convai/            # SDK 公共头文件（convai_api/event/platform/types.h）
├── libs/                      # SDK 预编译库（ws63 / win / esp32-s3）
└── examples/
    ├── goldieos/              # GoldieOS demo（WS63 + 模拟器两种平台）
    │   ├── CMakeLists.txt
    │   ├── cmake/
    │   │   ├── goldieos-common.cmake
    │   │   ├── goldieos-ws63.cmake
    │   │   ├── goldieos-win.cmake
    │   │   ├── ws63-mbedtls.cmake
    │   │   └── ws63-firmware.cmake
    │   ├── sdk_integration/   #   SDK 集成桥接层
    │   ├── apps/ services/ drivers/   #   应用层 / 系统服务 / 硬件驱动
    │   ├── platform/{win,ws63} + HAL 实现
    │   ├── include/ libs/ init/ compat/ third_party/
    │   ├── docs/              #   开发文档 + 平台编译指南
    │   └── tools/{build,burn} #   构建与烧录工具
    └── szpi-esp32s3/          # ESP32-S3 demo（ESP-IDF 工程）
        ├── CMakeLists.txt
        ├── main/ components/ tools/
        ├── docs/esp32s3_开发最佳实践.md
        └── sdkconfig.defaults
```

---

## SDK 与 Examples 获取

### 下载 Examples 包

从以下地址下载示例代码包：

> **下载地址：** `https://github.com/PartnerEcoDep/ai-hardware-agent-examples.git`

可以通过 `git clone` 获取，也可以下载仓库的 `.tar.gz` 发布包后解压。

### 下载 SDK 包

从以下地址下载 AI Hardware Agent SDK 发布包：

> **下载地址：** `https://download.huaweicloud-koophone.com/ai-hardware-agent-sdk/ai-hardware-agent-sdk.26.8.0.tar`

---

## 工程集成

### 步骤 1：创建工作目录并解压 Examples 包

在 **PowerShell** 中执行：

```powershell
mkdir ai-hardware-agent-examples
cd ai-hardware-agent-examples
tar -xvf /path/to/ai-hardware-agent-examples.tar
```

解压后得到 `CMakeLists.txt`、`CMakePresets.json`、`cmake/` 和 `examples/`。

### 步骤 2：将 SDK 包解压到工程根目录

SDK 的 `include/` 和 `libs/` 放在工程根目录：

```powershell
tar -xvf /path/to/ai-hardware-agent-sdk-x.x.x.tar
```

### 步骤 3：验证集成结果

```powershell
# 确认 SDK 文件在根目录
Test-Path include/convai/convai_api.h              # SDK 公共头文件
Test-Path libs/ws63/libconvai_sdk.a                # WS63 SDK 静态库
Test-Path libs/win/libconvai_sdk.a                 # WIN 模拟器 SDK 静态库
Test-Path libs/esp32-s3/libconvai_sdk.a            # ESP32-S3 SDK 静态库

# 确认桥接层文件完整
Test-Path examples/goldieos/sdk_integration/convai_bridge.c
Test-Path examples/goldieos/sdk_integration/convai_config_file.c
Test-Path examples/goldieos/sdk_integration/convai_codec_g711a.c

# 确认顶层构建脚本
Test-Path CMakeLists.txt
Test-Path CMakePresets.json
Test-Path cmake/toolchains/ws63-riscv-gcc.cmake
Test-Path cmake/toolchains/goldieos-mingw-gcc.cmake
```

> **说明：** 各平台的工具链准备、编译、烧录 / 运行等详细步骤都属于对应示例工程，请直接查阅 [平台编译指南](#平台编译指南) 与 [开发文档](#开发文档)。

---

## 设备凭证配置

> **公共前置步骤：** 本小节适用于所有接入华为云后端的示例工程（GoldieOS 的 WS63 / 模拟器平台，以及 ESP32-S3 平台）。

编译前，需要在 `convai_bridge_defaults.c`（或 `convai.cfg`）中完成**鉴权配置**。工程支持两种鉴权方式：

| 鉴权方式                   | 需要配置的字段                                                                                    |
| -------------------------- | ------------------------------------------------------------------------------------------------- |
| **API-Key 鉴权**     | `agent_id` + `api_key`                                                                       |
| **Product-Key 鉴权** | `agent_id` + `product_id` + `product_key` + `product_secret` + `device_name`（五元组） |

各示例工程均内置该文件，且内容逐字节对齐（只有注释差异），配置方式完全一致：

| 示例工程                  | 文件位置                                                       |
| ------------------------- | -------------------------------------------------------------- |
| GoldieOS（WS63 / 模拟器） | `examples/goldieos/sdk_integration/convai_bridge_defaults.c` |
| ESP32-S3                  | `examples/szpi-esp32s3/main/sdk/convai_bridge_defaults.c`    |

### 鉴权方式一：API-Key（推荐）

需要**公共字段 `agent_id`**（智能体 ID，所有鉴权方式均需配置）加上 `api_key`。其中 `agent_id` 通过 `BRIDGE_DEFAULT_BOT_ID` 配置，`api_key` 按平台不同在对应位置更新：

| 平台          | `agent_id` 配置入口                                                                           | `api_key` 配置入口                                           |
| ------------- | ----------------------------------------------------------------------------------------------- | -------------------------------------------------------------- |
| WS63          | `convai_bridge_defaults.c` 的 `BRIDGE_DEFAULT_BOT_ID`                                       | `convai_bridge_defaults.c` 的 `BRIDGE_DEFAULT_API_KEY`     |
| ESP32-S3      | `convai_bridge_defaults.c` 的 `BRIDGE_DEFAULT_BOT_ID`                                       | `convai_bridge_defaults.c` 的 `BRIDGE_DEFAULT_API_KEY`     |
| 模拟器（WIN） | `convai_bridge_defaults.c` 的 `BRIDGE_DEFAULT_BOT_ID` 或 `convai.cfg` 的 `agent_id=...` | 运行时配置文件`convai.cfg`（与应用同目录）的 `api_key=...` |

配置后 `bridge_build_config_json()` 会生成 `{"info":{"api_key":"..."}}` 连接配置；`agent_id` 作为公共字段用于引擎创建。

### 鉴权方式二：Product-Key（五元组）

需要设备五元组信息，联系平台获取：

| 参数               | 说明               | 示例                                         |
| ------------------ | ------------------ | -------------------------------------------- |
| `agent_id`       | 智能体 ID          | `"goldieos-agent"`                         |
| `product_id`     | 产品 ID            | `"your_product_id"`                        |
| `product_key`    | 产品密钥           | `"your_product_key"`                       |
| `product_secret` | 产品密钥（加密用） | `"your_product_secret"`                    |
| `device_name`    | 设备名称           | `"goldieos-ws63"`（WS63 由 WiFi MAC 生成） |

### 修改默认配置

在对应示例工程的 `convai_bridge_defaults.c` 中找到默认配置宏定义，替换为实际值：

```c
/* ---- default config ---- */
#define BRIDGE_DEFAULT_BOT_ID         "your_agent_id"       // ← 替换为实际的 agent_id
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"     // ← 替换为实际的 product_id
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"    // ← 替换为实际的 product_key
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret" // ← 替换为实际的 product_secret
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"    // ← 替换为实际的 device_name
#define BRIDGE_DEFAULT_API_KEY        NULL                  // ← API-Key 鉴权：WS63/ESP32 填这里（NULL 走五元组）
```

> **鉴权优先级：** 若配置了 `api_key`（WS63/ESP32 在 `BRIDGE_DEFAULT_API_KEY`，模拟器在 `convai.cfg` 的 `api_key`），则以 **API-Key** 方式鉴权（见[鉴权方式一](#鉴权方式一apikey推荐)）；否则回退到 **Product-Key**（五元组）方式，使用上面这些宏或 `convai.cfg` 中的对应字段。
>
> **`device_name` 平台差异：** WS63 平台的实际 device_name 由本机 **WiFi MAC** 自动生成并由 `ws63_device_id()` 写入 bridge，`BRIDGE_DEFAULT_DEVICE_NAME` 仅作为未获取到 MAC 时的兜底；模拟器（WIN）与 ESP32-S3 平台则直接使用这里的硬编码值。

---

## 平台编译指南

各平台的编译、烧录 / 运行指南已按示例工程拆分到对应目录下：

GoldieOS 的 WS63 和 Windows 模拟器构建统一使用 CMake Presets，需要
CMake 3.21 或更高版本。两端均提供 `Release`、`Debug` 和
`RelWithDebInfo` 配置；运行 `cmake --list-presets` 可查看全部名称。

| 平台          | 所属示例                               | 指南                                                                         |
| ------------- | -------------------------------------- | ---------------------------------------------------------------------------- |
| WS63          | `examples/goldieos`（GoldieOS demo） | [WS63 平台编译指南](examples/goldieos/docs/platform_ws63.md)                  |
| 模拟器（WIN） | `examples/goldieos`（GoldieOS demo） | [模拟器（WIN）平台编译指南](examples/goldieos/docs/platform_win_simulator.md) |
| ESP32-S3      | `examples/szpi-esp32s3`              | [ESP32-S3 开发最佳实践](examples/szpi-esp32s3/docs/esp32s3_开发最佳实践.md)   |

> **平台归属：** 上述 **WS63** 与 **模拟器（WIN）** 两个平台均属于 **GoldieOS demo**（`examples/goldieos`）的两种目标平台；**ESP32-S3** 对应 `examples/szpi-esp32s3`。

---

## 开发文档

| 文档                                                                 | 说明                                                              |
| -------------------------------------------------------------------- | ----------------------------------------------------------------- |
| [SDK 接入开发指南（以 ESP32-S3 为例）](examples/szpi-esp32s3/docs/esp32s3_sdk接入开发指南.md) | 如何接入 SDK 从零实现一个新 demo：平台适配层、桥接层、音频管线与接入 Checklist |
| [Function Call 开发最佳实践](examples/goldieos/docs/function_call.md) | GoldieOS：如何新增 function call handler、参数解析规范、JSON 协议 |

---

> **版本信息：** 本文档基于 AI Hardware Agent SDK 26.8.0 版本编写。
