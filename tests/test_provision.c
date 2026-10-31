#include "ai_provision.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool parse(const char *input, size_t expected) {
    char header[2200], method[8], path[128]; size_t length;
    snprintf(header,sizeof(header),"%s",input);
    bool ok = ai_provision_headers(header,method,path,&length);
    if (ok) assert(length == expected);
    return ok;
}
int main(void) {
    assert(parse("GET /scan HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n",0));
    assert(parse("POST /submit HTTP/1.1\r\ncOnTeNt-LeNgTh: 1024\t\r\n\r\n",1024));
    assert(parse("POST /exit HTTP/1.1\r\nContent-Length: 0\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\nContent-Length: 1025\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\nContent-Length: -1\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n",0));
    assert(!parse("POST /submit HTTP/1.1\r\nContent-Length: 999999999999999999999999999999\r\n\r\n",0));
    assert(!parse("GET /scan HTTP/1.1 extra\r\n\r\n",0));
    assert(!parse("GET /scan HTTP/1.1\r\nHeader: truncated",0));
    assert(ai_wifi_index("index=0",1)==0);
    assert(ai_wifi_index("index=9",10)==9);
    assert(ai_wifi_index("index=-1",10)==-1);
    assert(ai_wifi_index("index=10",10)==-1);
    assert(ai_wifi_index("index=1junk",10)==-1);
    assert(ai_wifi_index("index=1&index=2",10)==-1);
    assert(ai_wifi_index("index=999999999999999999999999999999",10)==-1);
    assert(ai_wifi_credentials("quoted\"network\\name",""));
    assert(!ai_wifi_credentials("","password"));
    assert(!ai_wifi_credentials(NULL,"password"));
    assert(!ai_wifi_credentials("123456789012345678901234567890123", "password"));
    ai_wifi_profiles_t saved = {.magic=0x41495731,.generation=1,.count=1};
    strcpy(saved.items[0].ssid,"network"); strcpy(saved.items[0].password,"password");
    saved.checksum=ai_wifi_profiles_checksum(&saved); assert(ai_wifi_profiles_valid(&saved));
    saved.items[0].password[0]^=1; assert(!ai_wifi_profiles_valid(&saved));
    saved.items[0].password[0]^=1;
    saved.count=11; saved.checksum=ai_wifi_profiles_checksum(&saved); assert(!ai_wifi_profiles_valid(&saved));
    saved.count=1; memset(saved.items[0].ssid,'x',33);
    saved.checksum=ai_wifi_profiles_checksum(&saved); assert(!ai_wifi_profiles_valid(&saved));
    return 0;
}
