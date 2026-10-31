# 验证记录

## GoldieOS 运行归档体积限制（2026-10-29）

- runtime.tar.gz 从 71,753,078 字节重新压缩为 49,332,709 字节，满足严格的 50,000,000 字节上限。
- 保留全部 1794 个文件，逐文件路径、大小和 SHA-256 与 files.json 一致；未修改源码、库或打包模板。更新归档校验值，CMake 增加体积上限检查。
- 本轮验证归档完整性及 CMake 解压接入；未重新编译或烧写，既有功能验收和已知问题状态不变。

## GoldieOS 独立工程迁移（2026-10-20）

- GoldieOS 必要运行依赖固定归档到 third_party/goldieos/runtime.tar.gz，1794 个文件的路径/大小/校验值见随包 JSON。移除旧 apps、AI bridge、旧入口及历史版本打包模板；保留与 ABI 配套的系统/模拟器库，不声称裁剪闭源库内部功能。运行归档与清单上库；WS63 编译器、链接/签名工具包与解压缓存忽略不上库。
- 平台 CMake、工具链定义和打包逻辑归本工程；固件、模拟器、主机测试及 skill 不再依赖原同级示例仓库。GOLDIEOS_ROOT 是构建内解压路径，不是外部仓库入口；旧 BspRoot 参数替换为 GoldieosToolsRoot。工具包可本地提供或配置固定 HTTPS URL/校验值，目前无已确认公开下载地址。
- 四目标在全新 build/solo-* 目录通过更新后的 skill 构建，两款 ESP 合并镜像生成成功；14/14 主机测试、依赖加载器本地/缓存/错误校验测试、架构检查、两款润和 skill 元数据及 PowerShell 语法检查通过。运行归档全部逐文件 SHA-256 校验通过。
- 检查五个新构建目录（含主机测试）的 build.ninja，无原示例仓库路径；迁移准备工具时复制了本机已有编译器到忽略缓存，此后构建只使用工程依赖及已安装/缓存工具，不声称干净 Windows 下载全链路已验证。
- ESP 首轮实战派遇到 GCC 内部 Segmentation fault；降低并行度重试通过。共用 skill 已修正为先 IDF reconfigure，再由 cmake --build --parallel 使用 Jobs 参数。润和 BSP 既有编译警告保留。
- 利尔达为后续烧写恢复此前 build-lrd/sdkconfig 的本地配置并重编；鉴权未输出、不上库。润和沿用 config/ws63.local.h；模拟器随后按用户要求启动：进程 19716，窗口 Responding=True；界面与对话功能待用户验证。
- 本地日志与 build-result.json 位于各 build/solo-*；测试日志位于 build/migration/tests-*.log。工具包详情见 [依赖说明](../third_party/goldieos/README.md)。原示例目录未修改。

| 目标 | 产物（工程相对路径） | 字节数 | SHA-256 |
|---|---|---:|---|
| 利尔达 | `build/solo-lrd/unified_ai_lrd_merged.bin` | 1654208 | `f891545002f933ec92c35f756e6b8786ef27744b9fc468004b9d397715673289` |
| 实战派 | `build/solo-szpi/unified_ai_szpi_merged.bin` | 1657408 | `ca13bb71b25c71acb3919f29b08091fa44d900cf4c9c4197b02e0d0e1cf6328e` |
| 润和 WS63 | `build/solo-ws63/out/unified_ai_hoperun.fwpkg` | 2238984 | `0c9c9d105363c506e6518900b4c211ce2e4e5c052c4303c0bab653d8136ef9e2` |
| Windows 模拟器 | `build/solo-win/unified_ai_simulator.exe` | 3679163 | `71ce1536558d0d17f65a3547e24220b94349c3a031c016525c20f4e59831f4e4` |

- 润和已烧写上述独立工程镜像；成功日志为 build/migration/standalone-flash.log，功能待用户验证。
- 利尔达已烧写上述独立工程镜像；成功日志为 build/migration/lrd-flash.log，功能待用户验证。
- 润和烧写工具退出码 0；随后串口启动日志采集未获执行授权，未据此确认初始化或对话成功。


## 板级头文件按平台范围归位（2026-10-20）

- `src/boards/include/board.h` 移至 `src/boards/esp32/include/board.h`，`ws_board.h` 移至 `src/boards/ws63/include/ws_board.h`；保留现有 esp32 目录名及头文件内容，不修改接口、硬件参数或运行行为。
- 同步 ESP board 组件、GoldieOS 公共构建、主机测试 include 路径，架构检查覆盖两套板级契约；更新架构、培训、平台说明、板卡对比和上库清单。历史迁移表保留旧来源路径，链接指向现位置。
- 利尔达、实战派、润和 WS63 固件和 Windows 模拟器均构建成功，退出码 0；两款 ESP 合并镜像生成成功。14/14 主机测试及架构检查通过。
- 本地日志：WS63/模拟器的 `build-*/board-headers-build.log`，ESP 的 `build-*/board-headers-final-build.log`、`board-headers-merge.log`，测试的 `build-hoperun-win/tests/board-headers-build.log` 及 CTest LastTest.log。日志、固件和 EXE 不上库；原有 BSP 编译警告保留，ESP 配置阶段出现 ROM ELF 调试辅助文件环境变量缺失提示，固件构建及合并仍成功。
- 润和签名打包工具在沙箱外重试成功；本轮没有烧写、启动模拟器或实机验收。已有媒体/TLS 诊断代码和此前待排查问题未因本次头文件移动而消除。

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `6b4c7162146e16dd63b204d533d760c082e5920faca3ed4a803fb8ce7c6248a4` |
| `build-szpi/unified_ai_app.bin` | `00d784944ddfbedc7712cb5d65f28fc0acf8b43593834022cdb151b5bae213ef` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `5b06964f6857196185adfd8b792d368ef5d0ff6e1069b8ee5cfac4cb877eadae` |
| `build-hoperun-win/unified_ai_simulator.exe` | `771ad9dda499757e45487d906646cc286993b0b145e2c8ffe5278a6cbbea6e34` |

## TLS/socket/媒体交付诊断版：构建完成，暂缓烧写（2026-10-16）

- 新增 WS63 TLS/BIO 字节、socket 就绪与 TLS 读取后剩余明文快照统计，媒体成功交付间隔和统一单调时间；只增加被动观测，保持播放策略、21/22 优先级、poll 算法、TLS 返回语义与证书验证。字段含义和判读边界见 [定位记录第 8 节](hoperun-audio-investigation.md)。
- 14/14 主机测试及架构检查通过，润和固件编译成功，2237992 字节，SHA-256：5b06964f6857196185adfd8b792d368ef5d0ff6e1069b8ee5cfac4cb877eadae。构建日志 tls-delivery-build.log；未重编 ESP 或模拟器应用。
- 烧写尝试无法打开 COM4，工具退出码 3，未开始下载。用户随后要求保留版本、先梳理代码，已停止烧写与采集。本诊断版未烧写、实机未验证；长回答卡顿、Error -3、Listening 不识别及启动 set_rc 仍待复现确认。
- 本轮修改：WS63 sdk_platform.c、新增 delivery_diag.h；GoldieOS media.c；tests/CMakeLists.txt、test_goldie_media.c、新增 test_ws_delivery_diag.c，时钟替身从 lwip/arch/sys_arch.h 移到 lwip/sys.h；本记录、音频定位记录及上库清单。日志和镜像仍为本地产物，不上库。

## 21/22 固件复测：Speaking 长停顿后恢复（2026-10-16）

- 后续继续采集在 01:38:13 附近再次确认 Error -3 分支：incoming=240、queued=15920、capacity=16000、reason=media_push。此前三个相邻窗口合计 3130 ms 收到 32640 字节、取出 24560 字节，净积压 8080 字节；播放取出速率仍接近实时。失败后出现 5 次 SDK 发送队列满、3000 ms stop timeout，330 ms output 锁等待位于停止/重置阶段，不能当作先发原因。跟进快照和哈希见定位文档第 7.4 节。
- 用户复现长回答卡顿、中途停在 Speaking，随后补充约半分钟后恢复、仍严重卡顿。原始日志捕获到约 69 秒媒体活动汇总间隔；以 68 个采集窗口累计估算，不能等同于精确静音时长。期间采集/上传持续，恢复后回答最终完成，暂不能解释为播放线程或 UI 永久死锁。
- 相近音频量的第二次完整回答对照：调整前 185280 字节、response.created→done 为 31 秒；本次 185760 字节、105 秒。最大队列采样值 7424→7408，最大设备写耗时均 120 ms，循环间隔 530→550 ms。未显示稳定改善，也不能由不同请求的单次结果证明优先级调整使问题加重。
- 第一份复现快照中的完整回答未出现下行队列拒绝，中途有 6 次 SDK 发送队列满；后续新采集已再次出现下行拒绝，见本节首条，Error -3 未解决。细节、快照哈希和诊断边界见 [音频定位记录](hoperun-audio-investigation.md)第 7 节。
- 优先级设置运行时确认尚缺：本次原日志从运行中开始，没有两条 priority/set_rc 启动记录。ELF 已确认含设置代码，但不能据此声称 set_rc=0。第一次补采重启时串口断开，原采集异常退出；已开启支持重连的 priority-21-22-boot-capture.log 继续采集，等待实际启动结果。
- 本轮只采集、分析并更新 docs/hoperun-audio-investigation.md、docs/validation.md、docs/repository-guide.md；未改运行源码或重新编译烧写。分析脚本、复现快照及日志位于忽略的 build-hoperun-app。

## 单变量：润和音频线程恢复 21/22（2026-10-16）

- COM4、115200 baud 全部分区烧写成功，工具报告 Burned successfully，退出码 0；日志 priority-21-22-flash.log。本次无需再次请求复位握手。已开启独立 priority-21-22-capture.log 十分钟串口采集，待正常重启后确认两个线程 set_rc=0，并由用户验证长回答、Error -3 和连续对话，尚未认定修复。
- 仅在 PLATFORM_TYPE_WS63 分支将播放线程设为 21、采集线程设为 22；创建和设置位于调度锁内，设置后解锁，再输出一次 set_rc 供实机核对。线程创建失败仍沿用原 -19/-20；优先级设置返回值记录在日志中，不另改初始化错误策略。Windows 模拟器保持原默认创建方式，ESP 不受影响。
- 保留当前一次预缓冲、16000 字节 G711A 队列、4096 设备补充目标、512 字节/最多 8 批、休眠、采集生命周期、SDK .835、poll、TLS、会话参数及既有周期诊断，避免同时改动其他变量。
- 13/13 主机测试及架构检查通过。媒体测试新增检查：调度锁内创建并按采集 22/播放 21 设置、设置完成后解锁、创建失败也释放调度锁且不设置空句柄；原播放与预缓冲回归仍通过。主机桩不模拟真实抢占，不能据此认定卡顿已修复。
- 使用已配置的最新 WS63 目标和现有工具链增量构建成功，固件 2237256 字节，SHA-256：c7a509cb9962d29889cd1b193c77a94f1c8b9172dd6954bdbccde152690a19f3。产物 build-hoperun-app/out/unified_ai_hoperun.fwpkg，构建日志 priority-21-22-build.log。未重新构建其他三个目标。
- 本轮上库修改：src/platforms/goldieos/components/media/media.c、tests/test_goldie_media.c、tests/stubs/goldie_media/goldie_osal.h、docs/hoperun-audio-investigation.md、docs/validation.md、docs/repository-guide.md。本地烧写/采集脚本及日志不上库。

## 原始 GoldieOS 示例对照（2026-10-16）

- 已将单次预缓冲复测结论与本轮原始示例对比保存到 [润和音频定位记录](hoperun-audio-investigation.md)。本轮基线为 D:/Code/ai-app-arch/ai-hardware-agent-examples/examples/goldieos，不是此前 dbwu 下的旧 unified-app 快照。用户反馈原始版本历史流畅，本轮尚未复测该原始固件。
- 最新 bdcbfdea… 固件日志确认重复预缓冲已取消，但仍由 16000 字节下行队列满触发 Error -3；详细数值及证据边界见下节。本轮结束采集并释放 COM4，未改运行代码、未编译或烧写新固件。
- 明确差异：原始播放/采集优先级 21/22，当前未设置、默认 10/10。对最新 ELF 的相关符号反汇编确认 BSP 音频线程为 20，且数值越小优先级越高。因此当前应用音频线程优先于底层音频，调度关系与原始设置相反。尚未通过运行时调度追踪或 A/B 证明其为卡顿根因；下一轮优先仅恢复 21/22 对照，其余参数保留，poll 截止时间调整后置单独验证。
- 原始满队列行为为丢弃当前包、累计 dropped_bytes 并继续；当前显式失败并停止。原始 16000 字节是 PCM、约 1 秒，当前为 G711A、约 2 秒。不能将无 Error -3 等同于无溢出，也不能判断当前队列更小。设备补充目标 6144/4096、补充批次及休眠不同，需独立对照。
- 原仓库默认 WS63 SDK 为 26.8.0.597-0eac16a，当前为 26.8.0.835-2c8da6d9；原仓库 test/sdk 也存在与当前一致的 .835。已找到的原构建缓存为 Windows，不能证明历史流畅 WS63 固件链接了 .597。VAD、TTS 启动参数及录音生命周期另有差异，未盲目恢复旧配置或削弱 TLS 校验。
- 对两份实际 C 编解码实现执行主机穷举：下行解码 256/256 一致；上行编码 65536 个输入中 61023 个结果不同，此数字只表示差异，不表示错误数。下行解码公式排除为差异来源，上行差异与连续识别另行评估。此专项对照不覆盖设备调度或云端表现；本轮未重跑固件全套测试。
- 本轮上库文件：新增 docs/hoperun-audio-investigation.md，更新本记录及 docs/repository-guide.md。比较程序、反汇编保留在 build-hoperun-app，连同串口日志不上库。

## 单次预缓冲版本实机复测（2026-10-16）

