# 产品静态页面设计预览

独立预览 `product_ui_design.c`，用于页面设计与模拟点击，不接入真实业务、网络或硬件。
后续设计修改集中在该 C 文件。复用原预览的窗口、示例状态及 LVGL 配置。

在 unified-app 目录执行（工具依赖及编译器配置参见 ../ui-preview/README.md）：

```powershell
cmake -S tools/ui-design-static -B build/ui-design-static -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/ui-design-static --parallel 6
```

运行 `build/ui-design-static/unified_ui_design_static.exe`，窗口标题为 AI UI Static Design。
修改设计后重新构建，运行中的旧窗口需先关闭。

原 `tools/ui-preview` 继续预览正式页面 `src/application/ui/ai_lvgl.c`，运行入口仍为
`build/ui-preview/unified_ui_preview.exe`。

