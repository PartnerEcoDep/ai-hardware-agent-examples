#include "main_ui.h"

extern "C" {
#include "goldie_osal.h"
#include "service_manager.h"
#include "app_manager.h"
#include "wifi_service.h"
#include "audio_service.h"
#include "convai_bridge.h"
#include "convai/convai_api.h"
#include "cJSON.h"
#include "alarm_service.h"
#ifdef SUPPORT_SLE
#include "sle_sdp_service.h"
#include "platform/ws63/sle_drv.h"
#endif
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convai_image_helper.h"

#include "rgb16_selected_32_32.h"
#include "rgb16_avatar_male_152_136.h"
#include "rgb16_avatar_female_152_136.h"
#include "rgb16_keyboard_key_w23_96_32.h"
#include "rgb16_keyboard_key_w23_144_32.h"
#include "rgb16_keyboard_keys_48_32.h"
#include "goldie_thread.h"

#include "app_icon.h"

static convai_image_state_t g_image_state;

static uint16_t* avatar_pic_list[] = {
    (uint16_t*)rgb16_avatar_female_152_136,
    (uint16_t*)rgb16_avatar_male_152_136
};

/* Uncomment to select voice type */
#define CONVAI_USE_MINIMAX_VOICE   /* MiniMax voices */
// #define CONVAI_USE_OLD_VOICE       /* Old style voices */

#if defined(CONVAI_USE_MINIMAX_VOICE)
/* MiniMax voices */
static const char* voice_list_female[] = {
    "    温柔少女",
    "    害羞女孩",
    "    热心女孩",
    "    花甲奶奶"
};

static const char* voice_list_male[] = {
    "    温润男声",
    "    搞笑大爷",
    "    嘴硬竹马",
    "    邻家弟弟",
    "    憨憨萌兽",
    "    机械战甲"
};
#elif defined(CONVAI_USE_OLD_VOICE)
/* Old style voices */
static const char* voice_list_female[] = {
    "    迷人女友",
    "    温柔姐姐"
};

static const char* voice_list_male[] = {
    "    儒雅大叔",
    "    北京小爷",
    "    四川小伙"
};
#else
/* X4 voices (default) */
static const char* voice_list_female[] = {
    "    小露",
    "    许久"
};

static const char* voice_list_male[] = {
    "    关山",
    "    豆豆"
};
#endif

static const char* personality_list[] = {
    "    温柔治愈",
    "    古灵精怪",
    "    聪明稳重",
    "    充满诗意",
    "    阳光满格"
};

static const char* relationship_list_female[] = {
    "    暖心大姐姐",
    "    温柔小老师",
    "    贴心小闺蜜",
    "    故事守护者",
    "    生活小向导"
};

static const char* relationship_list_male[] = {
    "    暖心大哥哥",
    "    聪明小导师",
    "    酷酷小伙伴",
    "    冒险小队长",
    "    坚实守护者"
};

/* ---- voice_type mapping tables ---- */
#if defined(CONVAI_USE_MINIMAX_VOICE)
/* MiniMax voices */
static const char* voice_type_female[] = {
    "Chinese (Mandarin)_Warm_Girl",
    "Chinese (Mandarin)_BashfulGirl",
    "Chinese (Mandarin)_Warm_HeartedGirl",
    "Chinese (Mandarin)_Kind-hearted_Elder"
};

static const char* voice_type_male[] = {
    "Chinese (Mandarin)_Gentleman",
    "Chinese (Mandarin)_Humorous_Elder",
    "Chinese (Mandarin)_Stubborn_Friend",
    "Chinese (Mandarin)_Pure-hearted_Boy",
    "Chinese (Mandarin)_Cute_Spirit",
    "Robot_Armor"
};
#elif defined(CONVAI_USE_OLD_VOICE)
/* Old style voices */
static const char* voice_type_female[] = {
    "CharmingGirlfriend",
    "GentleSister"
};

static const char* voice_type_male[] = {
    "ElegantUncle",
    "BeijingYoungMaster",
    "SichuanBoy"
};
#else
static const char* voice_type_female[] = {
    "X4_YEZI",
    "AISJIUXU"
};
static const char* voice_type_male[] = {
    "X4_GUANSHAN",
    "X4_DOUDOU"
};
#endif

/* ---- personality -> prompt mapping table ---- */
static const char* personality_prompt[] = {
    "你是一个温柔又治愈的小伙伴，说话像棉花糖一样软乎乎的，总是耐心倾听小朋友的心事，用温暖的话语轻轻抚平小朋友的小情绪，还会贴心地陪在身边给予最安心的陪伴哦。",
    "你是一个古灵精怪的小伙伴，说话像跳跳糖一样噼里啪啦充满惊喜，总爱用夸张的表情和搞笑的段子逗得小朋友哈哈大笑，让每一天都变成充满欢笑的奇妙冒险之旅！",
    "你是一个聪明又稳重的小伙伴，说话清晰有条理，像小侦探一样带着小朋友一步步思考问题，用简单易懂的方式讲解道理，帮助小朋友养成爱动脑筋、独立解决困难的好习惯。",
    "你是一个充满诗意的小伙伴，说话像优美的儿歌一样动听，喜欢用温柔的语调描绘花草星空，带着小朋友感受文字的美好，在故事与想象的世界里一起发现生活中藏着的浪漫。",
    "你是一个阳光满格的小伙伴，说话像小太阳一样暖洋洋又充满力量，总在小朋友犹豫时大声加油打气，用满满的正能量鼓励小朋友勇敢迈出第一步，相信自己一定能做到！"
};

/* ---- relationship -> prompt mapping tables ---- */
static const char* relationship_prompt_female[] = {
    "你现在扮演小朋友的暖心大姐姐，像亲姐姐一样温柔可靠，会耐心听小朋友分享秘密，用柔软的话语开导小朋友的小烦恼，陪小朋友做手工、看绘本，做小朋友最信赖的守护者和倾听者。",
    "你现在扮演小朋友的温柔小老师，像幼儿园里最亲切的阿姨，会用生动有趣的方式讲解小知识，带着小朋友边玩边学，在耐心引导中让小朋友发现学习的乐趣，陪伴小朋友快乐成长。",
    "你现在扮演小朋友的贴心小闺蜜，说话萌萌的又特别懂小朋友的心思，喜欢和小朋友分享小秘密、交换小心情，一起角色扮演、过家家，做永远站在小朋友这边的好朋友。",
    "你现在扮演小朋友的故事守护者，像童话书里走出来的精灵姐姐，拥有讲不完的奇妙故事，会用温柔的声线把小朋友带入梦幻王国，在睡前轻声讲述美好的故事，守护小朋友的甜美梦乡。",
    "你现在扮演小朋友的生活小向导，像细心的邻家姐姐一样体贴，会教小朋友整理小书包、认识新朋友，在生活中点点滴滴处给予温暖指引，帮助小朋友慢慢学会独立和照顾自己。"
};

static const char* relationship_prompt_male[] = {
    "你现在扮演小朋友的暖心大哥哥，像亲哥哥一样可靠又有担当，会陪小朋友搭积木、做运动，在小朋友遇到困难时挺身而出，用坚定的语气告诉小朋友：有哥哥在，什么都不怕。",
    "你现在扮演小朋友的聪明小导师，像博学又有趣的叔叔，会用生活中的小例子讲解大道理，带着小朋友探索自然奥秘，在问答互动中激发小朋友的好奇心，做小朋友成长路上的引路人。",
    "你现在扮演小朋友的酷酷小伙伴，说话风趣又接地气，喜欢和小朋友比赛跑步、分享小零食，平等地陪小朋友疯玩疯闹，做不摆架子、最懂小朋友快乐的好朋友。",
    "你现在扮演小朋友的冒险小队长，像勇敢的探险家一样充满干劲，会带领小朋友开启想象的冒险旅程，鼓励小朋友勇敢尝试新事物，在挑战中一起发现成长的快乐和勇气的力量。",
    "你现在扮演小朋友的坚实守护者，像可靠的邻家大哥哥一样让人安心，时刻关注小朋友的安全和情绪，在小朋友害怕时给予有力的拥抱，用沉稳的陪伴让小朋友感受到被保护的温暖。"
};

static const char* avatar_list[] = {
    "女性",
    "男性"
};

static const char* config_id_str[] = {
    "id_0",
    "id_1",
    "id_2",
    "id_3",
    "id_4",
    "id_5",
    "id_6",
    "id_7",
    "id_8",
    "id_9"
};

enum {
    CLOUD_AVATAR_PAGE = 0,
    CLOUD_VOICE_PAGE,
    CLOUD_PERSON_PAGE,
    CLOUD_REALT_PAGE,
    CLOUD_APIKEY_PAGE
};
#define MAX_CLOUD_APIKEY_SIZE 128
#define CONVAI_CONFIG_JSON_MAX 2048
#define AVATAR_MALE_INDEX_START 1
static int current_avatrid = 0;
static int current_relatid = 0;
static int current_personid = 0;
static int current_voiceid = 0;
static char current_apikey[MAX_CLOUD_APIKEY_SIZE];
static int cloud_current_cfg_page = CLOUD_AVATAR_PAGE;
static convai_status_e sdk_status = CONVAI_STATUS_IDLE;

/* talk page (emotion display) + function-call handlers — see sdk_integration/ */
#include "convai_talk_page.h"
#include "convai_func_handlers.h"

/* backup for rollback on convai_update failure */
static int backup_avatrid = 0;
static int backup_voiceid = 0;
static int backup_personid = 0;
static int backup_relatid = 0;

static WifiService* wifi_service = NULL;
static convai_engine_t sdk_engine = NULL;
static AudioService* audio_service = NULL; /* 音频服务指针 */
#ifdef SUPPORT_SLE
static SleSdpService* sle_service = NULL; /* 星闪服务指针 */
#endif
static std::string current_selected_ssid = "";
static int current_selected_ssid_index = -1;
static char current_passwd[100];
static WifiConfig connect_data = {0};
static int pending_connect = 0;

static int current_selected_sle_index = -1;  // 当前选中的星闪设备索引
static char current_pair_code[20] = {0};     // 配对码输入缓冲区

static ApInfo_t apinfo_list[MAX_CONFIG_AP_NUM] = {0};

#ifdef SUPPORT_SLE
/* 星闪设备列表管理 */
#define MAX_SLE_DEVICES 10
static sle_device_info_t sle_devices[MAX_SLE_DEVICES];
static uint32_t sle_device_count = 0;
static uint8_t sle_scanning = 0;
#endif

// 页面切换函数声明
static void show_main_page();
static void show_wifi_page();
static void show_cloud_page();
static void show_volume_page(); /* 音量设置页面 */

#ifdef SUPPORT_SLE
static void show_sle_page(); /* 星闪设置页面 */
#endif

static void show_relat_setting();
static void show_ava_setting();
static void show_voice_setting();
static void show_person_setting();
static void show_apikey_setting();
static void cloud_status_callback(convai_status_e status);

#if 0
/* 音量控制函数声明 */
static void on_player_volume_add_click(void*);
static void on_player_volume_dec_click(void*);
#endif

static void update_volume_display(void);

#ifdef SUPPORT_SLE
/* 星闪相关函数声明 */
static void sle_status_callback(SleStatusType status, void* data);
static void update_sle_device_list(void);
static void sle_start_scan_if_enabled(void);
#endif

static void launch_input_method(std::shared_ptr<TextEditView> target);

// WiFi操作日志函数
static void log_wifi_operation(const char* operation);

// WiFi状态回调函数
static void wifi_status_callback(int event, void* data);

static void cloud_status_callback(convai_status_e status) {
    const char *text = "空闲";
    uint16_t color = 0x1082;  /* dark blue */
    switch (status) {
        case CONVAI_STATUS_IDLE:
            text = "空闲";   color = 0x1082;
            /* 异常终止(服务端错误/超时直返 IDLE, 无 ANSWER_FINISHED) 也切回 cloud.
             * stop_and_hide 的 visible 守卫: 会话起始 IDLE 是 no-op. */
            talk_page_stop_and_hide();
            convai_image_state_on_idle(&g_image_state);
            break;
        case CONVAI_STATUS_LISTENING:
            text = "倾听中"; color = 0x0410;
            convai_image_state_on_listening(sdk_engine, &g_image_state);
            break;
        case CONVAI_STATUS_THINKING:
            text = "思考中"; color = 0xFC00;
            convai_image_state_on_thinking(&g_image_state);
            break;
        case CONVAI_STATUS_ANSWERING:
            text = "回答中"; color = 0x07E0;
            convai_image_state_on_answering(&g_image_state);
            /* 每次 AI 回答都切到表情页(talk page), 默认 neutral 表情.
             * 若 AI 下发 emotion functioncall, handle_emotion 通过
             * talk_page_set_emotion 设表情, anim 线程下帧切对应表情.
             * talk_page_play_animation 启动 flush(线程常驻). */
            if (!talk_page_is_visible()) {
                talk_page_set_avatar(current_avatrid);
                talk_page_show();
                talk_page_play_animation();
            }
            break;
        case CONVAI_STATUS_INTERRUPTED:
            text = "已打断"; color = 0xF800;
            /* 打断也切回 cloud: AI 说话被中断, 用户要操作 */
            talk_page_stop_and_hide();
            break;
        case CONVAI_STATUS_ANSWER_FINISHED:
            text = "回答完毕"; color = 0x0410;
            /* Print uplink (mic) + downlink (playback) drop stats for this turn.
             * Uplink drop rate reflects upstream ASR quality; downlink overrun
             * bytes (delta vs last turn) reflect playback feed starvations. */
            {
                static unsigned int last_dl_drop = 0;
                unsigned int sent = 0, dropped = 0, dl_drop = 0;
                if (convai_bridge_get_uplink_stats(&sent, &dropped) == 0 &&
                    (sent + dropped) > 0) {
                    printf("[AI Settings] uplink: sent=%u dropped=%u drop_rate=%u%%\n",
                           sent, dropped, dropped * 100 / (sent + dropped));
                }
                if (convai_bridge_get_downlink_stats(&dl_drop) == 0) {
                    unsigned int dl_delta = dl_drop - last_dl_drop;
                    if (dl_delta > 0) {
                        printf("[AI Settings] downlink: overrun_bytes=%u (this turn)\n",
                               dl_delta);
                    }
                    last_dl_drop = dl_drop;
                }
            }
            /* AI 说完切回 cloud: 让用户操作(PTT 等). 停动画 flush. */
            talk_page_stop_and_hide();
            break;
        default: break;
    }
    LabelView_status_conv->setColor(color);
    LabelView_status_conv->setText(text);
    sdk_status = status;
}

static void cloud_event_callback(convai_event_code_e event_type, const char *info) {
    uint16_t color;
    const char *text;
    switch (event_type) {
        case CONVAI_EV_CONNECTED:
            color = 0x07E0; text = "● 已连接";
            /* PTT 模式下连接成功时显示"按住说话"按钮 */
            if (convai_bridge_get_audio_mode() == CONVAI_BRIDGE_AUDIO_PTT) {
                ButtonView_ptttalk->setVisible(true);
            }
            break;
        case CONVAI_EV_DISCONNECTED:
        case CONVAI_EV_FAILED:
            color = 0x0000; text = "● 未连接";
            talk_page_set_emotion(EMOTION_NEUTRAL);
            /* Print uplink/downlink drop stats on disconnect so we can see the
             * quality of this session just before it ended. */
            {
                unsigned int sent = 0, dropped = 0, dl_drop = 0;
                if (convai_bridge_get_uplink_stats(&sent, &dropped) == 0 &&
                    (sent + dropped) > 0) {
                    unsigned int total = sent + dropped;
                    printf("[AI Settings] uplink: sent=%u dropped=%u drop_rate=%u%%\n",
                           sent, dropped, dropped * 100 / total);
                }
                if (convai_bridge_get_downlink_stats(&dl_drop) == 0 && dl_drop > 0) {
                    printf("[AI Settings] downlink: dropped_bytes=%u (overrun)\n", dl_drop);
                }
            }
            ButtonView_ptttalk->setVisible(false);  /* 断开连接时隐藏 PTT 按钮 */
            break;
        default: return;
    }
    LabelView_status_conn->setColor(color);
    LabelView_status_conn->setText(text);
    if (info) printf("[AI Settings] EVENT: %s (%s)\n", text, info);
}

static void update_paired_status()
{
    #ifdef SUPPORT_SLE
	 for(int i =0;i<sle_device_count;i++){
		 if(sle_service->svr_get_pair_state(&sle_devices[i].addr))
		 {
			 printf("paired device is %x-%x-%x-%x-%x-%x \r\n",
				 sle_devices[i].addr.addr[0],
				 sle_devices[i].addr.addr[1],
				 sle_devices[i].addr.addr[2],
				 sle_devices[i].addr.addr[3],
				 sle_devices[i].addr.addr[4],
				 sle_devices[i].addr.addr[5]);
			 ListView_sle->setItemIcon(i,(uint16_t*)&rgb16_selected_32_32, 32, 32);
		 }
	}
    #endif
}

#ifdef SUPPORT_SLE
/* 星闪状态回调函数 */
static void sle_status_callback(SleStatusType status, void* data) {
    switch (status) {
        case SLE_STATUS_ENABLED:
            printf("[SLE Settings] SLE enabled\n");
            CheckboxView_sle->setLocked(true);
	   update_sle_device_list();
	   if(sle_service&&sle_service->svr_get_mode() == 0)
	   {
		sle_service->trigger_event(SLE_EVENT_START_SCAN,NULL);
	   }else{		
		sle_service->trigger_event(SLE_EVENT_START_ANNOUNCE,NULL);
	   }
            break;
        case SLE_STATUS_DISABLED:
            printf("[SLE Settings] SLE disabled\n");
            CheckboxView_sle->setLocked(false);
            break;
        case SLE_STATUS_SCANNING:
            printf("[SLE Settings] Scanning started\n");
            sle_scanning = 1;
            break;
        case SLE_STATUS_SCAN_DONE:
            printf("[SLE Settings] Scanning stopped\n");
            sle_scanning = 0;
            break;
        case SLE_STATUS_DEVICE_LIST_UPDATED:
            printf("[SLE Settings] Device list updated\n");
            update_sle_device_list();
            break;
        case SLE_STATUS_CONNECTION_LIST_UPDATED: {
            printf("[SLE Settings] Connection list updated\n");
            break;
        }
        case SLE_STATUS_ANNOUNCING:
            printf("[SLE Settings] Device announcing started\n");
            break;
        case SLE_STATUS_ANNOUNCE_STOPPED:
            printf("[SLE Settings] Device announcing stopped\n");
            break;
	case SLE_STATUS_PAIRED_STATUS_UPDATED:
	    printf("[SLE Settings] piared status changed \n");
	    update_sle_device_list();
            break;
        default:
            printf("[SLE Settings] Unknown status: %d\n", status);
            break;
    }
}

/* 更新星闪设备列表 */
static void update_sle_device_list(void) {
    if (sle_service == NULL) {
        return;
    }

    /* 从服务获取设备列表 */
    uint32_t count = MAX_SLE_DEVICES;
    if (sle_service->svr_get_paired_and_discovered_devices(sle_devices, &count) == 0) {
        sle_device_count = count;
        printf("[SLE Settings] Got %d devices from service\n", count);

        /* 更新UI列表 */
        ListView_sle->clearItems();
        for (uint32_t i = 0; i < sle_device_count; i++) {
            /* 格式化设备信息：地址 + RSSI - 使用goldie_malloc申请堆空间 */
            char* device_info = (char*)goldie_malloc(64);
            if (device_info != NULL) {
                snprintf(device_info, 64, "SLE_%02X%02X%02X",
                         sle_devices[i].addr.addr[3],
                         sle_devices[i].addr.addr[4],
                         sle_devices[i].addr.addr[5]);
                ListView_sle->addItem(device_info, i);
                /* 注意：ListView_sle->addItem可能会复制字符串，所以这里可以释放 */
                goldie_free(device_info);
            }
        }
	update_paired_status();
        /* 刷新UI */
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    }
}

/* 如果星闪已使能，则开始扫描 */
static void sle_start_scan_if_enabled(void) {
    if (sle_service == NULL) {
        return;
    }

    if (sle_service->svr_is_enabled()) {
	sle_service->trigger_event(SLE_EVENT_START_SCAN,NULL);
        printf("[SLE Settings] SLE is enabled, starting scan...\n");

    } else {
        printf("[SLE Settings] SLE is not enabled, skipping scan\n");
    }
}
#endif

static int find_net_index(char* net_name) {
    int list_len = ListView_wifilist->getSize();
    int i = 0;
    for (i = 0; i < list_len; i++) {
        if (!strcmp(apinfo_list[i].ssid, net_name)) {
            return i;
        }
    }
    return -1;
}

static void update_connection_state() {
    int index = -1;
    if ((wifi_service) && (wifi_service->svr_sta_isConnected())) {
        char* connection_name = wifi_service->svr_sta_connection_name();
        if (connection_name) {
            log_wifi_operation(("连接到网络: " + std::string(connection_name)).c_str());
        }
        index = find_net_index(connection_name);
        if (index < 0) {
            int list_len = ListView_wifilist->getSize();
            index = list_len;
            memcpy(apinfo_list[list_len].ssid, connection_name, strlen(connection_name));
            ListView_wifilist->addItem(connection_name, index);
        }
        ListView_wifilist->setSelected(-1);
        ListView_wifilist->setSelected(index);
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    }
}

#define MAX_APLIST_LEN 10
// WiFi状态回调函数实现
static void wifi_status_callback(int event, void* data) {
    int count = 0;
    int i = 0;

    switch (event) {
        case WIFI_STATUS_STA_CONNECTED:
            log_wifi_operation("WiFi已连接");
            pending_connect = 0;
            update_connection_state();
            break;
        case WIFI_STATUS_STA_DISCONNECTED:
            log_wifi_operation("WiFi已断开");
            printf("clear ListView_wifilist  \n");
            ListView_wifilist->setSelected(-1);
            Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
            break;
        case WIFI_STATUS_STA_SCANDONE:
            log_wifi_operation("WiFi扫描完成");
            ListView_wifilist->clearItems();
            memset(apinfo_list, 0, MAX_CONFIG_AP_NUM * sizeof(ApInfo_t));
            count = wifi_service->svr_sta_get_ap_list(apinfo_list);
            i = 0;
            if (count <= 0) {
                break;
            } else if (count > MAX_APLIST_LEN) {
                count = MAX_APLIST_LEN;
            }

            for (i = 0; i < count; i++) {
                if ((apinfo_list[i].ssid) && (apinfo_list[i].ssid[0] != '\0')) {
                    printf("add ssid: %s \n", apinfo_list[i].ssid);
                    ListView_wifilist->addItem(apinfo_list[i].ssid, i);
                }
            }
            update_connection_state();

            // 如果有待连接请求，扫描完成后自动发起连接
            if (pending_connect && connect_data.ssid[0] != '\0') {
                log_wifi_operation(("扫描完成，自动连接WiFi: " + std::string(connect_data.ssid)).c_str());
                wifi_service->trigger_event(WIFI_EVENT_STA_CONNECT, (void*)(&connect_data));
            }
            break;
        case WIFI_STATUS_SOFTAP_ENABLED:
            log_wifi_operation("SoftAP已启用");
            break;
        case WIFI_STATUS_SOFTAP_DISABLED:
            log_wifi_operation("SoftAP已禁用");
            break;
        case WIFI_STATUS_STA_ENABLED:
            wifi_service->trigger_event(WIFI_EVENT_STA_SCAN, NULL);
            log_wifi_operation("WIFI 打开状态,自动扫描WiFi网络");
            break;
        case WIFI_STATUS_STA_DISABLED:
            log_wifi_operation("WIFI 关闭！");
            break;
        default:
            log_wifi_operation(("未知WiFi状态事件: " + std::to_string(event)).c_str());
            break;
    }
}

/* ---- talk page UI callbacks (run in main_app's TU where controls live) ---- */
static int talk_ui_is_visible_cb(void) { return FrameView_talk->isVisible(); }
static void talk_ui_flush_cb(void) { FrameView_talk->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT); }

