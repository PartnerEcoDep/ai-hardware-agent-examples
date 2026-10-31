#include "dhcp_gateway.h"
#include <string.h>
static unsigned word(const uint8_t *p) { return ((unsigned)p[0]<<8)|p[1]; }
static unsigned sum_bytes(const uint8_t *p, size_t n) {
    unsigned sum=0;
    while (n>=2) { sum+=word(p); p+=2; n-=2; }
    if (n) sum+=(unsigned)p[0]<<8;
    return sum;
}
int ws_dhcp_add_gateway(uint8_t *p, size_t n, const uint8_t gw[4]) {
    if (!p || !gw || n<20 || (p[0]>>4)!=4 || p[9]!=17 ||
        (word(p+6)&0x3fff)) return 0;
    size_t ihl=(p[0]&15)*4, total=word(p+2);
    if (ihl<20 || total>n || total<ihl+8+240) return 0;
    uint8_t *udp=p+ihl, *dhcp=udp+8;
    size_t size=word(udp+4);
    if (word(udp)!=67 || word(udp+2)!=68 || size!=total-ihl ||
        dhcp[0]!=2 || memcmp(dhcp+236,"\x63\x82\x53\x63",4)) return 0;
    size_t end=size-8, at=240;
    unsigned message=0;
    while (at<end && dhcp[at]!=255) {
        unsigned type=dhcp[at++];
        if (!type) continue;
        if (at>=end) return 0;
        unsigned count=dhcp[at++];
        if (count>end-at || type==3 || type==52) return 0;
        if (type==53 && count==1) message=dhcp[at];
        at+=count;
    }
    if (at>=end || end-at<7 || (message!=2 && message!=5)) return 0;
    dhcp[at++]=3; dhcp[at++]=4;
    memcpy(dhcp+at,gw,4); dhcp[at+4]=255;
    /* UDP covers the changed option; IPv4 length/header stay unchanged. */
    udp[6]=udp[7]=0;
    unsigned sum=sum_bytes(p+12,8)+17+(unsigned)size+sum_bytes(udp,size);
    while (sum>>16) sum=(sum&65535)+(sum>>16);
    unsigned checksum=(~sum)&65535;
    if (!checksum) checksum=65535;
    udp[6]=(uint8_t)(checksum>>8); udp[7]=(uint8_t)checksum;
    return 1;
}
