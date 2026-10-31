#include "ai_portal.h"
#include <string.h>

size_t ai_portal_dns_reply(uint8_t *p, size_t n, size_t cap, const uint8_t ip[4]) {
    if (!p || !ip || n > cap || n < 17 || (p[2] & 0xf8) ||
        p[4] != 0 || p[5] != 1) return 0;
    size_t end = 12;
    while (end < n && p[end]) {
        size_t label = p[end];
        if (label > 63 || label + 1 >= n - end) return 0;
        end += label + 1;
    }
    if (end >= n || end - 12 + 1 > 255 || n - end < 5) return 0;
    end += 5;
    if (p[end-2] != 0 || p[end-1] != 1) return 0;
    int a = p[end-4] == 0 && p[end-3] == 1;
    if (a && cap - end < 16) return 0;
    p[2] = (uint8_t)(0x80 | (p[2] & 1));
    p[3] = 0x80;
    memset(p + 6, 0, 6);
    p[7] = (uint8_t)a;
    if (a) {
        const uint8_t answer[] = {0xc0,12,0,1,0,1,0,0,0,28,0,4};
        memcpy(p + end, answer, sizeof(answer));
        memcpy(p + end + sizeof(answer), ip, 4);
        end += 16;
    }
    return end;
}
