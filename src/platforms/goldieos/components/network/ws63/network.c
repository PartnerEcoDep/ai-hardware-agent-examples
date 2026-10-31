#include "ws_network.h"
#include "ai_network_status.h"
#include "ws_wifi_abi.h"
#include "ai_provision.h"
#include "ai_wifi_store.h"
#include "ai_portal.h"
#include "goldie_osal.h"
#include "driver_manager.h"
#include "service_manager.h"
#include "wifi_store.h"
#include "cJSON.h"
typedef struct poll_table poll_table;
#include "lwip/sockets.h"
#include "lwip/netifapi.h"
#include "lwip/dns.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stddef.h>
#include <errno.h>

extern unsigned int sys_now(void);
int ws_portal_gateway_start(struct netif *netif);
void ws_portal_gateway_stop(struct netif *netif);
extern const char ws_wifi_html[], ws_wifi_done_html[];
static Wlan_Drv *wlan;
static WifiService *service;
static bool provision;
static atomic_bool enabled = true, configuring, online, dns_ready;
static goldie_mutex status_lock;
static ai_network_status_t network_status = {.state = AI_NET_STARTING};
static char hotspot[32];
static int listener = -1;
static ip_addr_t saved_dns[DNS_MAX_SERVERS];
static bool dns_overridden;
static bool portal_dns(void) {
    ip_addr_t local;
    ipaddr_aton("192.168.4.1", &local);
    for (unsigned i=0; i<DNS_MAX_SERVERS; ++i) {
        ip_addr_t current;
        err_t rc = lwip_dns_getserver(i,&current);
        if (rc == ERR_OK && !ip_addr_cmp(&current,&local)) rc = lwip_dns_setserver(i,&local);
        if (rc != ERR_OK) { printf("[portal] DNS configuration failed: slot=%u rc=%d\n",i,(int)rc); return false; }
    }
    return true;
}
/* The HTTP reply can be lost while APSTA changes channel. Retain the result
 * so the browser can reconcile its request without submitting credentials twice. */
static char connect_id[33];
static const char *connect_result = "{\"success\":false,\"error\":\"No matching connection request\"}";
/* Two alternating, checksummed snapshots preserve the previous credentials
 * if power is lost during a write. Stored in internal NV; no SD required.
 * Only the network task accesses profiles. */
