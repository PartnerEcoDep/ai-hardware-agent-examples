---
name: build-hoperun-simulator
description: 在 Windows 准备本地或在线构建依赖并生成Hoperun Windows 模拟器程序，用于从零构建或排查该目标的编译问题；调用 unified-app 当前架构的共用构建引擎。
---

# Hoperun Windows 模拟器构建

## 目标与项目定位

目标固定为 `hoperun-win`，后端为GoldieOS Windows 后端、MinGW x64 与 Windows SDK 库。从用户指定路径/当前目录定位最新 unified-app，确认根 CMakeLists.txt、skills/_shared/scripts/build.ps1 和 src/platforms/goldieos/targets；不要把 skill 安装目录当成源码目录。此入口依赖项目随源码提交的 skills/_shared 共用资源，不依赖安装其他 skill。

读取项目 README.md、`skills/_shared/references/hoperun.md` 及 `skills/_shared/references/dependencies.md`，只加载本目标相关参考。Hoperun 必须使用最新 src/platforms/goldieos/targets 入口，不套用旧 GoldieOS demo presets。

## 执行

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "<RepoRoot>/skills/_shared/scripts/build.ps1" -Target hoperun-win -RepoRoot "<RepoRoot>"
```

Windows x64、PowerShell 5.1+。脚本从进程 PATH/用户明确路径解析工具，缺失时按 skills/_shared/manifests/dependencies.json 下载并校验便携工具。归档可放该 manifests 目录，解压缓存默认 manifests/cache；二进制和 local.json 忽略不上库。版本固定，不自动升级。可传 DependencyManifest、CacheRoot、BuildRoot、Offline、PrepareOnly；不覆盖真实鉴权。

GoldieOS 运行依赖随工程固定归档提供，不读取外部示例仓库。WS63 使用清单 ws63_tools 的本地/HTTPS 工具包，或传 GoldieosToolsRoot 指向包含 compiler/、链接器和签名器的工具根。模拟器只需 MinGWRoot 或清单中的 MinGW，不需要 WS63 工具包。工具缺失时报告缺项；不恢复旧 BspRoot 参数。

## 验证与交付

默认输出 `build/skills/hoperun-win/unified_ai_simulator.exe`。每个目标独立 BuildRoot，共享依赖缓存。脚本清除旧交付文件后配置/编译，退出码零、文件非空并计算 SHA-256 后生成 build-result.json。配置或编译失败只报告失败和私有日志位置，不用旧产物/空接口凑成功。

提供产物绝对路径、大小、摘要及日志路径。区分本机已有工具构建、下载验证、干净 Windows 全链路、程序启动和功能验收。没有凭据仍可编译，真实对话需要用户私有配置；不贴完整日志或密钥。默认不烧写、不启动程序；用户明确要求后再执行相关动作。禁止为构建成功删除功能或关闭 TLS 校验。

FWPKG 需匹配单板厂商工具；Windows EXE 直接使用电脑网络和音频，不提供 Wi-Fi 配网。
