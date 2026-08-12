/**
 * @file convai_memory_budget.h
 * @brief Single source of truth for ConvAI demo buffer and thread-stack budgets.
 *
 * Keep this header dependency-free: it is consumed by both the demo sources
 * and the WS63 mbedTLS configuration.  Values are byte counts unless the name
 * explicitly says otherwise.
 */

#ifndef CONVAI_MEMORY_BUDGET_H
#define CONVAI_MEMORY_BUDGET_H

/* Bridge/configuration buffers. */
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

/* Audio pipeline buffers. */
#define CONVAI_BUDGET_AUDIO_RECORD_BYTES           640
#define CONVAI_BUDGET_PLAYBACK_RING_BYTES        16000
#define CONVAI_BUDGET_PLAYBACK_READ_BYTES         1024
#define CONVAI_BUDGET_PCM_DECODE_BYTES            2048

/* Demo-owned thread stacks. */
#define CONVAI_BUDGET_AUDIO_UPLINK_STACK_BYTES    0x2000
#define CONVAI_BUDGET_AUDIO_DOWNLINK_STACK_BYTES  0x2000
#define CONVAI_BUDGET_COMFORT_STACK_BYTES          0x800

/* TLS record buffers (consumed by the WS63 mbedTLS user config). */
#define CONVAI_BUDGET_TLS_IN_CONTENT_BYTES         4096
#define CONVAI_BUDGET_TLS_OUT_CONTENT_BYTES        4096

#endif /* CONVAI_MEMORY_BUDGET_H */
