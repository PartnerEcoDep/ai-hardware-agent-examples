/**
 * @file ai_chat_ui.c
 * @brief AI 对话界面 — 实现
 *
 * 基于 lcd_ui 模块（8×16 ASCII 字体 + 2x 缩放 + 圆角矩形）。
 * 消息环形缓冲最多 8 条，屏幕显示最近 3~4 条。
 */

#include "ai_chat_ui.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lcd_ui.h"
#include "voice_config.h"

static const char *TAG = "ai_chat_ui";

/* ---- 颜色定义 ---- */
#define COLOR_BG             0x0000  /* 纯黑 */
#define COLOR_WHITE          0xFFFF
#define COLOR_CYAN           0x07FF  /* AI 气泡 */
#define COLOR_USER_BUBBLE    0x18E3  /* 用户气泡（深灰蓝） */
#define COLOR_GRAY           0x8410
#define COLOR_GREEN          0x07E0
#define COLOR_RED            0xF800

/* ---- 布局常量 ---- */
#define STATUS_BAR_Y         0
#define STATUS_BAR_H         24
#define CHAT_AREA_TOP        24
#define CHAT_AREA_BOTTOM     184
#define STATUS_AREA_Y        188
#define BUBBLE_MARGIN        8
#define BUBBLE_PAD_X         6
#define BUBBLE_PAD_Y         4
#define BUBBLE_R             6
#define AI_MAX_CHARS         30
#define USER_MAX_CHARS       28
#define MSG_BUF_SIZE         8
#define MSG_MAX_LEN          128
#define MAX_VISIBLE_MSGS     4

/* ---- 消息缓冲 ---- */
typedef struct {
    char text[MSG_MAX_LEN];
    bool is_user;
} chat_msg_t;

static chat_msg_t s_msg_buf[MSG_BUF_SIZE];
static int s_msg_head = 0;       /* 最旧消息索引 */
static int s_msg_count = 0;      /* 缓冲中消息数 */
static chat_state_t s_state = CHAT_IDLE;
static int s_anim_frame = 0;     /* 动画帧计数 */

/* ---- 连接状态 ---- */
static char s_ssid[32] = "";
static char s_ip[32] = "";
static bool s_connected = false;
static bool s_cloud_connected = false;

/* ---- 音色选择面板 ---- */
static bool s_voice_sel_open = false;
static int  s_voice_sel_idx = 0;    /* 当前高亮音色 0~9 */

/* ---- 内部函数声明 ---- */
static void render_all(void);
static int wrap_text(const char *text, int max_chars,
                     char lines[][32], int max_lines);
static void draw_bubble(int x, int y, const char *text, bool is_user,
                         int *height_out);
static void draw_status_bar(void);
static void draw_status_area(void);
static void draw_idle_capsules(void);
static void draw_listening_capsules(void);
static void draw_speaking_waves(void);
static void draw_voice_selector(void);

/* ===================================================================
 *  公开 API
 * =================================================================== */

void ai_chat_ui_init(void) {
    lcd_ui_init();
    s_msg_head = 0;
    s_msg_count = 0;
    s_state = CHAT_IDLE;
    s_anim_frame = 0;
    ESP_LOGI(TAG, "AI Chat UI initialized");
    render_all();
}

void ai_chat_ui_set_state(chat_state_t state) {
    s_state = state;
    s_anim_frame = 0;
    render_all();
}

chat_state_t ai_chat_ui_get_state(void) {
    return s_state;
}

void ai_chat_ui_add_message(const char *text, bool is_user) {
    if (text == NULL) return;

    /* 环形写入 */
    int idx = (s_msg_head + s_msg_count) % MSG_BUF_SIZE;
    strncpy(s_msg_buf[idx].text, text, MSG_MAX_LEN - 1);
    s_msg_buf[idx].text[MSG_MAX_LEN - 1] = '\0';
    s_msg_buf[idx].is_user = is_user;

    if (s_msg_count < MSG_BUF_SIZE) {
        s_msg_count++;
    } else {
        s_msg_head = (s_msg_head + 1) % MSG_BUF_SIZE;
    }

    render_all();
}

