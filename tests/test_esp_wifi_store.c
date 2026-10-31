#include "esp_wifi_store.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ai_wifi_profiles_t disk[2], pending;
static bool present[2], namespace_exists, legacy, fail_open, fail_read, fail_write, fail_commit, bad_readback;
static int pending_slot = -1;
static unsigned slot(const char *key) { assert(!strcmp(key,"profiles0") || !strcmp(key,"profiles1")); return (unsigned)(key[8]-'0'); }
esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *h) {
    assert(!strcmp(name,"wifi")); *h=1;
    if (fail_open) return ESP_FAIL;
    if (!namespace_exists && mode==NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
    namespace_exists=true; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *data, size_t *size) {
    (void)h; unsigned i=slot(key);
    if(fail_read) return ESP_FAIL;
    if(!present[i]) return ESP_ERR_NVS_NOT_FOUND;
    assert(*size>=sizeof(disk[i])); *size=sizeof(disk[i]); memcpy(data,&disk[i],*size);
    if(bad_readback) ((ai_wifi_profiles_t*)data)->checksum ^= 1;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size) {
    (void)h; assert(size==sizeof(pending));
    if(fail_write) return ESP_FAIL;
    pending_slot=(int)slot(key); memcpy(&pending,data,size); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    (void)h; if(fail_commit) return ESP_FAIL;
    assert(pending_slot>=0); disk[pending_slot]=pending;present[pending_slot]=true;pending_slot=-1;return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h;pending_slot=-1; }
esp_err_t nvs_get_str(nvs_handle_t h,const char *key,char *text,size_t *size) {
    (void)h; const char *value = !strcmp(key,"ssid") ? "legacy-network" : !strcmp(key,"password") ? "example-password" : NULL;
    if(!legacy || !value) return ESP_ERR_NVS_NOT_FOUND;
    assert(*size>strlen(value));strcpy(text,value);*size=strlen(value)+1;return ESP_OK;
}
int main(void) {
    ai_wifi_profiles_t current,next,loaded;
    assert(esp_wifi_store_load(&current) && !current.count);
    namespace_exists=legacy=true;
    assert(esp_wifi_store_load(&current) && current.count==1 && !strcmp(current.items[0].ssid,"legacy-network"));
    next=current;strcpy(next.items[0].ssid,"new-network");
    fail_open=true;assert(!esp_wifi_store_save(&next,&current));fail_open=false;
    fail_read=true;assert(!esp_wifi_store_save(&next,&current));fail_read=false;
    fail_write=true;assert(!esp_wifi_store_save(&next,&current));fail_write=false;
    fail_commit=true;assert(!esp_wifi_store_save(&next,&current));fail_commit=false;
    assert(esp_wifi_store_load(&loaded) && !strcmp(loaded.items[0].ssid,"legacy-network"));
    assert(esp_wifi_store_save(&next,&current));current=next;
    assert(esp_wifi_store_load(&loaded) && !memcmp(&loaded,&current,sizeof(current)));
    next=current;next.count=0;memset(next.items,0,sizeof(next.items));
    assert(esp_wifi_store_save(&next,&current));current=next;
    assert(esp_wifi_store_load(&loaded) && !loaded.count); /* never resurrect legacy credentials */
    disk[current.generation&1].checksum^=1;
    assert(esp_wifi_store_load(&loaded) && loaded.count==1); /* previous good generation */
    current=loaded; next=current;
    disk[(current.generation+1)&1].magic=0x12345678;
    assert(!esp_wifi_store_save(&next,&current)); /* foreign data not overwritten */
    disk[0].magic=0;disk[1].magic=0;
    assert(!esp_wifi_store_load(&loaded)); /* corrupt snapshots don't fall back to stale legacy */
    present[0]=present[1]=false;memset(&current,0,sizeof(current));next=current;
    bad_readback=true;assert(!esp_wifi_store_save(&next,&current));bad_readback=false;
    puts("ESP NVS snapshots, legacy read, failure propagation, CRC fallback and ownership passed");
    return 0;
}
