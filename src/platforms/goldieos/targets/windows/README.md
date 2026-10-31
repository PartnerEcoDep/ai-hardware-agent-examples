# 润和 Windows 模拟器

构建入口已迁移至 src/platforms/goldieos/targets/windows；旧目录的缓存需重新配置。Windows 网络实现位于 components/network/windows，运行时位于 components/runtime/windows。

与 WS63 烧写固件复用应用生命周期、LVGL 对话页面、云服务封装和音频转换/缓冲代码，通过 runtime 和 SDK HAL 适配 Windows。使用 ConvAI SDK 26.8.0 Windows 静态库，以及润和 BSP 的虚拟显示、鼠标触摸和音频设备。

模拟器不提供 Wi-Fi 配网：不显示 Wi-Fi 按钮，不启动热点、配网 HTTP/DNS 或保存单板 Wi-Fi 账号。直接使用 Windows 已连接的有线或无线网络；更换网络请在 Windows 中操作。网络状态表示系统报告的网络可用性，不代表云端一定可达。Start/Stop 和会话状态仍使用公共页面，单板继续保留原配网功能。

## 构建

需要本工程内的 GoldieOS 运行归档、64 位 MinGW GCC/G++、CMake 和 Ninja，不需要同级示例仓库。在 unified-app 根目录运行（工具不在 PATH 时使用其绝对路径）：

```powershell
cmake -S src/platforms/goldieos/targets/windows -B build-hoperun-win -G Ninja -DCMAKE_C_COMPILER=<MinGW根>/bin/gcc.exe -DCMAKE_CXX_COMPILER=<MinGW根>/bin/g++.exe -DCMAKE_BUILD_TYPE=Release
cmake --build build-hoperun-win -j6
```

可传入 CONVAI_SDK_ROOT、WS63_LOCAL_CONFIG 覆盖默认配置；GOLDIEOS_ROOT 由工程内归档解压得到。公共 CMake 校验并解压本地 LVGL 源码包。固件的独立构建入口和命令见 [GoldieOS README](../../README.md)，两个目标使用不同构建目录。

运行 build-hoperun-win/unified_ai_simulator.exe。建议以该目录为工作目录，系统网络连接和时间应正确，电脑需具备麦克风和扬声器。实际对话通过 Start 开始。

两种目标默认读取同一个 config/ws63.local.h，修改鉴权后重新配置并编译。SDK 在 Windows 也校验证书。独立工程不再复制旧 convai.cfg 示例；鉴权以编译时的 ws63.local.h 为准。私有头文件和包含鉴权的 EXE、固件均不入库。

## 验证范围

Windows 上行取左声道麦克风，参考声道置为 G711A 静音；电脑右声道不是 WS63 硬件提供的播放回声参考。音频采用系统默认录音设备，请在系统中选好要使用的麦克风。

已完成 Windows EXE 编译链接和窗口检查：修复虚拟音频库缺少可选采样率配置回调导致的初始化失败后，界面显示 Ready、Windows network: available，Start 已启用。WS63 固件也完成重新编译打包。用户已确认历史版本模拟器界面和对话正常；本轮 src 迁移版仅完成构建，未重新运行验收；连续启停等压力场景未单独验证。
