#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>
#include "runtime.h"
#include "ws_board.h"
#include "driver_manager.h"
extern int convai_platform_win_init(void);
extern int win_device_id(char *, size_t);
unsigned int sys_now(void) { return GetTickCount(); }
void ws_runtime_prepare(void) {
    wait_drv(DISP_DRV_INDEX); wait_drv(I2S_DRV_INDEX);
    ws_board_prepare();
}
int ws_runtime_init(void) { return ws_board_init(); }
int ws_runtime_sync_time(void) { return time(NULL) >= 1704067200 ? 0 : -1; }
int ws_runtime_sdk_init(void) { return convai_platform_win_init(); }
int ws_runtime_device_id(char *text, size_t size) { return win_device_id(text,size); }