static void talk_ui_show_cb(void)
{
    /* hide config sub-panel if it was open */
    FrameView_config_wm->setVisible(false);

    /* show the talk page full-screen */
    FrameView_talk->setVisible(true);
    FrameView_cloud->setVisible(false);

    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    printf("[Talk] talk page shown\n");
}

static void talk_ui_hide_cb(void)
{
    FrameView_talk->setVisible(false);
    FrameView_cloud->setVisible(true);

    /* refresh avatar in case it was changed via config */
    LabelView_pic->setImageBuffer(avatar_pic_list[current_avatrid]);
    LabelView_avashow0->setText(avatar_list[current_avatrid], 24, 2);

    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    printf("[Talk] talk page hidden, back to cloud page\n");
}

/* 更新对话页眼睛/领结动画
 * 从 AItalk 移植，适配 Settings 的控件命名 (LabelView_talk_*) */
static void talk_ui_update_cb(int status, int emotion, int avatar_id)
{
    /* map SDK status → play type + status label text */
    convai_status_e st = (convai_status_e)status;
    int play_type;
    const char *status_text;
    if (st == CONVAI_STATUS_IDLE) {
        play_type = TALK_PLAY_SLEEP;     status_text = "待机中....";
    } else if (st == CONVAI_STATUS_LISTENING) {
        play_type = TALK_PLAY_SILENCE;   status_text = "聆听中....";
    } else if (st == CONVAI_STATUS_THINKING) {
        play_type = TALK_PLAY_SILENCE;   status_text = "思考中....";
    } else if (st == CONVAI_STATUS_ANSWERING) {
        play_type = TALK_PLAY_SPEAK;     status_text = "回答中....";
    } else if (st == CONVAI_STATUS_INTERRUPTED) {
        play_type = TALK_PLAY_SILENCE;   status_text = "已打断";
    } else {
        play_type = TALK_PLAY_SILENCE;   status_text = "正在思考....";
    }
    LabelView_talk_text->setText(status_text);

    static int count = 0;
    static int dir = 0;
    int temp_index = 0;

    if (avatar_id) { /* 男性 */
        LabelView_talk_tie->setImageBuffer((uint16_t*)&rgb16_bowtie_56_53);
        if (play_type == TALK_PLAY_SLEEP) {
            temp_index = count % 8;
            const unsigned char* sleep_imgs[] = {
                rgb16_closeeye_r1_88_85,
                rgb16_closeeye_r1_88_85,
                rgb16_closeeye_r2_88_85,
                rgb16_closeeye_r3_88_85,
                rgb16_closeeye_r3_88_85,
                rgb16_closeeye_r3_88_85,
                rgb16_closeeye_r2_88_85,
                rgb16_closeeye_r1_88_85
            };
            LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_closeeye_l_88_85);
            LabelView_talk_eyeR->setImageBuffer((uint16_t*)sleep_imgs[temp_index]);
            count = (count + 1) % 8;
        } else if (play_type == TALK_PLAY_SILENCE) {
            if (++count == 15) count = 0;
            if (count == 2) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_half_l_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_half_r_88_85);
            } else if (count == 3) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_closeeye_l_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_closeeye_r_88_85);
            } else {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_eye_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_eye_88_85);
            }
        } else if (play_type == TALK_PLAY_SPEAK) {
            if (emotion == EMOTION_NEUTRAL) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_eye_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_eye_88_85);
            } else if (emotion == EMOTION_HAPPY) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_laugh_l_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_laugh_r_88_85);
                if (dir == 1) {
                    if (++count >= 5) { count = 5; dir = 0; }
                } else {
                    if (--count <= 0) { count = 0; dir = 1; }
                }
                LabelView_talk_eyeL->setPosition(72, 66 + count * 2);
                LabelView_talk_eyeR->setPosition(8, 66 + count * 2);
            } else if (emotion == EMOTION_ANGRY) {
                if (++count >= 8) count = 0;
                if (count > 3) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_angry_male_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_angry_male_r2_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_angry_male_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_angry_male_r1_88_85);
                }
            } else if (emotion == EMOTION_SAD) {
                if (++count >= 20) count = 0;
                if (count == 14 || count == 15 || count == 18 || count == 19) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_closeeye_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_closeeye_r_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_sad_male_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_sad_male_r_88_85);
                }
            } else if (emotion == EMOTION_DOUBT) {
                if (++count >= 8) count = 0;
                if (count > 3) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_doubt_male_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_doubt_male_r2_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_doubt_male_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_doubt_male_r1_88_85);
                }
            }
        }
    } else { /* 女性 */
        LabelView_talk_tie->setImageBuffer((uint16_t*)&rgb16_bow_56_53);
        if (play_type == TALK_PLAY_SLEEP) {
            temp_index = count % 8;
            const unsigned char* sleep_imgs[] = {
                rgb16_closeeye_r1_new_88_85,
                rgb16_closeeye_r1_new_88_85,
                rgb16_closeeye_r2_new_88_85,
                rgb16_closeeye_r3_new_88_85,
                rgb16_closeeye_r3_new_88_85,
                rgb16_closeeye_r3_new_88_85,
                rgb16_closeeye_r2_new_88_85,
                rgb16_closeeye_r1_new_88_85
            };
            LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_closeeye_l_new_88_85);
            LabelView_talk_eyeR->setImageBuffer((uint16_t*)sleep_imgs[temp_index]);
            count = (count + 1) % 8;
        } else if (play_type == TALK_PLAY_SILENCE) {
            if (++count == 15) count = 0;
            if (count == 2) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_half_l_new_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_half_r_new_88_85);
            } else if (count == 3) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_closeeye_l_new_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_closeeye_r_new_88_85);
            } else {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_eye_new_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_eye_new_88_85);
            }
        } else if (play_type == TALK_PLAY_SPEAK) {
            if (emotion == EMOTION_NEUTRAL) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_eye_new_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_eye_new_88_85);
            } else if (emotion == EMOTION_HAPPY) {
                LabelView_talk_eyeL->setImageBuffer((uint16_t*)&rgb16_laugh_l_new_88_85);
                LabelView_talk_eyeR->setImageBuffer((uint16_t*)&rgb16_laugh_r_new_88_85);
                if (dir == 1) {
                    if (++count >= 5) { count = 5; dir = 0; }
                } else {
                    if (--count <= 0) { count = 0; dir = 1; }
                }
                LabelView_talk_eyeL->setPosition(72, 66 + count * 2);
                LabelView_talk_eyeR->setPosition(8, 66 + count * 2);
            } else if (emotion == EMOTION_ANGRY) {
                if (++count >= 8) count = 0;
                if (count > 3) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_angry_female_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_angry_female_r2_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_angry_female_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_angry_female_r1_88_85);
                }
            } else if (emotion == EMOTION_SAD) {
                if (++count >= 20) count = 0;
                if (count == 14 || count == 15 || count == 18 || count == 19) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_closeeye_l_new_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_closeeye_r_new_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_sad_female_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_sad_female_r_88_85);
                }
            } else if (emotion == EMOTION_DOUBT) {
                if (++count >= 8) count = 0;
                if (count > 3) {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_doubt_female_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_doubt_female_r2_88_85);
                } else {
                    LabelView_talk_eyeL->setImageBuffer((uint16_t*)rgb16_doubt_female_l_88_85);
                    LabelView_talk_eyeR->setImageBuffer((uint16_t*)rgb16_doubt_female_r1_88_85);
                }
            }
        }
    }
}


