# 第 8 步：异常处理、测试与定位

[培训索引](README.md) · [上一步](07-platforms-and-boards.md)

## 1. 学习目标

理解错误沿哪条路径影响状态和资源；知道哪些问题适合主机测试、哪些必须实机验证；能保留可比较的故障证据，而不是只凭错误文字猜原因。

阅读入口：[ai_app.c](../../src/application/core/ai_app.c)、[ai_cloud.c](../../src/application/services/cloud/ai_cloud.c)、[测试注册](../../tests/CMakeLists.txt)、[验证记录](../validation.md)。

## 2. 错误不只有一条来源

```text
初始化失败 → 平台入口 require/require_ok → 初始化错误处理
同步 Start 失败 → 返回错误码 → app stop → Error
SDK 回调失败 → 平台消息队列 → owner → app stop → Error
采集/播放失败 → 平台消息队列 → owner → app stop → Error
消息队列溢出 → owner 清队列 → controller_overflow → Error
网络断开 → NETWORK_DOWN → stop → Offline 或停止错误
```

先区分发生在初始化阶段还是会话阶段；初始化错误并不都经过公共状态机。先找到产生日志的模块，再查返回值。

## 3. 当前常见应用错误

| 错误值 | 当前可见产生路径 | 阅读提醒 |
|---|---|---|
| -1 | SDK 失败/断开事件转成通用错误等 | 不是单一网络根因 |
| -2 | app 检查 Connecting 超过 30 秒 | owner 阻塞会影响检查时机 |
| -3 | cloud 下行格式不支持或 media.push 失败 | 必须区分 format 与 media_push 日志 |
| -4 | ESP 连续采集读取异常路径 | 不是所有平台都用它表示同一错误 |
| -5 | 连续上传/采集处理失败路径 | 先查平台对应分支与 SDK 返回 |
| -6 | 平台音频写入失败 | 检查设备和输出生命周期 |
| -7 | 当前平台云配置的失败路径 | WS63 为缺少凭据；ESP 还用于设备 ID、JSON 组装等失败，SDK 也可能返回同值 |
| -8 | controller 的 time_ready 不满足 | 网络在线不等于 TLS 时间就绪 |
| -9 | 平台消息队列溢出转成本地错误 | owner 会清理已积压消息 |

这不是整个 SDK 的统一错误码表。SDK create/start、配置准备和 stop 也可能返回自己的负值；相同数值在不同来源可能含义不同。历史 Error -7 应结合调用来源与日志分析，不能仅根据数字认定 TLS 根因。

配置入口可对照 [ESP config.c](../../src/platforms/esp_idf/components/config/config.c) 与 [GoldieOS config.c](../../src/platforms/goldieos/components/config/config.c)。后者还将设备身份、JSON 分配、组装、序列化细分为 -71 至 -74，读取错误时要保留阶段日志。

## 4. 三个重要保护机制

### session 过滤

SDK 类事件必须匹配活动 session，Stop 后迟到的旧事件不能更新新会话。它保护逻辑状态，不替代 SDK 对象释放。

### 上传与播放停止屏障

enable(false) 返回前停止旧上传回调；reset 与实际设备输出同步；SDK destroy 按契约等待回调退出。要同时观察这些边界，不能只检查 UI 已经变为 Ready。

### 队列溢出显式失败

平台消息队列为有限容量。入队失败时设置 overflow，owner 清理队列后调用 controller_overflow。这样不会在丢失 Stop/断网消息后继续假装一切正常；被清理的命令不重放。

注意消息队列和音频队列是不同对象：前者满产生 -9；后者 push 失败可能经 cloud 产生 -3。

## 5. 主机测试覆盖什么

当前完整环境注册 14 项测试，其中网页测试需要可用 Node；如果没有 Node，该测试不会被注册，应报告实际数量，不能一律写 14 项通过。

| 测试名 | 对应重点 | 不证明什么 |
|---|---|---|
| core_lifecycle | 状态、旧 session、断网、超时回绕、重复启停 | 真实线程调度 |
| application_coordination | 前置检查、配网停止顺序、能力缺失 | 真实热点创建 |
| cloud_lifecycle | SDK 替身下的会话与回调边界 | 云服务稳定性 |
| audio_mapping_buffer | 编解码、通道、环形队列和预缓冲 | 麦克风声学质量 |
| goldie_media_pipeline | 设备替身下的锁、写入、reset、排空 | BSP 消费速率与扬声器流畅性 |
| ws63_tls_delivery_diag | 诊断计数、缓存快照与时钟回绕 | 实际 TLS 网络就绪缺陷的根因 |
| provision_validation | 配网输入和 HTTP 解析规则 | 手机兼容性 |
| network_contract | 公共网络列表与状态行为 | 实际网络可达 |
| portal_dns_protocol | DNS 包处理 | 系统一定弹窗 |
| ws63_dhcp_gateway | DHCP 网关兼容处理 | 所有 BSP 版本均兼容 |
| ws63_nv_storage | NV 替身下存储恢复路径 | 所有真实掉电场景 |
| esp_nvs_storage | NVS 替身下快照与迁移 | Flash 硬件耐久性 |
| ws63_cloud_config | 配置组装 | 真实鉴权有效 |
| wifi_web_contract | 网页请求、结果处理等约定 | 真实手机 AP 切换 |

