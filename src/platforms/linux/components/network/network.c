#define _DEFAULT_SOURCE
#include "linux_network.h"
#include <stddef.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
bool linux_network_up(void) {
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces)) return false;
    bool up = false;
    for (struct ifaddrs *it = interfaces; it; it = it->ifa_next) {
        if (!it->ifa_addr || (it->ifa_flags & IFF_LOOPBACK)) continue;
        int family = it->ifa_addr->sa_family;
        if ((family == AF_INET || family == AF_INET6) &&
            (it->ifa_flags & IFF_UP) && (it->ifa_flags & IFF_RUNNING)) {
            up = true; break;
        }
    }
    freeifaddrs(interfaces);
    return up;
}
