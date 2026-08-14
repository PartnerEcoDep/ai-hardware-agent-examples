/**
 * @file platform/esp32_osal.c
 * @brief OSAL layer of the ESP32-S3 ConvAI platform HAL.
 *
 * Maps the SDK's OS abstraction onto FreeRTOS / ESP-IDF:
 *   - memory : libc malloc/free (ESP-IDF heap)
 *   - time   : gettimeofday (wall clock) + esp_timer (monotonic)
 *   - mutex  : recursive FreeRTOS semaphore
 *   - thread : xTaskCreate plus a binary semaphore for join()
 *   - random : esp_fill_random (hardware TRNG)
 */

#include "convai_platform_esp32_internal.h"

#include "esp_random.h"
#include "esp_timer.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* ===================================================================
 *  Memory
 * =================================================================== */

void *esp32_malloc(size_t size) {
  return malloc(size);
}

void esp32_free(void *ptr) {
  free(ptr);
}

/* ===================================================================
 *  Time
 *
 *  get_time_ms: UTC milliseconds (valid after SNTP sync); used for logs
 *               and token expiry.
 *  get_tick_ms: monotonic milliseconds since boot; used for timeouts and
 *               intervals, never jumps when the wall clock is corrected.
 * =================================================================== */

uint64_t esp32_get_time_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

void esp32_sleep_ms(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

uint64_t esp32_get_tick_ms(void) {
  /* esp_timer_get_time() is monotonic microseconds since boot. */
  return (uint64_t)(esp_timer_get_time() / 1000);
}

/* ===================================================================
 *  Mutex (recursive: the SDK re-enters some locked sections)
 * =================================================================== */

int esp32_mutex_create(convai_mutex_t **mutex) {
  if (mutex == NULL) {
    return -1;
  }
  convai_mutex_t *m = (convai_mutex_t *)malloc(sizeof(*m));
  if (m == NULL) {
    return -1;
  }
  m->handle = xSemaphoreCreateRecursiveMutex();
  if (m->handle == NULL) {
    free(m);
    return -1;
  }
  *mutex = m;
  return 0;
}

void esp32_mutex_destroy(convai_mutex_t *mutex) {
  if (mutex == NULL) {
    return;
  }
  if (mutex->handle != NULL) {
    vSemaphoreDelete(mutex->handle);
  }
  free(mutex);
}

void esp32_mutex_lock(convai_mutex_t *mutex) {
  if (mutex == NULL) {
    return;
  }
  xSemaphoreTakeRecursive(mutex->handle, portMAX_DELAY);
}

void esp32_mutex_unlock(convai_mutex_t *mutex) {
  if (mutex == NULL) {
    return;
  }
  xSemaphoreGiveRecursive(mutex->handle);
}

/* ===================================================================
 *  Thread
 * =================================================================== */

/** Trampoline payload; freed by the task itself right after it starts. */
typedef struct {
  convai_thread_func_t func;
  void *arg;
  convai_thread_t *thread;
} esp32_thread_args_t;

#define THREAD_DEFAULT_STACK     4096
#define THREAD_DEFAULT_PRIORITY  (tskIDLE_PRIORITY + 2)
#define THREAD_DEFAULT_NAME      "convai"

static void esp32_thread_entry(void *pv) {
  esp32_thread_args_t *args = (esp32_thread_args_t *)pv;
  convai_thread_func_t func = args->func;
  void *arg = args->arg;
  convai_thread_t *thread = args->thread;
  free(args);

  if (func != NULL) {
    func(arg);
  }

  /* Publish the exit so join()/destroy() can stop waiting. */
  if (thread != NULL) {
    thread->exited = 1;
    xSemaphoreGive(thread->exit_sem);
  }
  vTaskDelete(NULL);
}

int esp32_thread_create(convai_thread_t **thread, convai_thread_func_t func,
                        void *arg, const char *name, size_t stack_size,
                        int priority) {
  if (thread == NULL || func == NULL) {
    return -1;
  }

  convai_thread_t *t = (convai_thread_t *)calloc(1, sizeof(*t));
  if (t == NULL) {
    return -1;
  }

  esp32_thread_args_t *args =
      (esp32_thread_args_t *)malloc(sizeof(*args));
  if (args == NULL) {
    free(t);
    return -1;
  }
  args->func = func;
  args->arg = arg;
  args->thread = t;

  t->exit_sem = xSemaphoreCreateBinary();
  if (t->exit_sem == NULL) {
    free(args);
    free(t);
    return -1;
  }

  UBaseType_t task_priority =
      (priority > 0) ? (UBaseType_t)priority : THREAD_DEFAULT_PRIORITY;
  uint32_t task_stack =
      (stack_size > 0) ? (uint32_t)stack_size : THREAD_DEFAULT_STACK;
  const char *task_name = (name != NULL) ? name : THREAD_DEFAULT_NAME;

  BaseType_t ret = xTaskCreate(esp32_thread_entry, task_name, task_stack,
                               args, task_priority, &t->handle);
  if (ret != pdPASS) {
    vSemaphoreDelete(t->exit_sem);
    free(args);
    free(t);
    return -1;
  }

  *thread = t;
  return 0;
}

void esp32_thread_join(convai_thread_t *thread) {
  if (thread == NULL) {
    return;
  }
  xSemaphoreTake(thread->exit_sem, portMAX_DELAY);
}

void esp32_thread_destroy(convai_thread_t *thread) {
  if (thread == NULL) {
    return;
  }
  /* Never free the handle while the task body may still run. */
  if (!thread->exited) {
    xSemaphoreTake(thread->exit_sem, portMAX_DELAY);
  }
  if (thread->exit_sem != NULL) {
    vSemaphoreDelete(thread->exit_sem);
  }
  /* The task already called vTaskDelete(NULL); nothing else to delete. */
  thread->handle = NULL;
  free(thread);
}

/* ===================================================================
 *  Random / String
 * =================================================================== */

int esp32_fill_random(uint8_t *buf, size_t len) {
  if (buf == NULL) {
    return -1;
  }
  esp_fill_random(buf, len);
  return 0;
}

char *esp32_strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  return strdup(s);
}
