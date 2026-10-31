# 润和 WS63 独立 AI 应用

本目录仅适配当前 GoldieOS 服务体系，不代表所有 WS63 伙伴。组件与后端边界见 [平台说明](../README.md)。2026-10-13 从 src/platforms/ws63 迁入；本地旧 CMake 缓存需要重新配置，保留私有配置并显式传入原有工具链/板型选项。

`targets/ws63/` 构建启动后直接进入 AI 对话页面的固件，不再挂载到“设置 → AI 设置”。保留 GoldieOS/LiteOS 的板级驱动和系统服务，应用界面改用与 ESP32 同版本的 LVGL 9.5.0，排除原桌面、设置及其他应用源码、旧 AI bridge 和本地 aud_algo 应用服务。这里的“独立”指应用入口和界面独立，仍依赖厂商 BSP/操作系统。

## 代码与界面复用

| 层次 | 处理方式 |
|---|---|
| 应用状态 | 直接编译 src/application/core/ai_app.c，启停、超时、断线和旧事件过滤与 ESP32 相同 |
| 云接入 | 直接编译 src/application/services/cloud/ai_cloud.c，链接 SDK 26.8.0 的 WS63 静态库 |
| 音频处理 | 直接编译公共 G711A/声道转换和播放缓冲；线程、锁和 AudioService 调用位于 media.c |
| 页面设计 | 共用 src/application/ui/include/ai_screen.h 的 320×240 布局、配色、按钮文案和状态文本 |
| 页面绘制 | 三板直接编译 src/application/ui/ai_lvgl.c；平台 ui.c 负责 LVGL 输入及 UI 任务，Hoperun 显示写入由 src/boards/ws63/hoperun/display.c 适配 |
| 手机配网 | 三板直接嵌入 src/application/ui/web/ 同一份 HTML/JS；共用 src/application/services/network 输入校验 |
| 系统组装 | app.c 注册开机入口，唯一应用 owner 串行处理命令、网络变化和云事件 |

主页为板名、会话状态、网络信息、Start/Stop、Wi-Fi。点击 Wi-Fi 先停止对话，再开启 `AI-Setup-XXXX` 开放热点，屏幕显示 `http://192.168.4.1`。手机连接热点后，打开该地址操作同一份配网页面：

1. 扫描 2.4 GHz 网络或手动输入 SSID，填写密码；开放网络留空。
2. “连接并保存”测试连接成功后保存账号；最多 10 个，新增网络放在首位，已有网络更新密码。
3. 管理已保存网络：删除、设为默认。列表首项优先连接。
4. “退出配网”关闭热点、重新按保存顺序联网，时间同步后在设备点击 Start。

三板均移除配网页面的发射功率、BSSID 记忆等高级入口。SSID 按 UTF-8 最多 32 字节，密码最多 64 字节；页面安全显示特殊字符，不将 SSID 插入 HTML。无保存账号或离线约 30 秒后进入配网；WS63 的连接调用是 BSP 阻塞接口，超时检查可能延迟一个连接尝试。中文名称由手机输入，不再依赖设备端键盘。

UI 控件由单任务管理，输入/云回调经邮箱交给应用 owner。配网期间对话不可启动，即使连接测试已经取得路由器地址也要先退出配网。当前 WS63 后端使用独立网络任务处理驱动、HTTP 和 NV 配置存储，DNS 单独运行，避免配网阻塞 UI/云事件处理。

显示为 320×240 RGB565。Hoperun 板级刷新适配处理像素字节序，并把每次传输限制在 BSP 的 2040 像素以内；ESP32 等待 SPI DMA 完成后释放 LVGL 缓冲。实际颜色/触摸方向和流畅度仍需各板实测。

## 外部依赖和配置

运行依赖随工程 third_party/goldieos/runtime.tar.gz 提供，CMake 校验后解压到构建目录。平台 CMake 已内置，不再依赖外部示例仓库。另行准备 CMake/Ninja、RISC-V 编译器和厂商链接/签名工具；见 [依赖说明](../../../third_party/goldieos/README.md)。