void ai_chat_ui_tick(void) {
    if (s_state == CHAT_VOICE_SELECT) {
        s_anim_frame++;
        draw_voice_selector();
        lcd_ui_flush();
        return;
    }
    if (s_state == CHAT_IDLE) {
        s_anim_frame++;
        draw_status_area();           /* 底部状态 */
        draw_idle_capsules();         /* 中间胶囊动画 */
        lcd_ui_flush();
        return;
    }
    if (s_state == CHAT_LISTENING) {
        s_anim_frame++;
        draw_status_area();
        draw_listening_capsules();
        lcd_ui_flush();
        return;
    }
    if (s_state == CHAT_SPEAKING) {
        s_anim_frame++;
        draw_status_area();
        draw_speaking_waves();
        lcd_ui_flush();
        return;
    }
    if (s_state != CHAT_THINKING) return;
    s_anim_frame++;
    draw_status_area();
    lcd_ui_flush();
}

void ai_chat_ui_set_connection(const char *ssid, const char *ip, bool connected) {
    if (ssid) strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    if (ip)   strncpy(s_ip,   ip,   sizeof(s_ip)   - 1);
    s_connected = connected;
    render_all();
}

void ai_chat_ui_set_cloud_connection(bool cloud_connected) {
    s_cloud_connected = cloud_connected;
    render_all();
}

/* ===================================================================
 *  内部实现
 * =================================================================== */

static void render_all(void) {
    lcd_ui_clear(COLOR_BG);

    /* 音色选择面板覆盖整个画面 */
    if (s_state == CHAT_VOICE_SELECT) {
        draw_voice_selector();
        lcd_ui_flush();
        return;
    }

    draw_status_bar();

    /* 从底部向上渲染消息，最多 MAX_VISIBLE_MSGS 条 */
    int cur_y = CHAT_AREA_BOTTOM;
    int shown = 0;

    for (int i = s_msg_count - 1; i >= 0 && shown < MAX_VISIBLE_MSGS; i--) {
        int idx = (s_msg_head + i) % MSG_BUF_SIZE;

        /* 先计算气泡尺寸 */
        char lines[8][32];
        int max_chars = s_msg_buf[idx].is_user ? USER_MAX_CHARS : AI_MAX_CHARS;
        int line_count = wrap_text(s_msg_buf[idx].text, max_chars, lines, 8);

        /* 计算气泡宽度（取最宽行） */
        int bubble_w = 0;
        for (int ln = 0; ln < line_count; ln++) {
            int lw = (int)strlen(lines[ln]) * 8 + BUBBLE_PAD_X * 2;
            if (lw > bubble_w) bubble_w = lw;
        }

        int bubble_h = line_count * 16 + BUBBLE_PAD_Y * 2;
        int total_h = bubble_h + 4; /* 含间距 */

        cur_y -= total_h;
        if (cur_y < CHAT_AREA_TOP) break;

        int bubble_x;
        if (s_msg_buf[idx].is_user) {
            bubble_x = LCD_UI_WIDTH - BUBBLE_MARGIN - bubble_w;
        } else {
            bubble_x = BUBBLE_MARGIN;
        }

        int dummy;
        draw_bubble(bubble_x, cur_y, s_msg_buf[idx].text,
                    s_msg_buf[idx].is_user, &dummy);
        shown++;
    }

    /* 无消息时，根据状态绘制中心动画 */
    if (s_msg_count == 0) {
        switch (s_state) {
        case CHAT_IDLE:
            draw_idle_capsules();
            break;
        case CHAT_LISTENING:
            draw_listening_capsules();
            break;
        case CHAT_SPEAKING:
            draw_speaking_waves();
            break;
        default:
            break;
        }
    }

    draw_status_area();

    /* 右下角小球 — 长按入口提示，所有界面（面板除外） */
    if (s_state != CHAT_VOICE_SELECT) {
        lcd_ui_draw_rounded_rect(LCD_UI_WIDTH - 16, STATUS_AREA_Y + 6,
                                 12, 12, 6, 0xFFC0);
    }

    lcd_ui_flush();
}

