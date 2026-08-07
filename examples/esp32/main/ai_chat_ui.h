/**
 * @file ai_chat_ui.h
 * @brief AI 对话界面 — 纯黑底 + 气泡 + 状态指示
 *
 * 屏幕布局（320×240）：
 *   - 顶部 24px 状态栏（标题 + 状态指示）
 *   - 中间 161px 对话区（圆角气泡，AI 青色左对齐 / 用户深灰蓝右对齐）
 *   - 底部 55px 状态区（动画指示 + 状态文字）
 */

#ifndef AI_CHAT_UI_H
#define AI_CHAT_UI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 对话状态 */
typedef enum {
    CHAT_IDLE,         /**< 空闲，等待唤醒 */
    CHAT_LISTENING,    /**< 正在录音 */
    CHAT_THINKING,     /**< AI 处理中 */
    CHAT_SPEAKING,     /**< AI 回复中 */
    CHAT_VOICE_SELECT  /**< 音色选择面板 */
} chat_state_t;

/**
 * @brief 初始化 AI 对话界面
 */
void ai_chat_ui_init(void);

/**
 * @brief 设置状态并刷新屏幕
 * @param state 新状态
 */
void ai_chat_ui_set_state(chat_state_t state);

/**
 * @brief 获取当前状态
 */
chat_state_t ai_chat_ui_get_state(void);

/**
 * @brief 添加一条对话消息并刷新屏幕
 * @param text   消息文本（ASCII only，最长 127 字符）
 * @param is_user true=用户消息（右对齐），false=AI 消息（左对齐）
 */
void ai_chat_ui_add_message(const char *text, bool is_user);

/**
 * @brief 非阻塞动画轮询（在 while(1) 中每 ~300ms 调用一次）
 *
 * 仅在 CHAT_THINKING 状态下更新动画点并重绘底部状态区。
 * 若需连续动画，外部每 ~300ms 调用一次。
 */
void ai_chat_ui_tick(void);

/**
 * @brief 更新 WiFi 连接状态（显示在顶部状态栏左侧）
 * @param ssid      WiFi SSID
 * @param ip        本机 IP 地址
 * @param connected 是否已连接
 */
void ai_chat_ui_set_connection(const char *ssid, const char *ip, bool connected);

/**
 * @brief 更新云端连接状态（显示在顶部状态栏右侧 Online/Offline）
 *
 * 由 SDK 事件回调驱动：CONVAI_EV_CONNECTED → true，
 * CONVAI_EV_DISCONNECTED / CONVAI_EV_FAILED → false。
 *
 * @param cloud_connected SDK 云端是否已连接
 */
void ai_chat_ui_set_cloud_connection(bool cloud_connected);

/**
 * @brief 打开 / 关闭音色选择面板
 * @param show true=显示面板，false=关闭（不保存）
 */
void ai_chat_ui_show_voice_selector(bool show);

/**
 * @brief 音色选择面板内的导航
 * @return 当前选中的音色索引（0~9）
 */
int  ai_chat_ui_voice_select_next(void);  /* 下一项 */
int  ai_chat_ui_voice_select_prev(void);  /* 上一项 */

/**
 * @brief 获取当前面板选中的音色索引
 */
int  ai_chat_ui_voice_select_get(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_CHAT_UI_H */