1. 复制 `config/ws63.local.h.example` 为 `config/ws63.local.h`。
2. 填写 `WS_AI_AGENT_ID` 及 `WS_AI_API_KEY`，或完整的 PRODUCT_ID/KEY/SECRET。API Key 优先。
3. `WS_AI_SERVER_URL` 为空时使用 SDK 默认端点；TLS 要求校验服务端证书。优先使用 SDK 提供的 CA，缺失时使用 `WS_AI_CA_CERT_PEM` 的 PEM 信任证书；都为空则拒绝连接。
4. 首次创建配置文件后必须重新执行 CMake 配置，再构建；只运行旧构建目录的 build 可能仍沿用模板。不要提交 ws63.local.h、构建目录或含密钥的镜像。

默认空模板构建时，点击 Start 会报 -7；已联网但时间尚未同步时报 -8，同步后可重试。Wi-Fi 账号保存到板内 Flash NV 的应用键 `0x5100` / `0x5101`，交替写入带序号和 CRC 的快照，写入后读回核对；最新快照损坏时回退到上一份。不依赖 microSD，存储失败不会报告保存成功。账号不会通过 HTTP 返回密码。旧设置应用的 `wificfg.txt` 不修改、不自动导入，首次使用新应用需重新配网。SDK 配置 JSON 由 cJSON 转义生成。

## 构建与产物

同时支持 Windows 模拟器 EXE，使用独立入口 `src/platforms/goldieos/targets/windows` 和构建目录 `build-hoperun-win`。两种目标通过 `cmake/common.cmake` 复用应用、界面、云接入与媒体源码，runtime 和 SDK HAL 分别适配硬件/Windows。模拟器直接使用电脑网络，不显示 Wi-Fi 配网入口；具体命令见 [Windows 模拟器说明](targets/windows/README.md)。

LVGL 源码归档随仓库提供，CMake 校验 SHA-256 后在构建目录解压，无需为 WS63 单独联网下载 LVGL。Wi-Fi 开放热点调用使用 `ws_wifi_abi.h` 中的最小 ABI 声明，依据海思官方 Wi-Fi 头文件并与当前 BSP 结构偏移核对；更换 BSP 版本时必须重新核对。

当前已验证环境为 Windows、CMake 4.0.3、Ninja、BSP 的 RISC-V GCC 7.3。以下在 unified-app 根目录运行；请将工具链路径替换为实际绝对路径：

```sh
cmake -S src/platforms/goldieos/targets/ws63 -B build-hoperun-app -G Ninja -DCMAKE_TOOLCHAIN_FILE=src/platforms/goldieos/cmake/toolchains/ws63-riscv-gcc.cmake -DGOLDIEOS_TOOLS_ROOT=<工具包根> -DCMAKE_BUILD_TYPE=Release
cmake --build build-hoperun-app -j6
```

`GOLDIEOS_ROOT` 由运行归档解压过程管理；工具包位置使用 `-DGOLDIEOS_TOOLS_ROOT=<工具包根>`。可通过 WS63_LOCAL_CONFIG 指定私有配置文件，通过 CONVAI_SDK_ROOT 指定同版本 SDK 目录。

- build-hoperun-app/out/unified_ai_hoperun.fwpkg：厂商打包固件，供匹配该润和板的 BurnTool 流程使用。
- build-hoperun-app/out/unified_ai_hoperun.bin：应用二进制，不应自行推测裸烧地址。
- goldieos.*：厂商打包器固定名称的中间/原始产物，构建目标会复制为上述应用名称；内容是新独立应用。

Hoperun 固件已烧写并验证过基础对话及自动门户弹窗。清理和分层后的镜像已再次烧写，用户确认显示、配网和对话正常；打断、连续启停压力、开关机及内存/栈余量没有专项验收记录。BSP 头文件仍有历史编译告警；成功编译打包不代表硬件验收。

