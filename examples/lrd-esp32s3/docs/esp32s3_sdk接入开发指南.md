# AI Hardware Agent SDK 接入开发指南（以 ESP32-S3 为例）

本文面向要在**新硬件平台 / 新工程**上接入 AI Hardware Agent SDK、做一个全新 AI 语音对话 demo 的开发者，以 `examples/szpi-esp32s3`（立创·实战派 ESP32-S3）参考实现为例，讲清楚 SDK 的接入契约与每一步该写什么代码。

> **与已有文档的关系：**
>
> | 文档 | 定位 |
> | --- | --- |
> | 仓库根目录 [README.md](../../../README.md) | 仓库总览、SDK 包获取、设备凭证配置（公共章节） |
> | [esp32s3_开发最佳实践.md](esp32s3_开发最佳实践.md) | 本 demo 的**部署指南**：装环境、编译、烧录、使用 |
> | **本文** | **接入指南**：从零把 SDK 接进一个新工程，做出一个新 demo |
>
> **硬件参考：** 立创开发板 wiki《[实战派 ESP32-S3 准备篇](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/prepare.html)》（开发环境安装见其第 2 章，开发流程详解见第 3 章）。板载 ES7210（麦克风阵列采集）+ ES8311（扬声器播放）+ ST7789 LCD + FT6336 触摸。

---

## 目录