- 用户复现长回答卡顿后 Error -3；固件 bdcbfdea…，日志 prebuffer-once-capture.log。该改动未解决完整故障。
- 取消重复预缓冲行为已得到日志支持：回答开始后窗口出现 in_g711=960/pulled_g711=960/queue=0/writes=2，以及 720/720/0/writes=2；相较旧诊断连续多秒 queue=960 且不取出，本版小包能继续进入设备。首次尚未播放时出现 queue=240、pulled=0 属于保留的首次预缓冲，不是回归。
- 拒绝时 incoming=240、queued=15872、capacity=16000，仅剩 128 字节；last_loop_age_ms=370、last_device_bytes=3648。关联 1320 ms 窗口成功入队 27600 字节、取出 12752 字节、队列快照 14848，写入 27 次，gap_max_ms=470、write_max_ms=100、output_wait_max_ms=0。统计和拒绝来自不同线程/采样时点，不能把快照数值视为同一瞬间。到达量相当于 3.45 秒 G711A 音频，明显超过该窗口时长，仍不足以断定云端长期发送速率或网络根因。
- 报错前也出现 queue=0/device_pcm=0 且入队量偏少或无新增的窗口；整体表现为下行欠供与集中交付交替。设备消费没有因软件队列门槛再次停止，但有限缓冲仍可能被突发填满。
- 采集线程持续运行，报错前多数窗口无失败，报错停机阶段出现发送队列满及 3000 ms stop timeout。poll 请求 100 ms 而实际多为 200 ms 的问题仍在；当前数据不能证明该超时偏差单独导致本次 Error -3。
- 当时建议下一单变量将 WS63 poll 从固定休眠次数改为单调时钟实际截止时间，再核查 TLS 已缓冲明文与 socket 就绪处理。后续原始示例对照发现更直接的优先级回归候选，最新验证顺序见上节；poll 偏差仍保留为待验证项。本轮仅分析、更新验证记录和清单，未改运行代码或烧写。

## 单变量：同一回答仅首次预缓冲（2026-10-16）

- COM4、115200 baud 全部分区烧写成功，Burned successfully，退出码 0。首次握手超时未开始下载，用户复位后重试成功，日志 prebuffer-once-flash-retry.log。已开启本版独立 prebuffer-once-capture.log 十分钟采集，等待正常重启与实机长回答复测。
- 按用户要求仅调整 ai_playback_pull 的 playing 状态：首次实际取出数据后保持 true，队列暂时取空或空读不重新等待预缓冲；ai_playback_reset 仍清除此状态，下一轮首次预缓冲及短回答 finish 放行保留。注释明确 playing 表示该回答已开始取出，不代表扬声器当前有声。
- 未修改 WS63 播放批次/水位/锁、SDK poll、TLS、优先级、队列容量或诊断输出。改动位于公共媒体服务，未来重编 ESP 与模拟器也会采用此规则；本轮仅编译润和单板，其他目标未重新构建或实测。
- 新增公共队列及 GoldieOS 媒体链路回归：旧实现两项均失败；改后 13/13 主机测试及架构检查通过。覆盖同回答取空后的连续小包、空读不重置、下一轮重新预缓冲及 finish 放行短回答，包含日志中 queue=960/device=0 的复现。
- 润和编译打包成功，2237160 字节，SHA-256：bdcbfdea8add0cade4bf47a40f9384d86fbb4b5798367b3381a635d5611857eb。构建日志 prebuffer-once-build.log；实机长回答及 Error -3 是否改善待验证，不能由单测认定修复全部故障。
- 本轮修改 src/application/services/media/ai_playback.c、其 include/ai_playback.h、tests/test_audio.c、tests/test_goldie_media.c、docs/validation.md 和 docs/repository-guide.md。

## 最新诊断版再次复现的链路分析（2026-10-16）

- 用户复现长回答卡顿后 Error -3。latest-sdk-restored-capture.log 确认 incoming=240、queued=15840、capacity=16000，拒绝时最后完成循环距今 240 ms，历史设备水位 3328 字节；仍为下行入队失败。
- 连续七个约 1 秒窗口（1020/1030/1020/1010/1030/1020/1060 ms）共收到 67680 字节 G711A、取出 57344 字节，合计 7190 ms，约 9413/7976 字节每秒；播放取出速率接近 8 kHz G711A 实时速率，积压增加 10336 字节。不能把这一段队列满直接解释为播放线程不运行，也不证明云端长期发送速率过快，网络/SDK集中交付仍需区分。
- 纠正先前推断：gap_max_ms 约 500–550 是整轮循环起点间隔，含最多 8 次设备写入；每次写入可等待 80–120 ms。它不是独立测得的调度饥饿时长。已链接 BSP ring_buffer_bulk_write_block 在空间不足时 sleep(10) 重试，批量设备写入本身能形成长循环。报错前 output_wait_max_ms=0；报错后的 290 ms 出现在 reset/停止阶段，不应倒置为先发原因。
- 明确发现重复预缓冲停播路径：ai_playback_pull 在取空后将 playing 置 false，下次不足 1600 字节便不取。日志连续多秒 queue=960、device_pcm=0、pulled_g711=0、writes=0，说明已经到达的 120 ms 音频因重复预缓冲门槛等待。它能解释部分静音/延迟，不足以单独解释满队列，旧版本也有此逻辑。
- 采集侧持续约 46–54 次读取/上传每秒、bad_reads=0，send_max_ms 通常为 0，偶有 SDK 发送队列满及负返回。因此此段不是麦克风完全停采或上传调用长期阻塞；调用成功不等于网络发送成功。poll 请求 100 ms，实测多为 200 ms，确认切片计次数实现未控制实际截止时间；与既有 Listening 异常的因果关系仍待验证。未记录慢 TLS 日志只说明未命中当前限频/阈值，不代表网络没有问题。
- 下一步建议按独立变量验证：先处理同一回答内取空后重复预缓冲；再修正 poll 的实际截止时间并观测发送队列；最后根据下行到达速率检查有界缓冲/背压策略。不能以调大缓冲或修改优先级替代上述定位。本轮仅更新文档与清单，未修改运行代码或烧写。

## 恢复当前最新诊断版继续定位（2026-10-16）

- 用户反馈旧版表现很差，明确后续使用当前目录最新版本定位，暂停旧版及旧媒体策略对照。旧版卡顿结果保留，不推导为新版本已修复。
- 已恢复 build-hoperun-app/out/unified_ai_hoperun.fwpkg，SHA-256：49566e48ed01c98f8a46e3e5993b9da0d27b2685d7894826c6ec7bc50119c6df。COM4、115200 baud 全部分区烧写成功，Burned successfully，退出码 0；成功日志 latest-sdk-restore-retry.log。首次串口打开失败未开始下载。
- 停止旧版采集，恢复版使用独立 latest-sdk-restored-capture.log 十分钟采集；等待正常重启并复现长回答与连续对话。未修改运行代码或重新构建，本轮只更新验证记录与上库清单。

## 旧固件基线同环境复测仍卡顿（2026-10-16）

- 用户确认已烧写的旧固件 5ac86baa… 同板同网络测试也有较多卡顿。因此历史流畅反馈未在本轮重现，不能将当前卡顿简单归因于近期架构整改或播放批次/锁调整；也不能据此完全排除这些改动影响严重程度。
- 本轮 baseline-old-capture.log 已记录至少三次 response.done，检查时未发现 send queue full、录音读取失败或 SDK stop timeout。日志中的 dhcp timeout timeout_ms value is: 10000 是参数输出，不能当作实际 DHCP 超时故障。回答完成事件不证明声音流畅，也不能替代用户听感。
- 旧固件是否同时出现 Error -3 或 Listening 无识别仍待用户确认，不沿用新固件结论。下一步应优先以同一旧固件更换网络做对照，并结合本地音频播放区分网络/云端到达抖动与公共板级播放路径问题；尚未完成这些测试，不认定网络或云端为根因。本轮未改运行代码或烧写，仅更新验证记录与上库清单。

## 旧固件同板同网络基线烧写（2026-10-16）

- 按用户要求直接烧写 D:/Code/dbwu/ai-hardware-agent-examples/build-hoperun-app/out/unified_ai_hoperun.fwpkg，未重新编译，未修改基线目录或当前运行代码。核对 SHA-256：5ac86baa28a325e5b6c0403adcd275ec0a4b233a369e50fc02e2d8255f8950d9，大小 2234760 字节。
- COM4、115200 baud 烧写全部分区成功，Burned successfully，退出码 0。日志为本地 build-hoperun-app/baseline-old-flash.log。旧诊断采集进程已结束；本次使用独立 baseline-old-capture.log 记录基线启动和对话。
- 待用户在同一网络验证长回答、连续多轮对话及 Listening 后继续识别；未把历史“流畅”反馈当作本轮验证结果。当前版本单变量对照尚未实施，等待基线实测反馈。
- 本轮仅修改 docs/validation.md 和 docs/repository-guide.md；烧写及采集脚本、日志不上库。

## 与用户提供的流畅基线对比（2026-10-16）

- 基线目录 D:/Code/dbwu/ai-hardware-agent-examples 实际是旧 unified-app 布局（core/services/ports/boards），不是原始 GoldieOS 设置应用。仅只读检查，未修改基线。其现存固件 SHA-256 为 5ac86baa28a325e5b6c0403adcd275ec0a4b233a369e50fc02e2d8255f8950d9；旧 CMakeCache 仍引用原 D:/Code/ai-app-arch 路径，不能直接在旧目录增量构建，亦不能仅凭缓存证明所有源码与固件完全对应。
- 两目录 SDK 26.8.0 WS63 静态库 SHA-256 相同；生成 ws63_config.h 整文件哈希相同（未输出私有内容），包括 16000 字节播放容量。ai_audio.c、ai_playback.c、ai_lvgl.c 无差异。cloud 音频回调仅新增拒绝日志，config.c 仅更换服务头文件。
- SDK 平台文件除本轮临时诊断外无行为差异：线程优先级转发、poll 切片、TLS 读写及非阻塞设置相同。对照不能支持这些逻辑被本次架构整理改坏的结论，但相同逻辑仍可能存在共同缺陷。
- 比较两份 ws63-link-root/libs 中 155 个 BSP 库：154 个文件哈希一致，仅 libmbedtls.a 不同；该库 objdump -d 输出除归档路径外相同，未发现指令差异。归档文件哈希差异不直接等价于运行逻辑差异。独立 opus_build/libopus.a 哈希不同，尚未逐对象核实；当前下行是 G711A。
- 主要媒体行为差异：旧版每轮最多取 160 字节、持 rx 锁调用设备、休眠 5 ms；当前每批最多 512 字节、最多 8 批、独立 output 锁且设备调用不持 rx。旧版设备等待会间接阻塞入队，当前允许下行继续入队，可能改变 SDK 回调节奏与队列峰值。该改动是 Error -3 出现后的尝试，不能解释最初回归，尚未证明其造成后续加重。
- 板级电源轮询及 GPIO 日志行为未变；驱动等待与服务查询只是迁入平台层。新控制器将网络描述刷新检查由 1000 ms 改为 500 ms，但仅内容变化时更新 UI；新增公共网络列表及状态封装、入口消息封装，尚未发现其直接造成音频长间隔的证据。
- 建议下一步按单变量验证：先在同板同网络重测基线现存固件（确认配置/服务条件相同）；再在当前分层架构中恢复旧版媒体节奏并关闭临时周期诊断对照。基线若也卡顿，应转向共同网络/云端/SDK条件；基线流畅且媒体对照恢复，才支持媒体路径回归。不能据源码对比直接宣布根因已定位。本轮未编译、烧写或修改运行代码。

## SDK 收发与采集补充诊断（2026-10-16）

- 用户复位后通过 COM4、115200 baud 烧写成功，全部分区完成，退出码 0；成功日志 error3-sdk-flash-final.log。此前握手超时和串口打开失败均未开始下载。随后开启 error3-sdk-capture.log 十分钟采集，等待正常重启及复现，尚无本版实机结论。
- 检查本地已链接 ELF：goldie_thread_create 转发 osal_kthread_create，默认创建优先级为 10；goldie_thread_set_priority 直接转发 LOS_TaskPriSet。未据此认定调度根因，未修改优先级。
- BSP audio_get_valid_length(NULL) 读取用户播放环形缓冲计数，play_audio_write 调用阻塞式环形缓冲写入。因此先前“设备水位”不能直接称为 DMA 水位；write 耗时可能包含缓冲等待与调度抢占。
- 新增 WS63 SDK 线程创建时请求优先级及设置结果日志；每个 socket 每秒最多一次 poll 的请求等待、实际耗时和就绪事件；TLS read/write 耗时至少 100 ms 时每方向每秒最多一条，含返回值及 read 返回后剩余明文字节。未改变收发、轮询、TLS 校验或调度策略。低频日志仍可能影响时序。
- 采集线程每秒汇总读取次数/非正返回次数、调用上传次数/负返回次数、最大读取/上传耗时及 enabled。上传调用非负不证明网络已经发出，SDK 内部发送队列仍需与这些记录对照；无日志也不能单独证明线程停止。
- 润和固件编译成功，SHA-256：49566e48ed01c98f8a46e3e5993b9da0d27b2685d7894826c6ec7bc50119c6df；13/13 主机测试、架构检查通过。现有测试不覆盖真实 TLS、串口日志开销或 RTOS 调度。未重新构建 ESP 与 Windows 模拟器。
- 本轮修改 media.c、components/convai/ws63/sdk_platform.c、docs/validation.md 和 docs/repository-guide.md；本地 error3-sdk-build.log、error3-sdk-flash.log、diagnostic-disassembly.txt 不上库。问题仍待本版复现定位。

## 润和耗时诊断版复现结果（2026-10-16）

