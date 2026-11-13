#include "linux_ui.h"
#include "linux_runtime.h"
#include "ai_screen.h"
#include "lvgl.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static lv_indev_t *pointer;
static unsigned scale;
static int pointer_x, pointer_y;
static bool down, running, lv_started, failed;
static uint16_t draw_buffer[AI_SCREEN_WIDTH * 20];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    SDL_Rect rect = {area->x1, area->y1, area->x2-area->x1+1, area->y2-area->y1+1};
    if (SDL_UpdateTexture(texture, &rect, pixels, rect.w * 2) < 0) {
        fprintf(stderr, "Display upload failed: %s\n", SDL_GetError());
        running = false; failed = true;
    }
    if (lv_display_flush_is_last(display)) {
        if (SDL_RenderClear(renderer) < 0 || SDL_RenderCopy(renderer, texture, NULL, NULL) < 0) {
            fprintf(stderr, "Display render failed: %s\n", SDL_GetError());
            running = false; failed = true;
        }
        SDL_RenderPresent(renderer);
    }
    lv_display_flush_ready(display);
}

static void read_pointer(lv_indev_t *input, lv_indev_data_t *data) {
    (void)input;
    data->point.x = pointer_x; data->point.y = pointer_y;
    data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int linux_ui_open(const char *title, unsigned requested_scale) {
    if (requested_scale < 1 || requested_scale > 4) return -1;
    scale = requested_scale;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) goto failed;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        AI_SCREEN_WIDTH * scale, AI_SCREEN_HEIGHT * scale, SDL_WINDOW_SHOWN);
    if (!window) goto failed;
    /* Software rendering also works with VNC desktops and SDL's dummy driver. */
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) goto failed;
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
        AI_SCREEN_WIDTH, AI_SCREEN_HEIGHT);
    if (!texture) goto failed;
    lv_init(); lv_started = true; lv_tick_set_cb(linux_now_ms);
    lv_display_t *display = lv_display_create(AI_SCREEN_WIDTH, AI_SCREEN_HEIGHT);
    if (!display) goto failed;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    pointer = lv_indev_create();
    if (!pointer) goto failed;
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, read_pointer);
    running = true;
    return 0;
failed:
    fprintf(stderr, "Cannot open Linux desktop UI: %s\n", SDL_GetError());
    linux_ui_close();
    return -1;
}

bool linux_ui_pump(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT: running = false; break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
            break;
        case SDL_MOUSEMOTION:
            pointer_x = event.motion.x / (int)scale;
            pointer_y = event.motion.y / (int)scale;
            lv_indev_read(pointer);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event.button.button != SDL_BUTTON_LEFT) break;
            pointer_x = event.button.x / (int)scale;
            pointer_y = event.button.y / (int)scale;
            down = event.type == SDL_MOUSEBUTTONDOWN;
            /* Read every edge, including clicks shorter than an LVGL timer tick. */
            lv_indev_read(pointer);
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                down = false; lv_indev_reset(pointer, NULL);
            }
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                SDL_RenderCopy(renderer, texture, NULL, NULL); SDL_RenderPresent(renderer);
            }
            break;
        default: break;
        }
    }
    lv_timer_handler();
    return running;
}

bool linux_ui_failed(void) { return failed; }

void linux_ui_test_pointer(int x, int y, bool pressed) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x * (int)scale; event.button.y = y * (int)scale;
    SDL_PushEvent(&event);
}

void linux_ui_close(void) {
    if (lv_started) { lv_deinit(); lv_started = false; }
    pointer = NULL;
    SDL_DestroyTexture(texture); texture = NULL;
    SDL_DestroyRenderer(renderer); renderer = NULL;
    SDL_DestroyWindow(window); window = NULL;
    SDL_Quit(); running = false;
}