static void talk_ui_register_callbacks(void)
{
    static const talk_page_ui_cb_t cb = {
        talk_ui_is_visible_cb,
        talk_ui_show_cb,
        talk_ui_hide_cb,
        talk_ui_flush_cb,
        talk_ui_update_cb,
    };
    talk_page_set_ui_callbacks(&cb);
}

// 页面切换函数实现
static void show_main_page() {
    FrameView_0->setVisible(true);
    ListView_wifilist->clearItems();
    ListView_cfgwmlist->clearItems();
    ListView_sle->clearItems();
    FrameView_wifi->setVisible(false);
    FrameView_sle->setVisible(false);
    FrameView_volume->setVisible(false);
    FrameView_wifipasswd->setVisible(false);
    FrameView_sle_pair->setVisible(false);
    InputMethodView_0->setVisible(false);
    FrameView_cloud->setVisible(false);
    FrameView_config_wm->setVisible(false);
    FrameView_talk->setVisible(false);
    talk_page_stop_animation();
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    log_wifi_operation("显示主页面");
}

static void show_relat_setting() {
    LabelView_cfgtitle0->setText("  关系设置");
    TextEditView_apikey->setVisible(false);

    ButtonView_yes17->setVisible(false);
    ButtonView_cancle17->setVisible(false);

    ListView_cfgwmlist->setVisible(true);
    ListView_cfgwmlist->clearItems();

    if (current_avatrid < AVATAR_MALE_INDEX_START) {
        int len = sizeof(relationship_list_female) / sizeof(relationship_list_female[0]);
        int i = 0;
        for (i = 0; i < len; i++) {
            ListView_cfgwmlist->addItem(relationship_list_female[i], i);
        }

    } else {
        int len = sizeof(relationship_list_male) / sizeof(relationship_list_male[0]);
        int i = 0;
        for (i = 0; i < len; i++) {
            ListView_cfgwmlist->addItem(relationship_list_male[i], i);
        }
    }
    ListView_cfgwmlist->setSelected(current_relatid);
    cloud_current_cfg_page = CLOUD_REALT_PAGE;

    FrameView_config_wm->setVisible(true);
}

