# Windows 静态 UI 预览

本入口查看产品正式页面 `src/application/ui/ai_lvgl.c`。独立产品设计预览见 `../ui-design-static/README.md`，使用 `../ui-design-static/product_ui_design.c`。

双击 `build/ui-preview/unified_ui_preview.exe`。单个 EXE，无需安装、开发环境、GoldieOS、ConvAI SDK、鉴权信息或硬件。

- 复用 `src/application/ui/ai_lvgl.c`、`ai_screen.h`，逻辑画布为 320×240，窗口放大 2 倍。
- 默认 Ready。Start 演示 Connecting → Listening → Thinking → Speaking → Listening，每步约 2 秒；Stop 返回 Ready。
- Wi-Fi 切换模拟配网提示，再点一次恢复 Ready；不发射热点，也不打开手机配网页面。
- Preview state 菜单固定展示选定状态，便于静态检查；Start/Stop 可恢复演示。
- Board title 菜单切换三块板的名称，布局共用。本预览保留 Wi-Fi 按钮，不模拟实际 Windows 对话程序隐藏该按钮的配置。
- 不使用麦克风、扬声器、串口、网络或本地鉴权；IP/热点为演示文字，Error -3 为人工选择的显示样例。
- 这是界面与点击预览，不是 AI 功能或硬件性能验证。桌面显示使用 XRGB8888，单板 RGB565 色彩可能存在细微差异。

## 构建

需要 CMake 3.21+、Ninja、MinGW GCC/G++ 和 LVGL 9.5 源码。默认复用项目 `managed_components/lvgl__lvgl`；也可指定 `-DLVGL_SOURCE_DIR=...`。复用 GoldieOS 的 LVGL 配置头，不依赖 GoldieOS 运行归档。

```powershell
cmake -S tools/ui-preview -B build/ui-preview -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui-preview --parallel 6
```

编译器和 Ninja 不在 PATH 时，通过 `CMAKE_C_COMPILER`、`CMAKE_CXX_COMPILER`、`CMAKE_MAKE_PROGRAM` 指定。MinGW 静态链接运行库，EXE 仅依赖 Windows 系统 DLL。修改公共 UI 后需重新构建，运行中的旧窗口须先关闭。

源码和说明上库；build 下的 EXE、依赖缓存和日志不上库。现有单板及完整模拟器构建入口不变。