- 用户反馈首轮约 100 字回答后显示 Listening 但不能识别语音，Stop/Start 后恢复；再次请求 100 字故事仍明显卡顿，约十几个字后 Error -3。固件为 a742958e… 诊断版，问题未修复。
- 首轮 response.done 后软件队列及设备缓冲均排空；随后日志反复出现 SDK `send queue full`。这支持上传链路异常的排查方向，但不证明麦克风停止采集，也不能证明具体 SDK 线程死锁。
- 再次失败前，1250 ms 窗口成功入队 16560 字节 G711A、取出 9904 字节、队列积压 8560 字节；之后拒绝时 incoming=240、queued=16000、capacity=16000。拒绝时最后完成的播放循环距今 530 ms，上次设备水位 3104 字节（历史快照）。后续汇总 gap_max_ms=530、write_max_ms=120、output_wait_max_ms=0、level_max_ms=0。停止阶段仍出现 3000 ms 超时。
- 实际证据同时包含下行集中到达与播放循环长间隔，不能仅归因于设备 write 或缓冲容量。8 kHz G711A 正常播放约消耗 8000 字节/秒；单个 1250 ms 窗口的到达量明显超过对应播放量，但尚不能推断长期发送速率。循环间隔包含执行、休眠、抢占及日志开销；单次写入耗时也包含被抢占时间。
- 当前 output 锁等待和水位查询耗时未见明显增加；仍需定位循环长间隔的来源，以及 SDK 收发积压是否相关。串口十分钟采集已完成并释放 COM4，证据为本地 error3-timing-capture.log。本轮仅更新验证记录与上库清单，未改运行代码或烧写。

## 润和播放耗时与水位诊断版（2026-10-15）

- COM4、115200 baud 烧写成功，全部分区完成，Burned successfully，退出码 0。随后开启 error3-timing-capture.log 专用采集（最长十分钟），待用户正常重启并复现长回答卡顿/Error -3；烧写成功不代表问题修复。
- 新增仅 WS63 编译启用的媒体诊断，使用 lwIP sys_now 单调毫秒时钟。活动期间每秒最多汇总一次：窗口长度、成功入队/取出 G711A 字节数、软件队列字节、设备 PCM 缓冲字节、循环起点间隔最大值、output 锁等待最大值、audio_write 耗时最大值、水位查询耗时最大值、写入次数和最近返回值。取出量不等于实际扬声器播放量，最大耗时包含线程被抢占时间。
- 入队失败额外输出最后完成循环距今毫秒数及上次设备水位；该水位是历史快照，SDK 回调不调用设备接口。共享计数及快照由原 rx 锁保护，汇总打印在 output/rx 锁外；不改变队列大小、水位、播放批次、线程优先级或丢弃策略。诊断仍有测量开销，不能视为最终发布配置。
- 13/13 主机测试通过，goldie_media_pipeline 编译覆盖 WS63 诊断分支，验证慢写入计时快照、字节计数及拒绝包不计入成功接收量；架构检查通过。润和编译打包成功，镜像 SHA-256：a742958ea03d999ae8d2eadc9d096c96b3f1d51442930aa9b4442bff728151e3，2236200 字节。未重新构建 ESP 和 Windows 模拟器程序。
- 本地构建/烧写日志为 build-hoperun-app/error3-timing-build.log 和 error3-timing-flash.log；烧写及实机复测状态见后续记录。问题仍未修复，需本版复现时序数据进一步定位。
- 本轮文件修改：src/platforms/goldieos/components/media/media.c、tests/test_goldie_media.c、新增 tests/stubs/goldie_media/lwip/arch/sys_arch.h、docs/validation.md、docs/repository-guide.md。

## 润和播放调整版长回答复测未通过（2026-10-15）

- 用户反馈播放调整版长回答仍卡顿并出现 Error -3；COM4 重新采集期间用户再次确认复现。对应固件为下方 64ad8970… 版本，主机测试通过不代表实机播放问题解决。
- 本轮追加日志确认 `incoming=240 queued=15904 capacity=16000`，紧接 `reason=media_push type=0 bytes=240 push_rc=-1`。本次仍为下行音频队列空间不足，并非音频格式拒绝。与旧诊断的 `queued=15840 playing=1` 区分，避免误用历史日志。
- 根因仍待定位：当前记录不能区分线程调度停顿、设备写入阻塞、设备缓冲水位异常或下行突发积压。日志中存在重复的 GPIO14 输出，尚无证据将其认定为卡顿原因。后续需测量播放循环间隔、设备写入耗时和缓冲水位。
- 本轮仅更新验证记录与上库清单，未改运行代码、未编译或烧写新的固件。诊断证据追加在本地 build-hoperun-app/error3-diagnostic-capture.log，不上库。

## 润和播放调整版测试与构建（2026-10-15）

- 已通过 COM4、115200 baud 烧写上述调整版，全部分区下载完成，工具报告 Burned successfully，退出码 0；日志为 build-hoperun-app/error3-refill-flash.log。烧写工具已退出，COM4 已释放，待用户正常重启并验证长回答。
- 用户确认继续执行后，新增 goldie_media_pipeline 及全部主机测试 13/13 通过，架构依赖检查通过。新增测试验证设备写入期间允许音频入队、按水位补充、停止清理、设备排空后仅发送一次完成事件，以及写失败处理；不模拟真实 RTOS 调度或音频 DMA 时序。
- 润和 WS63 调整版编译打包成功，SHA-256：64ad8970cd7cc875d137c94e1b6009fd6a7ddb88569a00f43f1167d36e4c0bf0。既有 BSP 编译警告保留。ESP 和 Windows 模拟器本轮未重新构建。
- 构建证据：build-hoperun-app/error3-refill-build.log；测试证据：build-hoperun-win/tests/error3-refill-build.log 和 Testing/Temporary/LastTest.log，仅本地保留。
- 长回答卡顿、无声及 Error -3 是否改善，需本版实机验证，不能由主机测试推断已修复。本轮继续使用上一轮播放调整源码，未再次修改运行逻辑。
- 本轮文件修改仅 docs/validation.md、docs/repository-guide.md；其余播放调整及测试源码沿用上一轮，未新增运行代码改动。

## Error -3 触发确认与待验证播放调整（2026-10-15）

- 用户补充：Speaking 无声超过半分钟后偶尔播出一两个字，很久后恢复但仍卡顿，随后 Error -3。
- 诊断日志确认：`incoming=240 queued=15840 capacity=16000 playing=1`，紧接 `reason=media_push type=0 bytes=240 push_rc=-1`。本次 -3 的直接触发是下行队列空间不足，排除本次为格式拒绝；播放消费为何长时间停滞仍需进一步验证。发送队列满为另一条 SDK 告警，不能等同于下行队列溢出。
- 待验证改动仅在 GoldieOS 媒体适配：独立 output 锁串行化播放设备与 reset，队列锁不跨设备调用；按原 4096 字节设备水位、有界批次补充音频，每次最多 512 字节编码数据。仍保留 16000 字节队列与溢出报错，不丢包，不改线程优先级。
- 新增 goldie_media_pipeline 主机测试，覆盖写设备期间可入队、水位控制、reset、真实播放排空后完成、设备写失败。但测试执行授权被拒绝，本批测试未执行，播放调整未编译、未烧写；此前 12/12 通过只对应失败日志诊断版，不覆盖本批修改。
- 单板仍运行已烧写的诊断固件 ac807bb43e86920db3fa6a39620d323c4de6090d65b6178ac4d6088231385a12。问题未宣布修复。
- 本轮新增/修改：媒体实现、cloud 失败日志、test_cloud.c、tests/test_goldie_media.c、tests/stubs/goldie_media/goldie_osal.h、tests/stubs/goldie_media/service_manager.h、tests/CMakeLists.txt、验证记录及上库清单。串口证据为本地 error3-diagnostic-capture.log，不上库。

## 润和回答卡顿与 Error -3 诊断（2026-10-15）

- 用户在五项基本功能验收后补充：润和回答过程中出现卡顿并报 Error -3；COM4 采集期间再次复现。因此五项验收保留为当次测试结果，不能推导为长回答播放无异常。
- 源码确认回答期间 -3 来自下行音频格式拒绝或媒体入队失败；设备播放写失败另报 -6。当前固件播放队列为 16000 字节 G711A，约 2 秒音频。现有采集日志未区分两个 -3 分支；停止阶段有 SDK 3000 ms 等待超时记录，尚不能据此认定卡顿根因。
- 仅新增失败日志：ai_cloud.c 输出格式/入队失败分支、类型及包长；GoldieOS media.c 输出入队失败时包长、队列占用和播放状态。不增加队列、不丢包、不改播放时序。test_cloud.c 补充入队失败触发 -3 及后续音频不重复入队的检查。
- 诊断版润和构建通过，12/12 主机测试及架构检查通过。镜像 SHA-256：ac807bb43e86920db3fa6a39620d323c4de6090d65b6178ac4d6088231385a12。诊断不等于修复；ESP 和模拟器未重新构建，仍为此前版本。
- 诊断版通过 COM4、115200 baud 烧写成功，全部分区下载完成，工具退出码 0。首次握手超时未开始下载，重试成功；证据为 error3-flash.log、error3-flash-retry.log。随后开启 error3-diagnostic-capture.log 采集，等待用户复现，不预先认定队列溢出。
- 诊断版测试时用户先复现历史配网问题：手机输入密码并点击连接保存后，配网页面退出。当前日志记录 STA 连接及 DHCP 成功，但尚不能证明页面退出的原因或手机热点是否断开；此问题与回答期间 -3 分开记录。音频诊断日志尚未捕获拒绝分支，需完成联网后继续复现。
- 随后用户反馈回答断续、界面 Speaking 但无声音。诊断日志出现 WebSocket send queue full，未出现新增格式拒绝/下行入队失败日志；这是 SDK 发送队列告警，不能当作下行播放队列溢出的证据。继续检查 Stop/Start 恢复行为，尚未确认根因或修复。
- 本地日志为 build-hoperun-app/error3-capture.log、error3-build.log 和 build-hoperun-win/tests/error3-build.log，不上库。
- 本轮文件修改：src/application/services/cloud/ai_cloud.c、src/platforms/goldieos/components/media/media.c、tests/test_cloud.c、docs/validation.md、docs/repository-guide.md。

## Windows 模拟器 src 迁移版启动（2026-10-15）

- 核对最新 build-hoperun-win/unified_ai_simulator.exe 的 SHA-256 为 ab5edea88183a5a78456afd3c2a86934cfe14780f91577ca78d05b9cf8c70875，与 src 迁移构建记录一致。
- 以 build-hoperun-win 为工作目录启动，进程 ID 18188；启动后检查进程存活、存在窗口标题，Responding 为 True。此检查不替代页面及音频功能验收。
- 界面和对话功能待用户验证；模拟器使用 Windows 网络，不提供 Wi-Fi 配网。本轮仅更新验证记录与上库清单，未改运行代码或重新构建。

## 润和 src 迁移版用户验收（2026-10-15）

- 用户确认显示、配网、对话、重复启停和对话中进入配网五项验证正常。
- 对应镜像 SHA-256：c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2，与下方 COM4 烧写记录一致。
- 本次五项功能验收通过；此前首次配网偶发断连、Error -7、Thinking 停滞的历史反馈保留。本轮没有针对这些偶发问题修改代码或完成专项压力验证，不据此宣称根因已修复。

## 润和 src 迁移版烧写（2026-10-15）

- 使用 COM4、115200 baud，烧写 build-hoperun-app/out/unified_ai_hoperun.fwpkg；握手完成，全部分区下载成功，工具报告 Burned successfully，退出码 0。工具已退出，串口释放。
- 烧写前核对 SHA-256：c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2，与 src 迁移构建记录一致；该固件哈希与此前媒体契约版相同，目录整理不视为故障修复。
- 显示、配网、对话、重复启停和对话中进入配网，待用户正常启动单板后验收。首次配网偶发断连、Error -7、Thinking 停滞仍保留为已知问题，不以烧写成功替代验证。
- 本地日志：build-hoperun-app/src-layout-flash.log，不上库。本轮同时记录利尔达用户五项验收通过，仅更新验证记录与上库清单，未修改运行代码或重新构建。

## 利尔达 src 迁移版用户验收（2026-10-15）

- 用户确认显示、配网、对话、重复启停和对话中进入配网五项验证正常。
- 对应镜像 SHA-256：4fe77fd6b2e9cc0a9cca5d24122de10ae8a52dc531cd653c651803ab91204244，与下方 COM6 烧写记录一致。
- 本结论限于本次利尔达镜像和上述功能；不覆盖长期压力场景、润和或 Windows 模拟器。润和已知问题保持待定位。

## 利尔达 src 迁移版烧写（2026-10-15）

- 使用 COM6、460800 baud，按 build-lrd/flash_args 烧写 8 MB Flash 板型的启动、分区、OTA 初始数据及应用。工具识别 ESP32-S3 revision v0.2，四段数据校验通过，退出码 0，随后复位启动；未执行整片擦除。
- 对应合并镜像 unified_ai_lrd_merged.bin 的 SHA-256：4fe77fd6b2e9cc0a9cca5d24122de10ae8a52dc531cd653c651803ab91204244，与 src 迁移构建记录一致。
- 复位后采集 35 秒串口：记录一次 ROM 启动、应用加载、Wi-Fi 连接及获得 IP；未观察到 Guru Meditation、Brownout、abort 或断言失败标记。此短时检查不代表长期稳定性或交互功能验收。采集结束，COM6 已释放。
- 显示、配网、对话、重复启停及对话中进入配网待用户验证，不继承实战派验收结果。本轮未修改运行代码或重新编译。
- 本地烧写日志：build-lrd/src-layout-flash.log；启动采集日志：build-lrd/src-layout-boot.log。日志及辅助采集脚本不上库。

## 实战派 src 迁移版用户验收（2026-10-15）

- 用户确认显示、配网、对话、重复启停和对话中进入配网五项验证正常。
- 对应镜像 SHA-256：45c9179a7adb3f430183ffb9ac7a0b8d7319b74eb8892facf37c1abc7f5e6a48，与下方 COM8 烧写记录一致。
- 本结论限于本次实战派镜像及上述功能，不代表长期压力验证，也不覆盖利尔达、润和或模拟器；润和已知问题保持未解决。

## 实战派 src 迁移版烧写与启动检查（2026-10-15）

- 通过 COM8、460800 baud，按 build-szpi/flash_args 写入启动、分区、OTA 初始数据及应用；识别 ESP32-S3 revision v0.2，四段数据校验通过，工具退出码 0，并复位启动。未执行整片擦除。
- 对应合并镜像 unified_ai_szpi_merged.bin 的 SHA-256：45c9179a7adb3f430183ffb9ac7a0b8d7319b74eb8892facf37c1abc7f5e6a48，与本轮 src 迁移构建产物一致。
- 烧写后复位采集 35 秒串口：观察到一次 ROM 启动、应用加载、Wi-Fi 连接及获得 IP；未观察到 Guru Meditation、Brownout、abort 或断言失败标记。采集结束，COM8 已释放。此窗口不能证明长期稳定，也不代替显示、音频或对话验收。
- 显示、配网、对话、重复启停及对话中进入配网，待用户在实战派上验证。润和已知问题状态不变。
- 本地证据：build-szpi/src-layout-flash.log、src-layout-boot.log；日志及采集脚本不上库。本轮未改运行代码、未重新构建；仅更新本验证记录和上库清单状态/行数。