测试桩和 mock 让故障输入可重复，例如模拟写失败。但它们省略了真实硬件的时延、中断、缓存和射频条件。

## 6. 怎样运行已有主机测试

下面是教学命令示例，在项目根执行。需要 CMake、Ninja、适用于宿主的 GCC/MinGW、Node（网页测试），以及测试引用的外部 GoldieOS 头文件与 cJSON 源码。不要使用 WS63 交叉编译器来生成电脑上执行的测试。

```powershell
# 全新独立测试目录；gcc 必须已在 PATH 中且属于宿主工具链。
cmake -S tests -B build-training-tests -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build-training-tests
ctest --test-dir build-training-tests --output-on-failure

# 只查看或运行关注的一项。
ctest --test-dir build-training-tests -N
ctest --test-dir build-training-tests -R core_lifecycle --output-on-failure

# Python 入口需按本机环境替换。
python tools/check_architecture.py
```

每条命令成功后再执行下一条；构建失败时不要拿旧可执行文件的测试结果当作新代码结果。已有配置正确的测试目录可以直接 build/ctest，不要覆盖其他目标缓存。

`check_architecture.py` 检查公共层禁止的头文件、板级服务泄漏、媒体与 cloud/SDK 的耦合等。它是静态规则检查，不会执行会话或自动检测所有依赖。

这里的主机测试目录与未来固件 test/release 构建方案不同；当前没有因为运行这些测试就生成带串口测试协议的单板固件。

## 7. 四层验证证据

| 层次 | 合格表述 | 不应扩大为 |
|---|---|---|
| 静态检查 | 路径、接口边界、指定规则通过 | 所有逻辑无错误 |
| 主机测试 | 给定输入与替身条件下断言通过 | 实机音频必然流畅 |
| 编译链接 | 指定目标产生固件/EXE | 已烧写且功能正常 |
| 实机验收 | 指定固件、硬件和场景验证通过 | 所有网络、手机和压力场景都通过 |

当前利尔达、实战派已有 src 迁移版五项用户验证；润和基础功能曾通过，长回答专项仍有未闭环问题。模拟器最新版本的验收状态以验证记录为准，不把历史另一版本验收自动移到当前产物。

## 8. 排查一段长回答卡顿

建议按层核对，避免同时改变缓冲、优先级和网络算法：

1. 记录固件哈希、板型、网络、复现步骤、是否重启。
2. 检查采集 reads/sends 是否持续，区分读不到、暂时暂停和 SDK 拒绝。
3. 检查 socket 就绪、TLS/BIO 字节数及读取间隔。
4. 检查媒体成功入队间隔和收到的 G711A 字节数。
5. 检查应用 queue、pulled 与设备 PCM 水位，注意单位不同。
6. 检查设备写入耗时，以及错误之前/之后的时间顺序。
7. 一次改变一个行为，使用相近场景复测，保存原始日志。

仅凭 `Speaking` 无声，不能区分服务器未送音频、TLS/SDK 未交付、应用预缓冲、设备写入或功放问题。播放循环间隔包含阻塞写入，也不能直接等同于线程被饿死。

最新 WS63 诊断 `pending_after_read` 是快照，可能变旧；要结合 read_age_ms 与后续读取判读。诊断本身也有串口和 CPU 成本，所以当前使用汇总限频，不打印每一帧内容。

## 9. 实机验证记录模板

```text
目标/板型：
固件路径与 SHA-256：
构建、烧写结果：
环境：网络与音频条件，不记录密码
步骤：
期望：
实际：
发生时间/单调时间：
原始日志位置：
重复次数与成功次数：
是否能通过 Stop/Start 恢复：
结论：已确认 / 推测 / 尚待验证
下一次唯一改动：
```

同一用例跨板复用的方向是统一用户行为和验收条件，而不是假设所有板拥有完全相同的串口命令。统一发现、控制、日志通道和能力协商仍属于[阶段 0 规划](../stage0-contracts.md)。

## 10. 练习与参考答案

问题一：14 项主机测试通过，是否可以关闭卡顿问题？不可以，它们没有覆盖真实云端、网络和 BSP 时序。

问题二：报错之后出现输出锁等待，是否证明该锁导致报错？不能，先后关系必须对齐，停止清理本身也会等待锁。

问题三：把音频队列翻倍后暂不报 -3，是否说明根因修复？不一定，可能只是推迟溢出；还需检查音频完整性、交付节奏和长时间积压。

问题四：一个新板通过编译，最少还应验证什么？启动显示输入、配网与保存、对话、重复启停、对话中进入配网，以及长回答和恢复场景；具体能力差异应写入用例前提。

## 11. 八步结业练习

请独立画三张图：Start/Stop 控制链路、音频数据链路、手机配网链路。每个节点标注目录、关键函数和线程归属。

再对三个改动给出最小范围：更换功放引脚、切换非 GoldieOS 的 WS63 BSP、修改所有板的重连业务规则。能解释为什么不需要复制整套 application，就掌握了当前分层的主要目的。
