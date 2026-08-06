/* ---------- main.c ---------- */
#include <stdio.h>
#include "goldie_osal.h"
#include "service_manager.h"
#include "convai_bridge.h"
#include "driver_manager.h"
#include "app_manager.h"
#ifdef DRV_CORE
#include "driver_core.h"
#include <fcntl.h>
#endif
static BroadcastMessage timer_msg = {0};

// AW9523电源模式检测相关
#define AW9523B_DRV_NAME "aw9523b"
#define AW9523_P1_6 14  
#define AW9523_P1_5 13   

// 电源模式枚举
typedef enum {
    POWER_MODE_NORMAL = 0,     // 正常开机模式
    POWER_MODE_CHARGING_ONLY,  // 仅关机充电模式
} PowerMode;

// 函数声明
static PowerMode detect_power_mode(void);
static void handle_power_mode(PowerMode mode);
static PowerMode power_mode = POWER_MODE_NORMAL;

static AlarmService* alarm_service = NULL;
static AualgoService *aualgo_service = NULL;
/* CloudService replaced by convai_bridge */
#ifdef SUPPORT_SLE
static SleSdpService *sle_sdp_service = NULL;
#endif
static SdCardService *sdcard_service = NULL;

static int wakeup_ai_flag = 0;
static int wakeup_walkie_talkie_flag = 0;

struct tm last_time; 
struct tm current_time; 

NTPService* mntp_service =NULL;
static int time_sync_flag = -1;

#ifdef SUPPORT_BATTERY
#ifdef DRV_CORE
static int battery_fd = -1;
static int AW9523B_fd = -1;
#else
static BatDrv* battery_drv;
#endif
static  int bat_power = 50;
int current_charging_status;
static BroadcastMessage batmsg = {0};
#endif

#define MAX_CMD_LIST_LEN 255
enum TEST_CMD_INDEX{
	TEST_CMD_NOTHING = 0,
	TEST_CMD_PLAY_ANIMATION,
	TEST_CMD_TEST_CHATBOX,
	TEST_DUMP_FB,
	MAX_TEST_CMD_INDEX,
};

static WifiService *wifi_service =NULL;
static EventService *evt_service =NULL;
static AudioService *audio_service = NULL;

static App_t* malarm_app;
static App_t* maitalk_app;
static App_t* mwalkietalkie_app;


static App_t* chatbox_app = NULL;
static App_t* animationplayer_app = NULL;

static void wait_for_drvs(void){
	wait_drv(DISP_DRV_INDEX);	
#ifdef PLATFORM_TYPE_WS63
	wait_drv(FATFS_DISK_DRV_INDEX);
#endif
	wait_drv(I2S_DRV_INDEX);
	wait_drv(WLAN_DRV_INDEX);
}

static void wait_for_services(void){
    wifi_service = wait_service(WIFI_SERVICE_INDEX);
    evt_service = wait_service(EVENT_SERVICE_INDEX);
    audio_service = wait_service(AUDIO_SERVICE_INDEX);
    mntp_service = wait_service(NTP_SERVICE_INDEX);
    aualgo_service = wait_service(AUALGO_SERVICE_INDEX);
}

static void start_boot_animation(void)
{
	printf("start_boot_animation(sp)\n");
	animationplayer_app = wait_app("boot_animation");
	animationplayer_app->h_app_run();
}

static void stop_boot_animation(void)
{
	if(animationplayer_app!=NULL){
		animationplayer_app->h_app_exit();
	}
}

static void start_chatapp(void)
{
	chatbox_app = wait_app("chat_box");
	goldie_run_app(chatbox_app);
}

static void priv_KeyEventHandler(int key){
	if(key == EVENT_KEY_CODE_WAKEUP_AIATLK)
	{
		printf("recv  wakeup event ! \r\n");
		wakeup_ai_flag = 1;
	}else if(key == EVENT_KEY_CODE_SLE_WTP_EVENT){
		printf("recv  sle wtp event ! \r\n");
		wakeup_walkie_talkie_flag = 1;
	}
}

