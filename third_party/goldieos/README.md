# GoldieOS 固定运行依赖

更新日期：2026-10-29。本工程不再读取同级示例仓库。

## 来源与范围

`runtime.tar.gz` 从迁移前已验证的润和 GoldieOS 本地 BSP 提取，原始位置为 `ai-hardware-agent-examples/examples/goldieos`。公开平台参考：https://gitcode.com/hoperunos/Goldie_Os_Developer_Platform 。本地 BSP 与公开仓库的精确提交对应关系未知，不能标记为官方最新版本；本项目以归档 SHA-256 和逐文件清单锁定这一快照。

- SHA-256：`129cd99d1b5b573c823f580c1cb3ae4de92a64b5b1a022380d6c6bc33496f3c7`。
- 归档大小上限为 50 MB（50,000,000 字节）；当前为 49,332,709 字节。使用 gzip level 9 重新压缩，归档内文件内容不变。
- `files.json` 列出全部归档文件、字节数与 SHA-256，便于后续审计和升级。
- 保留头文件、板级驱动、平台 OSAL/驱动适配、NTP/必要服务、运行依赖的第三方组件、WS63 成套系统库、Windows 虚拟设备库及 WS63 链接配置/基础打包模板。
- 不包含旧桌面、设置等 apps、旧 AI bridge、旧入口、独立 aud_algo 服务、编译器和旧版镜像/链接工具。Windows 仍须链接厂商核心、GUI/音频算法等库，未声称这些闭源库内部所有功能都被裁剪。
- WS63 库及链接配置按 ABI 配套保留，不能根据当前源码调用列表随意拆分；旧 `_old.a` 不保留。模板 `ws63-liteos-app_all.fwpkg` 是厂商打包器输入，不能当作当前应用发布镜像。
- 第三方子目录中已有的许可证、版权头与说明随归档保留；本地 BSP 未提供统一授权说明，本文不为厂商库或工具新增授权声明。后续替换依赖包时保留原有授权材料。

## 构建接入

`src/platforms/goldieos/cmake/runtime.cmake` 校验归档后解压到各自构建目录的 `deps`，不修改源码目录。`GOLDIEOS_ROOT` 是构建内部的解压路径，不再是要求用户提供的外部示例仓库。

平台构建脚本归 `src/platforms/goldieos/cmake` 管理；润和板级只提供本板源文件，不再负责推导外部仓库路径。厂商 TLS 配置需要的 `convai_memory_budget.h` 单独保留在平台 ConvAI/ws63 目录。

## 工具包与上库边界

WS63 工具包 `goldieos-ws63-tools.tar.gz` 包含 `compiler/`、`ws63_link_v4.exe` 和 `ws63_sign_tool.exe`，SHA-256 为 `5c841c0bac8b8ac89b589c5b4583fdd251cb17613877e800430a747493104dac`。本地迁移已准备该包，放在 `skills/_shared/manifests`，按既有规则忽略不上库。

运行归档、逐文件清单、校验 CMake 和本说明上库；编译器、链接/签名工具包、解压缓存、私有配置与生成产物不上库。使用者可提供本地工具包、解压工具根，或在覆盖清单配置固定 HTTPS URL 和 SHA-256。当前没有已确认的公开工具包下载地址，不能宣称从干净 Windows 全联网一键验证完成。

直接构建时设置 `GOLDIEOS_TOOLS_ROOT` 为工具包根；可额外用 `WS63_TOOLCHAIN_ROOT` 选择兼容编译器。使用 skill 时传 `-GoldieosToolsRoot` 或让它校验并解压清单中的工具包。模拟器只需要常规 MinGW/CMake/Ninja 和工程内运行归档，不需要 WS63 工具包。
