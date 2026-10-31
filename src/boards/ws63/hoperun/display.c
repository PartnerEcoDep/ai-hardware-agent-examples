#include "ws_board.h"
#include "driver_manager.h"
#include <stddef.h>
static DispDrv *display;
int ws_board_display_init(void) {
    display=wait_drv(DISP_DRV_INDEX);
    return display && display->lock_region && display->fill_pixels && display->update_region ? 0 : -1;
}
void ws_board_display_write(int x, int y, int width, int height, uint16_t *pixels) {
    size_t count=(size_t)width*height;
    /* BSP consumes RGB565 in panel byte order; same contract in simulator. */
    for (size_t i=0;i<count;++i) pixels[i]=(uint16_t)((pixels[i]<<8)|(pixels[i]>>8));
    display->lock_region(x,y,width,height);
    for (size_t offset=0;offset<count;) {
        size_t chunk=count-offset>2040?2040:count-offset;
        display->fill_pixels(pixels+offset,(uint32_t)chunk);
        offset+=chunk;
    }
    display->update_region();
}