static void register_key_event(void)
{
	if(evt_service != NULL)
	{
		evt_service->register_keyevt_cb(EVENT_KEY_CODE_WAKEUP_AIATLK,priv_KeyEventHandler);
		evt_service->register_keyevt_cb(EVENT_KEY_CODE_SLE_WTP_EVENT,priv_KeyEventHandler);	
	}

}

#ifdef SUPPORT_GPIO_KEYBOARD
 extern   void gpio_keyboard_init();
#else
#ifdef SUPPORT_EXT_GPIO_KEYBOARD
 extern void ext_gpio_keyboard_init();
#endif
#endif

int is_time_changed(struct tm* last,struct tm* current)
{
	if((last->tm_year == current->tm_year)&&(last->tm_mon == current->tm_mon)
		&&(last->tm_mday==current->tm_mday)&&(last->tm_hour==current->tm_hour)
		&&(last->tm_min == current->tm_min))
	{
		return 0;
	}
	return 1;
}

// 检测电源模式函数
static PowerMode detect_power_mode(void)
{
    PowerMode mode = POWER_MODE_NORMAL;
    
#ifdef DRV_CORE
    printf("[Power Mode] Detecting power mode via AW9523 P1_5...\n");
    if (AW9523B_fd >= 0) {
        aw9523b_ioctl_arg_t ioctl_arg;
        // 配置P1_5为输入模式
        ioctl_arg.io_index = AW9523_P1_5;
        ioctl_arg.value = 0; // 输入模式
        goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);
        // 读取P1_5状态
        goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_GET_VAL, &ioctl_arg);
		int p1_5_value = ioctl_arg.value;
        // 根据P1_5电平确定电源模式
        if (p1_5_value != 0) {
            mode = POWER_MODE_NORMAL;
            printf("[Power Mode] Normal boot mode detected (P1_5=HIGH)\n");
        } else {
            mode = POWER_MODE_CHARGING_ONLY;
            printf("[Power Mode] Charging-only mode detected (P1_5=LOW)\n");
        }
    } else {
        printf("[Power Mode] ERROR: Failed to open AW9523 device\n");
    }
#endif
    
    return mode;
}

// 处理电源模式函数
static void handle_power_mode(PowerMode mode)
{
#ifdef DRV_CORE
	aw9523b_ioctl_arg_t ioctl_arg;
    if (AW9523B_fd >= 0) {
        aw9523b_ioctl_arg_t ioctl_arg;
 
        ioctl_arg.io_index = AW9523_P1_6;
        ioctl_arg.value = 1; // 输出模式
        goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);
        
        // 根据电源模式设置P1_6电平
        if (mode == POWER_MODE_NORMAL) {
            // 正常开机模式：设置P1_6为低电平保持供电
            ioctl_arg.value = 0; // 低电平
            goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_VAL, &ioctl_arg);
            printf("[Power Mode] P1_6 set to LOW to maintain power circuit\n");
        } else if (mode == POWER_MODE_CHARGING_ONLY) {
        	// 关机充电模式：设置P1_6为高电平
            ioctl_arg.value = 1; // 高电平
            goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_VAL, &ioctl_arg);
            printf("[Power Mode] P1_6 set to HIGH\n");
        }
    } else {
        printf("[Power Mode] ERROR: Failed to open AW9523 device for control\n");
    }
#endif
}

