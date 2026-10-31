# 第 5 步：完整语音链路与媒体契约

[培训索引](README.md) · [上一步](04-session-and-ui.md) · [下一步](06-network-provisioning.md)

## 1. 学习目标和必要概念

本节跟踪音频字节，而不是只看 Listening/Speaking 文字。需要区分采样点、采样帧、字节数、声道布局、应用队列和设备缓存。

| 概念 | 本项目中的例子 |
|---|---|
| PCM16 | 一个采样值用 16 位有符号整数表示，通常占 2 字节 |
| 采样帧 | 同一时刻各通道采样的集合；四 slot 输入一帧含四个 PCM16 值 |
| G711A | A-law 编码，一个通道每个采样编码为 1 字节 |
| 8 kHz | 每秒 8000 个采样时刻 |
| 参考通道 | 为上行提供的回声参考数据，不是另一位用户的语音 |
| 排空 | 应用队列及平台所判断的设备剩余播放已处理完毕 |

单声道 8 kHz G711A 每秒 8000 字节；PCM16 单声道每秒 16000 字节。两个缓冲即便都是“16000 字节”，对应的时长可能完全不同。

## 2. 源码阅读地图

| 层 | 文件 | 本节重点 |
|---|---|---|
| 公共契约 | [ai_media.h](../../src/application/services/media/include/ai_media.h) | enable/reset/push/finish 及返回值 |
| 公共算法 | [ai_audio.c](../../src/application/services/media/ai_audio.c) | 通道抽取、A-law 编解码 |
| 公共队列 | [ai_playback.c](../../src/application/services/media/ai_playback.c) | 环形缓存、首次预缓冲、epoch |
| 云接入 | [ai_cloud.c](../../src/application/services/cloud/ai_cloud.c) | send_audio、on_audio、on_status |
| ESP 媒体 | [media.c](../../src/platforms/esp_idf/components/media/media.c) | capture_task、playback_task |
| GoldieOS 媒体 | [media.c](../../src/platforms/goldieos/components/media/media.c) | capture、playback、BSP AudioService |

## 3. 上行：从麦克风到 SDK

```text
麦克风 / 设备录音接口
  → 平台采集线程读取 PCM
  → 根据板级布局选择 microphone / reference
  → ai_audio_encode_slots
  → 注入的 ai_uplink_fn，即 cloud 的 send_audio
  → convai_send_audio
  → SDK 内部传输及平台 socket/TLS
  → 云服务
```

ESP `board_audio_read` 返回 PCM 采样值数量，媒体要求是四 slot 的整数帧；GoldieOS `audio_read` 返回字节数，媒体根据 channels 和 `sizeof(int16_t)` 换算帧数。读代码时必须先确认返回值单位。

当前 ESP 采集布局：实战派 mic=0/reference=1，利尔达 mic=2/reference=1。润和实机由板级提供 channels=2/mic=0/reference=1；模拟器使用无参考配置。

`ai_audio_encode_slots` 的输出不是 `mic0,ref0,mic1,ref1` 交错排列，而是：

```text
前 frames 字节：mic0 mic1 ... micN
后 frames 字节：ref0 ref1 ... refN
```

没有参考通道时，后半段填的是 PCM 零值的 A-law 编码，不能直接以字节 0 代替。这里仅进行映射与编码，没有新增本地唤醒或 AEC 算法。

## 4. 采集持续运行与上传开关

采集线程和会话生命周期不是同一件事。ESP 即使空闲也继续读取 DMA，以避免下次会话上传旧音频；GoldieOS 录音线程也持续读取。

enable 控制是否把采集结果送到上传回调。capture_epoch 用来识别“读取期间已经启停过”的旧帧，避免将停止前开始读取的数据错误送入新的上传阶段。

`ai_uplink_fn` 返回 0 表示接受，+1 表示临时暂停，负数表示失败。cloud 在尚未 connected 时返回 +1。不要把暂停统计成连续失败，也不要把 SDK 接受当成云端已识别。

## 5. 下行：从 SDK 回调到扬声器

```mermaid
flowchart LR
    SDK[SDK 音频回调] --> Cloud[ai_cloud.on_audio]
    Cloud --> Push[media.push 复制数据]
    Push --> Queue[公共 G711A 环形队列]
    Queue --> Worker[平台播放线程]
    Worker --> Decode[A-law 解码]
    Decode --> Device[板级接口或 AudioService]
    Device --> Speaker[扬声器]
```

SDK 给 on_audio 的缓冲是借用的数据。`push` 必须在返回前复制，不能保存指针稍后播放。公共队列用 memcpy 完成入队，平台锁保证并发安全。

`on_audio` 先检查接受/连接/播放门控，过滤无效数据；不支持的格式或 push 返回失败会触发 -3。当前不是“缓冲满就悄悄丢掉继续播放”的策略。

