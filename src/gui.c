/* src/gui.c - Metin Modu Uyumlu Görev Çubuğu ve Saat Prototipi */
#include "gui.h"
#include "kernel.h"

// --- Harici Donanım ve Çekirdek Değişkenleri ---
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button;
extern volatile char last_pressed_key;
extern volatile int is_ctrl_pressed;
extern volatile unsigned int timer_ticks;

// VGA Metin Modu Bellek Adresi (80x25 karakter)
volatile char *vga_text_mem = (char*) 0xB8000;

static int is_dragging = 0;
static int dragged_window_idx = -1;
static int offset_x = 0;

// --- METİN MODU KARAKTER VE KUTU ÇİZİM FONKSİYONLARI ---

static void draw_text_char(int x, int y, char c, unsigned char color) {
    if (x >= 0 && x < 80 && y >= 0 && y < 25) {
        int idx = (y * 80 + x) * 2;
        GuiManager.back_buffer[idx] = c;
        GuiManager.back_buffer[idx + 1] = color;
    }
}

static void draw_text_rect(int x, int y, int w, int h, char c, unsigned char color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_text_char(x + j, y + i, c, color);
        }
    }
}

static void draw_text_string(int x, int y, char* str, unsigned char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        draw_text_char(x + i, y, str[i], color);
    }
}

// --- GÖREV ÇUBUĞU, BAŞLAT VE SAAT ÇİZİMİ ---

static void draw_taskbar(WindowSystem_t* self) {
    // 1. Görev Çubuğu (24. satır) - Gri arka plan (0x70)
    draw_text_rect(0, 24, 80, 1, ' ', 0x70);

    // 2. [START] Butonu - Koyu Gri/Siyah üzerine beyaz yazı
    draw_text_string(1, 24, "[START]", 0x07);

    // 3. Çalışan Program Butonları
    int button_x = 10;
    for (int i = 0; i < self->window_count; i++) {
        Window* w = &self->windows[i];
        
        // Aktif/Minimize durumuna göre renk (Aktifse Mavi 0x1F, pasifse Gri 0x70, minimize ise Koyu Gri 0x80)
        unsigned char btn_color = w->is_active ? 0x1F : (w->is_minimized ? 0x80 : 0x70);
        
        draw_text_char(button_x, 24, '|', 0x07);
        draw_text_string(button_x + 1, 24, w->title, btn_color);
        button_x += 12;
    }

    // 4. CANLI DİJİTAL SAAT MANTIĞI (timer_ticks / 100)
    unsigned int total_seconds = timer_ticks / 100;
    unsigned int secs = total_seconds % 60;
    unsigned int mins = (total_seconds / 60) % 60;
    unsigned int hours = (total_seconds / 3600) % 24;

    char clock_str[9];
    clock_str[0] = (hours / 10) + '0';   clock_str[1] = (hours % 10) + '0';   clock_str[2] = ':';
    clock_str[3] = (mins / 10) + '0';    clock_str[4] = (mins % 10) + '0';    clock_str[5] = ':';
    clock_str[6] = (secs / 10) + '0';    clock_str[7] = (secs % 10) + '0';    clock_str[8] = '\0';

    // Saati en sağ köşeye bas (Siyah arka plan, Yeşil yazı 0x0A)
    draw_text_string(71, 24, clock_str, 0x0A);
}

// --- MANTIK VE ETKİLEŞİM MOTORU ---

static void gui_update_logic(WindowSystem_t* self) {
    static int last_button_state = 0;

    // Sanal piksel koordinat evrenini (320x200) metin moduna (80x25) oranlama merkezi
    int mouse_x = cursor_pos_x / 4; 
    int mouse_y = cursor_pos_y / 8; 

    if (mouse_left_button && !last_button_state) {
        // A. GÖREV ÇUBUĞU TIKLAMA KONTROLÜ
        if (mouse_y == 24) {
            // Program butonlarına tıklandı mı?
            int button_x = 10;
            for (int i = 0; i < self->window_count; i++) {
                if (mouse_x >= button_x && mouse_x <= button_x + 10) {
                    Window* w = &self->windows[i];
                    if (w->is_minimized) {
                        w->is_minimized = 0;
                        w->is_visible = 1;
                        w->is_active = 1;
                    } else {
                        w->is_minimized = 1;
                        w->is_visible = 0;
                        w->is_active = 0;
                    }
                    break;
                }
                button_x += 12;
            }
        }

        // B. PENCERE TIKLAMA KONTROLÜ
        for (int i = self->window_count - 1; i >= 0; i--) {
            Window* w = &self->windows[i];
            if (!w->is_visible && !w->is_minimized) continue;

            // Kapatma butonu (X)
            if (mouse_x == (w->x + w->width - 2) && mouse_y == w->y) {
                w->is_visible = 0;
                w->is_minimized = 1;
                return;
            }

            // Başlık çubuğu sürükleme
            if (mouse_x >= w->x && mouse_x < (w->x + w->width) && mouse_y == w->y) {
                is_dragging = 1;
                dragged_window_idx = i;
                offset_x = mouse_x - w->x;

                for(int m = 0; m < self->window_count; m++) self->windows[m].is_active = 0;
                w->is_active = 1;

                if (i < self->window_count - 1) {
                    Window saved_win = self->windows[i];
                    for (int k = i; k < self->window_count - 1; k++) self->windows[k] = self->windows[k + 1];
                    self->windows[self->window_count - 1] = saved_win;
                    dragged_window_idx = self->window_count - 1;
                }
                break;
            }
        }
    }

    if (mouse_left_button && is_dragging && dragged_window_idx != -1) {
        Window* w = &self->windows[dragged_window_idx];
        w->x = mouse_x - offset_x;
        w->y = mouse_y;

        if (w->x < 0) w->x = 0;
        if (w->x + w->width > 80) w->x = 80 - w->width;
        if (w->y < 0) w->y = 0;
        if (w->y + w->height > 23) w->y = 23 - w->height; // Görev çubuğuna çarpma sınırı
    } else {
        is_dragging = 0;
        dragged_window_idx = -1;
    }

    last_button_state = mouse_left_button;

    // Kısayol Takibi (Ctrl + O / Ctrl + U)
    if (is_ctrl_pressed) {
        if (last_pressed_key == 'o' && self->window_count > 0) {
            self->windows[0].is_visible = !self->windows[0].is_visible;
            self->windows[0].is_minimized = !self->windows[0].is_visible;
            last_pressed_key = 0;
        }
        if (last_pressed_key == 'u' && self->window_count > 1) {
            self->windows[1].is_visible = !self->windows[1].is_visible;
            self->windows[1].is_minimized = !self->windows[1].is_visible;
            last_pressed_key = 0;
        }
    }
}

