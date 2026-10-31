#include "ai_provision.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
bool ai_wifi_credentials(const char *ssid, const char *password) {
    return ssid && password && ssid[0] && strlen(ssid) <= 32 && strlen(password) <= 64;
}
int ai_wifi_index(const char *query, unsigned count) {
    if (!query || strncmp(query,"index=",6) || !isdigit((unsigned char)query[6])) return -1;
    char *end; errno = 0; unsigned long index = strtoul(query+6,&end,10);
    return errno || *end || index >= count ? -1 : (int)index;
}
uint32_t ai_wifi_profiles_checksum(const ai_wifi_profiles_t *store) {
    uint32_t crc = 0xffffffff;
    const unsigned char *data = (const unsigned char *)store;
    for (size_t i = 0; i < offsetof(ai_wifi_profiles_t,checksum); ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
bool ai_wifi_profiles_valid(const ai_wifi_profiles_t *p) {
    if (p->magic != 0x41495731 || p->count > AI_WIFI_SAVED_MAX || p->checksum != ai_wifi_profiles_checksum(p)) return false;
    for (unsigned i = 0; i < p->count; ++i)
        if (!p->items[i].ssid[0] || !memchr(p->items[i].ssid,0,33) || !memchr(p->items[i].password,0,65)) return false;
    return true;
}
static bool prefix(const char *s, const char *name) {
    while (*name) if (tolower((unsigned char)*s++) != tolower((unsigned char)*name++)) return false;
    return true;
}
bool ai_provision_headers(char *header, char *method, char *path, size_t *body_length) {
    size_t size = strlen(header);
    if (size > 2048 || size < 4 || strcmp(header+size-4,"\r\n\r\n")) return false;
    char *line = strstr(header,"\r\n"), version[16], extra;
    if (!line) return false;
    *line = 0;
    if (sscanf(header,"%7s %127s %15s %c",method,path,version,&extra) != 3 || path[0] != '/' ||
        (strcmp(version,"HTTP/1.1") && strcmp(version,"HTTP/1.0"))) return false;
    *body_length = 0; bool has_length = false;
    for (line += 2; *line; ) {
        char *end = strstr(line,"\r\n"); if (!end) return false;
        *end = 0;
        if (!*line) break;
        if (*line == ' ' || *line == '\t' || !strchr(line,':') || prefix(line,"Transfer-Encoding:")) return false;
        if (prefix(line,"Content-Length:")) {
            const char *value = line+15; while (*value == ' ' || *value == '\t') ++value;
            if (has_length || !isdigit((unsigned char)*value)) return false;
            char *tail; errno = 0; unsigned long length = strtoul(value,&tail,10);
            if (errno || length > 1024) return false;
            while (*tail == ' ' || *tail == '\t') ++tail;
            if (*tail) return false;
            *body_length = length; has_length = true;
        }
        line = end+2;
    }
    return strcmp(method,"POST") || has_length;
}
