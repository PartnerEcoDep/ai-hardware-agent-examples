# Best Practices for AI Hardware Agent Examples

This guide is based on the Huawei AI Hardware Agent SDK and walks developers through building, flashing, and running the examples on each supported platform.

---

## Table of Contents

1. [Overview](#overview)
2. [Repository Structure](#repository-structure)
3. [Obtaining the SDK and Examples](#obtaining-the-sdk-and-examples)
4. [Project Integration](#project-integration)
5. [Device Credential Configuration](#device-credential-configuration)
6. [Platform Build Guides](#platform-build-guides)
7. [Development Documentation](#development-documentation)

---

## Overview

The AI Hardware Agent SDK is distributed separately from the Examples package:

| Component                 | Description                                                | Distribution Format                            |
| ------------------------- | ---------------------------------------------------------- | ---------------------------------------------- |
| **AI Hardware Agent SDK** | Core library, including `libconvai_sdk.a` and public headers | `ai-hardware-agent-sdk-<version>.tar`      |
| **Examples**              | Platform example code, including the GoldieOS demo application | `ai-hardware-agent-examples-<version>.tar` |

Place the SDK `include/` and `libs/` directories in the project root so they can be shared by all platforms.

> **Platform organization:** In this repository, both **WS63** and the **Windows simulator (WIN)** are target platforms of the **GoldieOS demo** under `examples/goldieos`. ESP32-S3 corresponds to `examples/szpi-esp32s3`.

---

## Repository Structure

```
ai-hardware-agent-examples/
├── CMakeLists.txt             # Top-level build entry point (WS63 / GoldieOS Windows simulator)
├── CMakePresets.json          # Symmetric build presets for both platforms
├── CMakeUserPresets.json      # Optional machine-local overrides (not committed)
├── cmake/
│   ├── convai-version.cmake   # Version information
│   ├── convai-sdk.cmake       # SDK source / prebuilt library resolution
│   ├── convai-build-options.cmake  # Common build policies
│   ├── convai-artifacts.cmake      # Versioned artifact rules
│   └── toolchains/
│       ├── ws63-riscv-gcc.cmake
│       └── goldieos-mingw-gcc.cmake
├── include/convai/            # SDK public headers (convai_api/event/platform/types.h)
├── libs/                      # SDK prebuilt libraries (ws63 / win / esp32-s3)
└── examples/
    ├── goldieos/              # GoldieOS demo (WS63 and Windows simulator targets)
    │   ├── CMakeLists.txt
    │   ├── cmake/
    │   │   ├── goldieos-common.cmake
    │   │   ├── goldieos-ws63.cmake
    │   │   ├── goldieos-win.cmake
    │   │   ├── ws63-mbedtls.cmake
    │   │   └── ws63-firmware.cmake
    │   ├── sdk_integration/   # SDK integration bridge layer
    │   ├── apps/ services/ drivers/   # Applications / system services / hardware drivers
    │   ├── platform/{win,ws63} + HAL implementations
    │   ├── include/ libs/ init/ compat/ third_party/
    │   ├── docs/              # Development documentation and platform build guides
    │   └── tools/{build,burn} # Build and flashing tools
    └── szpi-esp32s3/          # ESP32-S3 demo (ESP-IDF project)
        ├── CMakeLists.txt
        ├── main/ components/ tools/
        ├── docs/esp32s3_开发最佳实践.md
        └── sdkconfig.defaults
```

---

## Obtaining the SDK and Examples

### Download the Examples Package

Download the example code from:

> **Download URL:** `https://github.com/PartnerEcoDep/ai-hardware-agent-examples.git`

You can clone the repository with `git clone`, or download and extract a released `.tar.gz` archive.

### Download the SDK Package

Download the AI Hardware Agent SDK release package from:

> **Download URL:** `https://download.huaweicloud-koophone.com/ai-hardware-agent-sdk/ai-hardware-agent-sdk.26.8.0.tar`

---

## Project Integration

### Step 1: Create a Working Directory and Extract the Examples Package

Run the following commands in **PowerShell**:

```powershell
mkdir ai-hardware-agent-examples
cd ai-hardware-agent-examples
tar -xvf /path/to/ai-hardware-agent-examples.tar
```

After extraction, the directory contains `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, and `examples/`.

### Step 2: Extract the SDK Package into the Project Root

Place the SDK `include/` and `libs/` directories in the project root:

```powershell
tar -xvf /path/to/ai-hardware-agent-sdk-x.x.x.tar
```

### Step 3: Verify the Integration

```powershell
# Verify that the SDK files are in the project root
Test-Path include/convai/convai_api.h              # SDK public header
Test-Path libs/ws63/libconvai_sdk.a                # WS63 SDK static library
Test-Path libs/win/libconvai_sdk.a                 # Windows simulator SDK static library
Test-Path libs/esp32-s3/libconvai_sdk.a            # ESP32-S3 SDK static library

# Verify that the bridge layer is complete
Test-Path examples/goldieos/sdk_integration/convai_bridge.c
Test-Path examples/goldieos/sdk_integration/convai_config_file.c
Test-Path examples/goldieos/sdk_integration/convai_codec_g711a.c

# Verify the top-level build files
Test-Path CMakeLists.txt
Test-Path CMakePresets.json
Test-Path cmake/toolchains/ws63-riscv-gcc.cmake
Test-Path cmake/toolchains/goldieos-mingw-gcc.cmake
```

> **Note:** Toolchain setup, building, flashing, and running are specific to each example project. See the [Platform Build Guides](#platform-build-guides) and [Development Documentation](#development-documentation) for details.

---

## Device Credential Configuration

> **Common prerequisite:** This section applies to every example project that connects to the Huawei Cloud backend, including the WS63 and Windows simulator targets of GoldieOS, as well as ESP32-S3.

Before building, configure **authentication** in `convai_bridge_defaults.c` or `convai.cfg`. The project supports two authentication methods:

| Authentication Method          | Required Fields                                                                                              |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| **API-Key authentication**     | `agent_id` + `api_key`                                                                                   |
| **Product-Key authentication** | `agent_id` + `product_id` + `product_key` + `product_secret` + `device_name` (five-tuple)           |

Each example project includes this file. Their contents are byte-for-byte aligned apart from comments, so the configuration process is identical:

| Example Project                | File Location                                                        |
| ------------------------------ | -------------------------------------------------------------------- |
| GoldieOS (WS63 / Windows simulator) | `examples/goldieos/sdk_integration/convai_bridge_defaults.c`  |
| ESP32-S3                       | `examples/szpi-esp32s3/main/sdk/convai_bridge_defaults.c`          |

### Authentication Method 1: API-Key (Recommended)

This method requires the common `agent_id` field—the agent ID required by every authentication method—plus `api_key`. Configure `agent_id` through `BRIDGE_DEFAULT_BOT_ID`, and update `api_key` at the platform-specific location shown below:

| Platform        | `agent_id` Configuration Entry                                                                     | `api_key` Configuration Entry                                      |
| --------------- | ---------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| WS63            | `BRIDGE_DEFAULT_BOT_ID` in `convai_bridge_defaults.c`                                            | `BRIDGE_DEFAULT_API_KEY` in `convai_bridge_defaults.c`           |
| ESP32-S3        | `BRIDGE_DEFAULT_BOT_ID` in `convai_bridge_defaults.c`                                            | `BRIDGE_DEFAULT_API_KEY` in `convai_bridge_defaults.c`           |
| Windows simulator (WIN) | `BRIDGE_DEFAULT_BOT_ID` in `convai_bridge_defaults.c`, or `agent_id=...` in `convai.cfg` | `api_key=...` in the runtime `convai.cfg` beside the application |

After configuration, `bridge_build_config_json()` generates the connection configuration `{"info":{"api_key":"..."}}`. The common `agent_id` field is used when creating the engine.

### Authentication Method 2: Product-Key (Five-Tuple)

This method requires the following device five-tuple, which you can obtain from the platform:

| Parameter        | Description                          | Example                                            |
| ---------------- | ------------------------------------ | -------------------------------------------------- |
| `agent_id`     | Agent ID                             | `"goldieos-agent"`                               |
| `product_id`   | Product ID                           | `"your_product_id"`                              |
| `product_key`  | Product key                          | `"your_product_key"`                             |
| `product_secret` | Product secret used for encryption | `"your_product_secret"`                          |
| `device_name`  | Device name                          | `"goldieos-ws63"` (generated from Wi-Fi MAC on WS63) |

### Modify the Default Configuration

Find the default configuration macros in the corresponding example project's `convai_bridge_defaults.c` and replace them with actual values:

```c
/* ---- default config ---- */
#define BRIDGE_DEFAULT_BOT_ID         "your_agent_id"       // ← Replace with the actual agent_id
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"     // ← Replace with the actual product_id
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"    // ← Replace with the actual product_key
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret" // ← Replace with the actual product_secret
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"    // ← Replace with the actual device_name
#define BRIDGE_DEFAULT_API_KEY        NULL                  // ← API-Key auth: set here for WS63/ESP32 (NULL uses the five-tuple)
```

> **Authentication precedence:** If `api_key` is configured—through `BRIDGE_DEFAULT_API_KEY` on WS63/ESP32 or the `api_key` entry in `convai.cfg` on the Windows simulator—the project uses **API-Key authentication** (see [Authentication Method 1](#authentication-method-1-api-key-recommended)). Otherwise, it falls back to **Product-Key authentication** using the five-tuple macros above or the corresponding fields in `convai.cfg`.
>
> **Platform-specific `device_name` behavior:** On WS63, the effective device name is generated automatically from the local **Wi-Fi MAC** and written to the bridge by `ws63_device_id()`. `BRIDGE_DEFAULT_DEVICE_NAME` is only used as a fallback when the MAC address cannot be obtained. The Windows simulator (WIN) and ESP32-S3 use the hard-coded value directly.

---

## Platform Build Guides

Build, flashing, and runtime instructions for each platform are organized under the corresponding example directory:

The WS63 and Windows simulator targets of GoldieOS use CMake Presets and require CMake 3.21 or later. Both targets provide `Release`, `Debug`, and `RelWithDebInfo` configurations. Run `cmake --list-presets` to view all available preset names.

| Platform        | Example                                  | Guide                                                                                     |
| --------------- | ---------------------------------------- | ----------------------------------------------------------------------------------------- |
| WS63            | `examples/goldieos` (GoldieOS demo)    | [WS63 Platform Build Guide](examples/goldieos/docs/platform_ws63.md)                       |
| Windows simulator (WIN) | `examples/goldieos` (GoldieOS demo) | [Windows Simulator (WIN) Platform Build Guide](examples/goldieos/docs/platform_win_simulator.md) |
| ESP32-S3        | `examples/szpi-esp32s3`                | [ESP32-S3 Development Best Practices](examples/szpi-esp32s3/docs/esp32s3_开发最佳实践.md) |

> **Platform organization:** Both **WS63** and the **Windows simulator (WIN)** are target platforms of the **GoldieOS demo** under `examples/goldieos`. **ESP32-S3** corresponds to `examples/szpi-esp32s3`.

---

## Development Documentation

| Document                                                                | Description                                                                               |
| ----------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| [Function Call Development Best Practices](examples/goldieos/docs/function_call.md) | GoldieOS: adding function call handlers, argument parsing conventions, and the JSON protocol |

---

> **Version:** This document is based on AI Hardware Agent SDK 26.8.0.
