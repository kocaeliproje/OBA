/* * src/gui/gui_manager.c - OBA-32 Grafik Kullanıcı Arabirimi (GUI) Pencere ve Olay Yönetim Sistemi
 * Sürükleme, asenkron boyutlandırma, PANEL kilit mekanizması, FARE İMLECİ ve GÖREV ÇUBUĞU mühürlenmiştir.
 */

#include "gui/gui.h"
#include "kernel.h"
#include <stdint.h>

/* Çekirdek ana modülünden aktarılan global değişken bildirimleri */
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button;
extern uint32_t* vga_lineer_buffer;
extern uint32_t* graphics_back_buffer;

/* Harici klavye ve VFS fonksiyon bildirimleri */
extern char keyboard_getchar(void);
extern void terminal_handle_char(char c);

/* Harici grafik ilkel çizim fonksiyonlarının deklarasyonları */
extern void draw_rect(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_rect_outline(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_char(int start_x, int start_y, char c, uint32_t color);
extern void draw_string(int start_x, int start_y, const char* str, uint32_t color);

/* Harici uygulama render ve etkileşim referansları */
extern void panel_render(Window_t* self);
extern void panel_handle_click(Window_t* self, int local_mx, int local_my);
extern void files_render(Window_t* self);
extern void files_handle_click(Window_t* self, int local_mx, int local_my);
extern void terminal_render(int win_x, int win_y, int win_w, int win_h);

/* Durum Makinesi Değişkenleri */
static int is_start_menu_open = 0;
static int was_mouse_pressed = 0;
static int is_resizing_g = 0;
static int resized_win_idx_g = -1;
static int resize_mode_g = 0; 
static int is_dragging_g = 0; 
static int dragged_win_idx_g = -1;
static int offset_x_g = 0; 
static int offset_y_g = 0;

/* Global Masaüstü Durum Değişkenleri */
int gui_selected_file_idx = -1;
const char* notepad_file_content_pointer = 0;
uint32_t gui_desktop_background_color = 0x0F4C5C;

/* GRAFİK FARE İMLECİ MASKESİ (16x16) */
static const uint8_t mouse_arrow[16][16] = {
    {2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {2,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0}, 
    {2,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0}, {2,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0}, {2,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0}, 
    {2,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0}, {2,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0,0,0,0,0}, {2,1,1,1,1,1,2,2,2,2,2,0,0,0,0,0}, 
    {2,1,1,2,1,1,2,0,0,0,0,0,0,0,0,0}, {2,1,2,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0,0,0,0,0}, {0,0,0,0,0,2,1,1,2,0,0,0,0,0,0,0}, 
    {0,0,0,0,0,2,1,1,2,0,0,0,0,0,0,0}, {0,0,0,0,0,0,2,2,2,0,0,0,0,0,0,0}
};

static void draw_graphic_mouse(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = mx + x; int py = my + y;
            if (px >= 0 && px < 800 && py >= 0 && py < 600) {
                uint8_t pixel_type = mouse_arrow[y][x];
                if (pixel_type == 1) { graphics_back_buffer[py * 800 + px] = 0xFFFFFF; }
                else if (pixel_type == 2) { graphics_back_buffer[py * 800 + px] = 0x000000; }
            }
        }
    }
}

/* GÖREV ÇUBUĞU VE BAŞLAT MENÜSÜ GRAPHIC MOTORU */
static void draw_taskbar_graphic(WindowSystem_t* self) {
    (void)self;
    int bar_y = 600 - 40;
    draw_rect(0, bar_y, 800, 40, 0x222831); 
    draw_rect(0, bar_y, 800, 1, 0x393E46);
    draw_rect(10, bar_y + 6, 80, 28, 0x00ADB5);
    draw_string(28, bar_y + 14, "START", 0xFFFFFF);

    if (is_start_menu_open) {
        int menu_w = 200; int menu_h = 200; int menu_x = 10; int menu_y = bar_y - menu_h - 5;
        draw_rect(menu_x, menu_y, menu_w, menu_h, 0x222831);
        draw_rect_outline(menu_x, menu_y, menu_w, menu_h, 0x00ADB5); 
        draw_rect(menu_x, menu_y, menu_w, 25, 0x393E46);
        draw_string(menu_x + 10, menu_y + 8, "OBA APPLICATIONS", 0x00ADB5);
        draw_rect(menu_x + 10, menu_y + 35, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 45, "-> Open PANEL", 0xFFFFFF);
        draw_rect(menu_x + 10, menu_y + 75, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 85, "-> Open FILES", 0xFFFFFF);
        draw_rect(menu_x + 10, menu_y + 115, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 125, "-> Open TERMINAL", 0x00FF00);
        draw_rect(menu_x + 10, menu_y + 155, menu_w - 20, 30, 0x5C2626);
        draw_string(menu_x + 20, menu_y + 165, "[!] Shutdown", 0xFF6B6B);
    }
}

/* TERMINAL İçerik Bağlayıcı Köprü Fonksiyonu */
static void terminal_wrapper_render(Window_t* self) {
    terminal_render(self->x, self->y, self->width, self->height);
}

/* NOTEPAD İçerik Çizim Motoru */
static void notepad_wrapper_render(Window_t* self) {
    if (notepad_file_content_pointer != 0) {
        draw_string(self->x + 15, self->y + 45, "[OBA Text Editor Pro v1.0]", 0x777777);
        draw_rect(self->x + 15, self->y + 62, self->width - 30, 1, 0xDDDDDD);
        draw_string(self->x + 15, self->y + 75, notepad_file_content_pointer, 0x222831);
    } else {
        draw_string(self->x + 15, self->y + 75, "[No file content loaded]", 0x999999);
    }
}

void gui_init(WindowSystem_t* self) { 
    self->window_count = 0; 
}

void gui_create_window(WindowSystem_t* self, char* title, int x, int y, int w, int h, int type) {
    if(self->window_count >= 5) return;
    Window_t* win = &self->windows[self->window_count++];
    win->title = title; win->x = x; win->y = y; win->width = w; win->height = h;
    win->is_visible = 0; win->is_minimized = 0; win->is_active = 0; win->window_type = type;
    
    if (type == 0) {
        win->render_content = panel_render;
        win->handle_click = panel_handle_click;
    } else if (type == 1) {
        win->render_content = files_render;
        win->handle_click = files_handle_click;
    } else if (type == 2) {
        win->render_content = terminal_wrapper_render;
        win->handle_click = 0;
    } else if (type == 3) {
        win->render_content = notepad_wrapper_render;
        win->handle_click = 0;
    }
}

void gui_refresh(WindowSystem_t* self) {
    if (graphics_back_buffer == 0 || vga_lineer_buffer == 0) return;
    int mx = cursor_pos_x; int my = cursor_pos_y; int bar_y = 600 - 40;

    /* Klavye girdilerinin terminal tamponuna aktarılması */
    char incoming_char;
    while ((incoming_char = keyboard_getchar()) != 0) {
        terminal_handle_char(incoming_char);
    }

    /* Olay ve Sürükleme/Boyutlandırma Durum Makinesi */
    if (mouse_left_button) {
        if (!was_mouse_pressed) {
            was_mouse_pressed = 1;

            /* Görev Çubuğu ve Başlat Menüsü Tıklama Geometrisi */
            if (mx >= 10 && mx <= 90 && my >= bar_y + 6 && my <= bar_y + 34) {
                is_start_menu_open = !is_start_menu_open;
            } else if (is_start_menu_open && mx >= 10 && mx <= 210 && my >= (bar_y - 205) && my <= bar_y) {
                int menu_y = bar_y - 200 - 5;
                if (my >= menu_y + 35 && my <= menu_y + 65) {
                    for(int k=0; k<self->window_count; k++) { if(self->windows[k].window_type == 0) { self->windows[k].is_visible = 1; break; } }
                    is_start_menu_open = 0;
                } else if (my >= menu_y + 75 && my <= menu_y + 105) {
                    for(int k=0; k<self->window_count; k++) { if(self->windows[k].window_type == 1) { self->windows[k].is_visible = 1; break; } }
                    is_start_menu_open = 0;
                } else if (my >= menu_y + 115 && my <= menu_y + 145) {
                    for(int k=0; k<self->window_count; k++) { if(self->windows[k].window_type == 2) { self->windows[k].is_visible = 1; break; } }
                    is_start_menu_open = 0;
                } else if (my >= menu_y + 155 && my <= menu_y + 185) {
                    is_start_menu_open = 0; extern void sys_shutdown(void); sys_shutdown();
                }
            } else {
                for (int i = self->window_count - 1; i >= 0; i--) {
    Window_t* w = &self->windows[i]; 
    if (!w->is_visible) continue;
    
    // Boyutlandırma (Resize) sınır kontrolleri
    int in_resize_right = (mx >= (w->x + w->width - 5) && mx <= (w->x + w->width + 5) && my >= w->y && my <= (w->y + w->height));
    int in_resize_bottom = (mx >= w->x && mx <= (w->x + w->width) && my >= (w->y + w->height - 5) && my <= (w->y + w->height + 5));
    
    if ((in_resize_right || in_resize_bottom) && w->window_type != 0) {
        is_resizing_g = 1; resized_win_idx_g = i;
        if (in_resize_right && in_resize_bottom) resize_mode_g = 3;
        else if (in_resize_right) resize_mode_g = 1;
        else resize_mode_g = 2;
        break;
    }

    // Fare genel olarak pencere sınırları içinde mi?
    if (mx >= w->x && mx < (w->x + w->width) && my >= w->y && my < (w->y + w->height)) {
        
        // 1. Z-Order Odaklama: Tıklanan pencereyi en üst katmana (dizinin sonuna) taşı
        if (i < self->window_count - 1) {
            Window_t temp = self->windows[i];
            for (int j = i; j < self->window_count - 1; j++) {
                self->windows[j] = self->windows[j + 1];
            }
            self->windows[self->window_count - 1] = temp; 
            i = self->window_count - 1; 
            w = &self->windows[i];
        }

        // 2. Kapatma "X" Butonu Kontrolü (Öncelikli olmalı)
        int btn_x = w->x + w->width - 24; 
        int btn_y = w->y + 6;
        if (mx >= btn_x && mx < (btn_x + 18) && my >= btn_y && my < (btn_y + 18)) { 
            w->is_visible = 0; 
            break; 
        }

        // 3. Başlık Çubuğu Tıklaması (Sürükleme Modu)
        if (my >= w->y && my < (w->y + 30)) {
            is_dragging_g = 1; 
            dragged_win_idx_g = i;
            offset_x_g = mx - w->x; 
            offset_y_g = my - w->y;
            break;
        }

        // 4. Pencere İçi Uygulama Tıklaması (FILES, PANEL vb.)
        // ✨ DÜZELTME: files.c ve panel.c başlık dahil mutlak yerel koordinat sistemini bekler!
        if (w->handle_click != 0 && my >= (w->y + 30)) {
            int local_mx = mx - w->x;
            int local_my = my - w->y; 
            w->handle_click(w, local_mx, local_my);
            break; // ✨ Tıklama işlendi, alt pencerelere sızmasını engelle!
        }
        
        break; // Pencere yakalandığı için ana döngüden çık
    }
}
            }
        } else {
            if (is_resizing_g && resized_win_idx_g != -1) {
                Window_t* w = &self->windows[resized_win_idx_g];
                if (resize_mode_g == 1 || resize_mode_g == 3) {
                    int new_width = mx - w->x; if (new_width >= MIN_WINDOW_WIDTH) w->width = new_width;
                }
                if (resize_mode_g == 2 || resize_mode_g == 3) {
                    int new_height = my - w->y; if (new_height >= MIN_WINDOW_HEIGHT) w->height = new_height;
                }
            }
            else if (is_dragging_g && dragged_win_idx_g != -1) {
                Window_t* w = &self->windows[dragged_win_idx_g];
                w->x = mx - offset_x_g; w->y = my - offset_y_g;
            }
        }
    } else { 
        was_mouse_pressed = 0; is_dragging_g = 0; is_resizing_g = 0; 
    }

    /* Çerçeve Arabelleği İşlemleri ve Çizim Çıktısı */
    for (int i = 0; i < 800 * 600; i++) { graphics_back_buffer[i] = gui_desktop_background_color; }

    for (int i = 0; i < self->window_count; i++) {
        Window_t* w = &self->windows[i]; if (!w->is_visible) continue;
        
        if (w->window_type == 2) { draw_rect(w->x, w->y, w->width, w->height, 0x050505); }     
        else if (w->window_type == 1) { draw_rect(w->x, w->y, w->width, w->height, 0xF5F5F5); }
        else if (w->window_type == 3) { draw_rect(w->x, w->y, w->width, w->height, 0xFFFDE7); }
        else { draw_rect(w->x, w->y, w->width, w->height, 0xEEEEEE); }                        
        
        draw_rect(w->x, w->y, w->width, 30, 0x393E46);
        draw_string(w->x + 10, w->y + 11, w->title, 0xFFFFFF);

        if (w->render_content != 0) {
            w->render_content(w);
        }

        draw_rect(w->x + w->width - 24, w->y + 6, 18, 18, 0xD63031); 
        draw_char(w->x + w->width - 19, w->y + 11, 'X', 0xFFFFFF);

        if (w->window_type != 0) {
            draw_rect(w->x + w->width - 8, w->y + w->height - 2, 6, 2, 0x00ADB5); 
            draw_rect(w->x + w->width - 2, w->y + w->height - 8, 2, 6, 0x00ADB5);
        }

        if (w->window_type == 2) { draw_rect_outline(w->x, w->y, w->width, w->height, 0x00FF00); }
        else { draw_rect_outline(w->x, w->y, w->width, w->height, 0x000000); }
    }
    
    /* ✨ MÜHÜRLENDİ: Görev çubuğu ve fare imlecinin pencerelerin üst katmanında işlenmesi */
    draw_taskbar_graphic(self);
    draw_graphic_mouse(mx, my);

    /* Arka tamponun VGA doğrusal bellek alanına kopyalanması */
    for (int i = 0; i < 800 * 600; i++) { vga_lineer_buffer[i] = graphics_back_buffer[i]; }
}

WindowSystem_t GuiManager = { .window_count = 0, .init = gui_init, .create_window = gui_create_window, .refresh = gui_refresh };