#ifndef WS_BOARD_H
#define WS_BOARD_H
#include <stdint.h>
const char *ws_board_name(void);
void ws_board_prepare(void);
int ws_board_init(void);
/* Hardware layout and board lifecycle only; service lookup belongs to ports. */
typedef struct { unsigned channels, microphone, reference; } ws_board_capture_t;
ws_board_capture_t ws_board_capture_format(void);
int ws_board_display_init(void);
void ws_board_display_write(int x, int y, int width, int height, uint16_t *pixels);
#endif
