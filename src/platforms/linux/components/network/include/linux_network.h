#ifndef LINUX_NETWORK_H
#define LINUX_NETWORK_H
#include <stdbool.h>
/* An UP/RUNNING non-loopback IP interface is not proof of Internet access. */
bool linux_network_up(void);
#endif
