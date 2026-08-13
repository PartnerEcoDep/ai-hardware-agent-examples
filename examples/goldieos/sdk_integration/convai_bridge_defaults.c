/**
 * @file convai_bridge_defaults.c
 * @brief Hardcoded config defaults and JSON config builder for convai_bridge.
 *
 * Extracted from convai_bridge.c to separate credentials/defaults from
 * engine lifecycle and audio pipeline logic.
 */
#include "convai_bridge_defaults.h"
#include "convai_config_file.h"

#include <stdio.h>
#include <string.h>

#define BRIDGE_DEFAULT_BOT_ID         "your_agent_id"       // ← 替换为实际的 agent_id
#define BRIDGE_DEFAULT_PRODUCT_ID     "your_product_id"     // ← 替换为实际的 product_id
#define BRIDGE_DEFAULT_PRODUCT_KEY    "your_product_key"    // ← 替换为实际的 product_key
#define BRIDGE_DEFAULT_PRODUCT_SECRET "your_product_secret" // ← 替换为实际的 product_secret
#define BRIDGE_DEFAULT_DEVICE_NAME    "your_device_name"    // ← 替换为实际的 device_name
#define BRIDGE_DEFAULT_API_KEY        NULL                  // ← 使用 convai.cfg 中的 api_key

/* ---- Default startup config (AI personality/voice) ---- */
#define DEFAULT_STARTUP_CONFIG \
    "{" \
            "\"config\":{" \
                "\"llm_config\":{" \
                    "\"system_messages\":[" \
                        "\"你的名字叫小荷，你可以帮小朋友解决小烦恼哦。\"" \
                    "]" \
                "}," \
                "\"tts_config\":{" \
                    "\"provider_params\":{" \
                        "\"audio\":{" \
                            "\"voice_type\":\"Chinese (Mandarin)_Warm_Girl\"" \
                        "}" \
                    "}" \
                "}" \
            "}" \
        "}"

/* Return config value, or @p fallback if not set. */
static const char *cfg_or(const char *key, const char *fallback)
{
    const char *v = convai_config_file_get(key);
    return v ? v : fallback;
}

const char *bridge_get_default_agent_id(void)
{
    return cfg_or("agent_id", BRIDGE_DEFAULT_BOT_ID);
}

const char *bridge_get_default_startup_config(void)
{
    return DEFAULT_STARTUP_CONFIG;
}

/**
 * Build the create-time JSON config string, filling in values from the
 * config file where available, otherwise using hardcoded defaults.
 * Device name priority: device_name param (e.g. WiFi MAC from app layer) >
 * hardcoded default. Config file "device_name" is intentionally NOT supported
 * to avoid ambiguity (the device ID should be unique+automatic, or default).
 * Server URL: only included when "server_url" is set in convai.cfg;
 * if absent the field is omitted entirely (no hardcoded fallback).
 */
const char *bridge_build_config_json(char *buf, size_t buf_size,
                                     const char *device_name)
{
    const char *api_key = cfg_or("api_key", NULL);

    const char *server_url = cfg_or("server_url", NULL);

    int n;

    if (api_key != NULL && api_key[0] != '\0') {
        /* API-Key mode: onley api_key is needed */
        n = snprintf(buf, buf_size,
            "{"
                "\"info\":{"
                    "\"api_key\":\"%s\""
            "},"
            "\"ws\":{",
            api_key
        );
    } else {
        /* Product-Key mode: requires device_name, product_id, product_key, product_secret */
        if (device_name != NULL || device_name[0] != '\0') {
            device_name = BRIDGE_DEFAULT_DEVICE_NAME;
        }

        n = snprintf(buf, buf_size,
            "{"
                "\"info\":{"
                    "\"product_id\":\"%s\","
                    "\"product_key\":\"%s\","
                    "\"product_secret\":\"%s\","
                    "\"device_name\":\"%s\""
                "},"
                "\"ws\":{",
            cfg_or("product_id",      BRIDGE_DEFAULT_PRODUCT_ID),
            cfg_or("product_key",     BRIDGE_DEFAULT_PRODUCT_KEY),
            cfg_or("product_secret",  BRIDGE_DEFAULT_PRODUCT_SECRET),
            device_name
        );
    }

    if (server_url != NULL) {
        n += snprintf(buf + n, buf_size - n,
            "\"url\":\"%s\",", server_url);
    }

    n += snprintf(buf + n, buf_size - n,
            "\"audio\":{"
                "\"codec\":%d"
            "}"
        "}"
    "}", 0);
    (void)n; /* truncation is acceptable — engine will reject malformed JSON */
    return buf;
}
