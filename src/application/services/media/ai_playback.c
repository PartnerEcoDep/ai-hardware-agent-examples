#include "ai_playback.h"
#include <string.h>
int ai_playback_init(ai_playback_t *q, uint8_t *storage, size_t capacity, size_t prebuffer) {
    if (!q || !storage || !capacity || prebuffer > capacity) return -1;
    *q = (ai_playback_t){.data = storage, .capacity = capacity, .prebuffer = prebuffer};
    return 0;
}
void ai_playback_reset(ai_playback_t *q) {
    q->read = q->size = 0;
    q->playing = q->finished = false;
    ++q->epoch;
}
int ai_playback_push(ai_playback_t *q, const uint8_t *data, size_t size) {
    if (!data || size > q->capacity - q->size) return -1;
    size_t write = (q->read + q->size) % q->capacity;
    size_t first = q->capacity - write;
    if (first > size) first = size;
    memcpy(q->data + write, data, first);
    memcpy(q->data, data + first, size - first);
    q->size += size;
    return 0;
}
size_t ai_playback_pull(ai_playback_t *q, uint8_t *out, size_t capacity) {
    if (!out || !capacity) return 0;
    if (!q->playing && !q->finished && q->size < q->prebuffer) return 0;
    size_t count = q->size < capacity ? q->size : capacity;
    size_t first = q->capacity - q->read;
    if (first > count) first = count;
    memcpy(out, q->data + q->read, first);
    memcpy(out + first, q->data, count - first);
    q->read = (q->read + count) % q->capacity;
    q->size -= count;
    /* Prebuffer only once per answer. Reset re-arms it for the next answer;
     * an empty queue between packets must not delay the next small packet. */
    if (count) q->playing = true;
    return count;
}
void ai_playback_finish(ai_playback_t *q) { q->finished = true; }