/* ---- 文本换行 ---- */
static int wrap_text(const char *text, int max_chars,
                     char lines[][32], int max_lines) {
    int line_count = 0;
    const char *p = text;

    while (*p && line_count < max_lines) {
        /* 跳过行首空格 */
        while (*p == ' ') p++;
        if (*p == '\0') break;

        const char *start = p;
        int len = 0;

        /* 收集该行字符 */
        while (*p && len < max_chars) {
            if (*p == '\n') { p++; break; }
            p++;
            len++;
        }

        /* 回退到词边界（空格处），避免截断单词 */
        if (*p && *p != ' ' && *p != '\n' && len >= max_chars) {
            const char *back = p;
            while (back > start && *back != ' ') back--;
            if (back > start) {
                p = back + 1;
                len = (int)(back - start);
            }
        }

        int copy_len = (len < 31) ? len : 31;
        memcpy(lines[line_count], start, copy_len);
        lines[line_count][copy_len] = '\0';
        line_count++;
    }

    return line_count;
}

/* ---- 绘制单个气泡 ---- */
static void draw_bubble(int x, int y, const char *text, bool is_user,
                         int *height_out) {
    char lines[8][32];
    int max_chars = is_user ? USER_MAX_CHARS : AI_MAX_CHARS;
    int line_count = wrap_text(text, max_chars, lines, 8);

    int bubble_w = 0;
    for (int i = 0; i < line_count; i++) {
        int lw = (int)strlen(lines[i]) * 8 + BUBBLE_PAD_X * 2;
        if (lw > bubble_w) bubble_w = lw;
    }
    int bubble_h = line_count * 16 + BUBBLE_PAD_Y * 2;

    uint16_t bg = is_user ? COLOR_USER_BUBBLE : COLOR_CYAN;

    /* 圆角矩形背景 */
    lcd_ui_draw_rounded_rect(x, y, bubble_w, bubble_h, BUBBLE_R, bg);

    /* 文字（青色气泡用黑字更易读，深灰蓝气泡用白字） */
    uint16_t fg = is_user ? COLOR_WHITE : COLOR_BG;
    for (int i = 0; i < line_count; i++) {
        lcd_ui_draw_string(x + BUBBLE_PAD_X,
                           y + BUBBLE_PAD_Y + i * 16,
                           lines[i], fg, bg);
    }

    if (height_out) *height_out = bubble_h + 4;
}

/* ---- 顶部状态栏 ---- */
static void draw_status_bar(void) {
    /* 细线分隔 */
    lcd_ui_draw_rect(0, STATUS_BAR_H - 1, LCD_UI_WIDTH, 1, COLOR_GRAY);

    /* 左侧：WiFi 信号 + SSID */
    if (s_connected && s_ssid[0]) {
        /* 信号图标：4 条竖线（递增高度的矩形） */
        int sig_x = 8;
        int sig_base = 18;
        for (int i = 0; i < 4; i++) {
            int bar_h = 4 + i * 3;  /* 4, 7, 10, 13 */
            int bar_y = sig_base - bar_h;
            lcd_ui_draw_rect(sig_x + i * 4, bar_y, 3, bar_h, COLOR_GREEN);
        }
        lcd_ui_draw_string(sig_x + 20, 4, s_ssid, COLOR_WHITE, COLOR_BG);
    } else {
        /* WiFi 未连接 */
        lcd_ui_draw_string(8, 4, "WiFi: --", COLOR_GRAY, COLOR_BG);
    }

    /* 右侧：云端连接状态（由 SDK 事件驱动） */
    int dot_x = LCD_UI_WIDTH - 66;
    int dot_y = 8;
    int dot_sz = 6;

    if (s_cloud_connected) {
        lcd_ui_draw_rect(dot_x, dot_y, dot_sz, dot_sz, COLOR_GREEN);
        lcd_ui_draw_string(dot_x + 10, 4, "Online", COLOR_GREEN, COLOR_BG);
    } else {
        lcd_ui_draw_rect(dot_x, dot_y, dot_sz, dot_sz, COLOR_RED);
        lcd_ui_draw_string(dot_x + 10, 4, "Offline", COLOR_RED, COLOR_BG);
    }
}

