# GoldieOS · WS63 平台编译指南

> **所属示例：** `examples/goldieos`（AI Hardware Agent / GoldieOS demo，WS63 平台）
>
> **相关：** [模拟器（WIN）平台指南](platform_win_simulator.md) · 同属 `examples/goldieos` demo

WS63 平台与模拟器（WIN）平台均为 **GoldieOS demo** 的两种目标平台，共用同一套 `examples/goldieos` 工程源码；差异仅在编译工具链、链接与烧录方式（见下文）。

---

## 1. 交叉编译工具链

构建要求：

- CMake 3.21 或更高版本；
- `mingw32-make` 可从 `PATH` 找到；
- Windows 主机（固件链接器 `ws63_link_v4.exe` 是 Windows 程序）；
- 工程与构建路径不含空格（上游固件链接器的路径处理限制）。

RISC-V 交叉编译工具链已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/compiler/riscv/cc_riscv32_musl_105/cc_riscv32_musl_fp_win/bin/
├── riscv32-linux-musl-gcc.exe      # RISC-V C 交叉编译器
├── riscv32-linux-musl-g++.exe      # RISC-V C++ 交叉编译器
└── ...
```

`CMakePresets.json` 会自动选择
`cmake/toolchains/ws63-riscv-gcc.cmake`，因此不再需要把交叉编译器加入
`PATH`，也不需要手工指定 `CMAKE_C_COMPILER`、`CMAKE_CXX_COMPILER` 或
`CMAKE_*_COMPILER_WORKS`。

验证构建工具：

```powershell
cmake --version
mingw32-make --version
```

如果交叉编译器不在仓库默认位置，可在当前 PowerShell 会话中覆盖：

```powershell
$env:WS63_TOOLCHAIN_ROOT = "<WS63 交叉编译工具链根目录>"
```

该根目录的 `bin/` 下必须包含完整工具集：`gcc`、`g++`、`ar`、`ranlib`、
`objcopy` 和 `size`。环境变量应在首次配置对应 preset 前设置；若该构建
目录已有 CMake 缓存，可显式覆盖，例如：

```powershell
cmake --preset ws63-release -DWS63_TOOLCHAIN_ROOT="<WS63 交叉编译工具链根目录>"
```

---

## 2. 链接工具

固件链接器 `ws63_link_v4.exe` 已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/ws63_link_v4.exe
```

链接与签名规则封装在
`examples/goldieos/cmake/ws63-firmware.cmake`。板级包根目录按以下优先级解析：

1. CMake 参数 `WS63_BOARD_ROOT`；
2. 环境变量 `GOLDIEOS_ROOT`；
3. 仓库内的 `examples/goldieos` 相对路径。

如果使用外置板级包，可通过环境变量覆盖根目录；该目录必须同时包含
`libs/ws63/`、链接配置和 `tools/build/tools/`，不能只指向链接器所在目录：

```powershell
$env:GOLDIEOS_ROOT = "<GoldieOS 板级包根目录>"
```

---

## 3. CMake 配置说明

WS63 工具链文件负责选择 RISC-V 编译器、归档工具和 ABI 参数，并通过
`CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` 正确处理交叉编译器检测。

`cmake/convai-sdk.cmake` 统一解析 SDK：默认 `AUTO` 模式优先使用显式的
`CONVAI_SDK_LIBRARY`，其次尝试 `src/CMakeLists.txt` 源码，最后回退到
`libs/ws63/libconvai_sdk.a`。CI 也可通过
`CONVAI_SDK_MODE=SOURCE` 或 `PREBUILT` 固定来源。

固件模块通过 `ws63_link_v4.exe` 将应用库、Opus 库、SDK 库三者链接：

```
libgoldieos_ws63.a  ──┐
libopus.a           ──┤── ws63_link_v4.exe ──→ goldieos.fwpkg
libconvai_sdk.a     ──┘
```

链接前，CMake 会把板级库与链接配置复制到当前 preset 独有的
`build/ws63-<配置>/examples/goldieos/ws63-link-root/`，并只替换该工作副本
中的 mbedTLS。源码树里的 `libs/ws63/board/libmbedtls.a` 不会被覆盖，
不同构建类型也不会共享链接输入。

---

## 4. 编译

在工程根目录下，打开 **PowerShell** 执行：

```powershell
cmake --preset ws63-release
cmake --build --preset ws63-release
```

> **编译前：** 请先完成[设备凭证配置](../../../README.md#设备凭证配置)（公共前置步骤）。WS63 平台在 `bridge_build_config_json()` 中直接使用 `convai_bridge_defaults.c` 中的默认凭证值构建连接配置。

可用构建类型：

| Preset | 编译选项 | 用途 |
| ------ | -------- | ---- |
| `ws63-release` | `-Os`、`NDEBUG` | 正式烧录与发布 |
| `ws63-debug` | `-Og -g3` | 日常调试 |
| `ws63-relwithdebinfo` | `-Os -g`、`NDEBUG` | 接近发布配置的现场问题分析 |

将上述示例中的 preset 名称替换即可切换构建类型。可使用
`cmake --list-presets` 查看全部配置。

Release 的主要编译产物位于 `build/ws63-release/examples/goldieos/out/`：

```
build/ws63-release/examples/goldieos/out/
├── goldieos.fwpkg                 # 固件包（用于烧录）
├── goldieos.bin                   # 固件二进制文件
├── goldieos.elf                   # ELF 文件（Debug/RelWithDebInfo 含调试信息）
├── goldieos-<version>.fwpkg       # 版本化固件包
└── goldieos-<version>.bin         # 版本化二进制文件
```

---

## 5. 烧录

烧录使用 BurnTool，位于：

```
examples/goldieos/tools/burn/hisi/BurnTool_5.0.39/BurnTool/BurnTool.exe
```

**烧录步骤：**

1. **启动 BurnTool** — 双击 `BurnTool.exe`
2. **配置参数** — 设置波特率（推荐 `921600`）
3. **进入 ISP 模式** — 关机状态下按住 RESET → 接上 USB → 松开 RESET → 选择 COM 口 → 点击 Connect（连接后不断打印CCC）
4. **加载固件** — 点击"Select file"，选择 `build/ws63-release/examples/goldieos/out/goldieos.fwpkg`
5. **执行烧录** — 点击"Send file"，等待完成
6. **运行固件** — 长按开机按钮启动设备

**硬件连接：** 使用 USB 转串口线连接 WS63 开发板，确认[串口驱动](https://wch.cn/downloads/CH341SER_EXE.html)已安装。
