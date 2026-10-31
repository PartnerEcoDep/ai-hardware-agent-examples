# Hoperun 独立工程构建

入口为 src/platforms/goldieos/targets/{ws63,windows}。运行依赖来自工程内 third_party/goldieos/runtime.tar.gz，由 CMake 校验后解压。无需完整示例仓库，不使用旧 BspRoot 参数。依赖范围、来源及上库边界见 [运行包说明](../../../third_party/goldieos/README.md)。

```powershell
& "<repo>/skills/_shared/scripts/build.ps1" -Target hoperun-app -RepoRoot "<repo>" -GoldieosToolsRoot "<工具包根>"
& "<repo>/skills/_shared/scripts/build.ps1" -Target hoperun-win -RepoRoot "<repo>" -MinGWRoot "<mingw64>"
```

WS63 工具根包含 compiler/bin/riscv32-linux-musl-gcc.exe、ws63_link_v4.exe、ws63_sign_tool.exe。未显式提供工具根时按清单 ws63_tools 校验并解压，支持本地包或 HTTPS 固定地址；当前仅准备本地包，没有确认的公开下载地址。

输出为 build/skills/hoperun-app/out/unified_ai_hoperun.fwpkg 和 build/skills/hoperun-win/unified_ai_simulator.exe。可指定 BuildRoot、CacheRoot、CMakePath、NinjaPath、Offline、PrepareOnly。SDK 26.8.0 与 LVGL 源码归档随工程提供。没有凭据可以构建；对话使用 LocalConfig 或 config/ws63.local.h，禁止将真实凭据放入依赖包或输出日志。

模拟器使用电脑网络和默认录放音，不提供 Wi-Fi 配网。构建后检查 PE 导入并复制所需 MinGW 运行库，不自动启动。WS63 仍要求 Windows 厂商链接/签名工具；使用较短路径，保留 Ninja response file。构建、从干净环境安装、下载验证和功能验收分别记录。