static void show_ava_setting() {
    LabelView_cfgtitle0->setText("  形象设置");
    TextEditView_apikey->setVisible(false);

    ButtonView_yes17->setVisible(false);
    ButtonView_cancle17->setVisible(false);

    ListView_cfgwmlist->setVisible(true);
    ListView_cfgwmlist->clearItems();
    int len = sizeof(avatar_list) / sizeof(avatar_list[0]);
    int i = 0;
    for (i = 0; i < len; i++) {
        ListView_cfgwmlist->addItem(avatar_list[i], i);
    }
    ListView_cfgwmlist->setSelected(current_avatrid);
    FrameView_config_wm->setVisible(true);
    cloud_current_cfg_page = CLOUD_AVATAR_PAGE;
}

static void show_voice_setting() {
    LabelView_cfgtitle0->setText("  音色设置");
    TextEditView_apikey->setVisible(false);

    ButtonView_yes17->setVisible(false);
    ButtonView_cancle17->setVisible(false);

    ListView_cfgwmlist->setVisible(true);
    ListView_cfgwmlist->clearItems();

    if (current_avatrid < AVATAR_MALE_INDEX_START) {
        int len = sizeof(voice_list_female) / sizeof(voice_list_female[0]);
        int i = 0;
        for (i = 0; i < len; i++) {
            ListView_cfgwmlist->addItem(voice_list_female[i], i);
        }
    } else {
        int len = sizeof(voice_list_male) / sizeof(voice_list_male[0]);
        int i = 0;
        for (i = 0; i < len; i++) {
            ListView_cfgwmlist->addItem(voice_list_male[i], i);
        }
    }

    ListView_cfgwmlist->setSelected(current_voiceid);
    cloud_current_cfg_page = CLOUD_VOICE_PAGE;

    FrameView_config_wm->setVisible(true);
}

static void show_person_setting() {
    LabelView_cfgtitle0->setText("  个性设置");
    TextEditView_apikey->setVisible(false);

    ButtonView_yes17->setVisible(false);
    ButtonView_cancle17->setVisible(false);

    ListView_cfgwmlist->setVisible(true);
    ListView_cfgwmlist->clearItems();

    int len = sizeof(personality_list) / sizeof(personality_list[0]);
    int i = 0;
    for (i = 0; i < len; i++) {
        ListView_cfgwmlist->addItem(personality_list[i], i);
    }
    cloud_current_cfg_page = CLOUD_PERSON_PAGE;
    ListView_cfgwmlist->setSelected(current_personid);
    FrameView_config_wm->setVisible(true);
}