## src/application/core、platforms 与 convai 迁移（2026-10-15）

- 产品源码迁入 src；公共应用目录为 application/core、services、ui；平台目录为 platforms；两平台 components/sdk 改为 components/convai。保留内部接口、组件名、板型参数及产物名称。
- 对操作前工作区快照逐文件核对：运行源码、头文件及网页除一处注释路径更新外保持一致，config 上库模板不变；仅目录、构建引用、检查工具、构建技能入口和文档调整。
- 四目标构建成功、两款 ESP 合并镜像生成成功，12/12 主机测试及架构检查通过。润和两目标用新入口重新生成 CMake 配置；ESP 沿用原板型缓存重新配置。使用本机已有工具链，不声称干净环境安装验证。外部 BSP 既有警告保留。
- 日志为各 build-* 下 src-layout-build.log、ESP src-layout-merge.log、润和 src-layout-configure.log；主机测试为 build-hoperun-win/tests/src-layout-build.log 和 Testing/Temporary/LastTest.log。仅本地保留。
- 未烧写或运行模拟器。首次配网偶发断连、Error -7、Thinking 停滞仍待定位；本轮不是功能回归验收。
- 逐文件修改见 [src 迁移清单](changes-src-layout-2026-10-15.md)。

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654048 | `4fe77fd6b2e9cc0a9cca5d24122de10ae8a52dc531cd653c651803ab91204244` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1660704 | `45c9179a7adb3f430183ffb9ac7a0b8d7319b74eb8892facf37c1abc7f5e6a48` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235288 | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3655658 | `ab5edea88183a5a78456afd3c2a86934cfe14780f91577ca78d05b9cf8c70875` |


## shared/entry 目录迁移验证（2026-10-15）

- 公共 application/services/ui 集中到 shared，两个平台 app 入口改为 entry。迁移的源码、头文件及网页逐文件 SHA-256 比较一致；只更新目录、构建/测试引用、架构检查和文档，不改变功能、板型参数或产物名称。
- 利尔达、实战派、润和 WS63、Windows 模拟器均使用本机既有工具链及构建目录重新配置/编译成功；ESP 合并镜像生成成功。不是干净 Windows 依赖安装验证。
- 12/12 主机测试通过，公共层与媒体依赖架构检查通过。GoldieOS 外部 BSP 既有编译警告仍存在，不声称零警告。
- 构建日志为各 build-* 下 shared-layout-build.log；ESP 合并日志为 shared-layout-merge.log；主机测试证据为 build-hoperun-win/tests/shared-layout-build.log 与 Testing/Temporary/LastTest.log。日志和辅助脚本不上库。
- 本轮未烧写或启动模拟器验收。首次配网偶发断连、偶发 Error -7、重连后 Thinking 停滞仍未解决；历史实机验收不自动覆盖本轮新镜像。
- 逐文件变更见 [目录迁移记录](changes-shared-layout-2026-10-15.md)。

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654016 | `5b4b29437f4869787dd2525b146a514df8b0d9eb10fab6e808f6f337bb5da2c7` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1660672 | `c7eaf0a53f44e132a6009ce029ace51a8871b113c65fe91bb655558044bc0024` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235288 | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3655658 | `674d9d86d9567901887c1a5d226ed236ed38284f57e0ef2aed096fc644247b03` |


## 最新用户反馈与架构梳理（2026-10-15）

- 最新润和媒体契约镜像烧写后，用户反馈点击 Start 偶发 Error -7，重连后可能停在 Thinking。两项均未修复，按用户要求遗留后续定位，不将本版标记为完整验收通过。
- 本地采集日志 build-hoperun-app/start-thinking-capture.log 出现发送队列满、停止等待超时，随后也有线程退出记录。尚未确认这些日志与每次界面错误的完整因果关系；不能据此认定 TLS、鉴权或死锁为根因。日志仅本地保留，不上库。
- 首次手机配网偶发断连继续作为未解决问题；历史润和显示、对话、重复启停及对话中进入配网通过的记录仍只适用于对应镜像。
- 本轮仅整理 [整体架构与业务流程](architecture-walkthrough.md)、四张图和 14 页 PPT 提纲，同步文档入口与上库清单；未修改运行代码，未重新构建、烧写或制作 PPT。既有构建/测试证据与本轮文档检查分开记录。

## 精简与媒体契约调整版润和烧写（2026-10-15）

- 使用 COM4、115200 baud 烧写最新镜像；烧写前核对 SHA-256 为 c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2，与媒体契约构建记录一致。包含第三批精简、第四批邮箱回退及媒体契约归属调整。
- 全部分区下载成功，工具报告 Burned successfully，退出码 0。日志为 build-hoperun-app/media-contract-flash.log，仅本地保留；工具已退出。
- 显示、配网、对话、重复启停、对话中进入配网待用户验证。首次配网偶发断连仍为未解决已知问题，不能以烧写成功推断修复。本轮未修改应用源码或重新构建。

## 媒体契约归属整改（2026-10-15）

- 将现有 ai_media_ops_t 和 ai_uplink_fn 原样迁移到 src/application/services/media/include/ai_media.h；媒体模块不再引入云生命周期头，ESP 构建依赖同步调整。无业务 .c 实现、线程、存储或网络行为修改。
- 12/12 主机测试通过；架构检查新增媒体直接引入云/SDK 头的限制并通过。四目标构建通过，两款 ESP 合并镜像生成通过。外部 BSP 既有警告保留，不声称零警告构建。
- 四目标日志为各 build-* 下 media-contract-build.log，ESP 合并日志为 media-contract-merge.log；主机测试证据为 build-hoperun-win/tests/media-contract-build.log 和 Testing/Temporary/LastTest.log。文档链接、清单覆盖/行数及差异空白检查通过。
- 本轮没有烧写或启动模拟器验收；单板仍为此前烧写的第四批镜像。首次配网偶发断连未解决，单板 LVGL 配网暂不实施。此前第三批精简及第四批回退继续保留。

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `998c67fda0e49622c40141d8bd4ce20e5b31df3aa71b873db60dcbe1093ef385` |
| `build-szpi/unified_ai_szpi_merged.bin` | `0a1a7e26432a49ea61a97c6f8e6cfc6cbc3afbf73c8cae5724b396bcf223521e` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | `f63345533f7f491689ed1a3a4e30cae4529069cce3d611199cb9e95734937baa` |

逐文件范围见 [媒体契约整改记录](changes-media-contract-2026-10-15.md)。


## 第三批精简验证（2026-10-15）

- 按用户要求移除累计音频统计、播放队列统计和未使用的清空接口；网络状态查询改为三个后端文件内的 static 函数，公共头不再暴露查询类型。ESP 相关构建依赖收为私有。
- 保留公共网络列表操作、保存失败反馈、ESP 双快照及润和 NV 实现、原有低频音频日志、连续错误处理和采集长度检查。第四批公共邮箱保持已回退。首次配网偶发断连仍为未解决已知问题。
- 利尔达、实战派、润和固件及 Windows 模拟器均构建成功，ESP 合并镜像生成成功；12/12 主机测试通过。音频测试只删除统计断言，网络测试改为逐个删除到空；存储测试保持不变。
- 架构依赖、删除接口残留、文档链接、清单覆盖及文本行数、Git 差异空白检查通过。GoldieOS 外部 BSP 既有警告保留。日志为各 build-* 下 simplify-build.log、ESP simplify-merge.log；测试证据为 build-hoperun-win/tests/simplify-build.log 和 Testing/Temporary/LastTest.log。
- 本轮未烧写或启动模拟器验收；单板仍为此前烧写的第四批镜像。精简后的显示、配网、对话及启停需运行验证，不能继承历史镜像验收。单板 LVGL 配网和资源优化仅讨论方案，本轮未实现。

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `4d8557f1e0929be01301f004325feb74e003715970bfeadcbae8d3a7decd6f57` |
| `build-szpi/unified_ai_szpi_merged.bin` | `211cbec86922e9bbfa558ab949b2e2613e567f2a19dfb2e6cc627b47f91d221c` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `c294492fdc6e5ad569f3d398b24fcfe96910804da00e937b5c5976374347a7a2` |
| `build-hoperun-win/unified_ai_simulator.exe` | `6c7745ad76f7c1555230948a7df9b4e18697059811bc026d67a3d50c4a918fe5` |

本轮逐文件说明见 [第三批精简记录](changes-simplify-2026-10-15.md)。


## 第四批整体代码回退（2026-10-15）

按用户要求撤回公共邮箱源码、两个平台接入、三个构建注册及新增邮箱测试，恢复第三批队列实现。保留第三批网络/存储/音频代码、润和四项验收、首次配网偶发断连已知问题，以及第四批历史构建/烧写记录。

回退后四目标构建成功，ESP 两款合并镜像生成成功；12/12 主机测试通过，公共依赖、源码无邮箱残留、差异空白、文档链接及上库清单检查通过。日志为各 build-* 下 rollback-build.log、ESP rollback-merge.log，测试目录同名构建日志及 Testing/Temporary/LastTest.log。没有烧写；单板仍是之前烧写的第四批版本。

本轮没有实施第三批进一步删减。ESP 原 overflow 标志与队列清空的同步方式一并恢复，不将本次回退包装为新的并发修复。源码回退后重新生成的镜像以以下哈希为准，不能仅凭相同功能就假定二进制与历史镜像完全一致。

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `5898e9be62bf9346119b34859fdfb2aa57b4993a43dcab99c96f818ae7f602be` |
| `build-szpi/unified_ai_szpi_merged.bin` | `5f0fc7ebbe8ff3d36d5e026b83b3ccf6d577bb26d741e650e0c30acbb93bd08b` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `ae10adcb0a249204d404f744751c07f091487ed7cc1672708bedfe6840ae3ee0` |
| `build-hoperun-win/unified_ai_simulator.exe` | `769e08355590aece0a1bf22f243c434d6857ddbbd71b16732975b0cf42963c8b` |


## 第四批润和镜像烧写（2026-10-15）

- 经 COM4、115200 baud 烧写第四批镜像，烧写前核对 SHA-256 为 f68cca9b06209c2a6b83b977fdc3d3d40109308c0b21086ff96efde2b7316676，与构建记录一致。
- 全部分区下载成功，工具报告 Burned successfully，退出码 0。日志为 build-hoperun-app/mailbox-flash.log，仅本地保留；工具已退出。
- 当前镜像包含公共 ai_mailbox 实现；用户在烧写期间提出是否移除该抽象，本轮仅讨论取舍，未修改源码或替换烧写包。显示、对话、重复启停和对话中进入配网仍待用户验收，首次配网偶发断连仍为已知问题。

## 第四批公共应用邮箱整改（2026-10-15）

- 两平台统一使用 src/application/core/ai_mailbox.c：容量 32、值拷贝、FIFO、溢出锁存与整批清空。ESP 在同一临界区内管理消息和 overflow，以二值信号量唤醒；GoldieOS 保留互斥锁及 20 ms 轮询。controller、网络、媒体及界面业务未修改。
- 13/13 主机测试通过，新增 application_mailbox 覆盖环回、容量边界、命令/事件混排、拷贝隔离、溢出丢弃、恢复及无效输出不分发。既有 application_coordination 继续验证溢出后会话停止等协调语义。主机测试不模拟实际 RTOS 调度和 ISR。
- 利尔达、实战派、润和 WS63 和 Windows 模拟器均构建成功；两款 ESP 合并镜像生成成功，退出码均为 0。四目标日志分别为 build-*/mailbox-build.log，ESP 合并日志为 mailbox-merge.log；测试日志为 build-hoperun-win/tests/mailbox-build.log 和 Testing/Temporary/LastTest.log。
- 公共依赖边界、Git 差异空白、文档链接和上库清单覆盖/文本行数检查通过。GoldieOS 外部 BSP 头文件既有警告保留。本轮没有烧写或运行新模拟器验收，尚不能用第三批反馈证明第四批固件已通过实机验证。
- 第三批回退包保存于 build-hoperun-app/accepted-network-media/unified_ai_hoperun.fwpkg，SHA-256 ae10adcb0a249204d404f744751c07f091487ed7cc1672708bedfe6840ae3ee0，已核对；该版本有已知首次配网偶发断连问题，不是全功能无问题版本。
- 第四批待回归：显示、对话、重复启停、对话中进入配网、命令突发/队列溢出后的停止与恢复。首次配网偶发断连继续搁置，未宣称修复；串口协议、硬件自动化及 test/release 配置仍未实现。

最终产物（本地生成，不上库）：

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654432 | `7c2dd81ff7282e29d87cce14384f591a34a81f2baba82917631172b1b7dc0f8a` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1661104 | `d432dcc3055c86698efc13ab35851051465277f050aa9066a3a87cfb49e1aac6` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235448 | `f68cca9b06209c2a6b83b977fdc3d3d40109308c0b21086ff96efde2b7316676` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3656505 | `61db3313ab8d2ccc2f4a8d4ff2b0b46a0f126f6793d029e21f5120e1fbfa4e6e` |

逐文件修改见 [第四批变更清单](changes-batch4-2026-10-15.md)。

## 第三批润和用户验收与已知问题（2026-10-15）

- 用户确认第三批镜像显示、对话、重复启停和对话中进入配网正常。对应镜像 SHA-256 为 ae10adcb0a249204d404f744751c07f091487ed7cc1672708bedfe6840ae3ee0。
- 已知问题：手机首次连接润和热点并提交目标网络密码后，可能在收到成功提示前与热点断开；用户反馈润和屏幕没有变化，重试通常不复现。用户要求暂时搁置，保留为未解决问题；不将配网全流程标记为无问题验收。
- 排查时电脑未枚举到串口，未取得本次故障日志。代码检查显示提交密码不会直接关闭热点，但仅凭此不能确认无线切换、手机行为或 BSP 的具体根因。未因本问题修改配网代码。

