# GoldieOS · 模拟器（WIN）平台编译指南

> **所属示例：** `examples/goldieos`（AI Hardware Agent / GoldieOS demo，WIN 模拟器平台）
>
> **相关：** [WS63 平台指南](platform_ws63.md) · 同属 `examples/goldieos` demo

模拟器（WIN）平台与 WS63 平台均为 **GoldieOS demo** 的两种目标平台，共用同一套 `examples/goldieos` 工程源码；模拟器使用本机原生 MinGW 工具链编译，生成 `goldieos.exe`，可在 PC 上模拟整套 GoldieOS 界面与功能。

---

## 1. 前置准备

1. **Windows 版 SDK 库**（必需）：将 Windows (MinGW) 版 `libconvai_sdk.a` 放入工程根目录的 `libs/win/`：

   ```powershell
   Test-Path libs/win/libconvai_sdk.a
   ```

2. **MinGW 工具链**：本机原生 `gcc/g++` 与 `mingw32-make` 可用（如 MSYS2 mingw64，**不是** ws63 的 RISC-V 交叉工具链）。

---

## 2. 编译

在工程根目录下，打开 **PowerShell** 执行：

```powershell
mkdir build_win
cd build_win
cmake .. -G "MinGW Makefiles" -DCONVAI_PLATFORM=goldieos -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 -DCMAKE_MAKE_PROGRAM=mingw32-make
mingw32-make
```

> **编译前：** 请先完成[设备凭证配置](../../../README.md#设备凭证配置)（公共前置步骤），否则模拟器无法连接华为云后端。

编译产物位于 `build_win/examples/goldieos/`：

```
build_win/examples/goldieos/
├── goldieos.exe           # 模拟器可执行文件
├── convai.cfg             # 运行时配置（首次构建自动生成，默认全部注释）
└── convai.cfg.example     # 配置模板（每次构建同步）
```

---

## 3. 运行

```powershell
./goldieos.exe
```

启动后打开虚拟设备窗口，可体验开机动画、launcher、设置、闹钟、动画播放等应用，并通过华为云后端进行真实 AI 对话。

> **注意：**
> - `convai.cfg` 格式为每行一个 `键=值`（等号两侧**不能有空格**），行首 `#` 或 `;` 为注释。
> - `convai.cfg` 不会被后续构建覆盖；模板有更新时请对照 `convai.cfg.example` 手动同步。
