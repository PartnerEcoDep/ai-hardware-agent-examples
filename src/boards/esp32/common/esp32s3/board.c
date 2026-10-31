#include "board.h"
#include "board_display.h"
#include "board_init.h"
#include "board_pins.h"
#include "lcd_init.h"
#include "audio_codec_lckfb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static i2c_master_dev_handle_t touch;
static audio_codec_t *codec = &audio_codec_lckfb_szpi;

int board_init(void) {
    if (board_init_all() != ESP_OK) return -1;
    board_touch_reset();
    if (lcd_init() != ESP_OK) return -1;
    if (esp_lcd_panel_disp_on_off(g_lcd_panel, true) != ESP_OK) return -1;
    lcd_backlight_init();
    if (codec->init(codec, g_i2c_bus, board_pa_enable, NULL) != ESP_OK) return -1;
    if (codec->set_volume(codec, CONFIG_AI_SPEAKER_VOLUME) != ESP_OK) return -1;
    i2c_device_config_t tc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_I2C_ADDR,
        .scl_speed_hz = BOARD_I2C_CLK_SPEED,
    };
    if (i2c_master_bus_add_device(g_i2c_bus, &tc, &touch) != ESP_OK) return -1;
    if (board_profile.has_button) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << BOARD_BOOT_BUTTON_GPIO,
            .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        if (gpio_config(&cfg) != ESP_OK) return -1;
    }
    return 0;
}

int board_audio_read(int16_t *samples, size_t capacity) {
    int bytes = codec->read(codec, samples, (int)(capacity * sizeof(*samples)));
    return bytes < 0 ? -1 : bytes / (int)sizeof(*samples);
}
int board_audio_write(const int16_t *samples, size_t count) {
    int bytes = codec->write(codec, samples, (int)(count * sizeof(*samples)));
    return bytes == (int)(count * sizeof(*samples)) ? 0 : -1;
}
bool board_button_down(void) {
    return board_profile.has_button && gpio_get_level(BOARD_BOOT_BUTTON_GPIO) == 0;
}
bool board_touch_read(int *x, int *y) {
    uint8_t reg = 0x02, buf[7];
    if (i2c_master_transmit_receive(touch, &reg, 1, buf, sizeof(buf), 30) != ESP_OK)
        return false;
    unsigned points = buf[0] & 15;
    if (!points || points > BOARD_TOUCH_MAX_POINTS) return false;
    int raw_x = ((buf[1] & 15) << 8) | buf[2];
    int raw_y = ((buf[3] & 15) << 8) | buf[4];
    if (raw_x >= BOARD_DISPLAY_HEIGHT || raw_y >= BOARD_DISPLAY_WIDTH) return false;
    *x = raw_y;
    *y = BOARD_DISPLAY_HEIGHT - 1 - raw_x;
    return true;
}
esp_lcd_panel_handle_t board_display_panel(void) { return g_lcd_panel; }
esp_lcd_panel_io_handle_t board_display_io(void) { return g_lcd_io; }
