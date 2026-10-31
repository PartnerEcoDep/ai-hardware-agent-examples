#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
typedef struct audio_codec_s audio_codec_t;
struct audio_codec_s {
    const char *name;
    esp_err_t (*init)(audio_codec_t *, i2c_master_bus_handle_t,
                      void (*pa_enable)(int, void *), void *);
    esp_err_t (*set_volume)(audio_codec_t *, uint8_t);
    int (*read)(audio_codec_t *, void *, int bytes);
    int (*write)(audio_codec_t *, const void *, int bytes);
};
#endif