/* ---- 底部状态区 ---- */
static void draw_status_area(void) {
    /* 分隔线 */
    lcd_ui_draw_rect(0, STATUS_AREA_Y, LCD_UI_WIDTH, 1, COLOR_GRAY);

    switch (s_state) {
    case CHAT_IDLE: {
        /* 绿色圆点 + Ready */
        int cx = LCD_UI_WIDTH / 2;
        lcd_ui_draw_rect(cx - 3, STATUS_AREA_Y + 12, 6, 6, COLOR_GREEN);
        lcd_ui_center_text(STATUS_AREA_Y + 28, "Ready",
                           COLOR_GREEN, COLOR_BG);
        break;
    }
    case CHAT_LISTENING: {
        /* 脉冲圆点（红色） + Listening... */
        int cx = LCD_UI_WIDTH / 2;
        int dot_sz = (s_anim_frame % 2 == 0) ? 8 : 6;
        lcd_ui_draw_rect(cx - dot_sz/2, STATUS_AREA_Y + 12 - dot_sz/2 + 3,
                         dot_sz, dot_sz, COLOR_RED);
        lcd_ui_center_text(STATUS_AREA_Y + 28, "Listening...",
                           COLOR_WHITE, COLOR_BG);
        break;
    }
    case CHAT_THINKING: {
        /* 三个动画点 */
        int cx = LCD_UI_WIDTH / 2;
        int dots = s_anim_frame % 4;
        char dot_str[8];
        snprintf(dot_str, sizeof(dot_str), "%.*s", dots, "....");
        for (int i = 0; i < 3; i++) {
            uint16_t c = (i < dots) ? COLOR_CYAN : COLOR_GRAY;
            int dx = cx - 14 + i * 14;
            lcd_ui_draw_rect(dx, STATUS_AREA_Y + 12, 6, 6, c);
        }
        lcd_ui_center_text(STATUS_AREA_Y + 28, "Thinking...",
                           COLOR_GRAY, COLOR_BG);
        break;
    }
    case CHAT_SPEAKING: {
        /* 青色圆点 + Speaking... */
        int cx = LCD_UI_WIDTH / 2;
        lcd_ui_draw_rect(cx - 3, STATUS_AREA_Y + 12, 6, 6, COLOR_CYAN);
        lcd_ui_center_text(STATUS_AREA_Y + 28, "Speaking...",
                           COLOR_CYAN, COLOR_BG);
        break;
    }
    case CHAT_VOICE_SELECT:
        break;  /* 面板不经过此分支 */
    }
}

/* ---- 聆听波动胶囊 ---- */

static void draw_listening_capsules(void) {
    int mid_y = (CHAT_AREA_TOP + CHAT_AREA_BOTTOM) / 2;
    int capsule_w = 14;
    int capsule_r = 7;
    int spacing = 20;
    int base_h = 12;
    int amplitude = 28;
    float speed = 0.18f;

    int total_w = 3 * capsule_w + 2 * spacing;
    int start_x = (LCD_UI_WIDTH - total_w) / 2;

    for (int i = 0; i < 3; i++) {
        float phase = (float)i * 2.094f;
        float val = sinf((float)s_anim_frame * speed + phase);
        /* 不使用绝对值，让胶囊上下波动交替 */
        int h = base_h + (int)(amplitude * val);
        if (h < 4) h = 4;
        if (h > 40) h = 40;

        int x = start_x + i * (capsule_w + spacing);
        int y = mid_y - h / 2;

        lcd_ui_draw_rounded_rect(x, y, capsule_w, h, capsule_r, COLOR_RED);
    }
}

/* ---- AI回答波浪 ---- */

