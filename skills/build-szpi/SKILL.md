---
name: build-szpi
description: 在 Windows 准备本地或在线构建依赖并生成立创实战派 ESP32-S3程序，用于从零构建或排查该目标的编译问题；调用 unified-app 当前架构的共用构建引擎。
---

# 立创实战派 ESP32-S3构建

## 目标与项目定位

目标固定为 `szpi`，后端为ESP-IDF 6.1，16 MB Flash / Octal PSRAM。从用户指定路径/当前目录定位最新 unified-app，确认根 CMakeLists.txt、skills/_shared/scripts/build.ps1 和 src/platforms/goldieos/targets；不要把 skill 安装目录当成源码目录。此入口依赖项目随源码提交的 skills/_shared 共用资源，不依赖安装其他 skill。

读取项目 README.md、`skills/_shared/references/esp-idf.md` 及 `skills/_shared/references/dependencies.md`，只加载本目标相关参考。Hoperun 必须使用最新 src/platforms/goldieos/targets 入口，不套用旧 GoldieOS demo presets。

## 执行

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "<RepoRoot>/skills/_shared/scripts/build.ps1" -Target szpi -RepoRoot "<RepoRoot>"
```

Windows x64、PowerShell 5.1+。脚本从进程 PATH/用户明确路径解析工具，缺失时按 skills/_shared/manifests/dependencies.json 下载并校验便携工具。归档可放该 manifests 目录，解压缓存默认 manifests/cache；二进制和 local.json 忽略不上库。版本固定，不自动升级。可传 DependencyManifest、CacheRoot、BuildRoot、Offline、PrepareOnly；不覆盖真实鉴权。

ESP 可传 IdfRoot、PythonPath、GitPath；已有 EIM 再传 IdfToolsRoot 及其虚拟环境 PythonPath。缺 IDF 时脚本克隆固定 v6.1 并安装匹配工具；缺 Git/Python 时使用 winget 用户安装，无 winget 的环境需先提供工具。Offline 要求工具、子模块和组件缓存齐全。

## 验证与交付

默认输出 `build/skills/szpi/unified_ai_szpi_merged.bin`。每个目标独立 BuildRoot，共享依赖缓存。脚本清除旧交付文件后配置/编译，退出码零、文件非空并计算 SHA-256 后生成 build-result.json。配置或编译失败只报告失败和私有日志位置，不用旧产物/空接口凑成功。

提供产物绝对路径、大小、摘要及日志路径。区分本机已有工具构建、下载验证、干净 Windows 全链路、程序启动和功能验收。没有凭据仍可编译，真实对话需要用户私有配置；不贴完整日志或密钥。默认不烧写、不启动程序；用户明确要求后再执行相关动作。禁止为构建成功删除功能或关闭 TLS 校验。

ESP 合并镜像由目标实际 flash_args 生成，起始地址 0x0；不可跨板混刷。