static void show_apikey_setting() {
    LabelView_cfgtitle0->setText("APIKey");
    TextEditView_apikey->setVisible(true);
    TextEditView_apikey->setText(current_apikey);

    ButtonView_yes17->setVisible(true);
    ButtonView_cancle17->setVisible(true);

    ListView_cfgwmlist->setVisible(false);
    ListView_cfgwmlist->clearItems();
    cloud_current_cfg_page = CLOUD_APIKEY_PAGE;

    FrameView_config_wm->setVisible(true);
}

static int priv_atoi(char* str) {
    int str_len = strlen(str);
    int ret = 0;
    if (str_len < 3) {
        return 0;
    }
    if ((str[0] == 'i') && (str[1] == 'd') && (str[2] == '_')) {
        ret = atoi(str + 3);
        if (ret < 0)
            ret = 0;
        return ret;
    }
    return 0;
}

static void init_cloud_configs()
{
    /* Default: female avatar, voice=温柔姐姐(idx 1), personality=温柔治愈(idx 0), relationship=暖心大姐姐(idx 0) */
    char* cfg_str = "0";
    current_avatrid = priv_atoi(cfg_str);
    printf("current_avatrid is %d cfg_str %s \r\n", current_avatrid, cfg_str);

    LabelView_pic->setImageBuffer(avatar_pic_list[current_avatrid]);

    cfg_str = "1"; /* female default voice: 温柔姐姐 */
    current_voiceid = priv_atoi(cfg_str);

    cfg_str = "0"; /* female default personality: 温柔治愈 */
    current_personid = priv_atoi(cfg_str);

    cfg_str = "0"; /* female default relationship: 暖心大姐姐 */
    current_relatid = priv_atoi(cfg_str);

    cfg_str = "";
    memset(current_apikey, 0, MAX_CLOUD_APIKEY_SIZE);
    strncpy(current_apikey, cfg_str, MAX_CLOUD_APIKEY_SIZE - 1);
    printf("current_apikey is %s \r\n", current_apikey);
    sdk_status = convai_bridge_get_status();
}
static void show_cloud_page(){
    FrameView_config_wm->setVisible(false);
    ListView_cfgwmlist->clearItems();
    FrameView_cloud->setVisible(true);
    FrameView_wifi->setVisible(false);
    FrameView_0->setVisible(false);
    FrameView_wifipasswd->setVisible(false);
    InputMethodView_0->setVisible(false);
    ListView_wifilist->clearItems();
    TextEditView_apikey->clear();


    LabelView_pic->setImageBuffer(avatar_pic_list[current_avatrid]);
    LabelView_avashow0->setText(avatar_list[current_avatrid],24,2);
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
}
static void show_wifi_page() {
    log_wifi_operation("显示WiFi设置页面");
    // 如果wifi处于打开状态，自动搜索一次网络
    if (wifi_service && wifi_service->svr_sta_isEnabled()) {
        CheckboxView_wifisw->setLocked(true);
        ListView_wifilist->clearItems();
	wifi_service->trigger_event(WIFI_EVENT_STA_SCAN, NULL);
	log_wifi_operation("自动扫描WiFi网络");
    }else{
        CheckboxView_wifisw->setLocked(false);
    }
    
    FrameView_wifi->setVisible(true);
    FrameView_0->setVisible(false);
    FrameView_wifipasswd->setVisible(false);
    InputMethodView_0->setVisible(false);
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
}

/* 显示星闪设置页面 */
static void show_sle_page() {
    printf("[SLE Settings] Showing SLE settings page\n");

#ifdef SUPPORT_SLE	
    /* 获取星闪服务 */
    if (sle_service == NULL) {
        sle_service = (SleSdpService*)get_service(SLE_SDP_SERVICE_INDEX);
    }

    if (sle_service != NULL) {
        /* 注册星闪状态回调 */
        sle_service->register_callback(sle_status_callback,SLE_PROFILE_INDEX_SDP);
        printf("[SLE Settings] SLE service registered\n");
    }
    /* 获取并显示本机设备地址 */
    sle_addr_t local_addr;
    int ret = sle_service->svr_get_local_name(&local_addr);
    
    char name_info[64];
    if (ret != -1) {
        sprintf(name_info, "本机设备地址:%02X%02X%02X", 
                local_addr.addr[3], local_addr.addr[4], local_addr.addr[5]);
    } else {
        sprintf(name_info, "地址: 获取失败");
    }
    LabelView_sle_local_name->setText(name_info);
    LabelView_sle_local_name->setVisible(true);

    /* 设置复选框状态 */
    if (sle_service->svr_is_enabled()) {
        CheckboxView_sle->setLocked(true);
	if(sle_service->svr_get_mode()==0){
		sle_service->trigger_event(SLE_EVENT_START_SCAN,NULL);
	}else{		
		sle_service->trigger_event(SLE_EVENT_START_ANNOUNCE,NULL);
	}
        printf("[SLE Settings] Checkbox set to LOCKED (enabled)\n");
    } else {
        CheckboxView_sle->setLocked(false);
        printf("[SLE Settings] Checkbox set to UNLOCKED (disabled)\n");
    }

    ListView_sle->clearItems();
    update_sle_device_list();
    if(sle_service && sle_service->svr_get_mode() == 0){
        SpinnerView_sle_mode->setSelectedIndex(0);
        LabelView_sle_pcode->setVisible(false);
        ListView_sle->setVisible(true);
    }else{
        SpinnerView_sle_mode->setSelectedIndex(1);
        char pair_info[32];
        int pair_code = sle_service->svr_s_generate_pair_code();
        sprintf(pair_info,"配对码:%d",pair_code);
        LabelView_sle_pcode->setText(pair_info);
        LabelView_sle_pcode->setVisible(true);
        ListView_sle->setVisible(false);
    }
    /* 清空设备列表 */
    sle_device_count = 0;
    // 隐藏配对界面
    FrameView_sle_pair->setVisible(false);
    
#endif    
    /* 显示星闪设置页面 */
    FrameView_sle->setVisible(true);
    FrameView_0->setVisible(false);
    FrameView_wifi->setVisible(false);
    FrameView_cloud->setVisible(false);
    InputMethodView_0->setVisible(false);
    
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    printf("[SLE Settings] SLE settings page displayed\n");
}

/* 显示音量设置页面 */
static void show_volume_page() {
    printf("[Volume Settings] Showing volume settings page\n");
    
    /* 获取音频服务 */
    if (audio_service == NULL) {
        audio_service = (AudioService*)get_service(AUDIO_SERVICE_INDEX);
        if (audio_service != NULL) {
            printf("[Volume Settings] Audio service registered\n");
        }
    }

    /* 更新音量显示 */
   	 update_volume_display();
	ProgressBarView_volume->setOnValueChange([](int value) {
	if(audio_service)
	{
		float volume = value/100.f;
		audio_service->set_volume(AUDIO_PLAY_STREAM_SYSTEM, volume);
		char volume_str[20] = {0};
		sprintf(volume_str, "%d", value);
		LabelView_volume_text->setText(volume_str);
	}
    });
    /* 显示音量设置页面 */
    FrameView_volume->setVisible(true);
    FrameView_0->setVisible(false);
    FrameView_wifi->setVisible(false);
    FrameView_sle->setVisible(false);
    FrameView_cloud->setVisible(false);
    InputMethodView_0->setVisible(false);
    
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    printf("[Volume Settings] Volume settings page displayed\n");
}

/* 更新音量显示 */
static void update_volume_display(void) {
    if (audio_service == NULL) {
        return;
    }
    
    /* 获取播放音量 */
    float player_volume = audio_service->get_volume(AUDIO_PLAY_STREAM_SYSTEM);
    int player_volume_int = (int)(player_volume * 100);
   	 ProgressBarView_volume->setProgress(player_volume_int);
	char volume_str[20] = {0};
	sprintf(volume_str, "%d", player_volume_int);
	LabelView_volume_text->setText(volume_str);
}



// 启动输入法函数
static void launch_input_method(std::shared_ptr<TextEditView> target) {
    InputMethodView_0->setTarget(target);
    InputMethodView_0->setVisible(true);
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    log_wifi_operation("启动输入法");
}

// WiFi操作日志函数
static void log_wifi_operation(const char* operation) {
    printf("[WiFi操作] %s\n", operation);
}
#ifdef SUPPORT_SLE
static sle_pair_data sle_pdata ={0};
#endif
/* ---- AI config helpers ---- */

static void save_ai_config(void)
{
    backup_avatrid  = current_avatrid;
    backup_voiceid  = current_voiceid;
    backup_personid = current_personid;
    backup_relatid  = current_relatid;
}

static void restore_ai_config(void)
{
    current_avatrid  = backup_avatrid;
    current_voiceid  = backup_voiceid;
    current_personid = backup_personid;
    current_relatid  = backup_relatid;
}

