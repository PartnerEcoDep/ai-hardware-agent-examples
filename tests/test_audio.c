#include "ai_audio.h"
#include "ai_playback.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(ai_alaw_encode(0) == 0xd5);
    assert(ai_alaw_encode(-1) == 0x55);
    assert(ai_alaw_encode(32767) == 0xaa);
    assert(ai_alaw_encode(-32768) == 0x2a);
    assert(ai_alaw_decode(0xd5) == 8);
    assert(ai_alaw_decode(0xaa) == 32256);
    for (unsigned i = 0; i < 256; ++i)
        assert(ai_alaw_encode(ai_alaw_decode((uint8_t)i)) == i);
    int16_t raw[] = {1000,-2000,3000,4000,5000,-6000,7000,8000};
    uint8_t szpi[4], lrd[4];
    assert(ai_audio_encode_slots(raw, 2, 4, 0, 1, szpi, sizeof(szpi)) == 4);
    assert(ai_audio_encode_slots(raw, 2, 4, 2, 1, lrd, sizeof(lrd)) == 4);
    assert(szpi[0] == ai_alaw_encode(1000) && szpi[1] == ai_alaw_encode(5000));
    assert(lrd[0] == ai_alaw_encode(3000) && lrd[1] == ai_alaw_encode(7000));
    assert(szpi[2] == lrd[2] && szpi[3] == lrd[3]);
    /* A desktop stereo microphone is not an echo-reference device. Ignore
     * even a loud right channel, preserve left, encode reference as silence. */
    int16_t desktop[] = {1000,32767,-2000,-32768};
    uint8_t win[4];
    assert(ai_audio_encode_slots(desktop, 2, 2, 0, AI_AUDIO_NO_REFERENCE, win, 4) == 4);
    assert(win[0] == ai_alaw_encode(1000) && win[1] == ai_alaw_encode(-2000));
    assert(win[2] == 0xd5 && win[3] == 0xd5);
    assert(ai_audio_encode_slots(desktop, 2, 2, 0, 2, win, 4) < 0);
    assert(ai_audio_encode_slots(desktop, 2, 2, 0, AI_AUDIO_NO_REFERENCE, win, 3) < 0);
    assert(ai_audio_encode_slots(raw, 2, 4, 4, 1, szpi, sizeof(szpi)) < 0);
    assert(ai_audio_encode_slots(raw, SIZE_MAX, 4, 0, 1, szpi, sizeof(szpi)) < 0);
    int16_t stereo[8];
    assert(ai_audio_decode_stereo(szpi, 4, stereo, 8) == 8);
    assert(stereo[0] == stereo[1] && stereo[6] == stereo[7]);
    assert(ai_audio_decode_stereo(szpi, 4, stereo, 7) < 0);
    uint8_t storage[8], out[8], a[] = {1,2,3,4,5,6}, b[] = {7,8,9,10};
    ai_playback_t q;
    assert(!ai_playback_init(&q, storage, 8, 4));
    assert(!ai_playback_push(&q, a, 2));
    assert(!ai_playback_pull(&q, out, 8));
    ai_playback_finish(&q);
    assert(ai_playback_pull(&q, out, 8) == 2); /* Short reply bypasses prebuffer. */
    ai_playback_reset(&q);
    assert(!ai_playback_push(&q, a, 6));
    assert(ai_playback_pull(&q, out, 4) == 4);
    assert(!memcmp(out, a, 4));
    assert(!ai_playback_push(&q, b, 4));
    assert(ai_playback_push(&q, a, 3) < 0); /* No partial insertion on overflow. */
    assert(ai_playback_pull(&q, out, 8) == 6);
    const uint8_t expected[] = {5,6,7,8,9,10};
    assert(!memcmp(out, expected, 6));
    /* A temporary underrun within one answer must not re-arm prebuffering. */
    assert(!ai_playback_pull(&q, out, sizeof(out)));
    assert(!ai_playback_push(&q, a, 2));
    assert(ai_playback_pull(&q, out, sizeof(out)) == 2);
    assert(!memcmp(out, a, 2));
    assert(!ai_playback_pull(&q, out, sizeof(out)));
    assert(!ai_playback_push(&q, b, 1));
    assert(ai_playback_pull(&q, out, sizeof(out)) == 1 && out[0] == b[0]);
    assert(!ai_playback_push(&q, a, 6));
    uint32_t epoch = q.epoch;
    ai_playback_reset(&q);
    assert(q.epoch != epoch && !q.finished && !q.size && !ai_playback_pull(&q, out, 8));
    assert(!ai_playback_push(&q, a, 2));
    assert(!ai_playback_pull(&q, out, sizeof(out))); /* Next answer prebuffers again. */
    ai_playback_finish(&q);
    assert(ai_playback_pull(&q, out, sizeof(out)) == 2); /* Short final answer still drains. */
    puts("audio vectors, board slots, bounds, short reply, wrap, overflow and reset passed");
}
