# GoldieOS · 模拟器（WIN）平台编译指南

> **所属示例：** `examples/goldieos`（AI Hardware Agent / GoldieOS demo，WIN 模拟器平台）
>
> **相关：** [WS63 平台指南](platform_ws63.md) · 同属 `examples/goldieos` demo

模拟器（WIN）平台与 WS63 平台均为 **GoldieOS demo** 的两种目标平台，共用同一套 `examples/goldieos` 工程源码；模拟器使用本机原生 MinGW 工具链编译，生成 `goldieos.exe`，可在 PC 上模拟整套 GoldieOS 界面与功能。

---

## 1. 前置准备

1. **CMake**：安装 CMake 3.21 或更高版本。

2. **SDK**：若工程包含 `src/CMakeLists.txt`，默认直接从源码构建 SDK；
   否则将 Windows (MinGW) 版 `libconvai_sdk.a` 放入工程根目录的
   `libs/win/`：

   ```powershell
   Test-Path libs/win/libconvai_sdk.a
   ```

   SDK 来源由 `cmake/convai-sdk.cmake` 统一解析；默认 `AUTO` 模式也可用
   `CONVAI_SDK_LIBRARY` 显式覆盖，CI 可通过 `CONVAI_SDK_MODE=SOURCE` 或
   `PREBUILT` 固定来源。

3. **MinGW 工具链**：本机原生的完整 MinGW 工具集可用，包括 `gcc`、`g++`、`ar`、`ranlib`、`windres` 与 `mingw32-make`（如 MSYS2 mingw64，**不是** WS63 的 RISC-V 交叉工具链）。

默认从 `PATH` 查找 MinGW。需要固定某个安装时，可设置：

```powershell
$env:GOLDIEOS_MINGW_ROOT = "<MinGW 安装根目录>"
```

环境变量应在首次配置对应 preset 前设置；若构建目录已有 CMake 缓存，可显式覆盖：

```powershell
cmake --preset goldieos-win-release -DGOLDIEOS_MINGW_ROOT="<MinGW 安装根目录>"
```

`CMakePresets.json` 会自动选择
`cmake/toolchains/goldieos-mingw-gcc.cmake`，无需手工传入 C/C++ 编译器、
归档器或资源编译器参数。

---

## 2. 编译

在工程根目录下，打开 **PowerShell** 执行：

```powershell
cmake --preset goldieos-win-release
cmake --build --preset goldieos-win-release
```

> **编译前：** 请先完成[设备凭证配置](../../../README.md#设备凭证配置)（公共前置步骤），否则模拟器无法连接华为云后端。

可用构建类型：

| Preset | 编译选项 | 用途 |
| ------ | -------- | ---- |
| `goldieos-win-release` | `-O3`、`NDEBUG` | 正式运行与交付测试 |
| `goldieos-win-debug` | `-g` | 日常调试和运行时检查 |
| `goldieos-win-relwithdebinfo` | `-O2 -g`、`NDEBUG` | 优化状态下分析调用栈 |

Release 的主要编译产物位于
`build/goldieos-win-release/examples/goldieos/`：

```
build/goldieos-win-release/examples/goldieos/
├── goldieos.exe                 # 模拟器可执行文件
├── goldieos-<version>.exe       # 版本化可执行文件
├── convai.cfg                   # 运行时配置（首次构建自动生成，默认全部注释）
└── convai.cfg.example           # 配置模板（每次构建同步）
```

---

## 3. 运行

```powershell
./build/goldieos-win-release/examples/goldieos/goldieos.exe
```

启动后打开虚拟设备窗口，可体验开机动画、launcher、设置、闹钟、动画播放等应用，并通过华为云后端进行真实 AI 对话。

> **注意：**
> - `convai.cfg` 格式为每行一个 `键=值`（等号两侧**不能有空格**），行首 `#` 或 `;` 为注释。
> - `convai.cfg` 不会被后续构建覆盖；模板有更新时请对照 `convai.cfg.example` 手动同步。
