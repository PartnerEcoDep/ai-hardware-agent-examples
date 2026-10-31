#include "dhcp_gateway.h"
#include <assert.h>
#include <string.h>
static void put(uint8_t *p,unsigned v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static unsigned sum(const uint8_t *p,size_t n) {
    unsigned s=0;
    for (size_t i=0;i<n;i++) s+=(unsigned)p[i]<<((i&1)?0:8);
    return s;
}
static void fixture(uint8_t *p) {
    memset(p,0,336); p[0]=0x45; p[9]=17; put(p+2,336);
    p[12]=192;p[13]=168;p[14]=4;p[15]=1;
    memset(p+16,255,4);
    put(p+20,67);put(p+22,68);put(p+24,316);
    p[28]=2;memcpy(p+28+236,"\x63\x82\x53\x63",4);
    p[268]=53;p[269]=1;p[270]=2;p[271]=255;
}
int main(void) {
    uint8_t p[336],before[336],gw[]={192,168,4,1};
    fixture(p);memcpy(before,p,sizeof(p));
    assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==1);
    assert(memcmp(p,before,26)==0); /* IP header and UDP lengths unchanged */
    assert(p[271]==3 && p[272]==4 && memcmp(p+273,gw,4)==0 && p[277]==255);
    unsigned s=sum(p+12,8)+17+316+sum(p+20,316);
    while(s>>16) s=(s&65535)+(s>>16);
    assert(s==65535); /* independent verification of UDP pseudo-header checksum */
    memcpy(before,p,sizeof(p));
    assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0);
    assert(memcmp(p,before,sizeof(p))==0); /* no duplicate gateway */
    fixture(p);p[270]=5;assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==1);
    fixture(p);p[270]=6;assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0);
    for(size_t n=0;n<sizeof(p);n++) {
        fixture(p);assert(ws_dhcp_add_gateway(p,n,gw)==0);
    }
    fixture(p);p[6]=0x20;assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0);
    fixture(p);p[21]=68;assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0);
    fixture(p);p[269]=255;assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0);
    fixture(p);p[271]=52;p[272]=1;p[273]=1;p[274]=255;
    assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0); /* overloaded options */
    fixture(p);put(p+2,277);put(p+24,257);
    assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==0); /* no room for router and END */
    fixture(p);put(p+2,335);put(p+24,315);
    assert(ws_dhcp_add_gateway(p,sizeof(p),gw)==1); /* odd UDP length */
    s=sum(p+12,8)+17+315+sum(p+20,315);
    while(s>>16) s=(s&65535)+(s>>16);
    assert(s==65535);
    return 0;
}
