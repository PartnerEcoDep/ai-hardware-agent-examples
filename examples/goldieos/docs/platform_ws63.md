# GoldieOS · WS63 平台编译指南

> **所属示例：** `examples/goldieos`（AI Hardware Agent / GoldieOS demo，WS63 平台）
>
> **相关：** [模拟器（WIN）平台指南](platform_win_simulator.md) · 同属 `examples/goldieos` demo

WS63 平台与模拟器（WIN）平台均为 **GoldieOS demo** 的两种目标平台，共用同一套 `examples/goldieos` 工程源码；差异仅在编译工具链、链接与烧录方式（见下文）。

---

## 1. 交叉编译工具链

交叉编译工具链已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/compiler/riscv/cc_riscv32_musl_105/cc_riscv32_musl_fp_win/bin/
├── riscv32-linux-musl-gcc.exe      # RISC-V C 交叉编译器
├── riscv32-linux-musl-g++.exe      # RISC-V C++ 交叉编译器
└── ...
```

使用时将上述 `bin/` 目录加入 `PATH`。

### 方法一：临时设置（当前 PowerShell 窗口有效）

```powershell
$env:PATH += ";<你的工程目录>\examples\goldieos\tools\build\tools\compiler\riscv\cc_riscv32_musl_105\cc_riscv32_musl_fp_win\bin;<你的工程目录>\examples\goldieos\tools\build\tools"
```

### 方法二：永久设置（推荐）

1. 按 `Win + R`，输入 `sysdm.cpl`，回车打开"系统属性"
2. 点击"高级"选项卡 → "环境变量"
3. 在"系统变量"或"用户变量"中找到 `Path`，点击"编辑"
4. 点击"新建"，分别添加以下两个路径（将 `<你的工程目录>` 替换为实际路径，如 `E:\works\ai-hardware-agent-examples`）：
   ```
   <你的工程目录>\examples\goldieos\tools\build\tools\compiler\riscv\cc_riscv32_musl_105\cc_riscv32_musl_fp_win\bin
   <你的工程目录>\examples\goldieos\tools\build\tools
   ```
5. 一路点击"确定"保存
6. **重启终端**使环境变量生效

> **注意：** 工程目录不要嵌套太深，否则可能导致路径过长引发编译问题。建议将工程放在简洁的路径下，如 `D:\ai-hardware-agent-examples`。

验证安装：

```bash
riscv32-linux-musl-gcc --version
riscv32-linux-musl-g++ --version
```

---

## 2. 链接工具

固件链接器 `ws63_link_v4.exe` 已包含在 Examples 包中，位于：

```
examples/goldieos/tools/build/tools/ws63_link_v4.exe
```

在 `examples/goldieos/CMakeLists.txt` 中已预配置好路径：

```cmake
# 优先级: CMake 参数 > 环境变量 > 相对路径回退
if(WS63_BOARD_ROOT)
    set(WS63_LINK_ROOT "${WS63_BOARD_ROOT}")
elseif(DEFINED ENV{GOLDIEOS_ROOT})
    set(WS63_LINK_ROOT "$ENV{GOLDIEOS_ROOT}")
else()
    set(WS63_LINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(WS63_LINKER "${WS63_LINK_ROOT}/tools/build/tools/ws63_link_v4.exe")
```

如果你的 `ws63_link_v4.exe` 不在 `examples/goldieos/tools/build/tools/` 下，可通过以下方式覆盖：

```bash
# CMake 参数
cmake .. -DWS63_BOARD_ROOT=/path/to/your/goldieos
```

---

## 3. CMake 配置说明

顶层 `CMakeLists.txt` 中，WS63 平台的 `convai_sdk` 使用 `IMPORTED STATIC` 方式链接预编译的 `libconvai_sdk.a`：

```cmake
add_library(convai_sdk STATIC IMPORTED)
set_target_properties(convai_sdk PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a
)
```

固件链接通过 `ws63_link_v4.exe` 将应用库、Opus 库、SDK 库三者链接：

```
libgoldieos_ws63.a  ──┐
libopus.a           ──┤── ws63_link_v4.exe ──→ goldieos.fwpkg
libconvai_sdk.a     ──┘
```

---

## 4. 编译

在工程根目录下，打开 **PowerShell** 执行：

```powershell
mkdir build_ws63
cd build_ws63
cmake .. -G "MinGW Makefiles" -DCONVAI_PLATFORM=ws63 -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc -DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++ -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 -DCMAKE_MAKE_PROGRAM=mingw32-make
mingw32-make
```

> **编译前：** 请先完成[设备凭证配置](../../../README.md#设备凭证配置)（公共前置步骤）。WS63 平台在 `bridge_build_config_json()` 中直接使用 `convai_bridge_defaults.c` 中的默认凭证值构建连接配置。

| 参数                                            | 说明                                 |
| ----------------------------------------------- | ------------------------------------ |
| `-G "MinGW Makefiles"`                        | 使用 MinGW Makefiles 生成器          |
| `-DCONVAI_PLATFORM=ws63`                      | 指定目标平台为 WS63                  |
| `-DCMAKE_C_COMPILER=riscv32-linux-musl-gcc`   | RISC-V C 交叉编译器                  |
| `-DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++` | RISC-V C++ 交叉编译器                |
| `-DCMAKE_C_COMPILER_WORKS=1`                  | 跳过编译器可用性检测（交叉编译必需） |
| `-DCMAKE_CXX_COMPILER_WORKS=1`                | 跳过 C++ 编译器可用性检测            |
| `-DCMAKE_MAKE_PROGRAM=mingw32-make`           | 指定 make 程序                       |

编译产物位于 `build_ws63/examples/goldieos/out/`：

```
build_ws63/examples/goldieos/out/
├── goldieos.fwpkg     # 固件包（用于烧录）
├── goldieos.bin       # 固件二进制文件
└── goldieos.elf       # ELF 文件（含调试符号）
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
4. **加载固件** — 点击"Select file"，选择 `build_ws63/examples/goldieos/out/goldieos.fwpkg`
5. **执行烧录** — 点击"Send file"，等待完成
6. **运行固件** — 长按开机按钮启动设备

**硬件连接：** 使用 USB 转串口线连接 WS63 开发板，确认[串口驱动](https://wch.cn/downloads/CH341SER_EXE.html)已安装。