## 6. 环形队列与首次预缓冲

队列维护 read、size、capacity，写入位置由 `(read + size) % capacity` 计算。数据跨尾部时分两次复制。容量不足会整体拒绝该次 push，不进行半包写入。

首次预缓冲阈值为 1600 字节时，对应 200 ms 单声道 G711A。首次实际取出数据后，playing 保持 true；即使中途队列取空，后续小包也无需重新攒够阈值。

这里的 `queue.playing` 表示本次 reset 后已开始取数据，不表示扬声器此刻一定有声。下一次 reset 会重新启用预缓冲规则。

finish 让短回答即使不足阈值也可以取出。reset 清空 read/size/playing/finished 并推进 epoch，epoch 用于平台识别旧的排空判断是否已经失效。

公共队列不自己加锁。调用者必须串行化访问；不能因为算法在公共目录就认为它天然线程安全。

## 7. 两个平台如何消费队列

| 项目 | ESP 当前实现 | GoldieOS 当前实现 |
|---|---|---|
| 输出格式 | 将单声道 G711A 解码并复制为双声道 PCM16 | 单声道 PCM16 |
| 设备接口 | `board_audio_write` | `AudioService.audio_write` |
| 单次取出上限 | 160 字节 G711A | 512 字节 G711A |
| 播放驱动节奏 | 写入设备，空读时休眠 | 检查设备水位，最多连续 8 批，再休眠 |
| 设备结束判断 | 应用排空后等待约 180 ms，再检查 epoch 与队列 | 应用队列为空且设备 valid length 为零 |
| 队列容量 | 配置项决定，分配在 PSRAM | 由 WS_AI_PLAYBACK_BYTES 决定，当前诊断固件为 16000 字节 |

ESP 的 180 ms 来自当前 DMA 配置假设，不是通用声卡“已经播放完”的精确 API。更换硬件后应重新核对。GoldieOS 的设备水位也应按 BSP 实际语义解释。

WS63 当前目标设备水位为 4096 字节 PCM，播放线程优先级设置为 21、采集为 22；这些是当前实现及诊断对照参数，不是建议所有平台照抄。运行时设置成功仍待启动日志确认。

## 8. 为什么分 queue 锁与 output 锁

queue/rx 锁保护应用缓存；output 锁序列化实际设备写入与 reset。音频写入可能等待设备消费，若整个等待期间持有 queue 锁，SDK 回调就无法入队，可能拖住 SDK 收发线程。

当前两平台都尽量在取出数据后释放队列锁，再执行设备写入。reset 与输出仍需同步，否则 Stop 之后可能继续写入旧数据。

`enable(false)` 是上传停止屏障：返回后不能再有旧上传回调继续运行。不能只把一个变量设为 false 就假定正在执行的 send 已结束。

## 9. 回答结束不等于收到最后一包

SDK ANSWER_FINISHED 只调用 media.finish；平台完成排空后投递 AI_TURN_FINISHED，应用进入 Listening。网络传输、应用队列消费和扬声器输出是三个不同阶段。

SDK 也可能报告 LISTENING，对应 AI_HEARING。阅读时要同时留意这条状态来源，不能把屏幕文字当成设备缓存水位。

## 10. 当前诊断应怎样阅读

润和已有 `in_g711`、`pulled_g711`、queue、device_pcm、write_max 等日志，新增 `push_gap_max_ms`、`push_age_ms` 及 TLS/BIO/socket 统计。`gap_max_ms` 包含循环中的阻塞写入，不是单独测得的调度饥饿。

`pending_after_read` 是 TLS 最近一次 read 后的快照；`pending_wait` 是读请求未就绪而快照非零的计数。它不是实时 TLS 查询，也不能直接证明网络或云端有故障。详见[专项记录](../hoperun-audio-investigation.md)。

当前已观察到队列不足和集中入队两类表现，-3 的拒绝链路有日志依据，但不均匀交付根因仍未闭环。新诊断版待烧写，不作为已修复案例。

## 11. 练习与参考答案

1. 800 字节单声道 G711A 对应多久？100 ms。
2. 800 个采样解码成单声道 PCM16 占多少字节？1600；双声道复制后为 3200。
3. 队列已播放过，后来取空，再来 240 字节是否重新等到 1600？当前不会，除非已经 reset。
4. on_audio 能否把 SDK data 指针直接交给异步线程？不能，必须复制或另有明确的 SDK 所有权协议。
5. Listening 是否证明麦克风、上传、服务端识别均正常？不能，需要逐段验证。

阅读 [test_audio.c](../../tests/test_audio.c) 和 [test_goldie_media.c](../../tests/test_goldie_media.c)，找到与上述缓存、设备写入和 reset 边界对应的断言。