static int sys_init_Task(void *param){
	int msg_id =0;
	int cmd_index = 0;
	int timeout = 0;
	int hp_status = 0;
	int hp_status_last = 0;
#ifdef SUPPORT_BATTERY	
	int last_power;
	static int last_charging_status = -1;
	int bat_check_timout =30;//上电后更新一次电量状态
#endif	
    param = param;
	wait_for_drvs();

#ifdef SUPPORT_GPIO_KEYBOARD
	gpio_keyboard_init();
#else
#ifdef SUPPORT_EXT_GPIO_KEYBOARD
	ext_gpio_keyboard_init();
#endif
#endif

#ifdef SUPPORT_BATTERY
	#ifdef DRV_CORE
	battery_fd = goldie_open(BAT_DRV_NAME, O_RDONLY);
	batmsg.id = EVENT_BROADCAST_POWER; 
	#else
	battery_drv = wait_drv(BATTERY_DRV_INDEX);
    	battery_drv->init();
	batmsg.id = EVENT_BROADCAST_POWER;
	#endif
#endif
	wait_for_services();
	// ========== 电源模式检测和处理 ==========

	/* Initialize ConvAI SDK bridge with audio source */
    extern void convai_bridge_init(void);
    extern void convai_bridge_set_audio_source(void*, int, int, int);
    extern void convai_bridge_set_device_name(const char *name);

    /* Platform-specific init + device ID injection.
     * The bridge itself is platform-agnostic; each platform registers its
     * platform adapter and supplies a unique device ID here. */
#if defined(PLATFORM_TYPE_WS63)
    /* WS63 hardware: register the WS63 platform adapter, use WiFi MAC as
     * the unique device name. */
    extern int convai_platform_ws63_init(void);
    extern int ws63_device_id(char *buf, size_t len);
    convai_platform_ws63_init();
    {
        char dev_id[32] = {0};
        if (ws63_device_id(dev_id, sizeof(dev_id)) > 0) {
            convai_bridge_set_device_name(dev_id);
        }
    }
#elif defined(PLATFORM_TYPE_WIN)
    /* Desktop simulator (goldieos): register the Windows platform adapter
     * (winsock NetAL + mbedTLS TLSAL + goldie_osal OSAL from libwinvm),
     * use the host name as the device name (parallels WS63 using WiFi MAC). */
    extern int convai_platform_win_init(void);
    extern int win_device_id(char *buf, size_t len);
    convai_platform_win_init();
    {
        char dev_id[32] = {0};
        if (win_device_id(dev_id, sizeof(dev_id)) > 0) {
            convai_bridge_set_device_name(dev_id);
        }
    }
#endif

    convai_bridge_init();
    convai_bridge_set_audio_source(audio_service, 8000, 2, 16);

	#ifdef SUPPORT_BATTERY
		#ifdef DRV_CORE
		AW9523B_fd = goldie_open(AW9523B_DRV_NAME, O_RDWR);
		power_mode = detect_power_mode();
		handle_power_mode(power_mode);
		// 根据电源模式决定后续流程
		if (power_mode == POWER_MODE_CHARGING_ONLY) {
			sle_sdp_service = wait_service(SLE_SDP_SERVICE_INDEX);
			sdcard_service = wait_service(SDCARD_SERVICE_INDEX);
			//关闭不需要的服务
			wifi_service->trigger_event(WIFI_EVENT_STA_DISABLE, NULL); //关闭wifi服务
			convai_bridge_stop();      // 停止AI服务
			sle_sdp_service->svr_disable();//关闭星闪服务
			if(sdcard_service->IsSdCardExists())//卸载SD卡
			{
				sdcard_service->unmount_disk();
			}
			
			#ifdef ST7789_SPI_LCD
				// 关机充电模式：启动charging_only应用（关机充电应用）
				printf("[System Init] Starting charging-only mode: launching charging_only app...\n");
				// 启动charging_only应用
				App_t* charging_only_app = wait_app("charging_only");
				if (charging_only_app) {
					printf("[System Init] charging_only app found, starting...\n");
					goldie_run_app(charging_only_app);
					aw9523b_ioctl_arg_t ioctl_arg;
					// 配置P1_5为输入模式
					ioctl_arg.io_index = AW9523_P1_5;
					ioctl_arg.value = 0; // 输入模式
					goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);
					while(1){
						// 读取P1_5状态
						goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_GET_VAL, &ioctl_arg);
						int p1_5_value = ioctl_arg.value;				
						// 根据P1_5电平确定电源模式
						if (p1_5_value == 1) {
							goldie_close(AW9523B_fd);
							//重启
							hal_reboot_chip();
						} 
						goldie_msleep(100);
					}
					return 0;
				}
			#elif defined GC9D01_SPI_LCD
				aw9523b_ioctl_arg_t ioctl_arg;
				// 配置P1_5为输入模式
				ioctl_arg.io_index = AW9523_P1_5;
				ioctl_arg.value = 0; // 输入模式
				goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg);
				while(1){
					// 读取P1_5状态
					goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_GET_VAL, &ioctl_arg);
					int p1_5_value = ioctl_arg.value;				
					// 根据P1_5电平确定电源模式
					if (p1_5_value == 1) {
						goldie_close(AW9523B_fd);
						//重启
						hal_reboot_chip();
					} 
					goldie_msleep(100);
				}
				return 0;
			#endif

		}
		#endif
	#endif

	// ========== 电源模式处理结束 ==========

