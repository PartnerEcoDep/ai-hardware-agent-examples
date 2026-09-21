/**
 * @file mem_monitor.c
 * @brief Periodic runtime memory-consumption logging (goldieos-style).
 *
 * Implements the memory budget test procedure:
 *
 *   1. WiFi connects            -> lock RAM_a (baseline), print 预期1
 *   2. Session connects         -> lock RAM_b, print 预期2 + RAM_b-RAM_a
 *   3. Every period (RAM_all)   -> print 预期3 + RAM_all-RAM_a
 *
 * Log format (bytes, per test spec):
 *
 *   [SYS INFO] mem: used:XX (RAM_a), fre:XXX; log: drop/al[u/s], at_recv N.
 *
 *   used    = RAM_a - current free heap   (consumed since the baseline)
 *   fre     = current free heap
 *   RAM_x   = baseline annotation (RAM_a / RAM_b / RAM_all)
 *   drop/al = uplink dropped / uplink sent
 *   at_recv = playback ring bytes dropped (audio receive path)
 *
 * Delta checks: RAM_b-RAM_a < 100 KB and RAM_all-RAM_a < 100 KB, with a
 * [WARN] line when the ceiling is exceeded.  The same task additionally
 * logs heap breakdown (DRAM/IRAM/PSRAM) and task stack high-water marks.
 */

#include "mem_monitor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

#if CONFIG_CONVAI_ENABLE
#include "convai_bridge.h"
#endif
#include "wifi_provisioning.h"

#include <stdio.h>
#include <stdint.h>

static const char *TAG = "SYS INFO";

/* ---- Sampling points (bytes of free heap at each trigger) ---- */
static int      s_wifi_locked;   /* RAM_a captured */
static uint32_t s_ram_a;         /* baseline: free heap after WiFi up */
static uint32_t s_ram_b;         /* free heap after session connect */

/** Consumed bytes relative to the RAM_a baseline. */
static inline int32_t consumed_since_a(uint32_t now_free) {
  if (!s_wifi_locked) return 0;
  return (int32_t)s_ram_a - (int32_t)now_free;
}

/* ---- Test-spec log line (units: bytes) ---- */
static void log_mem_line(uint32_t free_heap,
                         unsigned sent, unsigned dropped,
                         unsigned play_dropped) {
  ESP_LOGI(TAG, "mem: used:%d (RAM_a=%u), fre:%u; log: drop/al[%u/%u], "
                "at_recv %u.",
           consumed_since_a(free_heap), (unsigned)s_ram_a,
           (unsigned)free_heap, dropped, sent, play_dropped);
}

/* ---- Known task names for stack high-water marks ---- */
static const char *const s_task_names[] = {
    "lvgl",        /* LVGL event loop (main UI) */
    "audio_cap",   /* mic capture / uplink */
    "audio_play",  /* playback ring drain */
    "convai",      /* SDK IO thread (esp32_osal default name) */
    "voice_apply", /* voice-config apply worker */
    "touch_poll",  /* touch polling */
    "main",        /* app_main background loop */
};

static void mem_monitor_task(void *arg) {
  (void)arg;
  char stack_line[256] = {0};
  size_t pos = 0;
  unsigned int sent = 0, dropped = 0, play_dropped = 0;
  int32_t delta_all = 0;
#if CONFIG_CONVAI_ENABLE
  int was_session = 0;
  int32_t delta_b = 0;
#endif

  while (1) {
    uint32_t free_heap = (uint32_t)esp_get_free_heap_size();

    /* ---- Sampling point 1: WiFi up -> RAM_a baseline ---- */
    if (wifi_prov_is_connected() && !s_wifi_locked) {
      s_ram_a = free_heap;
      s_wifi_locked = 1;
      log_mem_line(free_heap, sent, dropped, play_dropped);
      ESP_LOGI(TAG, "预期1: WiFi connected, RAM_a=%u B (baseline)",
               (unsigned)s_ram_a);
    }

    /* ---- ConvAI bridge runtime counters ---- */
#if CONFIG_CONVAI_ENABLE
    {
      convai_status_e status = CONVAI_STATUS_IDLE;
      convai_bridge_get_runtime_stats(&sent, &dropped, &play_dropped,
                                      &status);

      /* ---- Sampling point 2: session connect -> RAM_b ---- */
      int in_session = (status != CONVAI_STATUS_IDLE);
      if (in_session && !was_session && !s_ram_b) {
        s_ram_b = free_heap;
        delta_b = consumed_since_a(free_heap);
        ESP_LOGI(TAG, "预期2: session connected, RAM_b=%u B, "
                      "RAM_b-RAM_a=%d B (%s 100KB)",
                 (unsigned)s_ram_b, delta_b,
                 (delta_b < MEM_MONITOR_BUDGET_BYTES) ? "PASS" : "FAIL");
      }
      was_session = in_session;
    }
#endif

    /* ---- Periodic heap / stack / bridge report ---- */
    size_t dram = heap_caps_get_free_size(MALLOC_CAP_8BIT |
                                          MALLOC_CAP_INTERNAL);
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "heap: free=%u min=%u dram=%u iram=%u psram=%u",
             (unsigned)free_heap,
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)dram,
             (unsigned)(internal > dram ? internal - dram : 0),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    pos = 0;
    for (size_t i = 0; i < sizeof(s_task_names) / sizeof(s_task_names[0]);
         i++) {
      TaskHandle_t h = xTaskGetHandle(s_task_names[i]);
      if (h == NULL) {
        continue;
      }
      UBaseType_t hwm = uxTaskGetStackHighWaterMark(h);
      int n = snprintf(stack_line + pos, sizeof(stack_line) - pos,
                       " %s=%u", s_task_names[i], (unsigned)hwm);
      if (n > 0 && pos + (size_t)n < sizeof(stack_line)) {
        pos += (size_t)n;
      } else {
        break;
      }
    }
    if (pos > 0) {
      ESP_LOGI(TAG, "stack:%s", stack_line);
    }

    /* ---- Sampling point 3: RAM_all (this period) + delta check ---- */
    if (s_wifi_locked) {
      delta_all = consumed_since_a(free_heap);
      log_mem_line(free_heap, sent, dropped, play_dropped);
      ESP_LOGI(TAG, "预期3: RAM_all-RAM_a=%d B (%s 100KB)",
               delta_all,
               (delta_all < MEM_MONITOR_BUDGET_BYTES) ? "PASS" : "FAIL");
    }

    vTaskDelay(pdMS_TO_TICKS(MEM_MONITOR_PERIOD_MS));
  }
}

esp_err_t mem_monitor_start(void) {
  BaseType_t ret = xTaskCreate(mem_monitor_task, "mem_mon",
                               MEM_MONITOR_STACK_SIZE, NULL,
                               MEM_MONITOR_PRIORITY, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "mem_monitor task create failed (ret=%d)", (int)ret);
    return ESP_ERR_NO_MEM;
  }
  ESP_LOGI(TAG, "mem_monitor started (period=%dms)", MEM_MONITOR_PERIOD_MS);
  return ESP_OK;
}