static void gui_init(WindowSystem_t* self) {
    self->window_count = 0;
    
    extern void* kmalloc(unsigned int size);
    
    // Metin modunda back_buffer boyutu: 80 sütun * 25 satır * 2 byte = 4000 byte
    self->back_buffer = (unsigned char*) kmalloc(80 * 25 * 2);
    
    // Çift tamponu temizle (Mavi arka plan, beyaz yazı)
    for(int i = 0; i < 80 * 25 * 2; i += 2) {
        self->back_buffer[i] = ' ';
        self->back_buffer[i+1] = 0x1F;
    }
}

static void gui_create_window(WindowSystem_t* self, char* title, int x, int y, int w, int h, int type) {
    if(self->window_count >= 5) return;
    Window* win = &self->windows[self->window_count++];
    win->title = title;
    win->x = x; win->y = y; win->width = w; win->height = h;
    win->is_visible = 1;
    win->is_minimized = 0;
    win->is_active = 0;
    win->window_type = type;
}

static void gui_refresh(WindowSystem_t* self) {
    // Öncelik mantık motorunda, koordinatlar burada güncellenir
    gui_update_logic(self);

    // Tekrardan temiz hesaplama
    int mouse_x = cursor_pos_x / 4;
    int mouse_y = cursor_pos_y / 8;

    // 1. Arka plan masaüstü dokusu (24. satıra yani görev çubuğuna kadar gri noktalar)
    for(int i = 0; i < 80 * 24 * 2; i += 2) {
        self->back_buffer[i] = '.';
        self->back_buffer[i+1] = 0x07;
    }

    // 2. Pencereleri Çiz
    for(int i = 0; i < self->window_count; i++) {
        Window* w = &self->windows[i];
        if(!w->is_visible || w->is_minimized) continue;
        
        for(int wy = w->y; wy < w->y + w->height; wy++) {
            for(int wx = w->x; wx < w->x + w->width; wx++) {
                int idx = (wy * 80 + wx) * 2;
                if(idx < 0 || idx >= 80 * 25 * 2) continue;

                if(wy == w->y) { 
                    self->back_buffer[idx] = ' ';
                    self->back_buffer[idx+1] = 0x70; // Başlık çubuğu rengi

                    int title_len = 0;
                    while(w->title[title_len] != '\0') title_len++;
                    int text_start = w->x + 1;
                    if(wx >= text_start && wx < text_start + title_len) {
                        self->back_buffer[idx] = w->title[wx - text_start];
                        self->back_buffer[idx+1] = 0x70;
                    }
                    if (wx == w->x + w->width - 2) {
                        self->back_buffer[idx] = 'X';
                        self->back_buffer[idx+1] = 0x4F; // Kapatma butonu
                    }
                } else {
                    self->back_buffer[idx] = ' ';
                    self->back_buffer[idx+1] = 0x1E; // Pencere gövdesi (Mavi)
                }
            }
        }
    }

    // 3. Görev Çubuğunu Çiz
    draw_taskbar(self);

    // 4. Fare İmlecini Çiz (Senkronizasyonu tam oturtulmuş sarı X imleci)
    int mouse_idx = (mouse_y * 80 + mouse_x) * 2;
    if (mouse_idx >= 0 && mouse_idx < 80 * 25 * 2) {
        self->back_buffer[mouse_idx] = 'X';     
        self->back_buffer[mouse_idx + 1] = 0x0E; // Sarı imleç
    }

    // 5. Tamponu VGA Belleğine Kopyala
    for(int i = 0; i < 80 * 25 * 2; i++) {
        vga_text_mem[i] = self->back_buffer[i];
    }
}

WindowSystem_t GuiManager = {
    .window_count = 0,
    .init = gui_init,
    .create_window = gui_create_window,
    .refresh = gui_refresh
};