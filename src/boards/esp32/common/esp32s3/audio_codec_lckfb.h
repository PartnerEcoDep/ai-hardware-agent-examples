#ifndef AUDIO_CODEC_LCKFB_H
#define AUDIO_CODEC_LCKFB_H
#include <stddef.h>
#include "audio_codec.h"
#include "driver/i2s_std.h"
#define AUDIO_CODEC_LCKFB_SZPI_NAME "es8311-es7210"
typedef struct audio_lckfb_s audio_lckfb_t;
extern audio_codec_t audio_codec_lckfb_szpi;
#endif