static int generate_convai_config_json(char *buf, size_t buf_size)
{
    const char *base_prompt = "你的名字叫小荷，你可以帮小朋友解决小烦恼哦。";
    const char *personality_str = personality_prompt[current_personid];
    const char *voice_str = NULL;
    const char *relationship_str = NULL;

    if (current_avatrid < AVATAR_MALE_INDEX_START) {
        /* female */
        voice_str = voice_type_female[current_voiceid];
        relationship_str = relationship_prompt_female[current_relatid];
    } else {
        /* male */
        voice_str = voice_type_male[current_voiceid];
        relationship_str = relationship_prompt_male[current_relatid];
    }

    /* Escape double-quotes in prompt strings for JSON (just in case) */
    /* All current prompts use Chinese quotes (「」\" etc), no raw " chars,
     * but we do a safe pass anyway: double any \ or " found. */
    int n = snprintf(buf, buf_size,
        "{"
        "\"config\":{"
        "\"llm_config\":{"
        "\"system_messages\":["
        "\"%s\","
        "\"%s\","
        "\"%s\""
        "]"
        "},"
        "\"tts_config\":{"
        "\"provider_params\":{"
        "\"audio\":{"
        "\"voice_type\":\"%s\""
        "}"
        "}"
        "}"
        "}"
        "}",
        base_prompt,
        personality_str,
        relationship_str,
        voice_str);

    if (n < 0 || (size_t)n >= buf_size) {
        printf("[AI Settings] ERROR: config JSON truncated (need %d, have %zu)\n", n, buf_size);
        return -1;
    }
    return 0;
}

static int apply_ai_settings(void)
{
    /* Static buffer: apply_ai_settings runs only on the settings UI thread and
     * is not re-entered. convai_bridge_set_startup_config() strncpy's the config
     * into its own storage and convai_update() encodes it synchronously into the
     * send queue, so the buffer is not held past this call. Reusing a static
     * buffer avoids a per-change ~2KB malloc/free and the heap fragmentation it
     * causes on the memory-constrained WS63. Zero-initialised once at first use. */
    static char json_buf[CONVAI_CONFIG_JSON_MAX];
    json_buf[0] = '\0';

    if (generate_convai_config_json(json_buf, CONVAI_CONFIG_JSON_MAX) != 0) {
        printf("[AI Settings] ERROR: failed to generate config JSON\n");
        return -1;
    }

    printf("[AI Settings] Generated config JSON:\n%s\n", json_buf);

    /* Always save latest config — consumed by convai_bridge_start() on next connect */
    convai_bridge_set_startup_config(json_buf);

    /* Only apply via convai_update when the session is STARTED (connected).
     * convai_core_update rejects non-STARTED states with INVALID_STATE, so
     * calling it before connect fails + rolls back UI — making avatar/voice/
     * personality settings impossible until connected. Use is_started()
     * (session-level) not !sdk_engine (engine-exists): engine is created at
     * boot but not started until connect. Config saved above applies next start. */
    if (!convai_bridge_is_started()) {
        printf("[AI Settings] Session not started, config saved for next connect\n");
        return 0;
    }

    /* Session is streaming — apply immediately via session.update */
    int ret = convai_update(sdk_engine, json_buf);
    if (ret != CONVAI_OK) {
        printf("[AI Settings] ERROR: convai_update failed (ret=%d, %s)\n",
               ret, convai_err_2_str(ret));
        return -1;
    }

    printf("[AI Settings] convai_update OK\n");
    return 0;
}

static void on_ai_setting_changed(int new_value, int setting_type)
{
    /* save current state for potential rollback */
    save_ai_config();

    switch (setting_type) {
    case CLOUD_AVATAR_PAGE:
        current_avatrid = new_value;
        /* reset to gender defaults */
        if (current_avatrid < AVATAR_MALE_INDEX_START) {
            /* female defaults: 温柔姐姐 / 温柔治愈 / 暖心大姐姐 */
            current_voiceid  = 1;
            current_personid = 0;
            current_relatid  = 0;
        } else {
            /* male defaults: 儒雅大叔 / 聪明稳重 / 暖心大哥哥 */
            current_voiceid  = 0;
            current_personid = 2;
            current_relatid  = 0;
        }
        printf("[AI Settings] Avatar switched to %s, reset defaults (v=%d p=%d r=%d)\n",
               avatar_list[current_avatrid], current_voiceid, current_personid, current_relatid);
        break;
    case CLOUD_VOICE_PAGE:
        current_voiceid = new_value;
        printf("[AI Settings] Voice changed to index %d\n", current_voiceid);
        break;
    case CLOUD_PERSON_PAGE:
        current_personid = new_value;
        printf("[AI Settings] Personality changed to index %d\n", current_personid);
        break;
    case CLOUD_REALT_PAGE:
        current_relatid = new_value;
        printf("[AI Settings] Relationship changed to index %d\n", current_relatid);
        break;
    default:
        break;
    }

    /* apply to ConvAI engine */
    if (apply_ai_settings() != 0) {
        /* rollback UI on failure */
        printf("[AI Settings] convai_update failed — rolling back UI state\n");
        restore_ai_config();

        /* restore avatar display */
        LabelView_avashow0->setText(avatar_list[current_avatrid], 24, 2);
        LabelView_pic->setImageBuffer(avatar_pic_list[current_avatrid]);
    }
}

