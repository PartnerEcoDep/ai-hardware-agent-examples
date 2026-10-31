#include "ai_wifi_store.h"
#include "ai_network_status.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool fail;
static unsigned writes;
static bool save(void *ctx,ai_wifi_profiles_t *next,const ai_wifi_profiles_t *current) {
    (void)ctx;++writes;next->generation=current->generation+1;
    next->magic=0x41495731;next->checksum=ai_wifi_profiles_checksum(next);
    return !fail && ai_wifi_profiles_valid(next);
}
int main(void) {
    ai_wifi_profiles_t p={0},before;
    ai_wifi_store_ops_t ops={.save=save};
    assert(!ai_wifi_store_put(&p,&ops,"","x") && !writes);
    for(unsigned i=0;i<11;++i) { char ssid[16];snprintf(ssid,sizeof(ssid),"network%u",i);assert(ai_wifi_store_put(&p,&ops,ssid,"password")); }
    assert(p.count==10 && !strcmp(p.items[0].ssid,"network10") && !strcmp(p.items[9].ssid,"network1"));
    assert(ai_wifi_store_put(&p,&ops,"network5","updated"));
    assert(!strcmp(p.items[5].ssid,"network5") && !strcmp(p.items[5].password,"updated"));
    before=p;fail=true;
    assert(!ai_wifi_store_remove(&p,&ops,0) && !memcmp(&p,&before,sizeof(p)));
    assert(!ai_wifi_store_default(&p,&ops,4) && !memcmp(&p,&before,sizeof(p)));
    assert(!ai_wifi_store_put(&p,&ops,"new","pass") && !memcmp(&p,&before,sizeof(p)));
    fail=false;assert(ai_wifi_store_default(&p,&ops,5));assert(!strcmp(p.items[0].ssid,"network5"));
    assert(!ai_wifi_store_remove(&p,&ops,99));assert(ai_wifi_store_remove(&p,&ops,0));assert(p.count==9);
    while (p.count) assert(ai_wifi_store_remove(&p,&ops,0));
    assert(!ai_wifi_store_remove(&p,&ops,0));
    ai_network_status_t status={.state=AI_NET_PROVISIONING};
    strcpy(status.setup_ssid,"AI-Setup-test");strcpy(status.setup_url,"http://192.168.4.1");
    char text[128];ai_network_format(&status,text,sizeof(text));assert(!strcmp(text,"Setup: AI-Setup-test\nhttp://192.168.4.1"));
    status.state=AI_NET_HOST_ONLINE;ai_network_format(&status,text,sizeof(text));assert(!strcmp(text,"Windows network: available"));
    char tiny[2]={1,1};ai_network_format(&status,tiny,sizeof(tiny));assert(!tiny[1]);
    ai_network_format(NULL,text,sizeof(text));assert(!text[0]);
    puts("network profile ordering, staged mutations, persistence failures and status formatting passed");
    return 0;
}
