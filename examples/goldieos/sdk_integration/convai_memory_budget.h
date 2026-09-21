/**
 * @file convai_memory_budget.h
 * @brief GoldieOS demo-layer memory budget — single source of truth.
 *
 * This header budgets the goldieOS demo (bridge/demo layer) buffers that sit
 * on top of the SDK: audio record/playback buffers, playback ring, JSON copy
 * buffers, startup config, and demo-owned task stacks.
 *
 * The SDK-layer budget (engine + WS transport + TLS + IO thread stack) is
 * accounted separately in src/internal/convai_sdk_limits.h, which carries its
 * own <100 KB _Static_assert.  Keeping the two budgets separate lets the demo
 * layer be sized independently of the SDK.
 *
 * Values are byte counts unless the name explicitly says otherwise.
 * Keep this header dependency-free (no includes beyond <stddef.h>).
 */

#ifndef CONVAI_MEMORY_BUDGET_H
#define CONVAI_MEMORY_BUDGET_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Bridge / configuration buffers ---- */

#define CONVAI_BUDGET_STARTUP_CONFIG_BYTES       2048
#define CONVAI_BUDGET_DEVICE_NAME_BYTES            64
#define CONVAI_BUDGET_JSON_COPY_BYTES            2048
#define CONVAI_BUDGET_CONFIG_LINE_BYTES           512
#define CONVAI_BUDGET_CONFIG_ENTRY_COUNT           32
#define CONVAI_BUDGET_CONFIG_KEY_BYTES              64
#define CONVAI_BUDGET_CONFIG_VALUE_BYTES           256
#define CONVAI_BUDGET_COMFORT_JSON_BYTES           256
#define CONVAI_BUDGET_FUNC_DISPATCH_COUNT            16
#define CONVAI_BUDGET_FUNC_OUTPUT_BYTES             256

/* ---- Audio pipeline buffers ---- */

#define CONVAI_BUDGET_AUDIO_RECORD_BYTES           640
#define CONVAI_BUDGET_PLAYBACK_RING_BYTES        16000
#define CONVAI_BUDGET_PLAYBACK_READ_BYTES         1024
#define CONVAI_BUDGET_PCM_DECODE_BYTES            2048

/* ---- Demo-owned thread stacks ---- */

#define CONVAI_BUDGET_AUDIO_UPLINK_STACK_BYTES    0x2000
#define CONVAI_BUDGET_AUDIO_DOWNLINK_STACK_BYTES  0x2000
#define CONVAI_BUDGET_COMFORT_STACK_BYTES          0x800
#define CONVAI_BUDGET_TAP_TIMEOUT_STACK_BYTES     0x1000 /* 4KB - watchdog only sleeps, minimal stack */

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_MEMORY_BUDGET_H */