## 第三批润和镜像烧写（2026-10-15）

- 使用 COM4、115200 baud 烧写第三批镜像 build-hoperun-app/out/unified_ai_hoperun.fwpkg；烧写前核对 SHA-256 为 ae10adcb0a249204d404f744751c07f091487ed7cc1672708bedfe6840ae3ee0，与下方构建记录一致。
- 设备握手成功，全部分区下载完成，工具报告 Burned successfully，退出码 0。日志为 build-hoperun-app/network-media-flash.log，仅本地保留。工具已退出。
- 用户将验证显示、配网、对话、重复启停和对话中进入配网；当前仅确认烧写成功，尚未收到本版功能验收反馈。未改动应用源码或重新构建镜像。


## 第三批网络与媒体整改（2026-10-15）

- 完成公共网络状态/提示、保存网络列表操作与媒体统计接入；保留现有 DHCP 网关、TLS 时间、无 SD 卡 NV、播放队列与会话代际修复。详细边界见 [架构文档](architecture.md)，文件范围见 [变更清单](changes-2026-10-15.md)。
- 利尔达、实战派、润和 WS63 和 Windows 模拟器构建均退出 0。两款 ESP 在 HTTP 任务栈调整为 8 KB 后再次构建并完成合并镜像生成，最终命令均退出 0。保留原板型参数和产物名称。
- 12/12 主机测试通过：原有协调层及协议/音频测试保留，新增 network_contract、esp_nvs_storage；覆盖排序/容量、保存失败不发布内存、旧键迁移、CRC 回退、空快照不恢复旧账号、外部键所有权及回读失败。NVS 为主机替身，不能代替真实掉电和存储介质验证。
- 架构检查通过；上库清单同步新增文件并重算文本行数；差异空白及本轮文档链接检查通过。未修改鉴权配置、外部 BSP 或 SDK 库。删除四个工作区根目录一次性 batch3_*.py 编辑脚本。
- GoldieOS 外部 BSP 头文件仍有既有 storage class / strict-prototypes 警告，未影响本轮编译、链接和打包；未将其描述为零警告构建。
- 本轮没有烧写、没有运行模拟器验收。上一次润和协调层版的用户反馈不覆盖当前产物。待验证：三板显示、自动门户与配网、保存/删除/默认/重启恢复、AI 对话、重复启停及对话中进入配网；ESP 特别验证旧账号升级、存储异常及 HTTP 栈余量；模拟器验证界面、麦克风和实际对话。
- 本轮不包含自动化串口协议、日志收集、设备发现、test/release 配置、FBB 迁移或树莓派后端。

构建与测试证据（本地保留，不上库）：

| 目标 | 日志 |
|---|---|
| 利尔达 | build-lrd/network-media-final-build.log、network-media-merge.log |
| 实战派 | build-szpi/network-media-final-build.log、network-media-merge.log |
| 润和 WS63 | build-hoperun-app/network-media-build.log |
| Windows 模拟器 | build-hoperun-win/network-media-build.log |
| 主机测试 | build-hoperun-win/tests/network-media-build.log、Testing/Temporary/LastTest.log |

最终产物（均含本地配置，不作为源码上库）：

| 产物 | 字节数 | SHA-256 |
|---|---:|---|
| `build-lrd/unified_ai_lrd_merged.bin` | 1654160 | `5898e9be62bf9346119b34859fdfb2aa57b4993a43dcab99c96f818ae7f602be` |
| `build-szpi/unified_ai_szpi_merged.bin` | 1660816 | `5f0fc7ebbe8ff3d36d5e026b83b3ccf6d577bb26d741e650e0c30acbb93bd08b` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | 2235400 | `ae10adcb0a249204d404f744751c07f091487ed7cc1672708bedfe6840ae3ee0` |
| `build-hoperun-win/unified_ai_simulator.exe` | 3656295 | `4f0dca020df629efca8c6d7ecef8ad77a66f815726d20f0f987599ee63754e84` |

## 公共应用协调层版：补记用户验收（2026-10-15）

此前协调层镜像已通过 COM4 烧写成功，日志为 build-hoperun-app/controller-flash.log。用户明确确认“润和显示、配网和对话功能验证正常，重复启停和对话中进入配网也正常”。因此该版本上述场景已完成润和实机验收；其他两板按用户要求暂不进行该轮实测，Windows 未做该轮新运行验收。

对应烧写包 SHA-256 为 bd954b82bef48b7fb4922d153645723cd7a817e5192e68e5993d15ea9a2ba42e。本轮已核对 build-hoperun-app/accepted-controller/unified_ai_hoperun.fwpkg 回退副本哈希一致。下方“第二批”章节中的“本轮未烧写”为该次编译结束时的历史状态，本补记补充其后烧写与用户验收事实，不追改历史构建结果。

## 公共应用协调层版润和烧写（2026-10-14）

- 经 COM4 烧写最新 build-hoperun-app/out/unified_ai_hoperun.fwpkg，SHA-256 为 bd954b82bef48b7fb4922d153645723cd7a817e5192e68e5993d15ea9a2ba42e，与公共协调层构建记录一致。
- 厂商工具报告全部镜像下载成功、Burned successfully，退出码 0。日志为 build-hoperun-app/controller-flash.log，不上库；烧写工具已结束并释放串口。
- 显示、Wi-Fi 配网及实际语音对话等待用户验收；本条仅确认烧写成功，不代表功能验收通过。


## 公共应用协调层整改验证（2026-10-14）

- 在 app 中新增平台无关协调层，ESP-IDF 与 GoldieOS 共用命令入口、时间检查、启动前配置重试、配网停止顺序、网络事件、超时和提示刷新。
- GoldieOS 改为命令与回调同一 FIFO；队列溢出后统一清空并报 -9。原有 TLS/时间、音频、NV、DHCP 网关修复保留，Windows 模拟器保持无配网功能。
- 三款单板与 Windows 模拟器均编译通过，ESP 合并镜像已更新；日志为各 build-* 目录中的 controller-build.log、ESP controller-merge.log。
- 10/10 主机测试通过，包括新增 application_coordination：离线/未校时启动、配置失败后重试、重复 Start/Stop、先停止后配网、旧回调过滤、活动/空闲溢出、超时计数回绕、启动失败清理、断网恢复不自动启动及不支持配网的目标。
- 架构依赖、文档链接、清单覆盖与差异空白检查通过。本轮未烧写任何单板，未进行模拟器运行验收；用户此前确认的润和目录调整版通过，不覆盖本轮新代码。其他单板按用户要求暂不实测。
- 已保留润和通过验收的上一版烧写包于 build-hoperun-app/accepted-layout/unified_ai_hoperun.fwpkg（SHA-256 7b69fbbf0844aa750ed5adf7a889733b2d0cf1bce3dc7a659bc827b5a888ada5），用于回退对比，不上库。

本轮新产物 SHA-256：

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `34d7807c30f999249475292263995f36883b11a7b0ccfa4551416b1331125982` |
| `build-szpi/unified_ai_szpi_merged.bin` | `36212ba92ec79db233867b5f16134a9ca1626a63c34b972f95734470059ee852` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `bd954b82bef48b7fb4922d153645723cd7a817e5192e68e5993d15ea9a2ba42e` |
| `build-hoperun-win/unified_ai_simulator.exe` | `cb530cb9e7c697b16f2da76096e18f3a643018916334701b65f3aa515b8432cf` |


## 润和目录调整版烧写（2026-10-14）

- 通过 CH340K COM4 烧写 build-hoperun-app/out/unified_ai_hoperun.fwpkg，SHA-256 为 7b69fbbf0844aa750ed5adf7a889733b2d0cf1bce3dc7a659bc827b5a888ada5，与上述目录调整构建产物一致。
- 厂商工具报告各镜像下载成功、Burned successfully，退出码 0；记录位于 build-hoperun-app/layout-flash.log。
- 烧写后串口采集收到系统内存统计和 AW9523B 电源任务日志，证明设备已进入运行阶段；当前采集片段未发现初始化失败或 panic/assert。未捕获完整启动过程，不能据此认定全部功能通过。
- 用户确认润和目录调整版验证 OK，显示、手机配网和实际语音对话通过。利尔达、实战派暂不进行本轮实测。原始串口日志位于 build-hoperun-app/layout-boot.log，仅本地保留，不上库。


## 公共应用及硬件目录调整验证（2026-10-14）

- core 更名为 app；boards 按 esp32/ws63 分组；公共状态机源文件内容与业务接口未改动。保留 ai_core 构建组件名、测试名及全部板型参数。
- ESP 专属配置迁入 src/platforms/esp_idf/targets/esp32s3；根入口轻量分发，默认仍为 ESP。现有三个单板构建目录和 Windows 模拟器构建目录均重新编译成功，ESP 合并镜像已更新。
- 9/9 现有主机测试通过；架构检查扫描 src/application/core、src/application/services、src/application/ui 并检查目录存在，避免改名后漏检。源码路径、文档链接及逐文件上库清单同步更新。
- 根入口分发到 GoldieOS WS63/Windows 的独立配置检查通过，日志分别为 build-hoperun-app/layout-root-configure.log 和 build-hoperun-win/layout-root-configure.log。新的独立 ESP 入口配置日志为 build-lrd/layout-entry-configure.log。
- 新的独立 ESP 入口配置通过；本机缺少 IDF Python constraints 文件，入口检查沿用已验证构建目录的 PYTHON_DEPS_CHECKED=1 设置，不将其写入项目。正常安装的工具环境仍执行默认依赖检查。
- 完整构建日志为四个 build-* 目录下 layout-build.log，ESP 合并日志为 layout-merge.log；主机测试位于 build-hoperun-win/tests。构建产物、缓存和日志不上库。
- 本轮未烧写、未启动模拟器进行功能回归，仍需验证最新产物的显示、配网及对话；不以历史人工验收替代本轮运行验收。

本轮产物 SHA-256：

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `ba90cba719251cb7ebaa40cfca65c66426137c2af97d867c985755610da7cf12` |
| `build-szpi/unified_ai_szpi_merged.bin` | `0232c5cd6b35e62419354d9b57550688bc626768e4fcf1f37f909f9c8fbb6d51` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `7b69fbbf0844aa750ed5adf7a889733b2d0cf1bce3dc7a659bc827b5a888ada5` |
| `build-hoperun-win/unified_ai_simulator.exe` | `bbcc66a301bf19e154c16d86430d1d35e82000d4debaccad51937537d55c40d5` |


## 第一批服务接口与依赖优化（2026-10-14）

- 拆分 esp_services.h/ws_services.h；ESP 媒体、网络、配置与 SDK 单独构建，SDK 内部头不向配置模块公开；GoldieOS 应用接口限制到直接使用的源文件。
- 音频/Wi-Fi 服务查找从润和 boards 移入平台模块，基础驱动等待移入 runtime。保留启动次序、可选音频配置、播放队列、TLS 时间、无 SD 卡 NV 和 DHCP 网关修复。
- ESP 显示句柄头归入 ESP32-S3 专属板级目录；通用板级契约不包含平台服务类型。架构检查覆盖公共依赖、板级服务归属和 SDK 内部头边界。
- 利尔达、实战派、润和 WS63 固件及 Windows 模拟器编译通过；ESP 两款合并镜像同步生成。9/9 现有主机测试通过，涵盖状态机、云生命周期、音频、门户 DNS、DHCP 网关、配网校验、NV 存储、云配置及网页契约。
- 首次构建遇到受限环境无法启动 ESP 编译器、WS63 签名程序的问题，切换允许的构建环境后成功；GoldieOS 源码级头文件路径使用正斜杠编译选项，适配厂商 GCC 响应文件解析。
- 日志：三款固件 build-*/batch1-build-final.log；模拟器 build-hoperun-win/batch1-build-retry.log；ESP 合并日志 batch1-merge.log；测试 build-hoperun-win/tests/batch1-build.log。所有产物与日志留在忽略的构建目录，不上库。
- 本轮未烧写、未启动模拟器做新一轮功能验收。此前用户确认不能替代新产物实机回归；未实现新的测试协议或 test/release 配置，未迁移 FBB SDK。


本次产物 SHA-256（构建产物不上库）：

| 产物 | SHA-256 |
|---|---|
| `build-lrd/unified_ai_lrd_merged.bin` | `a6f447badf2bea1e8b7bff19332d4f36e1d1de718baf2b7f0c981a4051630e7c` |
| `build-szpi/unified_ai_szpi_merged.bin` | `07685b756a329a8179078cae242a00c7fa3cb21e979fcc24cd1d59bdcb0a97c8` |
| `build-hoperun-app/out/unified_ai_hoperun.fwpkg` | `7b69fbbf0844aa750ed5adf7a889733b2d0cf1bce3dc7a659bc827b5a888ada5` |
| `build-hoperun-win/unified_ai_simulator.exe` | `fab5bfb7304424a770673f95aa8fdb2fe5c8608e1e1d6e010ba4178a266bf37d` |

## 平台组件整改验证（2026-10-13）

- 原 src/platforms/ws63 迁入 src/platforms/goldieos，按 media、network、config、ui、sdk、runtime 六个组件组织；network/sdk/runtime 分别选择 WS63 或 Windows 后端。GoldieOS 仅代表当前润和软件接口，不代表其他 WS63 伙伴。
- ESP 实现归入 src/application/core/components，IDF 注册位于 cmake/components；原公共 UI 目录中的 ESP 适配和仅 ESP 共用板级代码分别归位。
- 三款单板固件及 Windows 模拟器构建成功；本轮未烧写或启动验收，旧实机反馈不能替代本轮回归。
- 9/9 自动化测试通过，公共业务与 UI 的依赖边界检查通过。迁移源码逐文件核对：除 Windows runtime 拆分和一处注释路径更新外，迁移的 C/C++/头文件内容保持一致；Windows 网络函数体保持一致。
- WS63 首次构建遇到厂商批处理启动器命令行过长，改为 Ninja 响应文件后构建成功；未修改厂商 BSP。
- 最终构建日志：build-hoperun-app/components-build-retry.log、其他三个 build-* 下 components-build.log；测试位于 build-hoperun-win/tests。构建产物与日志不入库。
- 本轮没有实现阶段 0 测试协议、能力声明或 test/release 配置。CMake Release 优化配置不等于设计文档中完整的 release 交付方案。

