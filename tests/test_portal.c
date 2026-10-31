#include "ai_portal.h"
#include <assert.h>
#include <string.h>
static const uint8_t query[] = {0x17,0x58,1,0,0,1,0,0,0,0,0,0,1,'a',0,0,1,0,1};
int main(void) {
    uint8_t p[512], ip[] = {192,168,4,1};
    memcpy(p,query,sizeof(query));
    size_t n = ai_portal_dns_reply(p,sizeof(query),sizeof(p),ip);
    assert(n == sizeof(query)+16 && p[0] == 0x17 && p[1] == 0x58);
    assert(p[2] == 0x81 && p[3] == 0x80 && p[7] == 1);
    assert(memcmp(p+n-4,ip,4) == 0);
    assert(ai_portal_dns_reply(p,n,sizeof(p),ip) == 0); /* no reply loops */
    memcpy(p,query,sizeof(query)); p[16] = 28;
    assert(ai_portal_dns_reply(p,sizeof(query),sizeof(p),ip) == sizeof(query));
    assert(p[7] == 0); /* AAAA is not an IPv4 answer */
    for (size_t i=0; i<sizeof(query); ++i) {
        memcpy(p,query,sizeof(query));
        assert(ai_portal_dns_reply(p,i,sizeof(p),ip) == 0);
    }
    memcpy(p,query,sizeof(query));
    assert(ai_portal_dns_reply(p,sizeof(query),sizeof(query)+15,ip) == 0);
    p[12] = 0xc0;
    assert(ai_portal_dns_reply(p,sizeof(query),sizeof(p),ip) == 0);
    memcpy(p,query,sizeof(query)); p[5] = 2;
    assert(ai_portal_dns_reply(p,sizeof(query),sizeof(p),ip) == 0);
    memcpy(p,query,sizeof(query)); p[11] = 1;
    memset(p+sizeof(query),0,11); /* OPT additional section is not echoed */
    assert(ai_portal_dns_reply(p,sizeof(query)+11,sizeof(p),ip) == sizeof(query)+16);
    assert(p[11] == 0);
    return 0;
}