#ifdef GC9D01_SPI_LCD
	goldie_msleep(1000);
	aualgo_service->run();

		App_t* dualscreen_ai_app = wait_app("dualscreen_ai");
		if (dualscreen_ai_app) {
			goldie_run_app(dualscreen_ai_app);
		}
	
#else
	start_boot_animation();
	alarm_service =  (AlarmService*)get_service(ALARM_SERVICE_INDEX);
	if (alarm_service) {
		alarm_service->init();  // 从 Flash 加载闹钟到服务内部存储
	}
	goldie_msleep(3000);
	//启动launch
	while(!wifi_service->svr_sta_isConnected())
	{
		goldie_msleep(100);
		timeout++;
		if(timeout > 20)
		{
			break;
		}
	}
	register_key_event();
	stop_boot_animation();
	start_launcher_app();		
#endif
	
	while(1)
	{

#ifdef SUPPORT_BATTERY
#ifdef DRV_CORE
		if (AW9523B_fd >= 0) {
			aw9523b_ioctl_arg_t ioctl_arg;
			// 读取P1_5状态
			ioctl_arg.io_index = AW9523_P1_5;
			goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_GET_VAL, &ioctl_arg);
			int p1_5_value = ioctl_arg.value;
			// 如果P1_5为低电平（0），则执行关机流程
			
			 if (p1_5_value == 0) {
				printf("[System Init] Shutdown signal detected (P1_5 low), starting shutdown process...\n");
				#ifdef ST7789_SPI_LCD
				// 启动shut_down应用
				App_t* shut_down_app = wait_app("shut_down");
				if (shut_down_app) {
				printf("[System Init] shut_down app found, starting...\n");
				goldie_run_app(shut_down_app);
				}
				#endif
				// 1. 广播关机消息
				BroadcastMessage shutdown_msg = {0};
				shutdown_msg.id = EVENT_BROADCAST_SHUTDOWN;
				shutdown_msg.msg = NULL;
				shutdown_msg.msg_len = 0;
				if (evt_service) {
					evt_service->send_broadcast(&shutdown_msg);
					printf("[System Init] Shutdown broadcast sent\n");
				}
				
				// 2. 卸载SD卡
				if (sdcard_service && sdcard_service->IsSdCardExists()) {
					printf("[System Init] Unmounting SD card...\n");
					sdcard_service->unmount_disk();
					printf("[System Init] SD card unmounted\n");
				}
				goldie_msleep(1000);
				//3. 系统断电
				ioctl_arg.io_index = AW9523_P1_6;
				ioctl_arg.value = 1; 
				goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_DIR, &ioctl_arg); // 先设置为输出模式
				ioctl_arg.value = 1;
				goldie_ioctl(AW9523B_fd, AW9523B_IOCTL_SET_VAL, &ioctl_arg); 
				printf("[System Init] AW9523_P1_6 set to low, system power off\n");
				

				//4. 关闭电池设备
				goldie_close(AW9523B_fd);
				AW9523B_fd = -1;
				
				//系统进入休眠或停止状态
				printf("[System Init] System shutdown complete\n");

				// 5. 重启
				hal_reboot_chip();

			 }
		}
#endif
#endif

		#ifndef GC9D01_SPI_LCD
	    if(wakeup_ai_flag)
	    {
			goldie_run_app(maitalk_app);
			wakeup_ai_flag =0;
	    }else if(wakeup_walkie_talkie_flag)
	    {
			goldie_run_app(mwalkietalkie_app);
			wakeup_walkie_talkie_flag =0;
	    }
		#endif
