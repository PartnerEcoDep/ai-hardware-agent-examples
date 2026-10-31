# 依赖清单、本地包和验证边界

默认清单 skills/_shared/manifests/dependencies.json 上库；manifests/cache/、manifests/local.json 及直接放在 manifests 的二进制包忽略。不要把解压工具链散放到目录根，使用 cache 子目录。可以直接运行脚本或让 Codex 阅读 SKILL.md；是否自动发现取决于客户端技能安装方式，目录存在不等于已安装。

本地优先覆盖示例：复制 dependencies.json 为忽略的 local.json，保留全部其他项，仅修改 ws63_tools：

```json
"ws63_tools": {
  "version": "<内部发布版本>",
  "local_path": "goldieos-ws63-tools.tar.gz",
  "url": "https://<内部制品库>/<固定包名>.zip",
  "sha256": "<发布方提供的64位SHA-256>",
  "relative_root": "."
}
```

用 DependencyManifest 传入 local.json。local_path 相对清单文件目录；若填写却不存在则失败，不静默忽略。要联网回退可移除 local_path，仅配置 url，脚本先查校验缓存再下载。Offline 禁止所有下载及 IDF 安装。

ZIP/TAR 解压到内容哈希命名的缓存；relative_root 指工具包根。WS63 包只包含 compiler/、链接器、签名器，不包含示例工程。脚本对下载/本地归档核对 SHA-256，校验失败不继续；显式 GoldieosToolsRoot/MinGWRoot/SdkRoot 为用户管理的解压目录，检查结构而不是假装已对整目录校验。

依赖来源：CMake 4.0.3 官方 SHA-256 文件；WinLibs 13.2.0 官方 ZIP 校验文件；Ninja 1.12.1 的官方 ninja-win.zip 下载后计算 SHA-256 固定在清单（该资产未提供 API digest）；ConvAI 26.8.0 使用项目已记录的发布包校验值。

构建结果记录产物 SHA-256、大小、目标、构建路径、CMake 版本及依赖清单哈希。它不是功能验收证据。移植源码/BSP 的版本建议由项目仓库和内部 BSP 包版本共同锁定，不在 skill 中硬编码用户电脑路径。

验证分层：脚本解析、失败路径、本地缓存校验、当前机器独立目录构建、下载引导、干净 Windows 全链路分别记录。当前机器上借用已安装工具不能记为干净 Windows 通过。干净系统最终测试应只有源码、skill、系统工具和明确的依赖来源；不预先复制现有 build 产物。