## 目录整理验证（2026-10-12）

- ESP main 与组件目录迁至 src/platforms/esp_idf，根工程通过 EXTRA_COMPONENT_DIRS 注册；组件名称保持不变。
- WS63 单板/模拟器共用的 app、ui、media、config 及相关头文件移至 src/platforms/goldieos；firmware 保留实机专用实现。
- 根目录说明文档集中至 docs，构建引用、文档链接及上库清单已同步更新。
- 利尔达、实战派、Hoperun 固件与 Windows 模拟器全部重新构建成功；日志在各 build-* 目录的 structure-build.log，不上库。
- 9/9 自动化测试通过；测试构建位于 build-hoperun-win/tests。公共层架构检查通过。
- 本轮未烧写或进行新的界面/对话实测；下方 2026-10-10 为迁移前版本的用户验收，不能替代本轮实机回归。
- Windows 构建仍有外部 GoldieOS input_service.h 的既有编译警告，不影响链接。

## 当前验收结论（2026-10-10）

以下为用户对最新清理、分层版本的明确反馈，优先于后文历史排查中的“待验证”状态。

| 目标 | 用户确认结果 | 构建/部署证据 |
|---|---|---|
| Hoperun WS63 单板 | 显示、配网和对话正常 | 编译、FWPKG 烧写成功；自动门户网关修复此前已验证 |
| 利尔达 ESP32-S3 | 显示、配网和对话正常 | 编译、COM6 烧写及分区哈希校验成功 |
| 实战派 ESP32-S3 | 显示、配网和对话正常 | 编译、COM8 烧写及分区哈希校验成功 |
| Hoperun Windows 模拟器 | 界面和对话正常 | Windows EXE 编译、启动成功，用户完成运行验证 |

本轮四个目标的基础功能验收完成。模拟器不提供 Wi-Fi 配网，不能替代实体板硬件验证。9 项自动化测试此前通过；长时间压力、异常断电、跨手机兼容性和其他厂商 BSP 不属于本次反馈所确认的范围。

已清理临时诊断代码，保留共享门户、WS63 DHCP 网关补全、TLS 时间、NV 配置存储和播放同步修复。尚未执行 Git 暂存、提交或推送。

## 历史排查与逐次验证

以下按工作过程保留；旧日志/构建目录部分已按用户要求删除，文中的旧路径和“待验证”仅代表当时状态。

## 2026-10-10 同手机利尔达可弹窗、润和不可弹窗

域名诊断版反馈仍不弹窗。日志中 DNS 查询域名为 szlong.weixin.qq.com，不能把这些业务查询误认为系统联网探测。电脑临时连接热点实测得到 192.168.4.3，DNS 和默认网关均为 192.168.4.1，HTTP /generate_204 返回 302、/ 返回 200；电脑已恢复 Huawei-Guest。电脑 UDP DNS 探测超时且单板未记录对应查询，电脑存在 singbox_tun，不能直接以此判定单板 DNS 应答丢失。

发现并修正启动竞态：原 WS63 DHCP 先于 DNS 监听就绪，手机拿到租约后首轮探测可能失败。现先准备 HTTP，等待 DNS 非阻塞监听就绪后才启动 DHCP，退出时等待 DNS 线程释放端口。修复版编译通过，是否为当前手机问题根因仍待实测，不宣称已解决。

启动顺序修复版已烧写成功，首次握手超时后重试成功，全部下载项成功，Burned successfully，退出码 0。成功日志：build-verification/ws63-portal-startup-flash-retry.log。已开启 ws63-portal-startup-capture.log 采集，待手机复测。

用户复测启动顺序版仍不弹窗，故不能把启动竞态认定为当前根因。日志仍只有微信域名 DNS 查询，未见系统 HTTP 探测。电脑第二次测试明确指定 Wi-Fi 出接口后 DNS 仍超时、单板未记录电脑 DNS 查询；HTTP 302/200 正常，电脑已恢复 Huawei-Guest。代理隧道仍可能拦截 DNS，需要无代理干扰的测试才能解释这一结果，暂停进一步猜测性固件修改。

域名访问结果更正：用户首次输入有误，随后确认 http://example.com/generate_204 可打开配网页面。ws63-portal-confirmed-hotspot.log 记录 HTTP GET /generate_204、HTTP 302 sent=1 及后续根页面请求，证明手动域名解析与 302 链路可用；系统自动探测仍未成功。新增前 20 次 DNS 查询的域名日志，并把 A 应答 TTL 从 0 调为与利尔达一致的 28 秒，以缩小实现差异；此为诊断/兼容性验证改动，不等于已确认根因。新诊断镜像编译通过。

域名诊断镜像已烧写：首次握手超时，重试成功，所有下载项成功，Burned successfully，退出码 0。日志：build-verification/ws63-portal-domain-flash-retry.log。已开启 ws63-portal-domain-capture.log 采集，等待新一轮自动探测验证。

进一步实测：确认手机连接润和 xxx1758，等待 20 秒显示“不可上网”，利尔达则显示“需要验证”。润和日志记录多次 DNS A/AAAA 查询应答，自动探测阶段未记录到有效 HTTP 请求。用户手动访问 192.168.4.1 可打开页面，日志确认 HTTP GET /、/saved/list、/scan。因此页面服务可用；尚需验证按域名访问时 DNS 应答是否被手机接受以及 302 跳转链路，不能仅凭 sendto 成功判定解析有效。

实板反馈更正：用户随后确认刚才连的是利尔达热点，润和仍不能自动弹窗。撤回润和自动弹窗验证通过的结论，也不能据此归因于旧网络缓存。本次润和采集仅记录到 DNS A 查询成功应答（40/40 字节），未捕获完整 HTTP 302 链路。后续须核对手机连接的热点名称与润和屏幕一致，再复现排查；润和自动弹窗问题仍未解决。

诊断镜像已通过 COM4（CH340K）、115200 烧写成功：所有下载项成功，Burned successfully，退出码 0。日志：build-verification/ws63-portal-diagnostic-flash.log。串口采集已开启，待手机重新连接热点复现；采集文件为 build-verification/ws63-portal-diagnostic-capture.log。

用户确认同手机连接利尔达可以自动弹出配网页面，润和不行。此前 DNS/302 改进不能视为已解决问题，排查优先比较平台实现。WS63 增加 DNS 配置返回值检查及 AP HTTP 就绪、DNS bind/查询应答、HTTP 请求路径和 302 发送结果诊断，不记录 POST 密码、请求头和查询参数。日志用于区分服务启动失败、DNS 未到达和 HTTP 探测未到达；实际根因仍待润和连接后采集确认。

## 2026-10-10 最新利尔达镜像构建与烧写

基于最新代码使用 ESP-IDF 6.1 构建 build-lrd（UNIFIED_BOARD=lrd）成功，应用大小 0x173920，应用分区剩余 58%。COM6 识别为 USB-SERIAL CH340；通过 idf.py flash 烧写成功，数据 Hash 校验通过，退出码 0，并经 RTS 自动复位。沿用 build-lrd 中已有鉴权配置，未输出凭据。日志：build-verification/lrd-ui-regression.log 与 lrd-ui-regression-flash.log。本次包含共享配网页面改进；WS63 专用 DNS/结果查询后端不适用于 ESP 平台。利尔达本轮配网和对话功能待用户实板验证。

## 2026-10-10 配网自动弹窗与提交响应丢失改进

本轮镜像已通过 COM4、115200 烧写成功：所有下载项成功，Burned successfully，退出码 0。烧写日志：build-verification/ws63-portal-flash.log。用户将验证手机自动弹出配网页面及连接网络功能，结果待反馈；本次包含 NV 分区，可能需要重新配网。

WS63 配网期间将系统 DNS 指向 192.168.4.1（底层 dhcp_option_dns_server 从 dns_getserver 生成 DHCP option 6，已核对固件反汇编），退出恢复原 DNS，STA DHCP 随后刷新正常 DNS。手机 HTTP 探测和未知 GET 路径返回 302 到配网根页面；根页面与配网 API 保持直接响应。DNS 接收失败时主动休眠，避免错误忙循环。

共享网页为提交生成 request_id；WS63 保存最近一次提交结果，提供 /connection/status?id= 查询。网络传输错误时只查询同次结果，不自动重复提交密码；已确认保存成功后的列表刷新失败不再被误报为连接失败。ESP 原同步提交仍兼容，若传输失败且不支持结果查询，则明确提示结果未知。此改进不保证消除 APSTA 信道切换本身的短暂断连。

WS63 镜像编译通过，7 项测试通过，网页测试新增响应丢失恢复、不重复提交、保存失败传播及列表刷新失败场景。尚未烧写本轮镜像，手机自动弹窗、断连恢复和退出配网后的 DNS/AI 对话需实板验证。建议手机先忽略旧热点再重连，以获取新的 DHCP DNS 设置；自动弹窗仍受手机系统探测策略影响。

## 2026-10-10 TLS 校验时间修复

实板验证结果：用户在烧写 TLS 时间修复镜像并重启测试后反馈“功能已正常”。串口 ws63-tls-clock-capture.log 同时确认 TLS handshake complete 和 WebSocket handshake complete。本轮 Error -7 与 Start 操作阻塞已按用户反馈恢复；尚未单独验证长时间运行及多次冷启动稳定性。Wi-Fi 按钮和自动配网继续保留。

修复镜像已烧写成功：首次等待握手超时（退出码 4），重试所有下载项成功，Burned successfully，退出码 0。成功日志：build-verification/ws63-tls-clock-flash-retry.log。已重新开启串口采集，实际 TLS 握手及基础对话待用户重启后验证。

用户反馈 Ready 时 Start/Wi-Fi 均无按下反馈，暂未确定触摸原因。重新采集 ws63-ready-capture.log 后捕获到实际 Start：TLS 返回 -0x2700，verify flags=0x00000200，明确为证书尚未生效；libc time 异常且与 SDK 的 NTP 时间不一致。WS63 TLS 与平台适配统一启用 MBEDTLS_PLATFORM_TIME_ALT，并在 SDK 初始化前注册 NTP-backed 时间回调，保持 VERIFY_REQUIRED。修复镜像编译通过，现有 7 项回归测试通过（不覆盖硬件 TLS 握手）。烧写及握手结果待实板确认。用户说明当前板无电源拨动开关，不能以开关检查作为本板触摸故障结论。

## 2026-10-10 Error -7 定位到 TLS 证书校验

诊断镜像已经 COM4、115200 烧写成功，所有下载项成功，Burned successfully，退出码 0。烧写日志：build-verification/ws63-tls-diagnostic-flash.log。已启动新一轮串口采集，等待重启联网后复现；诊断结果以下续记为准。Wi-Fi 按钮及自动配网保留。

用户在最新镜像再次复现 Error -7。COM4 日志 ws63-start-network-capture.log 显示 DNS 解析成功、TCP 连接发起成功，TLS handshake 返回 -0x2700（证书校验失败）。SDK 的 -7 同时代表网络错误，此次不能归因为配置 JSON。新增失败时输出 verify flags、校验原因及 libc/SDK 时间对比，诊断镜像编译通过；尚未烧写，具体证书链或时间原因待确认。保持 VERIFY_REQUIRED，不绕过证书校验。

按用户最新决定，保留启动无保存网络/离线超时自动配网，撤回本轮手动触发配网调整；保留 Wi-Fi 按钮作为已有连接时重新配网的入口。

## 2026-10-10 联网后 Error -7 排查

Agent 和产品鉴权均非空、已编入 ELF，未输出凭据。原 -7 混合多个错误分支，尚未确认实板根因。现区分 -7 缺少鉴权、-71 设备标识、-72 JSON 分配、-73 字段创建、-74 序列化失败；Start 重新生成配置，允许启动时临时失败后恢复。固定 codec 改为 raw JSON 数字 0，避开浮点格式化依赖，但未确认此为根因。WS63 与 Windows 编译通过，新增配置生成测试后 7 项测试通过。

本轮已通过 COM4、115200 烧写最新 unified_ai_ws63.fwpkg，所有下载项成功，最终 Burned successfully，退出码 0。日志：build-verification/ws63-unified-2026-10-10-cloud-config-flash.log。此次包含 NV 分区，可能需要重新配网；Start 后云端连接与实际对话仍待用户实板验证，尚不能确认 Error -7 已解决。


## 2026-10-10 板内 NV 与配网忙循环修复版烧写成功

通过 COM4、115200 烧写当前 unified_ai_ws63.fwpkg，所有下载项成功，最终 Burned successfully，退出码 0。日志：build-verification/ws63-unified-2026-10-10-nv-wifi-flash.log。本次已写入内部 NV 保存网络与非阻塞 accept/主动休眠修复。无卡保存、重启后自动重连、配网长时间运行及基础对话尚待实板验证；手机自动弹窗未确认。

## 2026-10-10 无 SD 卡配网存储修复

用户确认未插卡。Wi-Fi 保存已从 FATFS 迁移为内部 NV 双快照（0x5100/0x5101），写入后读回核对，拒绝覆盖其他格式及忽略读错误。WS63 镜像编译通过，含新增 NV 故障/损坏回退测试的 6 项测试全部通过。仍包含此前非阻塞 accept + 每轮休眠的配网忙循环修复。此版本尚未烧写：无卡保存/断电重连、重启是否消失、手机自动弹窗仍需实板验证。


## 2026-10-10 配网重启与保存失败排查

- 用户确认手机可手动打开 http://192.168.4.1，HTTP 页面可用；未自动弹窗不等同 HTTP 服务失败。自动弹窗仍需结合手机探测及 DHCP/DNS 验证。
- 串口日志 ws63-reboot-capture.log 两次记录 ai_wifi 任务 NMI，任务 CPU 占用约 96%；回溯含 network_task 与 lwip_selscan。配网 select 循环没有显式让出执行时间，现改为非阻塞 accept，每轮（包括成功请求和出错）休眠 20 ms；已接受连接显式恢复阻塞模式并保留收发超时。此修改尚未实板确认消除重启。
- 同一日志显示 mount failed 3、SD card not available。当前 FATFS 配置保存依赖 microSD；网页原通用错误不能区分连接失败与存储失败，现拆分参数校验、连接失败、连接成功但保存失败三种提示。未添加静默忽略保存失败的行为。
- WS63 修复镜像编译成功，公共 5 项测试通过；未烧写本轮镜像。首次无保存网络自动进入热点是三板共有行为。

