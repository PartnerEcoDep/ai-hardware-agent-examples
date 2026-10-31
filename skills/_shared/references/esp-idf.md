# ESP-IDF：利尔达与实战派

两个目标共享根 CMakeLists.txt 与 src/platforms/esp_idf/cmake/components；板型分别为 UNIFIED_BOARD=lrd/szpi。独立输出避免 sdkconfig 和分区配置互相覆盖。

```powershell
& "<repo>/skills/_shared/scripts/build.ps1" -Target lrd -RepoRoot "<unified-app>" -IdfRoot "<esp-idf>" -PythonPath "<Python>/python.exe" -GitPath "<Git>/cmd/git.exe"
& "<repo>/skills/_shared/scripts/build.ps1" -Target szpi -RepoRoot "<unified-app>" -IdfRoot "<esp-idf>"
```

缺 IdfRoot 时克隆清单固定的 v6.1 与子模块；由该版本 install.bat esp32s3 准备匹配工具，再 export.bat 注入当前进程环境。缺 Python/Git 时使用 Windows winget 用户级安装；没有 winget 的裸系统需先准备 App Installer 或显式提供工具路径。工具许可提示/安装权限由执行环境处理，不绕过失败。

Windows 官方安装依据：https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/windows-setup.html 。离线模式要求完整 IDF（含子模块）、Python 环境与 IDF 工具缓存已就绪；不能只给 ESP-IDF ZIP 而遗漏子模块。IDF 已有安装可沿用 IDF_TOOLS_PATH；新安装默认放入 CacheRoot/idf-tools。

SDK 26.8.0 头文件与 esp32s3 静态库成套提供。LVGL、esp_codec_dev、cJSON 由项目 manifest 和 dependencies.lock 获取，不替换任意新版。网络失败保留日志；在用户环境授权允许后重试，不关闭 TLS 或切换不明镜像。

复用 EIM 安装时增加 IdfToolsRoot，指向直接包含 xtensa-esp-elf、cmake、ninja 等子目录的工具根，并明确 PythonPath 为其虚拟环境解释器。此模式从当前 IDF 的 tools/tools.json 导出推荐版本的已安装工具路径，不调用传统 export.bat，不安装缺失工具。普通从零安装仍走 install.bat/export.bat。不能仅传一个无虚拟环境的系统 Python 冒充 EIM 已准备环境。

利尔达为 8 MB Flash / Quad PSRAM；实战派为 16 MB / Octal。esptool 从实际 flash_args 合并引导、分区表、OTA 初始数据和应用；生成 unified_ai_lrd_merged.bin / unified_ai_szpi_merged.bin（地址 0x0），不可跨板使用。build-result.json 只报告编译，不报告烧写/功能成功。

config/local.defaults 仅首次生成配置时生效；不覆盖已有凭据。无该文件仍可编译，真实对话需要用户配置。构建日志可能含 Kconfig 值，不能公开完整日志。