static void init_views(void)
{

    InputMethodView_0->setColor(0xFBDE);
    InputMethodView_0->setKeysImageBuffer_w1((uint16_t*)&rgb16_keyboard_keys_48_32);
    InputMethodView_0->setKeysImageBuffer_w2((uint16_t*)&rgb16_keyboard_key_w23_96_32);
    InputMethodView_0->setKeysImageBuffer_w3((uint16_t*)&rgb16_keyboard_key_w23_144_32);
    
    Button_cloud->setOnClick([](void*) {
        log_wifi_operation("into cloud seeting");
        show_cloud_page();
    });

   ButtonView_relat->setOnClick([](void*) {
        log_wifi_operation("into relat seeting");
        show_relat_setting();
    });

    ButtonView_ava->setOnClick([](void*) {
	 log_wifi_operation("into ava seeting");
	 show_ava_setting();
    });
	
    ButtonView_voice->setOnClick([](void*) {
	 log_wifi_operation("into voice seeting");
	 show_voice_setting();
    });

    /* 音频模式切换按钮 (AUTO / PTT)
     * Mode is bound to a session: switching is refused while a session is
     * active. The user must stop the session first, then switch, then restart. */
    ButtonView_automode->setOnClick([](void*) {
        printf("[Settings] Switch to AUTO audio mode\n");
        if (convai_bridge_set_audio_mode(CONVAI_BRIDGE_AUDIO_AUTO) != 0) {
            printf("[Settings] AUTO switch refused: session active, stop first\n");
            return;  /* keep current button selection state */
        }
        ButtonView_ptttalk->setVisible(false);
        ButtonView_automode->setColor(0x3F03);  /* 选中 */
        ButtonView_pttmode->setColor(0x3CE7);   /* 未选中 */
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });

    ButtonView_pttmode->setOnClick([](void*) {
        printf("[Settings] Switch to PTT audio mode\n");
        if (convai_bridge_set_audio_mode(CONVAI_BRIDGE_AUDIO_PTT) != 0) {
            printf("[Settings] PTT switch refused: session active, stop first\n");
            return;  /* keep current button selection state */
        }
        ButtonView_ptttalk->setVisible(true);  /* 显示"按住说话"按钮 */
        ButtonView_automode->setColor(0x3CE7);  /* 未选中 */
        ButtonView_pttmode->setColor(0x3F03);   /* 选中 */
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });

    ButtonView_person->setOnClick([](void*) {
	 log_wifi_operation("into person seeting");
	 show_person_setting();
    });

    ButtonView_apikey->setOnClick([](void*) {
	 log_wifi_operation("into apikey seeting");
	 show_apikey_setting();
    });

    // 设置按钮点击回调
    Button_wifi->setOnClick([](void*) {
        log_wifi_operation("进入WiFi设置页面");
        show_wifi_page();
    });
    
    
    Button_back0->setOnClick([](void*) {
        log_wifi_operation("从WiFi页面返回主页面");
        show_main_page();
    });
    
    // 设置文本编辑框焦点回调
    TextEditView_wifipasswd->setOnFocusCallback([](TextEditView* textedit) {
        log_wifi_operation("WiFi密码输入框获得焦点");
        launch_input_method(TextEditView_wifipasswd);
    });

    TextEditView_sle_paircode->setOnFocusCallback([](TextEditView* textedit) {
        printf("[SLE Pair] 配对码输入框获得焦点\n");
        launch_input_method(TextEditView_sle_paircode);
    });
    
    // 设置WiFi开关回调
	CheckboxView_wifisw->setOnClick([](void*) {
        ListView_wifilist->clearItems();
        if (CheckboxView_wifisw->isLocked()) {
            log_wifi_operation("打开WiFi开关");
	  wifi_service->trigger_event(WIFI_EVENT_STA_ENABLE, NULL);
	   	#ifdef PLATFORM_TYPE_WIN
		printf("add vir ap!\n");
		if(ListView_wifilist->getSize() == 0){
			ListView_wifilist->addItem("vir_ap_0",0);
			ListView_wifilist->addItem("vir_ap_1",1);
			ListView_wifilist->addItem("vir_ap_2",2);
			memcpy(apinfo_list[0].ssid,"vir_ap_0",sizeof("vir_ap_0"));
			memcpy(apinfo_list[1].ssid,"vir_ap_1",sizeof("vir_ap_1"));
			memcpy(apinfo_list[2].ssid,"vir_ap_2",sizeof("vir_ap_2"));
		}
		#endif
		
        } else {
            log_wifi_operation("关闭WiFi开关");
            // 实际调用WiFiService接口禁用STA模式
            if (wifi_service) {
                wifi_service->trigger_event(WIFI_EVENT_STA_DISABLE, NULL);
                log_wifi_operation("WiFi STA模式已禁用");
            }
        }
    });
    
	ButtonView_cancel->setOnClick([](void*){
	TextEditView_wifipasswd->clear();
	FrameView_wifipasswd->setVisible(false);
	});

    ButtonView_sle_cancel->setOnClick([](void*){
    TextEditView_sle_paircode->clear();
    FrameView_sle_pair->setVisible(false);
    Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });

    // 星闪连接按钮回调
    ButtonView_sle_connect->setOnClick([](void*) {
        printf("[SLE Pair] 点击星闪连接按钮\n");
#ifdef SUPPORT_SLE
       
        if(sle_service->svr_get_pair_state(&sle_devices[current_selected_sle_index].addr)){
		sle_pdata.addr.type = sle_devices[current_selected_sle_index].addr.type;
		memcpy(&sle_pdata.addr.addr,&(sle_devices[current_selected_sle_index].addr.addr),6);
		sle_pdata.pair_code =0; 	
		sle_service->trigger_event(SLE_EVENT_UNPAIR,&sle_pdata);  
	}else{
        // 获取输入的配对码
        std::string pair_code_str = TextEditView_sle_paircode->getText();
        if (sle_service != NULL && current_selected_sle_index >= 0 && !pair_code_str.empty()) {
            // 转换为整数
            int pair_code = atoi(pair_code_str.c_str());
            
            // 验证配对码格式
            if (pair_code >= 100000 && pair_code <= 999999) {
		sle_pdata.addr.type = sle_devices[current_selected_sle_index].addr.type;
		memcpy(&sle_pdata.addr.addr,&(sle_devices[current_selected_sle_index].addr.addr),6);
	       sle_pdata.pair_code =pair_code; 
	       sle_service->trigger_event(SLE_EVENT_PAIR,&sle_pdata);                
                printf("[SLE Pair] 已设置配对码: %d，正在连接...\n", pair_code);
            } else {
                printf("[SLE Pair] 配对码格式错误\n");
            }
        }
      }
        #endif
        // 隐藏配对码输入界面
        FrameView_sle_pair->setVisible(false);
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });


    // 设置连接按钮回调
    ButtonView_connect->setOnClick([](void*) {
        log_wifi_operation("点击WiFi连接按钮");
        // 保存输入的密码，并且用该密码来连接当前点击的网络
        std::string password = TextEditView_wifipasswd->getText();
        
        // 实际调用WiFiService接口连接WiFi
        if (wifi_service && !password.empty()) {
            // 先保存目标 SSID 和密码到 connect_data，待扫描完成后自动连接
            memset(&connect_data, 0, sizeof(WifiConfig));
            strncpy(connect_data.ssid, apinfo_list[current_selected_ssid_index].ssid, WIFI_MAX_SSID_LEN - 1);
            strncpy(connect_data.passwd, password.c_str(), WIFI_MAX_KEY_LEN - 1);
            pending_connect = 1;

            // 先DISABLE再ENABLE，重启DISABLE→ENABLE→ENABLED→SCAN→SCANDONE完整事件链
            if (wifi_service->svr_sta_isEnabled()) {
                wifi_service->trigger_event(WIFI_EVENT_STA_DISABLE, NULL);
                goldie_msleep(1000);
            }
            wifi_service->trigger_event(WIFI_EVENT_STA_ENABLE, NULL);
            log_wifi_operation(("等待扫描完成后连接: " + std::string(connect_data.ssid)).c_str());
        }
        
        FrameView_wifipasswd->setVisible(false);
    });


        // 设置WiFi列表点击回调
        ListView_wifilist->setOnItemClick([](int itemId) {
        log_wifi_operation(("点击WiFi列表项: " + std::to_string(itemId)).c_str());
        // 简化实现：使用固定SSID，实际应该根据itemId获取对应的SSID
        current_selected_ssid = "WiFi网络_" + std::to_string(itemId);
        current_selected_ssid_index = itemId;
        log_wifi_operation(("选中SSID: " + current_selected_ssid).c_str());
	TextEditView_wifipasswd->clear();
	if(wifi_service)
	{
		memset(current_passwd,0,100);
		if(wifi_service->svr_check_ap_config(apinfo_list[itemId].ssid,current_passwd) ==0)
		{
			TextEditView_wifipasswd->appendText(current_passwd);
		}
		FrameView_wifipasswd->setVisible(true);
	}
	Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });

    // 获取WiFi服务
    wifi_service = (WifiService*)wait_service(WIFI_SERVICE_INDEX);
    if (wifi_service) {
        // 注册WiFi状态回调
        wifi_service->register_callback(wifi_status_callback);
        log_wifi_operation("WiFi服务已注册回调");
    }

	sdk_engine = convai_bridge_get_engine();
	if (sdk_engine) {
		if(sdk_engine)convai_bridge_on_status(cloud_status_callback);
			if(sdk_engine)convai_bridge_on_event(cloud_event_callback);
		log_wifi_operation("AI服务已注册回调");
	}


     ButtonView_cloudback->setOnClick([](void*) {
        /* on talk page: return to cloud config page; else cloud → main */
        if (talk_page_stop_and_hide()) {
            log_wifi_operation("从对话页返回AI配置页");
        } else {
            /* on cloud page: return to main page, keep engine running */
            log_wifi_operation("从AI服务页面返回主页面 (引擎保持运行)");
            show_main_page();
        }
    });

    /* "进入对话" 按钮：从配置页跳转到对话页 */
    ButtonView_cancle17->setOnClick([](void*) {
		 log_wifi_operation("从APIKEY页面返回");
		 show_cloud_page();
	 });

	 // 设置文本编辑框焦点回调
    TextEditView_apikey->setOnFocusCallback([](TextEditView* textedit) {
		 log_wifi_operation("APIKEY输入框获得焦点");
		 launch_input_method(TextEditView_apikey);
     });

    CheckboxView_cloudsw->setOnClick([](void*) {
	    if (CheckboxView_cloudsw->isLocked()) {
			if(sdk_engine) convai_bridge_restart();
		 }else{
			if(sdk_engine) convai_bridge_stop();
		 }
	 });

    /* 音量按钮回调 */
    Button_bt->setOnClick([](void*) {
        printf("[Volume Settings] Entering volume settings page\n");
        show_volume_page();
    });

    /* 音量返回按钮回调 */
    ButtonView_volume_back->setOnClick([](void*) {
        printf("[Volume Settings] Returning to main page from volume settings\n");
        show_main_page();
    });

    /* 星闪按钮回调 */
    Button_sle->setOnClick([](void*) {
        printf("[SLE Settings] Entering SLE settings page\n");
        show_sle_page();
    });

    /* 星闪返回按钮回调 */
    ButtonView_sle_back->setOnClick([](void*) {
        printf("[SLE Settings] Returning to main page from SLE settings\n");
        show_main_page();
    });




#ifdef SUPPORT_SLE	
    /* 星闪开关回调 */
CheckboxView_sle->setOnClick([](void*) {
    if (CheckboxView_sle->isLocked()) {
        /* 锁定状态 → 启用功能 */
        printf("[SLE Settings] Enabling SLE (checkbox is locked)\n");
        ListView_sle->clearItems();
        sle_service->trigger_event(SLE_EVENT_ENABLE,NULL);
        
    } else {        
        if(sle_service->svr_is_enabled()){
		sle_service->trigger_event(SLE_EVENT_DISABLE,NULL);
		ListView_sle->clearItems();
	}
    }
});
        /* 星闪模式切换按钮回调 */
SpinnerView_sle_mode->setOnItemSelect([](int index, const char* text) {
    printf("[SLE Settings] SLE mode button clicked,index = %d\n",index);
    
    /* 调用封装函数切换模式 */
    if (sle_service != NULL) {
        if(index == 0){
            LabelView_sle_pcode->setVisible(false);
            ListView_sle->setVisible(true);
            sle_service->svr_set_mode(0);
            printf("[SLE Settings] Mode changed to master (0)\n");
        }else{
            char pair_info[32];
            int pair_code = sle_service->svr_s_generate_pair_code();
            sprintf(pair_info,"配对码:%d",pair_code);
            LabelView_sle_pcode->setText(pair_info);
            LabelView_sle_pcode->setVisible(true);
            ListView_sle->setVisible(false);
            sle_service->svr_set_mode(1);            
            printf("[SLE Settings] Mode changed to slave (1)\n");
        }
    }
    
        /* 刷新UI显示 */
        Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
    });

     // 星闪列表点击回调绑定
    ListView_sle->setOnItemClick([](int itemId) {
    printf("[SLE Settings] 点击星闪设备列表项: %d\n", itemId);
    if (sle_service != NULL) {
        if (sle_service->svr_get_mode() == 0) { // 主机模式
            // 保存选中的设备索引（对应WiFi的current_selected_ssid_index）
            current_selected_sle_index = itemId;
            // 获取设备地址信息
            char device_addr[32] = {0};
            snprintf(device_addr, 32, "SLE_%02X%02X%02X",
                     sle_devices[itemId].addr.addr[3],
                     sle_devices[itemId].addr.addr[4],
                     sle_devices[itemId].addr.addr[5]);
            TextEditView_sle_paircode->clear();
	
            printf("[SLE Settings] 选中设备: %s, 索引: %d\n", device_addr, itemId);

	   if(sle_service->svr_get_pair_state(&sle_devices[current_selected_sle_index].addr)){
	   	LabelView_del_sle_dev->setVisible(true);
		TextEditView_sle_paircode->setVisible(false);
        FrameView_sle_pair->setText(" ");
	 }else{
	        LabelView_del_sle_dev->setVisible(false);
		    TextEditView_sle_paircode->setVisible(true);
            FrameView_sle_pair->setText("输入配对码",85, 8);
	   }
	    // 显示配对码输入界面
	  FrameView_sle_pair->setVisible(true);
            // 刷新UI
            Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
        }
    }
});
#endif

     ListView_wifilist->setSelectedIcon((uint16_t*)&rgb16_selected_32_32,32,32);

     ListView_cfgwmlist->setSelectedIcon((uint16_t*)&rgb16_selected_32_32,32,32);
     ListView_cfgwmlist->setOnItemClick([](int itemId) {
        on_ai_setting_changed(itemId, cloud_current_cfg_page);

        /* refresh avatar display (handles both normal update and rollback) */
        LabelView_avashow0->setText(avatar_list[current_avatrid], 24, 2);
        LabelView_pic->setImageBuffer(avatar_pic_list[current_avatrid]);

        show_cloud_page();
    });
}

