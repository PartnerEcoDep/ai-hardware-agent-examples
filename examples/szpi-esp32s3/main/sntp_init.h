/**
 * @file sntp_init.h
 * @brief SNTP time synchronization helper.
 */

#ifndef SNTP_INIT_H
#define SNTP_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Synchronize system time via SNTP (blocks up to ~10s). */
void sntp_init_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* SNTP_INIT_H */
