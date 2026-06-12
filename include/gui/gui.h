#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define MIN_WINDOW_WIDTH  150
#define MIN_WINDOW_HEIGHT 100

typedef struct Window {
    int x, y;
    int width, height;
    char* title;
    int is_visible;
    int is_minimized;  
    int is_active;     
    int window_type;   // 0: PANEL, 1: FILES, 2: TERMINAL, 3: NOTEPAD
    
    /* Uygulama soyutlama göstericileri */
    void (*render_content)(struct Window* self);
    void (*handle_click)(struct Window* self, int local_mx, int local_my);
} Window_t;

typedef struct WindowSystem {
    unsigned char *back_buffer; 
    Window_t windows[5];
    int window_count;
    
    void (*init)(struct WindowSystem* self);
    void (*create_window)(struct WindowSystem* self, char* title, int x, int y, int w, int h, int type);
    void (*refresh)(struct WindowSystem* self);
} WindowSystem_t;

extern WindowSystem_t GuiManager;

#endif