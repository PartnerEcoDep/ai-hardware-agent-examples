# Linux 桌面应用：Raspberry Pi 3 Model B

实现日期：2026-11-12。第一阶段为无 SDK 的交互演示，不提供真实 AI 对话、录音播放、热点配网或日志持久化。

## 系统与运行方式

首个部署基线为 Raspberry Pi OS 64-bit **带桌面版**（Debian 13 / Trixie）。截至本次核对，官方页面列出的镜像发布日期为 2026-10-06，并明确兼容 3B：[系统下载页](https://www.raspberrypi.com/software/operating-systems/)。无需安装 Full 版。Lite 不含桌面，本目标需要另行配置图形会话。

程序在 Linux 桌面中创建 SDL2 窗口，VNC 用来查看和点击该桌面，SSH 用于编译和部署。程序默认 320×240 逻辑画布、2 倍显示，可传 `--scale 1` 到 `--scale 4`。窗口尺寸不是实体屏幕的固定分辨率。鼠标和 VNC 指针已接入，专用触摸驱动及触屏设备尚未验证。

## 分层与实际功能

- `entry/main.c`：在单一线程内组装 controller、模拟会话及 UI；LVGL 回调只入队，循环处理命令和事件。退出时先停止模拟会话。
- `components/demo`：可取消的模拟会话，产生带 session 的 SDK 等价业务事件；不依赖 SDK。以后接入真实 cloud 后替换此组装，不把模拟行为放进公共业务。
- `components/runtime`：单调时钟、睡眠。当前不需要多线程，不提前添加线程封装。
- `components/network`：读取现有 IP 接口状态，不修改系统网络。接口 UP/RUNNING 不证明互联网或云服务可达。
- `components/ui`：LVGL 生命周期、SDL2 显示和输入；公共页面仍来自 application/ui。平台独立维护 LVGL 配置，不依赖 GoldieOS 运行包。
- `boards/raspberry_pi/pi3_model_b/board.cmake`：板名与产物名；没有显示/音频外设驱动，不代表已适配全部树莓派硬件。

Start → Connecting → Listening → Thinking → Speaking → Listening；Stop 返回 Ready。真实网络接口全部掉线后结束演示并显示 Offline；网络恢复后由用户重新点击 Start。使用系统已有网络，Wi-Fi 按钮隐藏。画面明确显示 `Demo only - no audio/cloud`。

`--demo-online` 可在断网环境强制模拟网络就绪，画面标注 `Simulated network`；它仅用于 UI 预览，不代表网络测试通过。状态日志输出到终端，不保存凭据。

## 在树莓派构建

先将工程源码放到树莓派，例如 `~/unified-app`；Windows 上的编译产物不可直接在树莓派运行。首阶段采用板上原生编译，避免交叉工具链与系统 ABI 不一致。

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build libsdl2-dev git
cd ~/unified-app
```

需要 LVGL **9.5.0** 源码。若已复制工程中同版本的 `managed_components/lvgl__lvgl`，可直接使用默认路径。否则下载到构建缓存中（CMake 不自动联网）：

```bash
mkdir -p build/deps
git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl.git build/deps/lvgl
cmake -S . -B build/linux-pi3-model-b/demo -G Ninja \
  -DUNIFIED_TARGET=linux -DLINUX_BOARD=pi3_model_b \
  -DLVGL_SOURCE_DIR="$PWD/build/deps/lvgl" -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-pi3-model-b/demo --parallel 2
ctest --test-dir build/linux-pi3-model-b/demo --output-on-failure
```

Pi 3 的内存有限，先使用并行度 2；如果内存紧张改为 1。无需 ConvAI SDK、GoldieOS 归档、鉴权配置或音频设备。也支持从 `src/platforms/linux/targets/native` 独立配置。

在 **VNC 桌面中的终端** 启动：

```bash
cd ~/unified-app
./build/linux-pi3-model-b/demo/unified_ai_pi3_model_b
```

关闭窗口、按 Esc 或发送 SIGINT/SIGTERM 均可退出。SSH shell 默认不继承桌面 DISPLAY/WAYLAND_DISPLAY 等会话信息；首次验证从 VNC 桌面终端启动，避免错误地假定 DISPLAY=:0。不要使用 sudo 启动图形应用。

## 测试与边界

CTest 包含公共状态机/协调器回归、模拟会话的取消/重启/时钟回绕，以及 SDL dummy 后端的真实 LVGL 按钮点击、状态流转、断网停止和恢复测试。dummy 后端不验证 VNC、真实屏幕或触摸硬件。

构建可在其他 Linux 主机上验证，但该可执行文件只匹配构建时 CPU/ABI；目标名含 pi3_model_b 不会把 x86 主机程序变成 ARM 程序。打包安装可用 `cmake --install ... --prefix ...`，运行机器仍需 SDL2 等动态依赖。

目前部署所需的树莓派 SSH 地址、用户和可用认证路径尚未提供；不据本地测试宣称树莓派实机通过。最新构建与测试结果见 [验证记录](../../../docs/validation.md)。后续需要另行接入匹配的 Linux SDK、音频设备、网络管理和文件日志。
