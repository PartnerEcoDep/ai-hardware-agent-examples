#ifndef AI_PLAYBACK_H
#define AI_PLAYBACK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Caller provides storage and serialization. All data is copied, never borrowed. */
typedef struct {
    uint8_t *data;
    size_t capacity, read, size, prebuffer;
    uint32_t epoch;
    bool playing, finished; /* playing latches the first pull until reset. */
} ai_playback_t;
int ai_playback_init(ai_playback_t *q, uint8_t *storage, size_t capacity, size_t prebuffer);
void ai_playback_reset(ai_playback_t *q);
int ai_playback_push(ai_playback_t *q, const uint8_t *data, size_t size);
size_t ai_playback_pull(ai_playback_t *q, uint8_t *out, size_t capacity);
void ai_playback_finish(ai_playback_t *q);
#endif