#ifdef SUPPORT_EXT_GPIO_PA	
#ifdef SUPPOR_EXT_GPIO_HP_DET
		hp_status = gpio_ext_drv->get_gpio_value(HP_DET_EXT_PIN);
		if(hp_status_last  != hp_status)
		{
			if(hp_status){
				evt_service->send_keyevent(EVENT_KEY_CODE_HEADSET_INSERT);
			}else{
				evt_service->send_keyevent(EVENT_KEY_CODE_HEADSET_REMOVE);
			}
			hp_status_last = hp_status;
			printf("current hp status :%d \r\n",hp_status_last);
		}
#endif
#endif

#ifdef SUPPORT_BATTERY
	#ifdef DRV_CORE
	if (battery_fd >= 0)
	#else
	if(battery_drv)
	#endif
	{
		bat_check_timout++;
		if(bat_check_timout>30){
			#ifdef DRV_CORE
			goldie_ioctl(battery_fd,BAT_IOCTL_CAL_POWER,NULL);
			int ret = goldie_read(battery_fd, &bat_power, sizeof(bat_power));
			printf("new bat_power:%d \r\n",bat_power);
			#else
			battery_drv->calc_soc();
			bat_power = battery_drv->read_power();
			printf("bat_power:%d \r\n",bat_power);
			#endif

			if((bat_power/10) != last_power)
			{
				batmsg.ext[0]= bat_power;
				batmsg.msg_len = sizeof(int);
				printf("send broadcast \r\n");
				evt_service->send_broadcast(&batmsg);
				printf("send broadcast end \r\n");
				last_power = bat_power/10;
			}
			bat_check_timout = 0;
		}
		#ifdef DRV_CORE
		goldie_ioctl(battery_fd,BAT_IOCTL_GET_CHGSTATUS,&current_charging_status);
		#else
		current_charging_status = battery_drv->is_charging();
		#endif
		if (current_charging_status != last_charging_status) {
			printf("charging status changed: %d -> %d. Sending broadcast.\n", 
					last_charging_status, current_charging_status);
			evt_service->send_broadcast(&batmsg);
			last_charging_status = current_charging_status;
		}
	}
#endif
        if(time_sync_flag <0){
#ifdef PLATFORM_TYPE_WS63			
		if(wifi_service->svr_sta_isConnected()){
			time_sync_flag = mntp_service->sync_time();
		}else{
			printf("network is disconnected \r\n");
		}
#else
	if(wifi_service->svr_sta_isConnected()){
		time_sync_flag = mntp_service->sync_time();
	}
#endif
	}
	mntp_service->get_time(&current_time);
        if(is_time_changed(&last_time,&current_time))
        {
			#ifndef GC9D01_SPI_LCD
			if(alarm_service->task_loop(current_time.tm_hour+8,current_time.tm_min,current_time.tm_wday)){
				if (malarm_app && malarm_app->app_status == APP_STATUS_RUNNING) {
					goldie_exit_app(malarm_app);
				}
				goldie_run_app(malarm_app);
			}
			#endif
        		printf("update time \r\n");
		printf("update to  time: %04d-%02d-%02d %02d:%02d:%02d (Week%d)\r\n",
			   current_time.tm_year+1900, current_time.tm_mon+1, current_time.tm_mday,
			   current_time.tm_hour, current_time.tm_min, current_time.tm_sec,
			   current_time.tm_wday);	
		
		timer_msg.msg = &current_time;
		timer_msg.msg_len = sizeof(current_time);
		memcpy(&last_time,&current_time,sizeof(current_time));
         }else{
		timer_msg.msg_len = 0;
	}
	

	timer_msg.id = EVENT_BROADCAST_TIMERUPDATE;
	evt_service->send_broadcast(&timer_msg);

	goldie_msleep(1000);
	}

	#ifdef DRV_CORE
	goldie_close(battery_fd);
	#endif
	return 0;
}

static void *task_handle = NULL;
static void system_init_entry(void){
	memset(&current_time,0,sizeof(current_time));
	memset(&last_time,0,sizeof(last_time));
	goldie_thread_lock();
	task_handle = goldie_thread_create(sys_init_Task, 0, "sys_init_Task", 0x2000);
	if (task_handle != NULL) {
		goldie_thread_set_priority(task_handle, 24);
	}
	goldie_thread_unlock();
}

GOLDIE_INIT_CALL_(system_init_entry);