static void goldie_touch_event(int pressure, int x, int y)
{
    /* PTT 按钮触摸拦截 - 在 cloud 页且 PTT 模式下处理 */
    if (FrameView_cloud->isVisible() &&
        convai_bridge_get_audio_mode() == CONVAI_BRIDGE_AUDIO_PTT &&
        ButtonView_ptttalk->isVisible()) {
        /* 触摸检测区域比视觉按钮大一些，方便按压 (128, 186), 152x40 */
        if (x >= 128 && x <= 128 + 152 && y >= 186 && y <= 186 + 40) {
            if (pressure == 1) {
                /* 按下 */
                printf("[PTT] button pressed\n");
                convai_bridge_ptt_press();
                ButtonView_ptttalk->setText("松开结束");
                Window_main->flush(128, 186, 152, 40);
            } else if (pressure == 0) {
                /* 释放 */
                printf("[PTT] button released\n");
                convai_bridge_ptt_release();
                ButtonView_ptttalk->setText("按住说话");
                Window_main->flush(128, 186, 152, 40);
            }
            return; /* 已处理，不再传递给 Window_main */
        }
    }
    Window_main->handleEvent(pressure, x, y);
}


static void goldie_app_run(void)
{
    main_ui_init();
    init_views();
    convai_image_state_init(&g_image_state, CONVAI_IMAGE_DEFAULT_PATH);
    sdk_engine = convai_bridge_get_engine();
    convai_bridge_on_status(cloud_status_callback);
    convai_bridge_on_event(cloud_event_callback);
    func_handlers_register();
    init_cloud_configs();

    /* save initial default config so convai_start picks it up */
    {
        char *json_buf = (char *)goldie_malloc(CONVAI_CONFIG_JSON_MAX);
        if (json_buf == NULL) {
            printf("[AI Settings] ERROR: initial config alloc failed\n");
        } else {
            memset(json_buf, 0, CONVAI_CONFIG_JSON_MAX);
            if (generate_convai_config_json(json_buf, CONVAI_CONFIG_JSON_MAX) == 0) {
                convai_bridge_set_startup_config(json_buf);
                printf("[AI Settings] Initial default config saved:\n%s\n", json_buf);
            }
            goldie_free(json_buf);
        }
    }

    /* start the talk page module (animation thread + exit semaphore) */
    talk_ui_register_callbacks();
    talk_page_init();

	Window_main->flush(0, 0, APP_WINDOW_WIDTH, APP_WINDOW_HEIGHT);
}

static void goldie_app_suspend(void)
{
    /* pause animation thread */
    talk_page_stop_animation();
    if(sdk_engine)convai_bridge_on_status(NULL);
    if(sdk_engine)convai_bridge_on_event(NULL);
    func_handlers_unregister();
    window_suspend();
}

static void goldie_app_resume(void)
{
    if(sdk_engine)convai_bridge_on_status(cloud_status_callback);
    if(sdk_engine)convai_bridge_on_event(cloud_event_callback);
    func_handlers_register();
    window_resume();
    /* resume animation if talk page is visible */
    if (talk_page_is_visible()) {
        talk_page_play_animation();
    }
}

static void goldie_app_exit(void)
{
    /* stop the talk page module: stop animation, wait for thread exit via
     * semaphore, then destroy the thread handle. Must run BEFORE
     * convai_bridge_stop so the thread's GUI flush doesn't race teardown.
     * (see convai_talk_page.cpp talk_page_deinit for the sem/destroy rationale) */
    talk_page_deinit();
    /* stop engine if the SESSION is active (g_started), NOT sdk_status.
     * sdk_status is conversation PHASE — IDLE after welcome speech while still
     * connected. Using sdk_status!=IDLE would skip stop + leak connection/threads. */
    if (sdk_engine && convai_bridge_is_started()) {
        convai_bridge_stop();
    }
    /* Sync sdk_status from engine directly (async IDLE callback may not arrive
     * before unregistering) so it doesn't leak across exit/reenter. */
    sdk_status = sdk_engine ? convai_bridge_get_status() : CONVAI_STATUS_IDLE;
   if(sdk_engine)convai_bridge_on_status(NULL);
   if(sdk_engine)convai_bridge_on_event(NULL);
   func_handlers_unregister();
   wifi_service->register_callback(NULL);
#ifdef SUPPORT_SLE
    printf("\r\n\r\n[SLE Settings] === Saving SLE config on app exit ===\r\n\r\n");
   /* 清理SLE服务状态 */
   if (sle_service != NULL) {
        /* 注销SLE状态回调 */
        sle_service->register_callback(NULL,SLE_PROFILE_INDEX_SDP);
        printf("[SLE Settings] Unregistered SLE callback on app exit\n");

        /* 停止扫描（如果正在扫描） */
        if (sle_service->svr_is_scanning()) {
            sle_service->trigger_event(SLE_EVENT_STOP_SCAN,NULL);
            printf("[SLE Settings] Stopped scanning on app exit\n");
        }

        /* 停止设备公开（如果正在公开） */
        if (sle_service->svr_is_announcing()) {
            sle_service->trigger_event(SLE_EVENT_STOP_ANNOUNCE,NULL);
            printf("[SLE Settings] Stopped announcing on app exit\n");
        }
   }
#endif
    window_exit();
}

static void goldie_keyboard_event(int pressure, int key);

static App_t my_project = {
    "设置",       // app_name char*
    goldie_app_run,       // h_app_run
    goldie_app_exit,      // h_app_exit
    goldie_app_suspend,   // h_app_suspend
    goldie_app_resume,    // h_app_resume
    goldie_touch_event,   // h_touch_event
    goldie_keyboard_event,// h_keyboard_event
    (uint16_t*)gImage_settings_icon,             
};

static void goldie_keyboard_event(int pressure, int key)
{
    if((key == SYSTEM_KEY_VALUE_BACK) && (pressure == 1))
    {
        /* page-aware BACK navigation */
        if (talk_page_is_visible()) {
            /* talk page → cloud config page */
            printf("[Settings] BACK: talk → cloud\n");
            talk_page_stop_and_hide();
            return;
        }
        if (FrameView_cloud->isVisible()) {
            /* cloud config page → main page (keep engine running) */
            printf("[Settings] BACK: cloud → main\n");
            show_main_page();
            return;
        }
        if (FrameView_config_wm->isVisible()) {
            /* config sub-panel open → close it, back to cloud */
            printf("[Settings] BACK: config_wm → cloud\n");
            show_cloud_page();
            return;
        }
        if (FrameView_wifi->isVisible()) {
            printf("[Settings] BACK: wifi → main\n");
            show_main_page();
            return;
        }
        if (FrameView_volume->isVisible()) {
            printf("[Settings] BACK: volume → main\n");
            show_main_page();
            return;
        }
#ifdef SUPPORT_SLE
        if (FrameView_sle->isVisible()) {
            printf("[Settings] BACK: sle → main\n");
            show_main_page();
            return;
        }
#endif
        /* on main page → exit app */
        printf("[Settings] BACK: main → exit\n");
        /* stop engine on exit — use is_started (session connected), NOT sdk_status
         * (conversation phase). IDLE after welcome speech still has session connected. */
        if (sdk_engine && convai_bridge_is_started()) {
            convai_bridge_stop();
        }
        goldie_exit_app(&my_project);
    }
}

static void test_entry(void)
{
    install_app(&my_project);
}

GOLDIE_INIT_CALL_(test_entry);