## 音频与生命周期约束

- Hoperun 当前 BSP 上行采用 AudioService 的 8 kHz、16 位双通道交错采样：L 是麦克风，R 是播放参考；公共转换输出 G711A 双声道平面数据。不能照搬 ESP32 的四槽 TDM 索引。
- 下行使用 G711A 缓冲，解码为 8 kHz 单声道 PCM16 写入 AudioService；ESP32 输出为双声道 PCM。
- 默认 WS_AI_PLAYBACK_BYTES=16000，约 2 秒编码音频，预缓冲 1600 字节即 200 ms。与 ESP32 的 PSRAM 大缓冲分开配置；溢出按公共层规则终止本次会话，不静默丢包。
- 录放音任务常驻。停止通过上行互斥 barrier 等待发送返回，清空软件队列及板级播放缓冲，随后停止/销毁 SDK。播放写入与 reset 使用同一把锁，避免 reset 后写回旧数据。
- SDK 回调只复制事件入队，由 owner 执行 stop/join。线程退出信号量用于等待 SDK 工作函数返回；不能在 SDK 回调中执行停止。
- finish 后等待软件缓冲及板级 DMA 数据排空才通知 Listening；DMA 清空/尾音的实际效果仍需实测。
- 业务上电直接进入网络和对话初始化，不再出现仅充电页面，也不根据电源拨动开关暂停业务。AW9523B 电源保持由 src/boards/ws63/hoperun/board.c 的板级任务管理：开关 ON 保持电源，OFF 释放保持；USB 供电使 CPU 继续运行时，配网和对话仍可使用。纯电池供电时 OFF 可能使硬件断电；此行为需实板验证。当前不是低功耗待机实现，关闭会话后采集任务仍常驻。

## 仅构建公共静态库

原来的 src/platforms/goldieos/cmake/ws63-core/CMakeLists.txt 仍保留，可单独生成 libunified_ai_portable.a，适合其他 WS63 产品接入自己的 UI/BSP。该入口不生成固件，与上述 targets/ws63 入口分开。

## 板内 Wi-Fi 配置存储

`wifi_store.c` 使用海思 uapi_nv_read/uapi_nv_write；两个应用键位于 SDK 用户普通区间 [0x5000,0xffff)，集成其他产品时须为本应用保留 0x5100/0x5101。每份快照 996 字节，低于 NV 单项 4060 字节上限。仅在保存、删除或设默认时写入；拒绝覆盖不同格式的数据，读取错误不作为空键处理。未从旧 SD 文件自动迁移。

普通重启保留配置；当前完整 fwpkg 包含 NV 镜像，重新全量烧写可能清除保存网络，需要重新配网。内部存储未增加密码加密。手机未自动弹窗时可访问 http://192.168.4.1；自动弹窗仍待各手机实测。

接口依据：[海思 NV 存储指南](https://docs.hisilicon.com/repos/fbb_ws63/zh-CN/master/software/NV%E5%AD%98%E5%82%A8%E7%94%A8%E6%88%B7%E6%8C%87%E5%8D%97/WS63V100%20NV%E5%AD%98%E5%82%A8%20%E7%94%A8%E6%88%B7%E6%8C%87%E5%8D%97.html)。键区间对照公开 SDK 的 [key_id.h](https://github.com/hispark-rs/fbb_ws63-qemu/blob/master/src/middleware/chips/ws63/nv/nv_config/include/key_id.h)，错误码对照本地 BSP errcode.h。

## 板级选择

固件和模拟器默认使用 `-DWS63_BOARD=hoperun`。`src/boards/ws63/hoperun/board.cmake` 提供 BSP 路径和源码；公共接口在 `src/boards/ws63/include/ws_board.h`。新增兼容单板的步骤与接口限制见 `src/boards/ws63/hoperun/README.md`。每块板使用独立构建目录。
