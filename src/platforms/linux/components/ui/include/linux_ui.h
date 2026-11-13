#ifndef LINUX_UI_H
#define LINUX_UI_H
#include <stdbool.h>
int linux_ui_open(const char *title, unsigned scale);
bool linux_ui_pump(void);
bool linux_ui_failed(void);
void linux_ui_close(void);
/* Used by the headless smoke test to exercise the actual SDL/LVGL input path. */
void linux_ui_test_pointer(int x, int y, bool pressed);
#endif
