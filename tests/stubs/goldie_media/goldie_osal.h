#ifndef TEST_GOLDIE_OSAL_H
#define TEST_GOLDIE_OSAL_H
typedef struct { int held; } goldie_mutex;
typedef int (*goldie_thread_handler)(void *);
void goldie_mutex_init(goldie_mutex *mutex);
void goldie_mutex_lock(goldie_mutex *mutex);
void goldie_mutex_unlock(goldie_mutex *mutex);
void goldie_msleep(int ms);
void *goldie_thread_create(goldie_thread_handler fn, void *arg, const char *name, unsigned int stack);
void goldie_thread_lock(void);
void goldie_thread_unlock(void);
int goldie_thread_set_priority(void *thread, unsigned int priority);
#endif