## 2026-10-10 media(-16) 修复镜像烧写成功

用户确认单板已进入下载模式后，经 COM4、115200 烧写 2,228,872 字节的 unified_ai_ws63.fwpkg。所有下载项及主应用均成功，工具最终返回 Burned successfully，退出码 0。日志：build-verification/ws63-unified-2026-10-10-media-fix-download-mode.log。本次已写入允许省略 audio_output_config 回调的修复；启动、Wi-Fi 配网与实际对话仍待实板复测。

## 2026-10-10 WS63 media(-16) 初始化修复

用户在最新固件启动时观察到 media(-16)。对应检查为 AudioService.audio_output_config 缺失；对照原示例 convai_audio_downlink.c，该回调本来就是可选的。此前只对 Windows 放宽检查，遗漏 WS63。现已统一允许省略回调，播放时仍仅在回调存在时调用，采集/播放/缓冲等必需接口检查保留。

WS63 镜像与 Windows EXE 编译通过，公共 5 项测试通过（不等同实板音频验收）。修复镜像 2,228,872 字节。首次烧写尝试未能打开 COM4，退出码 3；该次未写入固件，待串口释放后重试。

## 2026-10-10 更换润和单板后最新镜像烧写成功

- 用户确认更换为同型号润和 WS63 单板。前期原 GoldieOS 桌面的观察来自更换后的板，不能据此否定此前另一块板的烧写结果。
- 本次经 COM4、115200 使用 BurnToolCmd 烧写最新 unified_ai_ws63.fwpkg（2,228,888 字节，包含取消业务层仅充电阻塞的修改）。
- 引导、参数、NV、主应用均报告下载成功，最终 Burned successfully，退出码 0。日志：build-verification/ws63-unified-2026-10-10-new-board-continue.log。
- 最新版本已写入；重新启动后的界面、Wi-Fi 触摸及配网、基础对话仍待用户实板验证。

## 2026-10-10 统一上电业务体验

移除 WS63 应用 owner 的开关检查、仅充电等待和关闭网络分支，删除公共 runtime 的电源状态/保持接口。板级 runtime 私有任务独立管理 AW9523B 电源保持，USB 维持供电时业务持续运行。WS63 固件与 Windows EXE 重新编译通过；此版本尚未烧写，USB 下 OFF 启动、开关切换期间配网/对话与纯电池关机仍需验证。


## 2026-10-10 润和 WS63 烧写成功

- 使用官方 BurnToolCmd，经 COM4（USB-SERIAL CH340K）、115200 波特率烧写 build-ws63-app/out/unified_ai_ws63.fwpkg，文件大小 2,229,016 字节。
- 工具等待握手时，用户关闭再打开单板电源，握手成功；引导、参数、NV 与主应用均报告下载成功。
- 最终输出 Burned successfully，退出码 0。日志：build-verification/ws63-unified-2026-10-10-flash.log（本地忽略目录）。
- 烧写成功不代表应用功能验收；本次未采集复位后的启动日志，显示/触摸、Wi-Fi 配网与基础对话待实板验证。

当前状态（2026-10-10）：用户已反馈利尔达最小配网和基础对话验证正常。WS63 独立固件编译打包通过，尚未收到实板验收结果。新增 Windows 模拟器编译链接通过；不包含 Wi-Fi 配网，使用电脑网络。模拟器窗口交互和实际语音对话仍待验证。详细历史过程见下文。

## 2026-10-10 润和固件与 Windows 模拟器双目标

### 模拟器 Listening 但无语音识别排查

用户反馈云端已进入 Listening，但说话无识别。对照原示例 convai_audio_uplink.c，发现 Windows 原实现将参考声道强制置零；统一版本错误沿用了 WS63 的右声道回声参考映射。Windows 已改为左声道麦克风、参考声道 G711A 静音（0xd5），不再把电脑的第二麦克风声道当作播放参考；硬件映射保持不变。

新增无参考声道编码测试，覆盖大幅度右声道输入、左声道保留、静音编码、非法索引和输出容量。重新构建后 5 项测试通过。该差异已修复，但实际麦克风输入与云端识别效果仍需复测，不能仅凭编码测试判定语音识别恢复。

### 模拟器 Start 禁用修复

实测复现界面显示 Offline 与 Initialization failed。增加具体阶段错误后定位为 media (-16)：Windows BSP 的 AudioService.audio_output_config 为空。Windows 目标允许该可选回调缺失，播放前仅在提供回调时配置采样率；WS63 仍保留必需接口检查。初始化失败现在显示阶段与错误码，便于区分音频、网络和 SDK 问题。

重新编译并打开窗口验证，界面已显示 Ready、Windows network: available，Start 恢复蓝色启用状态。本次未点击 Start 发起云端对话，实际音频与连续会话仍待用户验证。

- 提取共用构建脚本和 runtime 接口，复用应用 owner、LVGL 页面、云接入、音频转换和缓冲；固件与 Windows 分别实现电源、网络、时钟和 SDK HAL。
- WS63 固件重新编译打包通过，未执行烧写。Windows 使用 MinGW GCC/G++ 13.2、真实 SDK 26.8.0 Windows 库编译链接成功。
- 模拟器进程运行 8 秒未退出，随后关闭；未据此认定窗口显示、录放音或云端对话通过。
- 5 项公共测试全部通过，core/services 架构依赖检查通过。模拟器编译定义 AI_UI_NO_PROVISION，不创建 Wi-Fi 按钮，不构建板级配网 HTTP/DNS 服务；硬件目标保留配网。

验证日期：2026-09-24。原有两份示例代码未修改。

## 2026-09-28 实战派实板烧写与启动检查

- 用户反馈利尔达验证通过后，切换到实战派；COM8 为 CH340K UART，COM7 为原生 USB 串口。实测 ESP32-S3 revision v0.2、16 MB Flash、8 MB Octal PSRAM。
- 使用实战派已配置的 Agent ID 与产品三元组构建，镜像大小为 1,608,048 字节（`0x188970`）。通过 COM8 烧写 bootloader、分区表、OTA 初始数据和应用，哈希校验通过。
- 启动确认新应用编译时间为 2026-09-28 17:00:33；PSRAM 内存测试、PCA9557、ST7789、ES8311/ES7210 初始化完成。初始化时出现两条对尚未启用的 I2S 通道调用 disable 的错误日志，随后 Codec open 与音频初始化返回成功；实际录放音质量仍需实板验收。
- 40 秒观察窗口内没有崩溃或重复重启，Wi-Fi 连接成功并取得 IP。观察到会话被启动，证书包校验成功、TLS/WebSocket 握手完成、`session.created` 和 `conversation.created` 到达，随后收到 `input_audio_buffer.speech_started`。这确认云会话已建立，但不能替代完整双向语音效果验证。
- SDK 对自定义地址打印了证书校验关闭提示，但平台 TLS 实现使用证书包，随后实际打印 `Certificate validated`；本次记录以实际 TLS 平台校验结果为准。
- 已脱敏构建日志：`build-verification/szpi-auth-build.log`；烧写日志：`build-verification/szpi-auth-flash.log`；已脱敏启动日志：`build-verification/szpi-boot-2026-09-28.log`。

## 2026-09-28 利尔达实板烧写与启动检查

- 在当前源码上再次执行 `idf.py -B build-lrd -DUNIFIED_BOARD=lrd build`，构建通过，应用镜像仍为 1,600,880 字节。
- 通过 COM6（CH340）确认目标为 ESP32-S3 revision v0.2、8 MB Flash；以 460800 波特率写入 bootloader、分区表、OTA 初始数据和应用，所有镜像哈希校验通过。
- 复位后读取 45 秒日志：8 MB PSRAM 内存测试通过，TPT29555A、ST7789、ES8311/ES7210 与 SDK HAL 初始化完成，观察期间未出现 panic、看门狗超时或重复重启。
- 已保存的 Wi-Fi 未找到匹配 AP，约 30 秒后自动进入配网，热点为 `AI-Setup-AE29`，配网页地址为 `http://192.168.4.1`，DNS、DHCP 与 Web 服务启动完成。
- 首次烧写时 Agent ID 和鉴权参数为空，尚不能发起云端对话。触摸交互、实际录放音、云端会话及长时间稳定性仍待验证。立创板与 WS63 的实板验证状态不变。
- 启动日志保存在本地忽略目录：`build-verification/lrd-boot-2026-09-28.log`。

### 同日更新鉴权配置后再次烧写

用户配置 Agent ID 与产品三元组后重新构建，应用镜像为 1,601,376 字节（`0x186f60`）。通过 COM6 烧写完成并通过哈希校验；启动日志确认运行新镜像（应用编译时间 2026-09-28 16:31:37），不再出现鉴权参数缺失提示。

复位后观察 40 秒，PSRAM、屏幕、音频与 Wi-Fi 初始化完成，未见崩溃或重复重启。设备处于 `AI-Setup-AE29` 配网模式，并有客户端连接；尚未观察到路由器联网成功或云端鉴权结果。构建与启动日志已过滤配置中的鉴权值，分别保存在 `build-verification/lrd-auth-build.log` 和 `build-verification/lrd-auth-boot-2026-09-28.log`，烧写日志为 `build-verification/lrd-auth-flash.log`。

## 构建结果

| 目标 | 工具链与配置 | 结果 |
|---|---|---|
| 立创实战派 `szpi` | ESP-IDF 6.1.0、Xtensa GCC 15.2.0，16 MB Flash / Octal PSRAM | 完整固件编译与链接通过；应用二进制 1,607,552 字节（`0x188780`） |
| 利尔达 `lrd` | ESP-IDF 6.1.0、Xtensa GCC 15.2.0，8 MB Flash / Quad PSRAM | 完整固件编译与链接通过；应用二进制 1,600,880 字节（`0x186d70`） |
| WS63 公共库 | 原 GoldieOS 工具链 GCC 7.3，`rv32imfc / ilp32f / --short-enums` | 同一份 core、cloud、audio、playback 源码交叉编译通过，生成 `libunified_ai_portable.a`；对象文件未引入额外原子操作运行库依赖 |

ESP32-S3 使用发布包原版 SDK 26.8.0 静态库，不使用符号重写、占位 SDK 或允许重复定义的链接选项。两套最终配置均启用两个 SNTP 服务器、关闭 LVGL 示例构建。

本地构建产物：

- `build-szpi/unified_ai_app.bin`、`build-lrd/unified_ai_app.bin`：应用分区镜像。
- 两个构建目录还包含 bootloader、分区表、OTA 初始数据与 `flash_args`。首次烧录使用 README 中的 `idf.py ... flash` 命令，由 IDF 按正确偏移写入完整镜像。
- `build-verification/szpi.log`、`build-verification/lrd.log`：最终构建日志。
- `build-ws63-core/libunified_ai_portable.a`：公共静态库，**不是 WS63 完整固件**。

构建目录和日志不纳入源码管理，重新构建可生成。

## 主机测试

MinGW GCC 13.2，C11，启用 `-Wall -Wextra -Werror`，断言保持启用。CTest **3/3 通过**：

| 测试 | 覆盖内容 |
|---|---|
| `core_lifecycle` | 启停、迟到事件过滤、网络断开、启动失败、连接超时与计时回绕、100 次重启 |
| `audio_mapping_buffer` | G711A 向量与边界、两块板的实际声道映射、缓冲回绕/溢出、短回复及 reset |
| `cloud_lifecycle` | 假 SDK 注入创建/启动失败、同步 CONNECTED、重复 ANSWERING、finish 与播放结束分离、打断、断线后拒绝自动重连回调、幂等停止和格式拒绝 |

`tools/check_architecture.py` 检查通过：core/services 没有直接引用 ESP-IDF、FreeRTOS、LVGL 或板级头文件。它只检查源码依赖边界，不替代运行时验证。

## 2026-09-24 验证边界（后续实板结果见上文）

本次未连接真实开发板、未烧录、未配置真实云凭据，也未发起云端会话。主机测试中的云回调由假 SDK 驱动；ESP32 固件则已链接真实发布库。

实板需验证两种触摸控制器、麦克风/参考通道、配网、TLS 和音频双向传输、打断尾音、断网恢复、反复启停及长时间运行。硬件 DMA 的约 180 ms 尾音预算尚未实测。该日期 WS63 仅验证了公共库；后续独立固件进度见下文。

## 2026-09-29：WS63 独立应用与 ESP32 编译回归

- 新增 src/platforms/goldieos/targets/ws63，排除旧 apps、sdk_integration、system_init 和 aud_algo 应用服务，开机入口为 standalone_entry，直接启动 Unified AI。
- WS63 的 CMake/Ninja 编译、厂商链接、签名及 fwpkg 打包成功。最终固件符号表含 standalone_entry、ws_ui_init、ai_app_dispatch；检索未发现 convai_talk_page/setting_app/convai_bridge 匹配符号。SDK 链接目标为本项目 26.8.0 的 WS63 库。
- 产物：build-ws63-app/out/unified_ai_ws63.fwpkg 和 unified_ai_ws63.bin；日志位于 build-verification/ws63-build-final.log。
- 共用 ai_screen.h 后，实战派与利尔达均重新编译成功，应用分区检查通过；利尔达随后经用户授权烧写 COM6，写入校验通过，实战派本次未烧写。
- 主机 CTest 3/3 通过，架构依赖检查通过。公共业务实现未改变，测试不代表 WS63 的真实 OS/GUI/驱动运行验证。
- WS63 使用空鉴权模板构建，未配置真实凭据，未烧写、未进行端云对话。BSP 头文件仍存在历史告警。触摸、配网、音频通道、DMA 收尾、TLS/CA、开关机和资源余量均待润和实板验收。

## 2026-09-29～30：利尔达卡顿与 Error -5，尚未闭环

