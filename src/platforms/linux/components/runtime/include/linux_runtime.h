#ifndef LINUX_RUNTIME_H
#define LINUX_RUNTIME_H
#include <stdint.h>
uint32_t linux_now_ms(void);
void linux_sleep_ms(unsigned ms);
#endif