1. [总体架构：SDK 给什么，你写什么](#1-总体架构sdk-给什么你写什么)
2. [SDK 公共 API 速览](#2-sdk-公共-api-速览)
3. [前置准备](#3-前置准备)
4. [六步接入流程](#4-六步接入流程)
   - [Step 1 建立 IDF 工程骨架](#step-1-建立-idf-工程骨架)
   - [Step 2 把 SDK 封装成组件](#step-2-把-sdk-封装成组件)
   - [Step 3 实现平台适配层](#step-3-实现平台适配层)
   - [Step 4 复用 G.711A 编解码](#step-4-复用-g711a-编解码)
   - [Step 5 实现桥接层（引擎生命周期 + 音频管线）](#step-5-实现桥接层引擎生命周期--音频管线)
   - [Step 6 应用层接线](#step-6-应用层接线)
5. [编译烧录验证](#5-编译烧录验证)
6. [调试观测点与常见问题](#6-调试观测点与常见问题)
7. [新平台接入 Checklist](#7-新平台接入-checklist)
8. [附录：参考实现文件索引](#8-附录参考实现文件索引)

---

## 1. 总体架构：SDK 给什么，你写什么

SDK 以 **预编译静态库 + 4 个公共头文件** 的形式发布：

| 交付物 | 路径 | 说明 |
| --- | --- | --- |
| SDK 静态库 | `libs/esp32-s3/libconvai_sdk.a` | 与平台相关，按目标芯片选目录 |
| 公共头文件 | `include/convai/{convai_api.h, convai_event.h, convai_platform.h, convai_types.h}` | 所有平台共用 |

SDK 内部已完成：云端连接（WebSocket over TLS）、鉴权握手、协议编解码、会话状态管理、事件分发。**你不需要写任何网络协议代码。**

端侧整体分层如下：

```
┌─────────────────────────────────────────────────────────┐
│  应用层：UI(LVGL) / 按键 / 配网 / SNTP / 音色切换          │  ← Step 6
├─────────────────────────────────────────────────────────┤
│  桥接层 convai_bridge                                    │  ← Step 5
│  引擎生命周期 · SDK 回调分发 · 上行采集任务 · 下行播放任务  │
├──────────────────────────────┬──────────────────────────┤
│  音频驱动 audio_codec_t       │  编解码 convai_codec_g711a│  ← Step 4
│  mic 采集 / 扬声器播放 / 音量  │  PCM ↔ G.711A（可直接复用）│
├──────────────────────────────┴──────────────────────────┤
│  平台适配层 convai_platform_t (OSAL/NetAL/TLSAL/Misc)     │  ← Step 3
│  FreeRTOS · lwIP · mbedTLS · 日志/设备ID/网络状态          │
├─────────────────────────────────────────────────────────┤
│  ConvAI SDK（libconvai_sdk.a，闭源预编译库）               │
│  WebSocket/TLS 协议 · 鉴权 · 会话/事件管理                 │
└─────────────────────────────┬───────────────────────────┘
                              │ WebSocket (TLS)
                        云端 AI 服务（LLM/TTS/ASR）
```

| 你要写的 | 工作量 | 能否直接抄参考实现 |
| --- | --- | --- |
| 平台适配层（4 个文件） | 中 | ESP32 平台可整目录复用；其他 RTOS 参考接口约定重写 |
| G.711A 编解码 | 小 | 直接复用 `convai_codec_g711a.c`（纯 C，无平台依赖） |
| 桥接层 | 中 | 结构可复用，音频管线参数按新板子硬件调整 |
| 音频硬件驱动 | 中（板级相关） | 抽象接口 `audio_codec_t` 保持不变，换板只换实现 |
| 应用层（UI/按键/配网） | 自由发挥 | 可裁剪：最小 demo 只需一个按键 + LED |

---

## 2. SDK 公共 API 速览

头文件契约（`include/convai/`）是接入的唯一依据，本节摘要最关键的部分。

### 2.1 生命周期（convai_api.h）

```c
// 1. 创建引擎（必须在平台适配层注册之后）
int  convai_create(convai_engine_t *handle, const char *config_json,
                   const convai_event_handler_t *handler, void *user_data);
// 2. 启动会话（连接云端、开始处理）
int  convai_start(convai_engine_t handle, const convai_opt_t *opt);
// 3. 输入：上行音频（G.711A）/ 文本
int  convai_send_audio(convai_engine_t handle, const void *data_ptr,
                       size_t data_len, const convai_audio_frame_info_t *info_ptr);
int  convai_send_message(convai_engine_t handle, const void *data_ptr,
                         size_t data_len, const convai_message_info_t *info_ptr);
// 4. 结束会话 / 5. 销毁引擎
int  convai_stop(convai_engine_t handle);
void convai_destroy(convai_engine_t handle);
// 运行时动态更新会话配置（如切换音色），需会话已连接
int  convai_update(convai_engine_t handle, const char *session_update_json);
// 工具
const char *convai_get_version(void);
const char *convai_err_2_str(int err_code);
```

### 2.2 四个回调（convai_event.h）

在 `convai_create()` 时通过 `convai_event_handler_t` 一次性注册，**回调运行在 SDK 内部线程**，结构体/缓冲区只在回调期间有效，需要留存必须拷贝：

| 回调 | 触发时机 | 典型用途 |
| --- | --- | --- |
| `on_convai_event` | `CONNECTED` / `DISCONNECTED` / `FAILED` / `UPDATED` | 连接状态展示、断连复位 |
| `on_convai_conversation_status` | 会话状态机变化（见 2.3） | UI 状态切换、指示灯 |
| `on_convai_audio_data` | 下行 TTS 音频（G.711A 裸流） | 解码 → 播放 |
| `on_convai_message_data` | 下行文本消息 | 展示回答文本 / 调试 |

### 2.3 会话状态机（convai_types.h）

```
IDLE ──┬──► LISTENING ──► THINKING ──► ANSWERING ──► ANSWER_FINISHED ─┐
       │                    ▲              │                           │
       │                    └── INTERRUPTED◄┘ (用户打断)                  │
       └──────────────────────────────────────────────────────────────────┘
```

- `LISTENING`：服务端在收音，此时持续上行 mic 数据；
- `ANSWERING`：下行 TTS 到达，开始播放；
- `INTERRUPTED`：用户打断，**应立即清空播放队列**（参考实现置 `s_playback_flush`）；
- `ANSWER_FINISHED`：本轮回答音频已全部下发（参考实现借此强制把尾部音频播完）。

### 2.4 两份 JSON 配置

**① 创建时配置**（`convai_create` 的 `config_json`，由 `bridge_build_config_json()` 拼装）：

```json
{
  "info": { "api_key": "..." },                       // 方式一：API-Key 鉴权（推荐）
  "ws":   { "audio": { "codec": 0 } }                 // codec 0 = G.711A
}
// 或方式二：Product-Key 五元组
{
  "info": { "product_id": "...", "product_key": "...",
            "product_secret": "...", "device_name": "..." },
  "ws":   { "url": "wss://...", "audio": { "codec": 0 } }   // url 可选
}
```

**② 启动参数**（`convai_start` 的 `opt.params`，定义人设与音色）：

```json
{
  "config": {
    "llm_config": { "system_messages": ["你的名字叫小荷，你可以帮小朋友解决小烦恼哦。"] },
    "tts_config": { "provider_params": { "audio": { "voice_type": "Chinese (Mandarin)_Warm_Girl" } } }
  }
}
```

`convai_opt_t` 的 `agent_id`（智能体 ID）与 `mode = CONVAI_MODE_WS` 也在此填入。运行中可通过 `convai_update()` 用同结构的 JSON 热更新音色等配置（未连接时 SDK 返回 `CONVAI_ERR_INVALID_STATE`，应只保存待下次 start 生效）。

### 2.5 音频格式约定

| 方向 | 格式 |
| --- | --- |
| 上行 `convai_send_audio` | G.711A（A-law），8 kHz；`info.commit` 传 0 表示流式追加 |
| 下行 `on_convai_audio_data` | G.711A，8 kHz 单声道 |

G.711A 每样本 1 字节，编解码后为 16-bit 小端 PCM（8 kHz）。若 codec 硬件采集/播放不是 8 kHz，需要在驱动层或桥接层做重采样；参考实现里 ES7210/ES8311 直接配置为 8 kHz，全程 1:1 无重采样。

### 2.6 常用错误码（convai_types.h）

`0 = CONVAI_OK`；负值为主要错误：`-2 INVALID_PARAM`、`-7 NETWORK`、`-11 TLS`、`-14 INVALID_STATE`（如未连接就 `update`）、`-17 SESSION_NOT_READY`、`-18 CONFIG_INCOMPLETE`（鉴权字段缺失）、`-19 INVALID_JSON`。可用 `convai_err_2_str()` 转为可读字符串。

---

## 3. 前置准备

### 3.1 硬件与知识

- **推荐硬件基线**：ESP32-S3（16 MB Flash + 8 MB Octal PSRAM），I2S 数字麦克风 codec + 扬声器功放；LCD/触摸可选。
- **知识要求**（同 [lckfb 准备篇](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/prepare.html) 的前置说明）：C/C++ 基础、FreeRTOS 任务/队列/信号量使用经验。

### 3.2 开发环境

| 工具 | 版本 | 用途 |
| --- | --- | --- |
| ESP-IDF | **6.0**（稳定版） | 编译框架。安装方式见乐鑫官方文档或 lckfb wiki 第 2 章 |
| Node.js + npm | LTS | 仅界面需要中文字体时用于 `lv_font_conv` 生成 CJK 字体 |
| 串口驱动 | CH340 / CP210x | 烧录串口 |

> 最小可行 demo（无 UI）不需要 Node.js，也不需要 LVGL 相关组件。

### 3.3 放置 SDK（必做）

按根目录 README「[工程集成](../../../README.md#工程集成)」解压 SDK 包后，确认仓库根目录下有：

```
<repo>/include/convai/convai_api.h          # 等四个公共头文件
<repo>/libs/esp32-s3/libconvai_sdk.a        # ESP32-S3 预编译库
```

> 未放置 SDK 库时工程只能链接到占位实现（编译能过，无法真实对话）。

### 3.4 凭证（必做）

准备 **`agent_id` + `api_key`**（推荐，API-Key 鉴权），或 **五元组**（`product_id/product_key/product_secret/device_name`）。获取方式见 README「[设备凭证配置](../../../README.md#设备凭证配置)」。凭证在 Step 6 写入 `convai_bridge_defaults.c`。

---

## 4. 六步接入流程

以下按新建一个 ESP32-S3 工程（`examples/my-demo/`）的顺序展开。每一步都标注了参考实现里对应的位置，**可以直接复制再改**。

### Step 1 建立 IDF 工程骨架

新建 ESP-IDF 工程，最小结构：

```
my-demo/
├── CMakeLists.txt            # IDF 工程入口
├── partitions.csv            # 分区表
├── sdkconfig.defaults        # 预设关键配置
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml     # 组件依赖
    └── main.c                # app_main
```

**顶层 CMakeLists.txt**（与 [参考实现](../CMakeLists.txt) 一致）：

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VER "1.0.0")
project(ai_hardware_agent_esp32)
```

**sdkconfig.defaults** 建议先从 [参考实现的 sdkconfig.defaults](../sdkconfig.defaults) 拷贝，其中与 SDK 稳定性直接相关的几组：

```ini
# ---- PSRAM（WiFi+TLS+LVGL 同跑，内存压力大，PSRAM 是刚需）----
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=2048     # 小对象仍走内部 RAM，保证性能
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=98304

# ---- mbedTLS（TLS 走外部 RAM，否则握手期内部 RAM 吃紧）----
CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y
CONFIG_MBEDTLS_SSL_PROTO_TLS1_2=y
CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384
CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096

# ---- lwIP（音频流对 TCP 吞吐敏感）----
CONFIG_LWIP_TCP_SND_BUF_DEFAULT=65534
CONFIG_LWIP_TCP_WND_DEFAULT=65534

# ---- FreeRTOS ----
CONFIG_FREERTOS_HZ=1000

# ---- 任务看门狗（配网/阻塞式初始化会长时间占 CPU0，关 IDLE 检查；任务卡死走 panic 留栈）----
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1=n
CONFIG_ESP_TASK_WDT_PANIC=y
```

**分区表**：至少预留双 OTA 分区（参考 [partitions.csv](../partitions.csv)：16 MB Flash，`ota_0`/`ota_1` 各 ~3.9 MB + 8 MB assets），并在 sdkconfig 里启用自定义分区表。

**main 组件依赖**：最小集合是 `nvs_flash`、`esp_wifi`、`esp_netif`、`esp_timer`、`esp_driver_i2s`、`mbedtls`、`lwip`、`esp_ringbuf`、`convai_sdk`（Step 2 创建）。有屏再加 `esp_lcd`、`lvgl`、`esp_lvgl_port` 等。

### Step 2 把 SDK 封装成组件

把预编译库包成 IDF 组件，全部内容见 [components/convai_sdk/CMakeLists.txt](../components/convai_sdk/CMakeLists.txt)，核心是三件事：

```cmake
# 仓库根目录（components/convai_sdk 上溯 4 级）
get_filename_component(CONVAI_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
set(SDK_LIB_PATH "${CONVAI_REPO_ROOT}/libs/esp32-s3/libconvai_sdk.a")
set(SDK_INC_PATH "${CONVAI_REPO_ROOT}/include/convai")

# 1) 注册一个含占位源文件的组件，让 ${COMPONENT_LIB} 成为普通静态库
idf_component_register(SRCS "convai_sdk_placeholder.c"
                       INCLUDE_DIRS "${SDK_INC_PATH}")

# 2) 把预编译库包成 IMPORTED target 并链接进来
add_library(__convai_sdk_impl STATIC IMPORTED)
set_target_properties(__convai_sdk_impl PROPERTIES IMPORTED_LOCATION "${SDK_LIB_PATH}")
target_link_libraries(${COMPONENT_LIB} INTERFACE __convai_sdk_impl)
```

3）在 **main 组件的 CMakeLists.txt 末尾**加链接选项，规避已知符号冲突：

```cmake
idf_build_set_property(LINK_OPTIONS "-Wl,--allow-multiple-definition" APPEND)
```

> **为什么必须加**：预编译库内含未重命名的 `cJSON_*` 符号，与组件管理器拉下来的 `espressif/cjson` 组件重复定义，直接链接会报 multiple-definition 错误。
>
> 参考实现还支持「仓库存在 `src/` 源码时改为源码构建」的回退逻辑（见该文件 `if(EXISTS ...)` 分支），接库时按需选用。

### Step 3 实现平台适配层

SDK 通过一个 **函数指针表（vtable）** 向宿主平台索取 OS / 网络 / TLS / 杂项能力。**必须在第一次 `convai_create()` 之前注册**，否则 SDK 无法分配内存、建线程、连网络。

先认识契约（`convai_platform.h`）：

```c
typedef struct convai_platform_s {
    uint16_t       abi_version;   // 必须填 CONVAI_ABI_VERSION（当前 1.1）
    convai_osal_t  osal;          // 内存/时间/互斥/线程/随机
    convai_netal_t netal;         // 非阻塞 BSD socket
    convai_tlsal_t tlsal;         // mbedTLS 客户端
    convai_misc_t  misc;          // 日志/设备ID/UUID/网络状态
} convai_platform_t;

int convai_platform_init(const convai_platform_t *platform);   // 注册给 SDK
```

参考实现按层拆成 5 个文件（`main/sdk/platform/`），**ESP32 平台可直接整目录复制**：

| 文件 | 层 | 背后依赖 |
| --- | --- | --- |
| `convai_platform_esp32.c` | vtable 装配 + 注册入口 | — |
| `esp32_osal.c` | OSAL | FreeRTOS / libc |
| `esp32_netal.c` | NetAL | lwIP BSD socket |
| `esp32_tlsal.c` | TLSAL | mbedTLS |
| `esp32_misc.c` | Misc | ESP-IDF |

各层的关键实现约定（这些语义 SDK 依赖很重，**逐条对照检查**）：

**OSAL（esp32_osal.c）**

- `malloc/free`：直接用 libc 即可（ESP-IDF 大分配自动落 PSRAM）；
- `get_time_ms`：**UTC 墙钟**毫秒（`gettimeofday`），用于日志与令牌有效期判断——**SNTP 校时之前它不准**，所以应用层必须先做 SNTP；
- `get_tick_ms`：单调毫秒（`esp_timer_get_time()/1000`），用于超时，**绝不能跟随墙钟回跳**；
- `mutex_*`：**必须用递归互斥锁**（`xSemaphoreCreateRecursiveMutex`），SDK 存在锁内重入；
- `thread_create/join/destroy`：join 用「任务退出前 give 二值信号量」实现；destroy 前必须确认任务已退出，不能提前释放句柄；SDK 默认线程栈 4096，priority 参数 ≤0 时用你设定的默认值（参考实现取 `tskIDLE_PRIORITY + 10`，高于 LVGL(7)、低于 lwIP(18)/WiFi(23)）；
- `fill_random`：硬件 TRNG（`esp_fill_random`），TLS 握手依赖它。

**NetAL（esp32_netal.c）——全程非阻塞，返回值约定如下**

| 函数 | 约定 |
| --- | --- |
| `socket_connect` | 先 `fcntl(O_NONBLOCK)` 再 `connect`，返回 `EINPROGRESS` **算成功**；SDK 之后用 `socket_poll(CONVAI_POLL_WRITE)` + `socket_get_error`（`SO_ERROR`）驱动连接完成 |
| `socket_send/recv` | 返回 0 = 成功（`*sent/*recvd` 为 0 表示当前 would-block，可重试）；返回 -1 = 致命错误或对端关闭（`recv` 读到 0 也是对端关闭 → -1） |
| `socket_poll` | `select()` 实现，`events`/`revents` 用 `CONVAI_POLL_READ/WRITE` 位 |

**TLSAL（esp32_tlsal.c）**

- BIO 回调直接挂在 NetAL 的裸 fd 上（不要用 `mbedtls_net_*`），把 `EAGAIN/EWOULDBLOCK` 翻译成 `MBEDTLS_ERR_SSL_WANT_READ/WRITE`，这样 SDK 能继续用 `socket_poll` 驱动**异步握手**；
- `tls_handshake_step` 是 1.1 新增接口：握手未完成时置 `*want_flags = CONVAI_POLL_READ/WRITE` 并返回 0，完成时置 `*done = 1`；
- `tls_connect` 的 `ca_cert` 为 NULL 时退化为 `VERIFY_NONE`（参考实现会打 WARNING）——生产环境建议内置 CA 证书走 `VERIFY_REQUIRED`；
- mbedTLS 4.x 走 PSA Crypto，RNG 由 PSA 管理，无需自建 entropy/ctr_drbg 上下文。

**Misc（esp32_misc.c）**

- `device_id`：返回唯一设备 ID，参考实现用 eFuse MAC（`esp_efuse_mac_get_default`）格式化为 12 位十六进制，并对全 0/全 FF 做兜底；
- `uuid`：随机 UUID 即可（SDK 只要求会话内唯一）；
- `info`：形如 `"esp32s3-freertos-idf<chip_rev>"` 的平台自述串；
- `network_available`：查 STA netif 是否拿到 IP；
- `log`：SDK 内部日志转发，注意单行截断（参考实现限 256 字节）。

**vtable 装配与注册**（`convai_platform_esp32.c`）：

```c
static const convai_platform_t g_convai_platform = {
    .abi_version = CONVAI_ABI_VERSION,
    .osal  = { .malloc = esp32_malloc, .free = esp32_free, /* ... 全量填满 */ },
    .netal = { .socket_create = esp32_socket_create, /* ... */ },
    .tlsal = { .tls_create = esp32_tls_create, /* ... */ },
    .misc  = { .log = esp32_log, .device_id = esp32_device_id, /* ... */ },
};

int convai_platform_esp32_init(void) {
    return convai_platform_init(&g_convai_platform);   // 0 = 成功
}
```

> vtable 里每个成员都要赋值，SDK 启动时会调用到全部函数；漏填 = 空指针调用。参考实现另有一个轻量 `platform_factory` 注册器（按名字选平台），是工程组织手段，非 SDK 要求，可简化。

### Step 4 复用 G.711A 编解码

直接复制 [main/sdk/convai_codec_g711a.c](../sdk/convai_codec_g711a.c)（纯 C、ITU-T 标准实现，无平台依赖）：

```c
// 上行：16-bit PCM(planar 排布) → G.711A。channels=2 时 pcm 按 [L 全帧][R 全帧] 平面排布
int convai_g711a_encode(const uint8_t *pcm, size_t pcm_len, int channels,
                        uint8_t *out, size_t out_cap, size_t *out_len);
// 下行：G.711A → 16-bit 小端 PCM（1 字节 → 2 字节）
int convai_g711a_decode(const uint8_t *encoded, size_t enc_len,
                        uint8_t *pcm, size_t pcm_cap, size_t *pcm_len);
```

> **注意上行多声道的含义**：`channels=2` 不是立体声音乐，而是参考实现把「麦克风人声（L）+ 扬声器回采（R，供云端做回声消除 AEC）」两路一起上行。若你的板子没有回采通路，`channels=1` 只上行人声即可（AEC 效果依赖板端硬件）。

### Step 5 实现桥接层（引擎生命周期 + 音频管线）

桥接层是**应用与 SDK 之间唯一的一层胶水**，参考实现 [convai_bridge.c](../sdk/convai_bridge.c) 的职责划分建议照搬：

- 持有唯一的 `convai_engine_t` 句柄与 4 个 SDK 回调；
- 内部再暴露 3 个更简单的回调注册口（`on_status/on_event/on_message`），让 UI 层与 SDK 解耦；
- 拥有**上行采集任务**与**下行播放任务**（生命周期绑定 start/stop）；
- 维护上行统计（sent/dropped）与可注入的 `startup_config`、`device_name`。

#### 5.1 初始化与启动

```c
void convai_bridge_init(void) {
    char config_json[2048];
    bridge_build_config_json(config_json, sizeof(config_json), g_device_name);

    convai_event_handler_t cb = {
        .on_convai_event               = on_sdk_event,   // 连接事件
        .on_convai_conversation_status = on_status,      // 状态机
        .on_convai_audio_data          = on_audio,       // 下行 TTS
        .on_convai_message_data        = on_message,     // 下行文本
    };
    int ret = convai_create(&g_engine, config_json, &cb, NULL);
    // ret != CONVAI_OK 时打日志并保持 g_engine = NULL
}

int convai_bridge_start(void) {
    convai_opt_t opt = {
        .mode     = CONVAI_MODE_WS,
        .agent_id = bridge_get_default_agent_id(),
        .params   = g_startup_config[0] ? g_startup_config          // UI 设置过的
                                        : bridge_get_default_startup_config(),
    };
    int ret = convai_start(g_engine, &opt);
    if (ret == CONVAI_OK) {
        g_started = 1;
        bridge_start_capture();          // 启动上行采集任务
    }
    return ret;
}
```

`stop()` 的顺序很重要：**先置 `g_started = 0` 让采集任务自行退出，再调 `convai_stop()`**；`restart()` = stop → 延时 100ms → start。收到 `DISCONNECTED/FAILED` 事件时只做本地状态复位（置 `g_started = 0`、状态回 IDLE），不要在 SDK 回调里反手调 `convai_stop`。

#### 5.2 上行管线（mic → 云端）

一个 FreeRTOS 任务循环：读 codec → 整理声道 → A-law 编码 → 发送。

```c
static void audio_capture_task(void *arg) {
    while (g_started && g_engine != NULL) {
        int received = codec->read(codec, buf, CAPTURE_BUF_SIZE);  // 20ms@8kHz
        if (received <= 0) { s_frames_dropped++; continue; }

        // TDM 多时隙 → 取有效声道（板级相关！见下方说明）
        //   planar[i] = 人声(mic)        → L
        //   planar[N+i] = 扬声器回采      → R（无回采可省略，channels=1）
        ...
        size_t g711_len = 0;
        convai_g711a_encode((uint8_t *)planar, planar_len, 2, g711_buf, cap, &g711_len);

        convai_audio_frame_info_t info = { .data_type = CONVAI_AUDIO_DATA_TYPE_G711A };
        if (convai_send_audio(g_engine, g711_buf, g711_len, &info) == CONVAI_OK)
            s_frames_sent++;
        else
            s_frames_dropped++;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDeleteWithCaps(NULL);
}
```

> **板级适配点**：参考实现使用 ES7210 的 I2S TDM 4 时隙（保证 MCLK_256 整数分频），实测物理顺序 slot0=MIC1（人声）、slot1=MIC3（回采），slot2/3 丢弃。换板子时**必须用示波器/日志实测声道映射**，否则会出现「有回声/没人声」。任务栈建议放 PSRAM（`xTaskCreateWithCaps(..., MALLOC_CAP_SPIRAM)`），内部 RAM 在 WiFi+LVGL 下很难给出 8 KB 连续块。

#### 5.3 下行管线（云端 TTS → speaker）

SDK 的 `on_convai_audio_data` 回调**不要直接写 I2S**（网络抖动会让播放断续）。参考实现的做法：

1. 回调里：G.711A 解码为 8 kHz mono PCM → 复制为立体声（L=R，匹配双声道功放）→ 写入 **1 MB 环形缓冲**；
2. 独立高优先级播放任务从 ring 取数据喂 codec，用**自适应预缓冲**平滑抖动：
   - 开播门槛 FAST = 500 ms（快速出声）；
   - 若中途把 ring 播空（服务器下发慢），自动升级 FULL = 1.5 s 重新攒缓冲；
3. **打断**：`INTERRUPTED` 状态置 `s_playback_flush = 1`，ring 与到达帧全部丢弃——barge-in 时扬声器立刻安静；
4. **收尾**：`ANSWER_FINISHED` 置 `s_playback_force = 1` 强制播完 ring 尾部（否则短回答攒不够预缓冲门槛，音频滞留拖到下一句）；
5. **背压**：ring 写满时**阻塞写入**（上限 2 s 兜底），让 TCP 背压把服务器数倍速率的音频倒灌压回 1x 实时，避免长回答尾部被丢。

这五个机制是长回答不卡顿、打断即时生效的关键，换平台时建议原样保留思路，参数（ring 大小、预缓冲时长）按内存预算调整。

#### 5.4 状态回调驱动 UI

```c
static void on_status(convai_engine_t e, convai_status_e status, void *ud) {
    g_status = status;
    switch (status) {
        case CONVAI_STATUS_LISTENING:       ui_set(LISTENING); led_on();  break;
        case CONVAI_STATUS_THINKING:        ui_set(THINKING);  break;
        case CONVAI_STATUS_ANSWERING:       ui_set(SPEAKING); reset_prebuffer(); break;
        case CONVAI_STATUS_INTERRUPTED:     s_playback_flush = 1; ui_set(IDLE); break;
        case CONVAI_STATUS_ANSWER_FINISHED: s_playback_force = 1; log_uplink_stats(); break;
        case CONVAI_STATUS_IDLE: default:   ui_set(IDLE);        break;
    }
}
```

**UI 状态只由这一个回调驱动**，按键/其他模块不直接改 UI 状态，保证单一数据源。

### Step 6 应用层接线

参考 `main.c` 的 8 步启动顺序，新 demo 的 `app_main()` 最少要做：

```c
void app_main(void) {
    nvs_flash_init();                    // 1. NVS（WiFi 凭证等）
    board_init();                        // 2. 板级：I2C/codec/LCD/按键（按需）
    wifi_provision();                    // 3. 配网并等连上（SoftAP+网页 最通用）
    sntp_init_sync();                    // 4. SNTP 校时 —— 必须在 SDK 会话之前！
                                         //    TLS 证书有效期校验依赖正确时钟
    sdk_init();                          // 5. 见下
    while (1) {                          // 6. 主循环：按键轮询 + UI tick
        button_poll();
        ui_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

`sdk_init()` 的顺序（见 [sdk_init.c](../sdk/sdk_init.c)）：

```c
esp_err_t sdk_init(void) {
    convai_platform_esp32_register();          // a. 平台 HAL 注册（按名注册）
    platform_factory_init_by_name("esp32");    // b. 实际调 convai_platform_init()
    char dev_id[32];
    if (esp32_device_id(dev_id, sizeof(dev_id)) > 0)
        convai_bridge_set_device_name(dev_id); // c. 注入唯一设备名（在 init 之前）
    convai_bridge_init();                      // d. 创建引擎（不 start）
    return ESP_OK;
}
```

会话默认**不自动启动**，由用户动作触发（参考实现：BOOT 按键短按切换 start/stop），避免误用云端资源。运行中切换音色：改 startup_config + 已连接时调 `convai_update()`（见 [voice/voice_config.c](../voice/voice_config.c)）。

**凭证写入**：编辑 `main/sdk/convai_bridge_defaults.c`（两选一）：

```c
// 方式一（推荐）：API-Key 鉴权
#define BRIDGE_DEFAULT_BOT_ID  "your_agent_id"   // agent_id
// api_key 不走宏 —— 通过 convai_config_file_get("api_key") 注入，
// 见下方「配置文件」；若只想最快跑通，也可以仿照五元组宏的方式硬编码。

// 方式二：Product-Key 五元组（api_key 未配置时回退到此）
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret"
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"   // 有 MAC 注入时仅兜底
```

`DEFAULT_STARTUP_CONFIG` 里改你的人设（`system_messages`）与默认音色（`voice_type`）。

> **配置文件机制**：`convai_config_file.c` 在 ESP32 上是 **stub**（`convai_config_file_get()` 恒返回 NULL，因此永远走硬编码默认值）。如需免编译改配置（如 `api_key`、`server_url`），实现 `convai_config_file_init_path()` 接 NVS/SPIFFS 即可，键名与 GoldieOS 的 `convai.cfg` 一致（`agent_id/api_key/product_id/.../server_url`）。`server_url` 存在时才会出现在 create 配置里，否则 SDK 直连默认云端。

---

## 5. 编译烧录验证

在 ESP-IDF 6.0 终端中：

```powershell
cd examples/my-demo
idf.py set-target esp32s3
idf.py build
idf.py -p <COMx> flash monitor     # 首次烧录建议先 erase-flash
```

成功判据与首次使用说明（配网流程等）见 [esp32s3_开发最佳实践.md](esp32s3_开发最佳实践.md) 第 3–5 章。

上电后的**预期日志时序**（新平台调试时逐条对照）：

```
[convai_hal] registered platform: esp32
[convai_hal] Registering ESP32 platform HAL (ABI 0x0101)
[sdk_init]    device name: AABBCCDDEEFF
[convai_bridge] engine created (v0.1.0)          ← convai_create 成功
（按键启动后）
[convai_bridge] START: agent_id=xxx
[convai_hal]   resolving <host>:443 ...           ← NetAL: DNS
[convai_hal]   connect initiated fd=xx (EINPROGRESS)
[convai_bridge] event: CONNECTED (...)            ← WebSocket 握手完成
[convai_bridge] status: LISTENING                 ← 状态机开始流转
[convai_bridge] capture RX ok: bytes=640 tdm_frames=80   ← 上行通路 OK
[convai_bridge] status: THINKING → ANSWERING
[convai_bridge] downlink write: n8=...            ← 下行通路 OK
[convai_bridge] status: ANSWER_FINISHED (uplink: sent=N dropped=M)
```

---

## 6. 调试观测点与常见问题

### 6.1 关键观测点

| 观测点 | 健康表现 | 异常含义 |
| --- | --- | --- |
| `capture RX ok` | start 后立刻出现 | 迟迟不出现 → I2S/codec 采集配置错（TDM 时隙/MCLK） |
| `uplink: sent/dropped/drop_rate` | dropped≈0 | drop_rate 高 → 任务优先级过低或编码缓冲不足 |
| `downlink` 间隔日志 | 匀速 15~60 ms | >200 ms（SLOW）= 服务端下发慢/网络延迟；<15 ms = 倒灌（正常发生在文本结束后） |
| `playback: ring_used` | 播放期间有涨有落 | 持续顶满 → 播放任务消费不动，检查 codec->write 阻塞 |
| `event: DISCONNECTED` 后本地复位 | sent/dropped 被打印 | 只复位本地状态，等待用户/策略重连 |

### 6.2 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| 链接报 `cJSON_*` multiple-definition | 预编译库与 espressif/cjson 符号冲突，按 Step 2 加 `-Wl,--allow-multiple-definition` |
| `convai_create` 返回 `-18 CONFIG_INCOMPLETE` | 鉴权字段没配：API-Key 与五元组二选一填全 |
| 卡在 TLS 握手 / `-11 TLS` | ① 未 SNTP 校时，证书有效期校验失败；② `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` 太小；③ mbedTLS 未允许外部内存导致 OOM |
| `convai_update` 返回 `-14 INVALID_STATE` | 会话未连接。先保存配置，连接成功后再 update，或留到下次 start 生效 |
| 采集任务创建失败（xTaskCreate 返回失败） | 内部 RAM 不足，任务栈改用 PSRAM（`xTaskCreateWithCaps` + `MALLOC_CAP_SPIRAM`） |
| 回声大 / 识别到设备自己的声音 | 上行把扬声器回采声道也当人声发了；核对 TDM 声道映射（5.2 节） |
| 播放断续、长回答越播越卡 | 下行未做预缓冲/背压；按 5.3 节补齐 ring + 预缓冲 + flush + force 四机制 |
| 看门狗复位（配网页面等待期间） | 关闭 IDLE 任务看门狗检查（sdkconfig.defaults 已给出配置），保留任务级 panic |
| 中文 UI 乱码 | LVGL 默认字体不含中文，需要生成并挂载 CJK 字体（参考 tools/gen_font.js） |

---

## 7. 新平台接入 Checklist

按顺序自查，全部打勾即可跑通最小 demo：

- [ ] `libs/<平台>/libconvai_sdk.a` + `include/convai/` 四头文件就位
- [ ] SDK 封装成组件并链接成功（cJSON 冲突选项已加）
- [ ] 平台适配层 vtable **全字段**赋值，`abi_version = CONVAI_ABI_VERSION`
- [ ] `convai_platform_init()` 在**第一次 `convai_create()` 之前**调用
- [ ] 互斥锁为递归锁；`get_tick_ms` 单调、`get_time_ms` 为 UTC 墙钟
- [ ] NetAL 全链路非阻塞，`EINPROGRESS` 视为成功，send/recv 返回值约定正确
- [ ] TLSAL would-block 映射 `WANT_READ/WRITE`，`handshake_step` 正确回填 want/done
- [ ] `device_id` 返回唯一 ID；`fill_random` 接硬件 TRNG
- [ ] SNTP 校时在 SDK 会话启动之前完成
- [ ] 凭证配置完成（agent_id + api_key，或五元组），`convai_create` 返回 `CONVAI_OK`
- [ ] G.711A 编解码自测通过（1 字节 ↔ 2 字节 PCM）
- [ ] 上行任务：8 kHz 采集、声道映射实测正确、`commit=0` 流式发送
- [ ] 下行：ring 缓冲 + 预缓冲 + `INTERRUPTED` flush + `ANSWER_FINISHED` force + 背压
- [ ] 会话由用户动作触发 start/stop；`DISCONNECTED` 后本地状态正确复位
- [ ] 上行统计 drop_rate ≈ 0，日志时序与第 5 节清单一致

---

## 8. 附录：参考实现文件索引

`examples/szpi-esp32s3/` 中与 SDK 接入直接相关的文件（其余为板级/UI/配网实现）：

| 文件 | 职责 | 接入相关性 |
| --- | --- | --- |
| `main/sdk/sdk_init.c` | 平台注册 → 设备名注入 → 引擎创建 | ★ 必看，入口 |
| `main/sdk/convai_bridge.c` | 引擎生命周期 + 4 回调 + 上/下行任务 | ★ 必看，核心 |
| `main/sdk/convai_bridge.h` | 桥接层对外 API（与 GoldieOS 对齐） | ★ 必看 |
| `main/sdk/convai_bridge_defaults.c` | 凭证宏 + create/start 两份 JSON 拼装 | ★ 必改 |
| `main/sdk/convai_config_file.c` | 配置文件 stub（可接 NVS/SPIFFS） | 可选 |
| `main/sdk/convai_codec_g711a.c` | G.711A 编解码 | 直接复用 |
| `main/sdk/platform/convai_platform_esp32.c` | vtable 装配 + 注册 | ESP32 直接复用 |
| `main/sdk/platform/esp32_osal.c` / `esp32_netal.c` / `esp32_tlsal.c` / `esp32_misc.c` | 四层实现 | ESP32 直接复用 |
| `components/convai_sdk/CMakeLists.txt` | SDK 预编译库组件封装 | 直接复用 |
| `main/audio/audio_codec.h` | 音频驱动抽象（init/read/write/set_volume） | 换板只换实现 |
| `main/audio/audio_codec_lckfb.c` | ES7210/ES8311 板级实现 | 板级参考 |
| `main/button_handler.c` | 按键 → start/stop | 交互参考 |
| `main/voice/voice_config.c` | 音色切换（convai_update） | 可选 |
| `main/main.c` | 8 步启动顺序 | 流程参考 |
| `sdkconfig.defaults` / `partitions.csv` | 关键系统配置 | 直接拷贝 |

> GoldieOS 参考实现（`examples/goldieos/`，覆盖 WS63 与 Windows 模拟器）与 ESP32 侧的桥接层 API 保持逐字节对齐，跨平台问题可互相对照。
