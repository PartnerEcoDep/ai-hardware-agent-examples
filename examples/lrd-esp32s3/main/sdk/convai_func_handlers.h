/**
 * @file convai_func_handlers.h
 * @brief Register the ESP32 example's lightweight function-call handlers.
 */
#ifndef CONVAI_FUNC_HANDLERS_H
#define CONVAI_FUNC_HANDLERS_H

#ifdef __cplusplus
extern "C" {
#endif

void func_handlers_register(void);
void func_handlers_unregister(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_FUNC_HANDLERS_H */