1. 共享 UI 版本烧写 COM6，写入校验通过。启动日志显示 8 MB Flash/PSRAM、ST7789、ES8311/ES7210 正常初始化，Wi-Fi 获取 IP，证书包验证通过，收到 session.created 与语音开始事件。用户反馈播报卡顿，最后 Error -5；日志有持续 send queue full 和停止超时。
2. 关闭 ESP Wi-Fi 省电并设置 TCP_NODELAY，增加每 5 秒上行统计。重新编译烧写后，统计多数区间约 250 帧/5 秒、无拒绝，观察到完整 response.done；但用户再次反馈相同错误。这些网络调整不能作为问题已解决的证据。
3. 检查发现播放任务持有音频队列锁时等待 I2S 写入，SDK 入队回调也需要该锁。已将队列锁与 output_lock 分开：入队仅锁短时内存操作，播放写入和 reset 仍受输出锁保护，避免旧音频跨 reset 写回。增加 SDK 上行真实返回码和连续 30 帧失败的日志，保留原失败处理阈值。
4. 最新利尔达版本已编译并烧写，写入哈希校验通过。随后 COM6 不存在，串口重新枚举请求被拒绝；未能取得该版本对话日志，卡顿/报错修复效果仍未确认。用户下一次复测需覆盖两轮长回复、播报中停止和重新 Start。
5. 公共层 CTest 3/3 与架构检查通过；它们不覆盖 ESP 实际线程调度、I2S 或网络质量。需要结合真实 SDK 返回码继续定位，不能仅凭 -5 断言所有失败都来自队列满。

本地日志：lrd-ui-regression-boot.log、lrd-ui-regression-conversation.log、lrd-audio-fix-conversation.log、lrd-ui-regression-flash.log，均在 build-verification 下。最后一个文件记录最新烧写；早先两次烧写日志被该临时脚本覆盖。新版本诊断日志文件 lrd-playback-lock-conversation.log 未产生，因为串口打开失败。

## 2026-09-30：三板共用 LVGL 页面与最小配网

- 三板共用 src/application/ui/ai_lvgl.c 的控件/交互、ai_screen.h 的布局和状态文案；WS63 改用 LVGL 9.5.0。src/application/ui/web 两份资源同时嵌入 ESP 和 WS63 固件；移除旧高级配网页和 WS63 设备端键盘。
- WS63 新增独立网络任务、开放热点 AI-Setup-XXXX、192.168.4.1 的 HTTP/DNS、扫描连接、最多 10 个保存网络、删除/设默认和退出配网。FATFS 两个快照使用序号和 CRC，最新无效时读取上一份。旧 wificfg.txt 保留，首次使用独立应用重新配网。
- 公共 src/application/services/network 模块进行输入/索引、请求头长度与存储校验。ESP 和 WS63 在配网期间均禁止启动 AI；退出后才恢复 Station 对话入口。
- 三板完整编译成功：利尔达应用 1,520,688 字节（0x173430），实战派应用 1,527,376 字节（0x174e50）；IDF 分区容量检查通过。WS63 bin 2,066,788 字节，fwpkg 2,227,208 字节，完成厂商链接、签名和打包。WS63 仍使用空鉴权模板。
- WS63 公共静态库重新交叉编译通过；最终固件不依赖新增原子 exchange 运行库，保留平台互斥锁用于命令交换。BSP 旧头文件仍有编译告警。
- CTest 5/5 通过：原有 3 项，加 provision_validation 和 wifi_web_contract。后者用 Node 执行真实 HTML 内脚本与模拟 DOM/HTTP，覆盖扫描去重、特殊 SSID 安全显示、中文名称、连接/保存、默认/删除、退出和请求失败；不代表浏览器像素检查或真实射频验证。没有 Node 时只运行 4 项 C 测试。
- 本轮尚未烧写润和 WS63；屏幕颜色/触摸、热点与 STA 共存、DHCP、音频、TLS、堆栈余量及连续对话仍待实板验证。此前利尔达播报卡顿/Error -5 的修订保留，不能凭本次编译和主机测试判定已解决。

构建日志：build-verification/ws63-network-build.log、lrd-ui-regression.log、szpi-auth-build.log。上库清单已更新共用页面、网络适配、测试、LVGL 源码归档及逐文件行数。

### 利尔达统一配网页面实板验收

- 使用 COM6（CH340）执行 IDF 构建和完整 flash，工具报告 Hash of data verified、Done；未执行整片擦除，原有 NVS 保留。日志：build-verification/lrd-unified-wifi-2026-09-30-flash.log。
- 后续串口日志采集中 COM6 消失，重新枚举为润和 COM4（CH340K），因此本次没有保存完整启动日志。
- 用户随后明确反馈“利尔达验证正常”，并切换到润和单板；本轮 Wi-Fi 和基础对话由用户验收通过。此前卡顿/Error -5 记录保留为历史，不以本次反馈推断所有长回复压力场景均已覆盖。

### 润和鉴权镜像与 COM4 烧写尝试

- 经用户授权，将已验证的利尔达 Agent、鉴权参数和服务 URL 复制到被 Git 忽略的 config/ws63.local.h；设备名仍取润和自身标识。通过本机默认信任库完成服务端证书链验证，将已验证根证书配置为 WS63 私有信任锚，保持 TLS 证书校验开启。
- 显式重新运行 CMake 配置与编译，确认生成头文件与本地私有配置一致，最终 bin 中包含所需 Agent/鉴权与 CA；没有打印配置值。fwpkg 为 2,228,904 字节，含实际鉴权，不上库。
- 官方 BurnToolCmd 在 COM4 使用 921600 波特率尝试及重试，均停在复位握手阶段，返回 Ack receiving timeout / Shakehand failed。尚未确认写入成功。使用隔离副本启用 resetBeforeBurn，并按工具 WS63 示例使用 115200 波特率重试，仍握手超时，退出码 4；未进入镜像写入阶段。
- 日志：ws63-unified-2026-09-30-flash.log、ws63-unified-2026-09-30-flash-retry.log、ws63-unified-2026-09-30-flash-soft-reset.log，均在 build-verification/。

## 2026-10-10 三板共用门户协议

新增 src/application/services/network/ai_portal.c，ESP32-S3 两板与 WS63 均调用同一 DNS 应答生成器。统一 A/AAAA 语义、TTL、长度检查和附加记录处理；ESP 不再无条件对 AAAA 回 A。HTTP 统一 302/no-store，并在 ESP 应用路由后增加未知 GET 兜底跳转。保留 ESP 的语言参数和各平台 DHCP、线程、HTTP 框架及存储适配。

验证：host 8/8 测试通过（新增 DNS 截断、容量、响应循环、AAAA、附加记录测试）；架构依赖检查通过；WS63、利尔达和实战派镜像均编译成功。本轮没有烧写，手机自动弹窗仍待硬件验证；不能把代码共用视为此前故障已修复。TUN 保持原状。

## 2026-10-10 共用协议镜像实测与 DHCP 诊断

共用协议镜像经 COM4 烧写成功，工具退出码 0。用户反馈仍显示不可上网、不弹配网页面。重新采集只看到微信域名 A/AAAA 查询与 sendto 成功，未见 HTTP 系统探测，不能推断手机已接收到 DNS 应答。

新增可选 WS63_PORTAL_DIAGNOSTICS（默认 OFF）：在 TCP/IP 线程安装 AP IPv4 output 观察器，原样调用原 output；只观察前 8 条 DHCP 应答的地址与选项类型，不记录客户端名称、标识或密码。用于核对发给手机的 DHCP 参数，不是已确认的功能修复。退出 AP 前恢复原 output。

## 2026-10-10 确认 DHCP 缺少 Option 3

DHCP 诊断镜像烧写成功。手机重连日志确认多轮 Offer(message=2)/ACK(message=5) 包含地址 192.168.4.2、掩码 255.255.255.0、DNS 192.168.4.1，但没有路由器选项 3。SDK 头文件 LWIP_DHCPS_GW 默认 0，实际 DHCP 实现在预编译库内，仅修改应用宏不能改变它。先前电脑有网关的结果不能代表本次手机收到的 DHCP 应答。

新增 WS63 AP 输出兼容适配：对 DHCP Offer/ACK 私有副本，在已有 END 后的余量内补齐 Option 3，保持 IP/UDP 长度不变并重算 UDP 校验和；已有网关、NAK、截断、分片、选项重载或余量不足则不改。仅影响 AP 服务端端口 67→68，退出 AP 恢复原 output。诊断观察器位于补全后，便于验证实际发出的网关。长期可用开启 LWIP_DHCPS_GW 的厂商库替代此兼容适配。

9/9 主机测试通过，WS63 镜像编译通过。自动弹窗的最终修复效果仍待新镜像实机验证。

## 2026-10-10 自动配网页面实机验证通过

网关补全镜像烧写成功（Burned successfully，退出码 0），用户正常重启、忽略热点后重连，明确反馈“已修复”。日志确认 DHCP Offer 与 ACK 均新增 Option 3=192.168.4.1，随后出现 /generate204 与 /generate_204 探测、HTTP 302、页面根路径及 /saved/list、/scan 请求。结合修复前缺失网关且没有系统 HTTP 探测的日志，确认本次自动弹窗问题由润和 DHCP 未下发默认网关导致。

本轮结论限定为自动门户弹窗已验证；尚未单独确认此版本提交 Wi-Fi 密码后的联网和 AI 对话回归。COM4 诊断采集已停止。WS63_PORTAL_DIAGNOSTICS 仍为构建选项（默认 OFF），当前烧写的诊断构建为 ON；网关补全不依赖此诊断选项。未再更换用户验证通过的镜像。

## 2026-10-10 提交前清理

删除 portal_trace.c、WS63_PORTAL_DIAGNOSTICS 构建选项、DHCP/DNS/HTTP 逐请求诊断日志及仅用于打印域名的重复解析。保留 DHCP Option 3 补全与 UDP 校验和、共享 DNS、TLS 时间、NV 存储、网络线程让出 CPU 和网页请求结果恢复等必要修复。历史诊断日志说明仅作排查记录，不再是当前构建选项。保留有用的失败错误码与 TLS 证书校验失败原因。

将清单重算脚本整理为 tools/update_repository_guide.py，去掉固定工作目录；清理 build-verification 中旧串口/烧写日志、反汇编和一次性探测脚本。保留忽略的构建缓存、烧写工具、本地鉴权和用于当前编译的辅助脚本。未执行提交、暂存、推送或自动烧写。当前 boards 仅含乐鑫板级实现，润和仍由 src/platforms/goldieos/targets/ws63 接入 BSP。

随后按用户要求增加 src/boards/ws63/hoperun 和通用 ws_board 接口、WS63_BOARD 构建选择，将板级电源、显示、音频布局及 BSP 路径分离。此调整取代上节“不执行目录迁移”的初始清理范围。

清理与 Hoperun 分层后的最终 WS63 固件、Windows 模拟器构建通过；9/9 主机测试和架构检查通过。移除 122 个旧临时文件。清单覆盖所有 Git 候选文件，鉴权/镜像/日志忽略规则有效。未做本轮硬件烧写，屏幕和音频分层仍需实机回归。

## 2026-10-10 清理版烧写与构建目录迁移

清理和 Hoperun 板级分层后的镜像通过 COM4 烧写成功（Burned successfully，退出码 0），用户待验证显示、配网和对话。

按板型区分输出：新建 build-hoperun-app 与 build-hoperun-win，沿用原工具链、Release 和 Hoperun BSP 配置重新生成并完整编译，两者成功。Windows 本地 convai.cfg 已迁移并核对哈希一致。随后删除旧 build-ws63-app、build-ws63-win；更早的可选公共库 build-ws63-core 也已按用户要求删除。历史记录中的旧路径仅用于历史追溯，当前命令与交付路径见 README 及 src/platforms/goldieos 文档。

## 2026-10-10 平台/板型命名复核

Hoperun 固件导出名改为 unified_ai_${WS63_BOARD}.fwpkg/.bin，当前为 unified_ai_hoperun；重新构建成功，新旧导出文件哈希一致后清除旧别名。内部 WS63 SDK 库、工具链、平台目录和通用配置接口保留芯片名称。修正文档中把 Hoperun 显示、电源、音频和 DHCP BSP 特性泛化为 WS63 平台特性的说明，并更新已烧写状态。

按用户要求删除 build-host 与 build-verification（包括日志、辅助脚本和烧写工具副本）。测试源码仍在 tests，可重新生成 build-host；固件及模拟器产物保留。此前测试结果仍见历史记录，本轮仅调整导出命名与文档，不改变固件内容。

## 2026-10-10 四目标编译供用户逐一验证

最新源码分别完成利尔达、实战派、Hoperun 固件和 Hoperun Windows 模拟器编译，全部退出码 0。ESP 合并镜像由各自 build 目录的 flash_args 经 esptool merge-bin 生成，利尔达 8 MB Flash、实战派 16 MB Flash，烧写偏移均为 0x0。Hoperun 使用 fwpkg。此轮未烧写/启动程序，显示、配网和对话验收等待用户逐一确认；模拟器不覆盖实体板 Wi-Fi/触摸/Codec 验证。

## 2026-10-10 Hoperun 清理分层版用户验收

用户明确确认润和显示、配网和对话功能均正常。此结论覆盖当前清理、板级分层及 DHCP 网关修复版本。随后用户切换利尔达单板继续验证。

利尔达最新镜像通过 COM6、460800 baud 烧写成功：bootloader、分区表、OTA 初始数据及应用均校验 Hash of data verified，esptool 退出码 0，已 RTS 自动复位。当前这轮利尔达功能验收待用户反馈。

## 2026-10-10 利尔达最新版本用户验收

用户确认利尔达显示、配网和对话功能均正常。随后换接实战派进行同项验证。

实战派最新 16 MB Flash 配置镜像通过 COM8、460800 baud 烧写成功：引导、分区表、OTA 初始数据和应用均完成哈希校验，esptool 退出码 0，已 RTS 自动复位。实战派当前轮功能验证待用户反馈。

## 2026-10-10 实战派验收与 Windows 模拟器启动

用户确认实战派显示、配网和对话正常；至此三块单板当前版本上述功能均得到用户确认。已启动 build-hoperun-win/unified_ai_simulator.exe，进程保持运行且窗口响应正常。模拟器界面和云端对话仍待用户验收。