static void draw_speaking_waves(void) {
    int mid_y = (CHAT_AREA_TOP + CHAT_AREA_BOTTOM) / 2;
    int bar_count = 13;
    int bar_w = 6;
    int bar_gap = 3;
    int max_bar_h = 36;
    int min_bar_h = 4;
    int amplitude = (max_bar_h - min_bar_h) / 2;
    int base_h = min_bar_h + amplitude;  /* 16 + 2 = 居中基准 */

    float speed = 0.15f;

    int total_w = bar_count * (bar_w + bar_gap) - bar_gap;
    int start_x = (LCD_UI_WIDTH - total_w) / 2;

    for (int i = 0; i < bar_count; i++) {
        /* 每根柱子不同相位，波浪从左向右滚动 */
        float phase = (float)i * 0.55f;
        float val = sinf((float)s_anim_frame * speed + phase);
        int h = base_h + (int)((float)amplitude * val);
        if (h < min_bar_h) h = min_bar_h;

        int x = start_x + i * (bar_w + bar_gap);
        int y = mid_y - h / 2;

        /* 按高度映射颜色：低=深蓝，高=青 */
        uint16_t color;
        if (val > 0.3f) {
            color = COLOR_CYAN;
        } else if (val > -0.3f) {
            color = 0x05BB;  /* 中亮蓝 RGB565(0,180,220) */
        } else {
            color = 0x1BD9;  /* 深蓝   RGB565(30,120,200) */
        }

        lcd_ui_draw_rect(x, y, bar_w, h, color);
    }
}

/* ---- 空闲胶囊动画 ---- */

static void draw_idle_capsules(void) {
    int mid_y = (CHAT_AREA_TOP + CHAT_AREA_BOTTOM) / 2;
    int capsule_w = 14;
    int capsule_r = 7;
    int spacing = 20;
    int base_h = 12;
    int amplitude = 28;
    float speed = 0.12f;

    int total_w = 3 * capsule_w + 2 * spacing;
    int start_x = (LCD_UI_WIDTH - total_w) / 2;

    for (int i = 0; i < 3; i++) {
        float phase = (float)i * 2.094f;
        float val = sinf((float)s_anim_frame * speed + phase);
        int h = base_h + (int)(amplitude * fabsf(val));
        if (h < 4) h = 4;

        int x = start_x + i * (capsule_w + spacing);
        int y = mid_y - h / 2;

        lcd_ui_draw_rounded_rect(x, y, capsule_w, h, capsule_r, COLOR_CYAN);
    }
}

/* ===================================================================
 *  音色选择面板
 * =================================================================== */

/* ---- 面板布局 ---- */
#define VOICE_CARD_X     36
#define VOICE_CARD_Y     28
#define VOICE_CARD_W     248
#define VOICE_CARD_H     170
#define VOICE_CARD_R     12
#define VOICE_COL_GAP    12   /* 两列间距 */
#define VOICE_ITEM_H     18   /* 每项高度 */
#define VOICE_SLOT_CNT   3    /* 可见槽位数 */

#define VOICE_BG         0x3186  /* 深蓝灰卡片底 */
#define VOICE_BORDER     0x5AEB
#define VOICE_SEP        0x39C7
#define VOICE_TEXT        0xFFFF  /* 白色 */
#define VOICE_HL_TEXT     0xFFE0  /* 亮黄 */
#define VOICE_DIM_TEXT    0x632C  /* 暗灰 */
#define VOICE_LABEL_TEXT  0xAD55  /* 淡绿（标签） */