typedef ai_wifi_profile_t profile;
typedef ai_wifi_profiles_t profile_store;
static profile_store profiles;
static void load_profiles(void) { ws_wifi_store_load(&profiles); }
static bool persist(void *ctx, profile_store *next, const profile_store *current) {
    (void)ctx; return ws_wifi_store_save(next, current);
}
static const ai_wifi_store_ops_t store_ops = {.save = persist};
static bool save_profile(const char *ssid, const char *password) {
    return ai_wifi_store_put(&profiles, &store_ops, ssid, password);
}
static void status(ai_network_state_t state, const char *address) {
    ai_network_status_t next = {.state = state};
    if (address) snprintf(next.address, sizeof(next.address), "%s", address);
    if (state == AI_NET_PROVISIONING) {
        snprintf(next.setup_ssid, sizeof(next.setup_ssid), "%s", hotspot);
        snprintf(next.setup_url, sizeof(next.setup_url), "http://192.168.4.1");
    }
    goldie_mutex_lock(&status_lock); network_status = next; goldie_mutex_unlock(&status_lock);
}
static void ws_network_status(ai_network_status_t *value) {
    if (!value) return;
    goldie_mutex_lock(&status_lock); *value = network_status; goldie_mutex_unlock(&status_lock);
    /* A just-requested provisioning transition must not report connected. */
    if (value->state == AI_NET_CONNECTED && !ws_network_connected()) value->state = AI_NET_CONNECTING;
}
void ws_network_description(char *text, size_t capacity) {
    ai_network_status_t value; ws_network_status(&value); ai_network_format(&value, text, capacity);
}
bool ws_network_connected(void) { return atomic_load(&online) && !atomic_load(&configuring); }
void ws_network_provision(void) {
    atomic_store(&online, false); goldie_mutex_lock(&status_lock); provision = true; goldie_mutex_unlock(&status_lock);
}
static bool take_provision(void) {
    goldie_mutex_lock(&status_lock); bool pending = provision; provision = false; goldie_mutex_unlock(&status_lock); return pending;
}
void ws_network_enable(bool value) { atomic_store(&enabled, value); if (!value) atomic_store(&online, false); }
static int bind_socket(int type, int port) {
    int fd = socket(AF_INET, type, 0);
    if (fd < 0) return -1;
    struct sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_port = (uint16_t)((port << 8) | (port >> 8));
    addr.sin_addr.s_addr = inet_addr("192.168.4.1");
    int reuse = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { closesocket(fd); return -1; }
    return fd;
}
static void stop_ap(void) {
    atomic_store(&configuring, false);
    /* Let the DNS owner close its socket before removing/recreating ap0. */
    unsigned wait_started = sys_now();
    while (atomic_load(&dns_ready) && sys_now()-wait_started < 1000) goldie_msleep(20);
    if (listener >= 0) { closesocket(listener); listener = -1; }
    struct netif *ap = netifapi_netif_find("ap0");
    if (ap) netifapi_dhcps_stop(ap);
    if (ap) ws_portal_gateway_stop(ap);
    wifi_softap_disable();
    if (dns_overridden) {
        for (unsigned i=0; i<DNS_MAX_SERVERS; ++i) lwip_dns_setserver(i,&saved_dns[i]);
        dns_overridden = false;
    }
}
static bool start_ap(void) {
    ws_softap_config cfg = {0}; snprintf((char *)cfg.ssid, sizeof(cfg.ssid), "%s", hotspot);
    cfg.security_type = 0; cfg.channel_num = 1;
    if (wifi_softap_enable(&cfg) != 0) return false;
    /* Vendor DHCP option 6 uses the global resolver, not the AP interface.
     * Override only during provisioning; station DHCP refreshes it after exit. */
    for (unsigned i=0; i<DNS_MAX_SERVERS; ++i) {
        if (lwip_dns_getserver(i,&saved_dns[i]) != ERR_OK) { wifi_softap_disable(); return false; }
    }
    dns_overridden = true;
    if (!portal_dns()) { stop_ap(); return false; }
    struct netif *ap = netifapi_netif_find("ap0");
    ip4_addr_t ip, mask; IP4_ADDR(&ip, 192,168,4,1); IP4_ADDR(&mask,255,255,255,0);
    if (!ap) { stop_ap(); return false; }
    netifapi_dhcps_stop(ap);
    if (netifapi_netif_set_addr(ap, &ip, &mask, &ip) != ERR_OK) { stop_ap(); return false; }
    if (!ws_portal_gateway_start(ap)) { stop_ap(); return false; }
    listener = bind_socket(SOCK_STREAM, 80);
    if (listener < 0 || listen(listener, 2) < 0) { stop_ap(); return false; }
    unsigned long nonblocking = 1;
    if (lwip_ioctl(listener, FIONBIO, &nonblocking) < 0) { stop_ap(); return false; }
    atomic_store(&configuring, true); atomic_store(&online, false);
    /* A client starts connectivity probes as soon as DHCP completes. Do not
     * hand out a lease before both portal listeners are actually ready. */
    unsigned dns_started = sys_now();
    while (!atomic_load(&dns_ready) && sys_now()-dns_started < 2000) goldie_msleep(20);
    if (!atomic_load(&dns_ready) || netifapi_dhcps_start(ap,NULL,0) != ERR_OK) {
        stop_ap(); return false;
    }
    status(AI_NET_PROVISIONING, NULL);
    return true;
}
static void scan_wait(void) {
    if (wlan->sta_scan() != 0) return;
    unsigned start = sys_now();
    do { goldie_msleep(100); } while (!wlan->sta_is_scan_done() && sys_now() - start < 5000);
}
static bool connect_profile(const char *ssid, const char *password) {
    ApInfo_t ap = {0}; snprintf(ap.ssid, sizeof(ap.ssid), "%s", ssid); snprintf(ap.passwd, sizeof(ap.passwd), "%s", password);
    /* The BSP's connect routine matches against its latest scan cache. */
    scan_wait(); int rc = wlan->sta_connect(&ap, 10000); memset(ap.passwd, 0, sizeof(ap.passwd));
    return rc == 0 && wlan->sta_isConneted();
}
static bool send_all(int fd, const char *data, size_t length) {
    while (length) { int n = send(fd, data, length, 0); if (n <= 0) return false; data += n; length -= n; }
    return true;
}
static void reply(int fd, int code, const char *type, const char *body) {
    char head[256]; size_t length = strlen(body);
    int n = snprintf(head, sizeof(head), "HTTP/1.1 %d %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %u\r\nCache-Control: " AI_PORTAL_CACHE_CONTROL "\r\nConnection: close\r\n\r\n",
                     code, code == 200 ? "OK" : "Bad Request", type, (unsigned)length);
    if (send_all(fd, head, (size_t)n)) send_all(fd, body, length);
}
static void json_reply(int fd, cJSON *json) {
    char *text = cJSON_PrintUnformatted(json);
    reply(fd, text ? 200 : 500, "application/json", text ? text : "{\"success\":false}"); cJSON_free(text); cJSON_Delete(json);
}
static void result(int fd, bool success) {
    reply(fd, 200, "application/json", success ? "{\"success\":true}" : "{\"success\":false,\"error\":\"Saving networks failed. Internal storage write failed; retry.\"}");
}
static void redirect_portal(int fd) {
    static const char response[] = "HTTP/1.1 " AI_PORTAL_HTTP_STATUS "\r\nLocation: http://192.168.4.1/\r\nCache-Control: " AI_PORTAL_CACHE_CONTROL "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    send_all(fd, response, sizeof(response)-1);
}
/* Bounded HTTP/1.1 request: no chunked bodies, pipelining or keep-alive. */
static bool request(int fd, char *method, char *path, char *body) {
    char header[2049]; size_t used = 0; unsigned started = sys_now();
    while (used < sizeof(header) - 1) {
        if (sys_now() - started > 5000) return false;
        if (recv(fd, header + used, 1, 0) != 1) return false;
        header[++used] = 0;
        if (used >= 4 && !memcmp(header + used - 4, "\r\n\r\n", 4)) break;
    }
    size_t length;
    if (used == sizeof(header)-1 || !ai_provision_headers(header,method,path,&length)) return false;
    for (size_t n = 0; n < length;) {
        if (sys_now() - started > 5000) return false;
        int got = recv(fd, body+n, length-n, 0); if (got <= 0) return false; n += got;
    }
    body[length] = 0; return true;
}
static bool handle_http(int fd) {
    char method[8], path[128], body[1025]; bool exit_ap = false;
    struct timeval timeout = {5,0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (!request(fd, method, path, body)) { reply(fd,400,"text/plain","Invalid request"); return false; }
    /* Paths only: never log passwords, headers, request IDs or POST bodies. */
    if (!strcmp(method,"POST") && !strcmp(path,"/submit")) {
        cJSON *json = cJSON_Parse(body), *ssid = cJSON_GetObjectItemCaseSensitive(json,"ssid"), *password = cJSON_GetObjectItemCaseSensitive(json,"password");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(json,"request_id");
        connect_id[0] = 0;
        if (cJSON_IsString(id) && strlen(id->valuestring) <= 32)
            snprintf(connect_id,sizeof(connect_id),"%s",id->valuestring);
        bool ok = cJSON_IsString(ssid) && cJSON_IsString(password) && ai_wifi_credentials(ssid->valuestring,password->valuestring);
        if (!ok) connect_result = "{\"success\":false,\"error\":\"Invalid network name or password length.\"}";
        else if (!connect_profile(ssid->valuestring,password->valuestring))
            connect_result = "{\"success\":false,\"error\":\"Wi-Fi connection failed. Check password and 2.4 GHz signal.\"}";
        else if (!save_profile(ssid->valuestring,password->valuestring))
            connect_result = "{\"success\":false,\"error\":\"Wi-Fi connected, but saving failed. Internal storage write failed; retry.\"}";
        else connect_result = "{\"success\":true}";
        portal_dns(); /* Station DHCP may have replaced the advertised DNS. */
        reply(fd,200,"application/json",connect_result);
        cJSON_Delete(json);
    } else if (!strcmp(method,"POST") && !strcmp(path,"/exit")) {
        result(fd,true); exit_ap = true;
    } else if (strcmp(method,"GET")) reply(fd,400,"text/plain","Unsupported method");
    else if (!strncmp(path,"/connection/status?id=",22)) {
        if (connect_id[0] && !strcmp(path+22,connect_id)) reply(fd,200,"application/json",connect_result);
        else reply(fd,404,"application/json","{\"success\":false,\"error\":\"Unknown request\"}");
    }
    else if (!strcmp(path,"/scan")) {
        scan_wait(); ApInfo_t aps[MAX_AP_NUM] = {0}; int count = wlan->sta_get_ap_list(aps);
        cJSON *json = cJSON_CreateObject(), *list = cJSON_AddArrayToObject(json,"aps"); cJSON_AddBoolToObject(json,"support_5g",false);
        for (int i = 0; i < count && i < MAX_AP_NUM; ++i) {
            aps[i].ssid[32] = 0; cJSON *ap = cJSON_CreateObject(); cJSON_AddStringToObject(ap,"ssid",aps[i].ssid);
            cJSON_AddNumberToObject(ap,"rssi",aps[i].rssi); cJSON_AddItemToArray(list,ap);
        }
        json_reply(fd,json);
    } else if (!strcmp(path,"/saved/list")) {
        cJSON *json = cJSON_CreateArray();
        for (unsigned i = 0; i < profiles.count; ++i) cJSON_AddItemToArray(json,cJSON_CreateString(profiles.items[i].ssid));
        json_reply(fd,json);
    } else if (!strncmp(path,"/saved/delete?index=",19) || !strncmp(path,"/saved/set_default?index=",24)) {
        bool remove = path[7] == 'd'; int index = ai_wifi_index(strchr(path,'?')+1,profiles.count);
        if (index < 0) result(fd,false);
        else {
            result(fd, remove ? ai_wifi_store_remove(&profiles, &store_ops, (unsigned)index)
                              : ai_wifi_store_default(&profiles, &store_ops, (unsigned)index));
        }
    } else if (!strcmp(path,"/done.html")) reply(fd,200,"text/html",ws_wifi_done_html);
    else if (!strcmp(path,"/") || !strncmp(path,"/?",2)) reply(fd,200,"text/html",ws_wifi_html);
    /* Includes Android/iOS/Windows HTTP connectivity probes. */
    else redirect_portal(fd);
    memset(body,0,sizeof(body)); return exit_ap;
}
static int dns_task(void *ctx) {
    (void)ctx; int fd = -1;
    bool bind_error_logged = false;
    for (;;) {
        if (!atomic_load(&configuring)) { if (fd >= 0) closesocket(fd); fd = -1; atomic_store(&dns_ready,false); bind_error_logged=false; goldie_msleep(100); continue; }
        if (fd < 0) {
            fd = bind_socket(SOCK_DGRAM,53);
            if (fd < 0) {
                if (!bind_error_logged) printf("[portal] DNS bind failed: errno=%d\n",errno);
                bind_error_logged=true; goldie_msleep(500); continue;
            }
            unsigned long nonblocking = 1;
            if (lwip_ioctl(fd,FIONBIO,&nonblocking) < 0) { closesocket(fd); fd=-1; goldie_msleep(100); continue; }
            atomic_store(&dns_ready,true);
        }
        unsigned char packet[512]; struct sockaddr_in peer; socklen_t size = sizeof(peer);
        int n = recvfrom(fd,packet,sizeof(packet),0,(struct sockaddr *)&peer,&size);
        if (n <= 0) { goldie_msleep(20); continue; }
        const uint8_t portal_ip[4] = {192,168,4,1};
        int end = (int)ai_portal_dns_reply(packet,(size_t)n,sizeof(packet),portal_ip);
        if (!end) continue;
        sendto(fd,packet,end,0,(struct sockaddr *)&peer,size);
    }
    return 0;
}
static int network_task(void *ctx) {
    (void)ctx; load_profiles();
    if (wlan->sta_isEnabled()) wlan->sta_disable();
    wlan->sta_enable();
    unsigned since = sys_now(), last_try = since - 10000, next_profile = 0;
    if (!profiles.count) ws_network_provision();
    for (;;) {
        if (!atomic_load(&enabled)) {
            if (atomic_load(&configuring)) stop_ap();
            wlan->sta_disable();
            while (!atomic_load(&enabled)) goldie_msleep(100);
            wlan->sta_enable(); since = sys_now(); next_profile = 0;
        }
        if (take_provision() && !atomic_load(&configuring)) {
            if (!start_ap()) { status(AI_NET_SETUP_FAILED, NULL); since = sys_now(); }
        }
        if (atomic_load(&configuring)) {
            static unsigned dns_checked;
            if (sys_now()-dns_checked >= 1000) { portal_dns(); dns_checked = sys_now(); }
            /* Do not depend on vendor select() blocking: an immediate return
             * previously left this priority-10 task spinning at 96% CPU. */
            {
                int fd = accept(listener,NULL,NULL);
                if (fd >= 0) {
                    unsigned long blocking = 0;
                    if (lwip_ioctl(fd, FIONBIO, &blocking) < 0) {
                        closesocket(fd); goldie_msleep(20); continue;
                    }
                    bool leave = handle_http(fd); closesocket(fd);
                    if (leave) {
                        goldie_msleep(200); stop_ap(); wlan->sta_disable(); wlan->sta_enable();
                        since = sys_now(); next_profile = 0; last_try = since - 1000;
                    }
                }
            }
            /* Yield on idle, errors AND successful requests. Lower-priority
             * BSP input/watchdog tasks must run even under HTTP traffic. */
            goldie_msleep(20);
            continue;
        }
        bool connected = wlan->sta_isConneted() != 0;
        atomic_store(&online,connected);
        if (connected) {
            char ip[64] = {0}; service->svr_get_netddr(ip,sizeof(ip));
            status(AI_NET_CONNECTED, ip); since = sys_now(); next_profile = 0;
        } else {
            status(AI_NET_CONNECTING, NULL);
            if (!profiles.count || sys_now()-since >= 30000) ws_network_provision();
            else if (sys_now()-last_try >= 1000 && next_profile < profiles.count) {
                profile *p = &profiles.items[next_profile++]; connect_profile(p->ssid,p->password); last_try = sys_now();
            }
        }
        goldie_msleep(100);
    }
    return 0;
}
int ws_network_init(void) {
    goldie_mutex_init(&status_lock); wlan = wait_drv(WLAN_DRV_INDEX); service = wait_service(WIFI_SERVICE_INDEX);
    if (!wlan || !service) return -1;
    uint8_t mac[6] = {0}; service->svr_get_hwddr(mac,sizeof(mac));
    snprintf(hotspot,sizeof(hotspot),"AI-Setup-%02X%02X",mac[4],mac[5]);
    if (!goldie_thread_create(dns_task,NULL,"ai_dns",4096)) return -1;
    return goldie_thread_create(network_task,NULL,"ai_wifi",16384) ? 0 : -1;
}
