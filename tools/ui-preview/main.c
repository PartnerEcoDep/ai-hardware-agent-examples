/* Standalone visual preview. No cloud SDK, credentials, audio or Wi-Fi access. */
#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <string.h>
#include "lvgl.h"
#include "ai_lvgl.h"
#include "ai_screen.h"
#ifndef UI_PREVIEW_WINDOW_TITLE
#define UI_PREVIEW_WINDOW_TITLE "AI UI Preview - simulated interaction - 320x240 (2x)"
#endif
static HWND window;
static uint32_t framebuffer[AI_SCREEN_WIDTH * AI_SCREEN_HEIGHT];
static uint32_t draw_buffer[AI_SCREEN_WIDTH * 20];
static int mouse_x, mouse_y;
static bool pressed, setup, auto_play;
static ai_view_t view;
static DWORD phase_at;
static const char *board = "Hoperun WS63";
static uint32_t tick(void) { return GetTickCount(); }
static void render(void) {
    ai_lvgl_render(&view);
    ai_lvgl_network(setup ? "Setup: AI-Setup-DEMO\nhttp://192.168.4.1" :
        view.network_up ? "Wi-Fi connected (preview)\n192.168.1.100" : "Offline (preview)");
}
static void select_state(ai_state_t state) {
    setup = false;
    view = (ai_view_t){.state=state, .network_up=state!=AI_OFFLINE,
        .active=state>=AI_CONNECTING && state<=AI_SPEAKING,
        .error=state==AI_ERROR ? -3 : 0};
    phase_at = GetTickCount();
    render();
}
static void command(ai_ui_command_t action) {
    if(action == AI_UI_PROVISION) {
        auto_play = false;
        setup = !setup;
        view = (ai_view_t){.state=setup ? AI_OFFLINE : AI_READY, .network_up=!setup};
        render();
    } else {
        select_state(view.active ? AI_READY : AI_CONNECTING);
        auto_play = view.active;
    }
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    int width = a->x2-a->x1+1;
    for(int y=a->y1; y<=a->y2; ++y)
        memcpy(framebuffer+y*AI_SCREEN_WIDTH+a->x1,
               data+(y-a->y1)*width*4, (size_t)width*4);
    InvalidateRect(window, NULL, FALSE);
    lv_display_flush_ready(d);
}
static void pointer(lv_indev_t *i, lv_indev_data_t *data) {
    (void)i; data->point.x=mouse_x; data->point.y=mouse_y;
    data->state=pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void mouse(LPARAM l) {
    RECT r; GetClientRect(window,&r);
    mouse_x=GET_X_LPARAM(l)*AI_SCREEN_WIDTH/(r.right ? r.right : 1);
    mouse_y=GET_Y_LPARAM(l)*AI_SCREEN_HEIGHT/(r.bottom ? r.bottom : 1);
}
static LRESULT CALLBACK procedure(HWND w, UINT message, WPARAM wp, LPARAM lp) {
    switch(message) {
    case WM_LBUTTONDOWN: mouse(lp); pressed=true; SetCapture(w); return 0;
    case WM_MOUSEMOVE: mouse(lp); return 0;
    case WM_LBUTTONUP: mouse(lp); pressed=false; ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED: case WM_KILLFOCUS: pressed=false; return 0;
    case WM_COMMAND:
        if(LOWORD(wp)>=100 && LOWORD(wp)<=106) {
            auto_play=false; select_state((ai_state_t)(LOWORD(wp)-100));
        }
        if(LOWORD(wp)>=200 && LOWORD(wp)<=202) {
            const char *names[]={"Hoperun WS63","LRD ESP32-S3","SZPI ESP32-S3"};
            board=names[LOWORD(wp)-200];
            lv_obj_clean(lv_screen_active()); ai_lvgl_create(board,command); render();
        }
        return 0;
    case WM_TIMER:
        if(auto_play && view.active && GetTickCount()-phase_at>2000) {
            ai_state_t next=view.state==AI_CONNECTING ? AI_LISTENING :
                view.state==AI_LISTENING ? AI_THINKING :
                view.state==AI_THINKING ? AI_SPEAKING : AI_LISTENING;
            select_state(next);
        }
        lv_timer_handler(); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; GetClientRect(w,&r);
        BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=AI_SCREEN_WIDTH; info.bmiHeader.biHeight=-AI_SCREEN_HEIGHT;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;
        SetStretchBltMode(dc,COLORONCOLOR);
        StretchDIBits(dc,0,0,r.right,r.bottom,0,0,AI_SCREEN_WIDTH,AI_SCREEN_HEIGHT,
            framebuffer,&info,DIB_RGB_COLORS,SRCCOPY);
        EndPaint(w,&ps); return 0;
    }
    case WM_DESTROY: KillTimer(w,1); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(w,message,wp,lp);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show) {
    (void)previous; (void)args;
    SetProcessDPIAware();
    WNDCLASSA cls={0}; cls.lpfnWndProc=procedure; cls.hInstance=instance;
    cls.lpszClassName="UnifiedUIPreview"; cls.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassA(&cls)) return 1;
    HMENU menu=CreateMenu(), states=CreatePopupMenu(), boards=CreatePopupMenu();
    const char *names[]={"Offline","Ready","Connecting","Listening","Thinking","Speaking","Error -3"};
    for(int i=0;i<7;i++) AppendMenuA(states,MF_STRING,100+i,names[i]);
    AppendMenuA(boards,MF_STRING,200,"Hoperun WS63");
    AppendMenuA(boards,MF_STRING,201,"LRD ESP32-S3");
    AppendMenuA(boards,MF_STRING,202,"SZPI ESP32-S3");
    AppendMenuA(menu,MF_POPUP,(UINT_PTR)states,"Preview state");
    AppendMenuA(menu,MF_POPUP,(UINT_PTR)boards,"Board title");
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    RECT size={0,0,AI_SCREEN_WIDTH*2,AI_SCREEN_HEIGHT*2}; AdjustWindowRect(&size,style,TRUE);
    window=CreateWindowA(cls.lpszClassName,UI_PREVIEW_WINDOW_TITLE,
        style,CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,
        NULL,menu,instance,NULL);
    if(!window) return 2;
    lv_init(); lv_tick_set_cb(tick);
    lv_display_t *d=lv_display_create(AI_SCREEN_WIDTH,AI_SCREEN_HEIGHT);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    lv_indev_t *input=lv_indev_create(); lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input,pointer);
    ai_lvgl_create(board,command); select_state(AI_READY);
    ShowWindow(window,show); SetTimer(window,1,10,NULL);
    MSG msg; while(GetMessage(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
