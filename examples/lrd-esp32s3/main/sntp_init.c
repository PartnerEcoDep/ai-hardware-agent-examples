/**
 * @file sntp_init.c
 * @brief SNTP time synchronization helper.
 */

#include "sntp_init.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <time.h>

static const char *TAG = "sntp_init";

void sntp_init_sync(void) {
  setenv("TZ", "CST-8", 1);
  tzset();

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "ntp.aliyun.com");
  esp_sntp_init();

  int retry = 0;
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 100) {
    vTaskDelay(pdMS_TO_TICKS(100));
    retry++;
  }
  ESP_LOGI(TAG, "SNTP %s", retry < 100 ? "synced" : "timeout");
}
