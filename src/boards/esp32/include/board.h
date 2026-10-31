#ifndef UNIFIED_BOARD_H
#define UNIFIED_BOARD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    unsigned mic_slot;
    unsigned reference_slot;
    bool has_button;
} board_profile_t;
extern const board_profile_t board_profile;
int board_init(void);
/* Native 16-bit, 8 kHz, four interleaved TDM slots; bounded read/write. */
int board_audio_read(int16_t *samples, size_t sample_capacity);
int board_audio_write(const int16_t *samples, size_t sample_count);
bool board_button_down(void);
/* Called only by the touch worker; no LVGL dependency. */
bool board_touch_read(int *x, int *y);
#endif
