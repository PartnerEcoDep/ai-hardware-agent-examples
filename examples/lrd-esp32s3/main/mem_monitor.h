/**
 * @file mem_monitor.h
 * @brief Periodic runtime memory-consumption logging (goldieos-style).
 *
 * Spawns a low-priority FreeRTOS task that tracks three sampling points
 * required by the memory budget test procedure:
 *
 *   RAM_a   — free heap right after WiFi connects        (baseline, 预期1)
 *   RAM_b   — free heap right after the session connects (建联,      预期2)
 *   RAM_all — free heap during conversation cycles        (对话过程,   预期3)
 *
 * Every MEM_MONITOR_PERIOD_MS the task logs one line in the [SYS INFO]
 * format used by the test spec (units: bytes):
 *
 *   [SYS INFO] mem: used:XX (RAM_a), fre:XXX; log: drop/al[u/1], at_recv N.
 *
 * and flags a warning when the delta against RAM_a exceeds the 100 KB
 * budget ceiling (RAM_b-RAM_a < 100K, RAM_all-RAM_a < 100K).
 *
 * The same task also logs heap breakdown and task stack high-water marks.
 */

#ifndef MEM_MONITOR_H
#define MEM_MONITOR_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default logging period (ms). Override at compile time if needed. */
#ifndef MEM_MONITOR_PERIOD_MS
#define MEM_MONITOR_PERIOD_MS 10000
#endif

/** Memory budget ceiling for the delta checks (100 KB). */
#ifndef MEM_MONITOR_BUDGET_BYTES
#define MEM_MONITOR_BUDGET_BYTES 102400
#endif

/** Task stack budget; the monitor itself only does snprintf + heap queries. */
#define MEM_MONITOR_STACK_SIZE 4096

/** Task priority: below lvgl(11)/SDK io(10) so logging never starves the
 *  UI or audio path. */
#define MEM_MONITOR_PRIORITY 5

/**
 * @brief Start the periodic memory monitor task.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if the task could not be created.
 */
esp_err_t mem_monitor_start(void);

#ifdef __cplusplus
}
#endif

#endif /* MEM_MONITOR_H */
