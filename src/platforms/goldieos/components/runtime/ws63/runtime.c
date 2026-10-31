#include "runtime.h"
#include "ws_board.h"
#include "driver_manager.h"
#include "service_manager.h"
extern int convai_platform_ws63_init(void);
extern int ws63_device_id(char *, size_t);
void ws_runtime_prepare(void) {
    wait_drv(DISP_DRV_INDEX); wait_drv(I2S_DRV_INDEX);
    wait_drv(WLAN_DRV_INDEX); wait_drv(FATFS_DISK_DRV_INDEX);
    ws_board_prepare();
}
int ws_runtime_init(void) { return ws_board_init(); }
int ws_runtime_sync_time(void) {
    NTPService *ntp = wait_service(NTP_SERVICE_INDEX);
    return ntp->sync_time();
}
int ws_runtime_sdk_init(void) { return convai_platform_ws63_init(); }
int ws_runtime_device_id(char *text, size_t size) { return ws63_device_id(text, size); }
