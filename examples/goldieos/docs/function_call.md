# ConvAI Examples Function Call 最佳实践

本文档基于华为 ConvAI SDK，说明如何在 Examples 中实现 Function Call 功能。

---

## 目录

1. [架构概述](#架构概述)
2. [快速开始：新增一个 Function Call](#快速开始新增一个-function-call)
3. [Handler 函数签名](#handler-函数签名)
4. [回复机制](#回复机制)
5. [注册回调](#注册回调)
6. [设置闹钟完整示例](#设置闹钟完整示例)
7. [表情切换示例（set_face）](#表情切换示例set_face)
8. [协议参考](#协议参考)

---

## 架构概述

Function Call 处理分为两层：

- **通用分派框架**（`sdk_integration/convai_func_dispatch.c/.h`）：app 无关。接收 AI 消息、解析 JSON、遍历 calls、按名字查注册表分派 handler、自动构建回复并发送。任何 app 都可复用。
- **业务 handler**（`apps/settings/convai_func_handlers.c/.h`）：settings app 专属。实现具体功能（表情、闹钟、天气），通过 `func_dispatch_register()` 注册到分派框架。

```
Cloud AI Server
    │  WebSocket
    ▼
ConvAI SDK → convai_bridge → func_dispatch_message_cb()   [sdk_integration/通用层]
                                 │
                                 ├─ 解析 type / calls[]
                                 ├─ 遍历 s_registry[] (运行时注册的 name→handler 表)
                                 ├─ 调用匹配的 handler       [apps/settings/业务层]
                                 └─ 自动构建回复 function_call_output 并 send
```

**为什么分两层**：通用层只做"解析+分派+回复"的管道工作，不含任何业务知识；业务层只写 handler 逻辑，不关心消息格式和回复组装。新增 app 时复用通用层，只写自己的 handler。

---

## 快速开始：新增一个 Function Call

### 第一步：编写 handler 函数

在 `apps/settings/convai_func_handlers.c` 中添加：

```c
static int handle_xxx(const char *call_id, cJSON *args_json,
                      char *output_buf, size_t buf_size,
                      const char **output_str)
{
    (void)call_id;

    // 1. 解析必选参数
    cJSON *param = cJSON_GetObjectItem(args_json, "param_name");
    if (!param || !cJSON_IsString(param)) {
        *output_str = "{\"result\":\"error\",\"message\":\"缺少xxx参数\"}";
        return true;
    }

    // 2. 执行业务逻辑（调用设备服务等）

    // 3. 自定义回复（可选，不设置则使用默认回复）
    snprintf(output_buf, buf_size,
             "{\"result\":\"success\",\"message\":\"操作完成\"}");
    *output_str = output_buf;

    return true;
}
```

### 第二步：在 func_handlers_register() 中注册

```c
void func_handlers_register(void)
{
    func_dispatch_init();
    func_dispatch_register("set_face",     handle_emotion);
    func_dispatch_register("set_alarm",    handle_set_alarm);
    func_dispatch_register("get_weather",  handle_get_weather);
    func_dispatch_register("xxx",          handle_xxx);   // ← 新增这一行
}
```

两步完成。分派框架在收到 `response.function_call_arguments.done` 消息时自动完成 JSON 解析、遍历 calls、按名字匹配 handler、构建回复并发送。

---

## Handler 函数签名

handler 类型定义在 `sdk_integration/convai_func_dispatch.h`：

```c
typedef int (*convai_func_handler_t)(
    const char *call_id,      // function call 的唯一 ID
    cJSON *args_json,         // arguments 已解析为 cJSON 对象
    char *output_buf,         // 256 字节栈缓冲区，供 snprintf 使用
    size_t buf_size,          // output_buf 大小
    const char **output_str   // 指向回复 JSON 字符串的指针
);
```

| 参数 | 说明 |
|------|------|
| `call_id` | Function Call 唯一 ID（一般无需使用，分派框架自动对应回复） |
| `args_json` | `arguments` 字段经 `cJSON_Parse` 解析后的对象（handler 不要 free，框架统一释放） |
| `output_buf` | 256 字节栈缓冲区 |
| `buf_size` | 缓冲区大小 |
| `output_str` | 输出指针，指向回复的 JSON 字符串 |

**返回值**：非零表示已处理（回复仍会发送）；0 表示未识别。注意：返回 0 时若 handler 已设置 `*output_str` 为 error JSON，回复仍会发送——返回值只影响"未注册函数"的日志，不影响回复发送。

---

## 回复机制

### 默认回复

handler 不修改 `*output_str` 时，主循环自动使用默认回复：

```json
{"result": "success", "message": "成功"}
```

### 自定义回复

```c
// 方式一：静态字符串（无动态数据时推荐，零开销）
*output_str = "{\"result\":\"error\",\"message\":\"参数无效\"}";

// 方式二：snprintf 动态构造（需要嵌入变量时使用）
snprintf(output_buf, buf_size,
         "{\"result\":\"success\",\"message\":\"已设置\",\"index\":%d}", index);
*output_str = output_buf;
```

> **注意**：`output_buf` 大小为 256 字节。snprintf 不会溢出，但超长内容会被截断。

### 主循环自动构建

handler 只需设置 `*output_str`，主循环自动构建完整的回复 JSON：

```json
{
  "type": "conversation.items.create",
  "items": [
    {
      "type": "function_call_output",
      "call_id": "<自动填入>",
      "output": "<*output_str 指向的内容>"
    }
  ]
}
```

---

## 注册回调

分派框架提供两个接口（`sdk_integration/convai_func_dispatch.h`）：

```c
void func_dispatch_init(void);                                      // 挂消息回调到 bridge
int  func_dispatch_register(const char *name, convai_func_handler_t handler);  // 注册 handler
void func_dispatch_unregister(void);                                // 注销（清空注册表 + 摘回调）
```

settings app 在 `convai_func_handlers.c` 里封装了 `func_handlers_register/unregister`，在生命周期里成对调用：

| 时机 | 调用 |
|------|------|
| `goldie_app_run` | `func_handlers_register()`（init + 注册所有 handler） |
| `goldie_app_suspend` | `func_handlers_unregister()` |
| `goldie_app_resume` | `func_handlers_register()` |
| `goldie_app_exit` | `func_handlers_unregister()` |

`func_dispatch_register` 同名注册会覆盖（last wins），最多 16 个（`FUNC_DISPATCH_MAX`）。注册表在 `unregister` 时清空，便于 app 反复进出。

---

## 设置闹钟完整示例

### 场景

用户说「帮我设置下午4点的开会闹钟」，Agent 下发 `set_alarm`：

```json
{
  "type": "response.function_call_arguments.done",
  "calls": [
    {
      "call_id": "call_alarm_001",
      "name": "set_alarm",
      "arguments": "{\"time\":\"16:00\",\"label\":\"开会\",\"repeat\":\"weekdays\"}"
    }
  ]
}
```

### 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| `time` | string | 闹钟时间 `HH:MM`（标准格式，优先解析） |
| `hour` / `minute` | int | 兼容旧格式：未提供 `time` 时用这两个数字字段 |
| `label` | string | 闹钟标签（当前仅日志记录，AlarmInfo 无此字段） |
| `repeat` | string | `none`（一次性）、`daily`（每天）、`weekdays`（工作日）；未知值默认每天 |
| `weekdays` | bool[] | 可选，按星期几设置的布尔数组（优先级高于 `repeat`） |
| `enabled` | bool | 可选，是否启用，默认 `true` |

### Handler 实现

```c
static int handle_set_alarm(const char *call_id, cJSON *args_json,
                            char *output_buf, size_t buf_size,
                            const char **output_str)
{
    (void)call_id;

    /* ---- 解析 time 字段："HH:MM" ---- */
    int hour = 0, minute = 0;
    cJSON *time_item = cJSON_GetObjectItem(args_json, "time");
    if (time_item && cJSON_IsString(time_item)) {
        const char *time_str = time_item->valuestring;
        if (strlen(time_str) == 5 && time_str[2] == ':') {
            hour   = (time_str[0] - '0') * 10 + (time_str[1] - '0');
            minute = (time_str[3] - '0') * 10 + (time_str[4] - '0');
        }
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        *output_str = "{\"result\":\"error\",\"message\":\"时间格式无效\"}";
        return true;
    }

    /* ---- 获取闹钟服务 ---- */
    AlarmService *alarm_svc = (AlarmService*)get_service(ALARM_SERVICE_INDEX);
    if (!alarm_svc) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟服务不可用\"}";
        return true;
    }

    /* ---- 构造闹钟对象 ---- */
    AlarmInfo alarm;
    memset(&alarm, 0, sizeof(AlarmInfo));
    alarm.m_hour     = (char)hour;
    alarm.m_min      = (char)minute;
    alarm.enabled    = true;
    alarm.ring_index = 0;

    /* ---- 解析 repeat ---- */
    cJSON *repeat_item = cJSON_GetObjectItem(args_json, "repeat");
    if (repeat_item && cJSON_IsString(repeat_item)) {
        const char *repeat = repeat_item->valuestring;
        if (strcmp(repeat, "daily") == 0)
            for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
        else if (strcmp(repeat, "weekdays") == 0)
            for (int w = 0; w < 5; w++) alarm.weekdays[w] = true;
    } else {
        for (int w = 0; w < 7; w++) alarm.weekdays[w] = true;
    }

    /* ---- 添加闹钟 ---- */
    int ret = alarm_svc->add_alarm(&alarm);
    if (ret >= 0) {
        snprintf(output_buf, buf_size,
                 "{\"result\":\"success\",\"message\":\"闹钟已设置\",\"index\":%d}", ret);
        *output_str = output_buf;
    } else if (ret == -2) {
        *output_str = "{\"result\":\"error\",\"message\":\"闹钟已满,最多10个\"}";
    } else {
        *output_str = "{\"result\":\"error\",\"message\":\"添加失败\"}";
    }

    return true;
}
```

### 注册到分派框架

在 `convai_func_handlers.c` 的 `func_handlers_register()` 中注册：

```c
void func_handlers_register(void)
{
    func_dispatch_init();
    func_dispatch_register("set_alarm",    handle_set_alarm);
    func_dispatch_register("get_weather",  handle_get_weather);
    /* ... 其它 handler ... */
}
```

---

## 表情切换示例（set_face）

AI 在对话中下发 `set_face` 切换设备表情页的显示表情。handler 不直接操作 UI，而是调 `talk_page_set_emotion()` 设状态，由 talk_page 模块的动画线程在下帧渲染时应用。

```c
static int handle_emotion(const char *call_id, cJSON *args_json,
                          char *output_buf, size_t buf_size,
                          const char **output_str)
{
    (void)call_id;
    cJSON *emotion_item = cJSON_GetObjectItem(args_json, "face_expression");
    if (!emotion_item || !cJSON_IsString(emotion_item)) {
        *output_str = "{\"result\":\"error\",\"message\":\"missing face_expression\"}";
        return false;
    }

    const char *emotion = emotion_item->valuestring;
    int new_emotion;
    if      (strcmp(emotion, "neutral") == 0) new_emotion = EMOTION_NEUTRAL;
    else if (strcmp(emotion, "happy")   == 0) new_emotion = EMOTION_HAPPY;
    else if (strcmp(emotion, "angry")   == 0) new_emotion = EMOTION_ANGRY;
    else if (strcmp(emotion, "sad")     == 0) new_emotion = EMOTION_SAD;
    else if (strcmp(emotion, "doubt")   == 0) new_emotion = EMOTION_DOUBT;
    else {
        /* 不支持的 emotion 返回 error，让后端知道端侧不支持（不静默落 neutral） */
        snprintf(output_buf, buf_size,
                 "{\"result\":\"error\",\"message\":\"unsupported emotion: %s\"}", emotion);
        *output_str = output_buf;
        return false;
    }
    talk_page_set_emotion(new_emotion);   /* 动画线程下帧应用 */
    return true;
}
```

**设计要点**：
- handler 只做"参数解析 + 状态设置"，UI 渲染由 talk_page 动画线程异步完成（解耦：functioncall 处理在 bridge 线程，UI 在动画线程）。
- 不支持的 emotion 值返回 error（`return false` + error JSON），不静默降级 neutral——便于后端感知端侧能力。
- 支持的 5 种表情：`neutral` / `happy` / `angry` / `sad` / `doubt`。

---

## 协议参考

### 消息流向

| 消息 | 方向 | 说明 |
|------|------|------|
| `response.function_call_arguments.done` | Agent → Examples | 携带调用参数 |
| `conversation.items.create` | Examples → Agent | 返回执行结果（主循环自动构建） |

### AI 下发的 JSON

```json
{
  "type": "response.function_call_arguments.done",
  "calls": [
    {
      "call_id": "call_xxx",
      "name": "set_alarm",
      "arguments": "{\"time\":\"16:00\",\"label\":\"开会\",\"repeat\":\"none\"}"
    }
  ]
}
```

注意：`arguments` 是 JSON 字符串（二次编码），由主循环调用 `cJSON_Parse` 解析后传给 handler。

### 回复 JSON（主循环自动构建）

```json
{
  "type": "conversation.items.create",
  "items": [
    {
      "type": "function_call_output",
      "call_id": "call_xxx",
      "output": "<handler 设置的 *output_str>"
    }
  ]
}
```

### 已注册的 Function Calls

| name | 功能 | 参数 | 实现位置 |
|------|------|------|------|
| `set_face` | 切换表情页表情 | `{"face_expression":"happy"}` （neutral/happy/angry/sad/doubt，不支持返回 error） | `convai_func_handlers.c` → `talk_page_set_emotion()` |
| `set_alarm` | 设置闹钟 | `{"time":"16:00","label":"开会","repeat":"weekdays"}` | `convai_func_handlers.c` → AlarmService |
| `get_weather` | 查询天气 | `{"location":"深圳"}` | `convai_func_handlers.c`（占位实现，无 HTTP 能力） |

---

> **版本信息：** 本文档基于华为 ConvAI SDK 26.8.0 版本编写。
