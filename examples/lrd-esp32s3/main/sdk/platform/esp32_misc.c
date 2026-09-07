/**
 * @file platform/esp32_misc.c
 * @brief Misc layer of the ESP32-S3 ConvAI platform HAL.
 *
 * Logging, device identity and network status. The SDK calls these for
 * diagnostics and for the handshake payload it sends to the cloud.
 */

#include "convai_platform_esp32_internal.h"

#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** Log line budget, including the timestamp/level/location prefix. */
#define LOG_LINE_MAX  256

/** Minimum device-id buffer: 12 hex chars + separators + terminator. */
#define DEVICE_ID_MIN_LEN  18

/** Default netif key created by esp_netif_create_default_wifi_sta(). */
#define STA_NETIF_KEY  "WIFI_STA_DEF"

/** SDK log levels, ordered as error/warn/info/debug. */
static char log_level_char(int level) {
  switch (level) {
    case 0:  return 'E';
    case 1:  return 'W';
    case 2:  return 'I';
    default: return 'D';
  }
}

void esp32_log(int level, const char *file, int line, const char *fmt, ...) {
  char buf[LOG_LINE_MAX];
  va_list args;

  uint64_t now_ms = esp32_get_time_ms();
  uint32_t sec = (uint32_t)(now_ms / 1000);
  uint32_t ms = (uint32_t)(now_ms % 1000);
  int pos = snprintf(buf, sizeof(buf),
                     "[%" PRIu32 ".%03" PRIu32 "] [%c] [%s:%d] ", sec, ms,
                     log_level_char(level), (file != NULL) ? file : "???",
                     line);

  va_start(args, fmt);
  vsnprintf(buf + pos, sizeof(buf) - pos - 2, fmt, args);
  va_end(args);

  /* Append the newline in place; two bytes were reserved above. */
  int len = (int)strlen(buf);
  if (len < (int)sizeof(buf) - 1) {
    buf[len] = '\n';
    buf[len + 1] = '\0';
  }
  printf("%s", buf);
}

int esp32_device_id(char *buf, size_t len) {
  if (buf == NULL || len < DEVICE_ID_MIN_LEN) {
    return -1;
  }

  uint8_t mac[6];
  if (esp_efuse_mac_get_default(mac) == ESP_OK) {
    /* Guard against blank/burnt-out eFuse (all 0x00 or all 0xFF). */
    bool valid = false;
    for (int i = 0; i < 6; i++) {
      if (mac[i] != 0x00 && mac[i] != 0xFF) {
        valid = true;
        break;
      }
    }
    if (valid) {
      snprintf(buf, len, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2],
               mac[3], mac[4], mac[5]);
      return (int)strlen(buf);
    }
  }

  snprintf(buf, len, "esp32s3-device");
  return (int)strlen(buf);
}

int esp32_random(uint8_t *buf, size_t len) {
  return esp32_fill_random(buf, len);
}

int esp32_uuid(char *buf, size_t size) {
  if (buf == NULL || size == 0) {
    return -1;
  }
  /* Random-only (RFC 4122 "version 4"-shaped) identifier; the SDK only needs
   * uniqueness per session, not strict version/variant bits. */
  uint8_t rnd[16];
  esp_fill_random(rnd, sizeof(rnd));
  snprintf(buf, size,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
           "%02x%02x%02x%02x%02x%02x",
           rnd[0], rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7],
           rnd[8], rnd[9], rnd[10], rnd[11], rnd[12], rnd[13], rnd[14],
           rnd[15]);
  return (int)strlen(buf);
}

int esp32_info(char *buf, size_t size) {
  if (buf == NULL || size == 0) {
    return -1;
  }
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  snprintf(buf, size, "esp32s3-freertos-idf%d", chip_info.revision);
  return (int)strlen(buf);
}

int esp32_network_available(void) {
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey(STA_NETIF_KEY);
  if (netif == NULL) {
    return 0;
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(netif, &ip) != ESP_OK) {
    return 0;
  }
  return (ip.ip.addr != 0) ? 1 : 0;
}

int esp32_network_get_type(char *buf, size_t size) {
  if (buf == NULL || size == 0) {
    return -1;
  }
  snprintf(buf, size, "wifi");
  return (int)strlen(buf);
}