static void draw_voice_selector(void) {
    const voice_entry_t *voices = voice_config_get_list();
    int total = voice_config_count();

    /* 半透明遮罩 + 卡片背景 */
    lcd_ui_draw_rect(0, 0, LCD_UI_WIDTH, STATUS_BAR_H, COLOR_BG);
    lcd_ui_draw_rounded_rect(VOICE_CARD_X, VOICE_CARD_Y,
                             VOICE_CARD_W, VOICE_CARD_H,
                             VOICE_CARD_R, VOICE_BG);

    /* 标题 */
    lcd_ui_draw_string(VOICE_CARD_X + 88, VOICE_CARD_Y + 8,
                       "voice-select", VOICE_LABEL_TEXT, VOICE_BG);

    /* 分隔线 */
    lcd_ui_draw_rect(VOICE_CARD_X + 12, VOICE_CARD_Y + 26,
                     VOICE_CARD_W - 24, 1, VOICE_SEP);

    /* 列标签 */
    lcd_ui_draw_string(VOICE_CARD_X + 28, VOICE_CARD_Y + 34,
                       "Gender", VOICE_LABEL_TEXT, VOICE_BG);
    lcd_ui_draw_string(VOICE_CARD_X + 148, VOICE_CARD_Y + 34,
                       "Timbre", VOICE_LABEL_TEXT, VOICE_BG);

    /* 对话框内绘制两个垂直拨盘 */
    int gx = VOICE_CARD_X + 16;
    int vx = VOICE_CARD_X + 132;
    int base_y = VOICE_CARD_Y + 56;

    for (int slot = 0; slot < VOICE_SLOT_CNT; slot++) {
        int idx = s_voice_sel_idx - 1 + slot; /* 上一项 / 当前 / 下一项 */
        int y  = base_y + slot * (VOICE_ITEM_H + 6);

        /* ---- 左列：性别 ---- */
        bool is_female = (s_voice_sel_idx >= 0 && s_voice_sel_idx < 4);
        const char *gender_text;
        uint16_t gender_color;

        if (idx == s_voice_sel_idx) {        /* 当前槽 → 显示当前性别 */
            gender_text = is_female ? "Female" : "Male";
            gender_color = VOICE_HL_TEXT;
        } else {                             /* 上下槽 → 反色显示 */
            gender_text = is_female ? "Male" : "Female";
            gender_color = VOICE_DIM_TEXT;
        }

        lcd_ui_draw_string(gx, y, gender_text, gender_color, VOICE_BG);

        /* ---- 右列：音色 ---- */
        if (idx >= 0 && idx < total) {
            uint16_t vc = (idx == s_voice_sel_idx) ? VOICE_HL_TEXT : VOICE_DIM_TEXT;
            lcd_ui_draw_string(vx, y, voices[idx].name, vc, VOICE_BG);
        }
    }

    /* 滚动条（右侧细线） */
    int bar_x = VOICE_CARD_X + VOICE_CARD_W - 6;
    int bar_y = VOICE_CARD_Y + 34;
    int bar_h = VOICE_CARD_H - 44;
    lcd_ui_draw_rect(bar_x, bar_y, 2, bar_h, VOICE_BORDER);

    int thumb_h = bar_h * 3 / total;
    int thumb_y = bar_y + (bar_h - thumb_h) * s_voice_sel_idx / (total > 1 ? total - 1 : 1);
    lcd_ui_draw_rect(bar_x, thumb_y, 2, thumb_h, VOICE_LABEL_TEXT);

    /* 底部提示 */
    lcd_ui_draw_string(VOICE_CARD_X + 16, VOICE_CARD_Y + VOICE_CARD_H - 18,
                       "short:next  long:OK", VOICE_DIM_TEXT, VOICE_BG);

    /* 右下角小球 */
    int ball_x = LCD_UI_WIDTH - 16;
    int ball_y = LCD_UI_HEIGHT - 18;
    lcd_ui_draw_rounded_rect(ball_x, ball_y, 12, 12, 6, 0xFFC0);
}

/* ---- 公开 API ---- */

void ai_chat_ui_show_voice_selector(bool show) {
    s_voice_sel_open = show;
    if (show) {
        s_voice_sel_idx = voice_config_get();  /* 初始选中 NVS 值 */
        s_state = CHAT_VOICE_SELECT;
    } else {
        s_state = CHAT_IDLE;
    }
    render_all();
}

int ai_chat_ui_voice_select_next(void) {
    int n = voice_config_count();
    s_voice_sel_idx = (s_voice_sel_idx + 1) % n;
    render_all();
    return s_voice_sel_idx;
}

int ai_chat_ui_voice_select_prev(void) {
    int n = voice_config_count();
    s_voice_sel_idx = (s_voice_sel_idx - 1 + n) % n;
    render_all();
    return s_voice_sel_idx;
}

int ai_chat_ui_voice_select_get(void) {
    return s_voice_sel_idx;
}
