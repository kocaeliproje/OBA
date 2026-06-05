/* * src/gui.c - OBA-32 Grafik Kullanıcı Arabirimi (GUI) ve Yazı Tipi Yönetim Sistemi
 * Bu modül; pencere yönetimi, başlat menüsü etkileşimleri, fare çizimi ve 
 * çift tamponlama (double buffering) mekanizmalarının yönetiminden sorumludur.
 */

#include "gui.h"
#include "kernel.h"
#include <stdint.h>

/* Çekirdek ana modülünden aktarılan global değişken bildirimleri */
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button;
extern volatile unsigned int timer_ticks;
extern uint32_t* vga_lineer_buffer;
extern uint32_t* graphics_back_buffer;

/* GUI dahili durum ve denetim değişkenleri */
static int is_start_menu_open = 0;
static int was_mouse_pressed = 0; /* Fare tıklama sinyalinin kararlılığını sağlamak amacıyla kullanılan kilit (debounce) */

/* * ISO/IEC 8859-1 Standart ASCII 8x8 Yazı Tipi Matrisi (Bitmap Font)
 * Her bir bayt, ilgili karakterin bir satırındaki 8 piksellik veri dağılımını temsil eder.
 */
static const uint8_t vga_font[128][8] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00},
    ['"'] = {0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#'] = {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00},
    ['$'] = {0x00, 0x1C, 0x3A, 0x18, 0x2E, 0x1C, 0x00, 0x00},
    ['%'] = {0x00, 0x66, 0x66, 0x14, 0x18, 0x2C, 0x66, 0x66},
    ['&'] = {0x30, 0x4A, 0x4A, 0x34, 0x4A, 0x4A, 0x34, 0x00},
    ['\'']= {0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['('] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')'] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    ['*'] = {0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, 0x00},
    ['+'] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-'] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/'] = {0x00, 0x02, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},
    
    /* Sayısal Karakterler */
    ['0'] = {0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00},
    ['1'] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['2'] = {0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00},
    ['3'] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4'] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5'] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6'] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7'] = {0x7E, 0x66, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00},
    ['8'] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9'] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    
    [':'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    [';'] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00},
    ['<'] = {0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00},
    ['='] = {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
    ['>'] = {0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00},
    ['?'] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
    ['@'] = {0x3C, 0x42, 0x5D, 0x55, 0x5D, 0x40, 0x3C, 0x00},
    
    /* Büyük Harf Karakterleri */
    ['A'] = {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    ['B'] = {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
    ['C'] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D'] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
    ['F'] = {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['G'] = {0x3C, 0x66, 0x60, 0x6C, 0x66, 0x66, 0x3E, 0x00},
    ['H'] = {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['I'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['J'] = {0x1E, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['K'] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L'] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M'] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N'] = {0x66, 0x6E, 0x76, 0x76, 0x7A, 0x6E, 0x66, 0x00},
    ['O'] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P'] = {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    ['Q'] = {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00},
    ['R'] = {0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
    ['S'] = {0x3C, 0x66, 0x30, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x66, 0x66, 0x66, 0x76, 0x66, 0x66, 0x3C, 0x00},
    ['V'] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    ['W'] = {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    ['X'] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y'] = {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    
    ['['] = {0x3E, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00},
    ['\\']= {0x00, 0x40, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00},
    [']'] = {0x3E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x3E, 0x00},
    ['^'] = {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
    ['`'] = {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
    
    /* Küçük Harf Karakterleri */
    ['a'] = {0x00, 0x00, 0x3C, 0x02, 0x3E, 0x46, 0x3B, 0x00},
    ['b'] = {0x40, 0x40, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x42, 0x40, 0x42, 0x3C, 0x00},
    ['d'] = {0x02, 0x02, 0x3E, 0x46, 0x46, 0x46, 0x3B, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x3C, 0x00},
    ['f'] = {0x1C, 0x22, 0x20, 0x78, 0x20, 0x20, 0x70, 0x00},
    ['g'] = {0x00, 0x00, 0x3B, 0x46, 0x46, 0x3E, 0x02, 0x3C},
    ['h'] = {0x40, 0x40, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i'] = {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j'] = {0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0x0C, 0x4C, 0x38},
    ['k'] = {0x40, 0x40, 0x44, 0x48, 0x70, 0x48, 0x44, 0x00},
    ['l'] = {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m'] = {0x00, 0x00, 0x6C, 0x92, 0x92, 0x92, 0x92, 0x00},
    ['n'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x40, 0x40},
    ['q'] = {0x00, 0x00, 0x3B, 0x46, 0x46, 0x3E, 0x02, 0x02},
    ['r'] = {0x00, 0x00, 0x5E, 0x30, 0x20, 0x20, 0x20, 0x00},
    ['s'] = {0x00, 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x7C, 0x00},
    ['t'] = {0x20, 0x20, 0x78, 0x20, 0x20, 0x22, 0x1C, 0x00},
    ['u'] = {0x00, 0x00, 0x46, 0x46, 0x46, 0x46, 0x3B, 0x00},
    ['v'] = {0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x00},
    ['w'] = {0x00, 0x00, 0x44, 0x44, 0x54, 0x54, 0x28, 0x00},
    ['x'] = {0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00},
    ['y'] = {0x00, 0x00, 0x46, 0x46, 0x3E, 0x02, 0x3C, 0x00},
    ['z'] = {0x00, 0x00, 0x7E, 0x08, 0x10, 0x20, 0x7E, 0x00},
    
    ['{'] = {0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00},
    ['|'] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['}'] = {0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00},
    ['~'] = {0x3A, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

// --- 2. GEOMETRİK VE METİN ÇİZİM MOTORU FONKSİYONLARI ---

/* Belirtilen koordinat doğrultusunda tek bir pikseli arka tampona işler */
void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    if (graphics_back_buffer != 0) graphics_back_buffer[y * SCREEN_WIDTH + x] = color;
}

/* Belirtilen koordinat ve boyutlarda içi dolu bir dikdörtgen alanı boyar */
void draw_rect(int start_x, int start_y, int w, int h, uint32_t color) {
    if (graphics_back_buffer == 0) return;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int dest_x = start_x + x;
            int dest_y = start_y + y;
            if (dest_x >= 0 && dest_x < SCREEN_WIDTH && dest_y >= 0 && dest_y < SCREEN_HEIGHT) {
                graphics_back_buffer[dest_y * SCREEN_WIDTH + dest_x] = color;
            }
        }
    }
}

/* Pencerelerin dış hat sınırlarını belirtilen renkte çerçeveler */
void draw_rect_outline(int start_x, int start_y, int w, int h, uint32_t color) {
    if (graphics_back_buffer == 0) return;
    for (int x = 0; x < w; x++) {
        int dest_x = start_x + x;
        if (dest_x >= 0 && dest_x < SCREEN_WIDTH) {
            if (start_y >= 0 && start_y < SCREEN_HEIGHT) graphics_back_buffer[start_y * SCREEN_WIDTH + dest_x] = color;
            if ((start_y + h - 1) >= 0 && (start_y + h - 1) < SCREEN_HEIGHT) graphics_back_buffer[(start_y + h - 1) * SCREEN_WIDTH + dest_x] = color;
        }
    }
    for (int y = 0; y < h; y++) {
        int dest_y = start_y + y;
        if (dest_y >= 0 && dest_y < SCREEN_HEIGHT) {
            if (start_x >= 0 && start_x < SCREEN_WIDTH) graphics_back_buffer[dest_y * SCREEN_WIDTH + start_x] = color;
            if ((start_x + w - 1) >= 0 && (start_x + w - 1) < SCREEN_WIDTH) graphics_back_buffer[dest_y * SCREEN_WIDTH + (start_x + w - 1)] = color;
        }
    }
}

/* Tek bir ASCII karakterini yazı tipi matrisi üzerinden ekrana çizer */
void draw_char(int start_x, int start_y, char c, uint32_t color) {
    if (graphics_back_buffer == 0) return;
    if ((unsigned char)c >= 128) return;
    for (int y = 0; y < 8; y++) {
        uint8_t row = vga_font[(unsigned char)c][y];
        for (int x = 0; x < 8; x++) {
            if (row & (1 << (7 - x))) {
                int dest_x = start_x + x;
                int dest_y = start_y + y;
                if (dest_x >= 0 && dest_x < SCREEN_WIDTH && dest_y >= 0 && dest_y < SCREEN_HEIGHT) {
                    graphics_back_buffer[dest_y * SCREEN_WIDTH + dest_x] = color;
                }
            }
        }
    }
}

/* Karakter dizilerini (string) ardışık olarak ekrana nakşeder */
void draw_string(int start_x, int start_y, const char* str, uint32_t color) {
    int current_x = start_x;
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(current_x, start_y, str[i], color);
        current_x += 8;
    }
}

// --- 3. ŞANLI AÇILIŞ LOGOSU (SPLASH SCREEN) MOTORU ---
static const uint8_t logo_O[8] = { 0b00111100, 0b01100110, 0b11000011, 0b11000011, 0b11000011, 0b11000011, 0b01100110, 0b00111100 };
static const uint8_t logo_B[8] = { 0b11111100, 0b11000110, 0b11000110, 0b11111100, 0b11000110, 0b11000110, 0b11000110, 0b11111100 };
static const uint8_t logo_A[8] = { 0b00011000, 0b00111100, 0b01100110, 0b11000011, 0b11111111, 0b11000011, 0b11000011, 0b11000011 };

/* Sistem açılış logosunun tekil karakter ölçeklendirmesini yürütür */
void draw_logo_char(const uint8_t font[8], int start_x, int start_y, int scale, uint32_t color) {
    if (vga_lineer_buffer == 0) return;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (font[y] & (1 << (7 - x))) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px = start_x + (x * scale) + sx;
                        int py = start_y + (y * scale) + sy;
                        if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) vga_lineer_buffer[py * SCREEN_WIDTH + px] = color;
                    }
                }
            }
        }
    }
}

/* Önyükleme sırasında LFB adresine doğrudan logo çıktısını üretir */
void draw_splash_screen() {
    if (vga_lineer_buffer == 0) return;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) vga_lineer_buffer[i] = 0x0D0D0D;
    int scale = 12; int char_width = 8 * scale; int spacing = 30; 
    int total_width = (char_width * 3) + (spacing * 2);
    int start_x = (SCREEN_WIDTH - total_width) / 2; int start_y = (SCREEN_HEIGHT - (8 * scale)) / 2;
    uint32_t logo_color = 0x00ADB5; 
    draw_logo_char(logo_O, start_x, start_y, scale, logo_color);
    draw_logo_char(logo_B, start_x + char_width + spacing, start_y, scale, logo_color);
    draw_logo_char(logo_A, start_x + (char_width * 2) + (spacing * 2), start_y, scale, logo_color);
    for(int bx = start_x; bx < start_x + total_width; bx++) {
        if (bx >= 0 && bx < SCREEN_WIDTH) vga_lineer_buffer[(start_y + (8 * scale) + 30) * SCREEN_WIDTH + bx] = 0x393E46;
    }
}

// --- 4. GRAFİK FARE İMLECİ VE SÜRÜCÜ ENTEGRASYONU ---
static const uint8_t mouse_arrow[16][16] = {
    {2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {2,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0}, {2,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0}, {2,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0}, {2,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0}, {2,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0}, {2,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0,0,0,0,0}, {2,1,1,1,1,1,2,2,2,2,2,0,0,0,0,0}, {2,1,1,2,1,1,2,0,0,0,0,0,0,0,0,0}, {2,1,2,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0,0,0,0,0}, {0,0,0,0,0,2,1,1,2,0,0,0,0,0,0,0}, {0,0,0,0,0,2,1,1,2,0,0,0,0,0,0,0}, {0,0,0,0,0,0,2,2,2,0,0,0,0,0,0,0}
};

/* Fare işaretçisini en üst katman verisi olarak arka tampona basar */
void draw_graphic_mouse(int mx, int my) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int px = mx + x; int py = my + y;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                uint8_t pixel_type = mouse_arrow[y][x];
                if (pixel_type == 1) graphics_back_buffer[py * SCREEN_WIDTH + px] = 0xFFFFFF; 
                else if (pixel_type == 2) graphics_back_buffer[py * SCREEN_WIDTH + px] = 0x000000; 
            }
        }
    }
}

// --- 5. EMÜLATÖR VE ACPI GÜÇ YÖNETİMİ ---

/* * x86 I/O portuna 16-bit veri (word) yazma işlemini yürüten statik satır içi fonksiyon.
 * Derleme hatasını önlemek amacıyla doğrudan assembly seviyesinde mühürlenmiştir.
 */
static inline void gui_outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (data));
}

/* QEMU/Bochs donanım portları üzerinden ACPI kapatma sinyali iletir */
void sys_shutdown() {
    gui_outw(0x604, 0x2000);   /* QEMU ACPI Güç Kapatma Sinyali */
    gui_outw(0xB004, 0x2000);  /* Bochs / Eski QEMU Güç Kapatma Sinyali */
    gui_outw(0x4004, 0x3400);  /* Alternatif Sanallaştırma Katmanı Sinyali */
    
    /* Güç kesme işlemi başarısız olursa işlemciyi emniyetli olarak kilitler */
    asm volatile("cli");
    while(1) {
        asm volatile("hlt");
    }
}

// --- 6. GÖREV ÇUBUĞU VE BAŞLAT MENÜSÜ GÖRSEL MOTORU ---
static void draw_taskbar_graphic(WindowSystem_t* self) {
    (void)self;
    int bar_height = 40;
    int bar_y = SCREEN_HEIGHT - bar_height;
    
    draw_rect(0, bar_y, SCREEN_WIDTH, bar_height, 0x222831); 
    draw_rect(0, bar_y, SCREEN_WIDTH, 1, 0x393E46);
    
    /* Başlat Butonu Alanı */
    draw_rect(10, bar_y + 6, 80, 28, 0x00ADB5);
    draw_string(28, bar_y + 14, "START", 0xFFFFFF);

    /* Başlat Menüsü Paneli Çizim Döngüsü */
    if (is_start_menu_open) {
        int menu_w = 200;
        int menu_h = 200; /* Kapatma seçeneği için yükseklik ölçeklendirildi */
        int menu_x = 10;
        int menu_y = bar_y - menu_h - 5;

        draw_rect(menu_x, menu_y, menu_w, menu_h, 0x222831);
        draw_rect_outline(menu_x, menu_y, menu_w, menu_h, 0x00ADB5); 

        draw_rect(menu_x, menu_y, menu_w, 25, 0x393E46);
        draw_string(menu_x + 10, menu_y + 8, "OBA APPLICATIONS", 0x00ADB5);

        /* Uygulama Tetikleme Butonları */
        draw_rect(menu_x + 10, menu_y + 35, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 45, "-> Open PANEL", 0xFFFFFF);

        draw_rect(menu_x + 10, menu_y + 75, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 85, "-> Open FILES", 0xFFFFFF);

        draw_rect(menu_x + 10, menu_y + 115, menu_w - 20, 30, 0x393E46);
        draw_string(menu_x + 20, menu_y + 125, "-> Open TERMINAL", 0x00FF00);

        /* Güç Yönetimi Butonu */
        draw_rect(menu_x + 10, menu_y + 155, menu_w - 20, 30, 0x5C2626);
        draw_string(menu_x + 20, menu_y + 165, "[!] Shutdown", 0xFF6B6B);
    }
}

// --- 7. MASAÜSTÜ YÖNETİMİ VE ETKİLEŞİM KATMANI ---
static void gui_init(WindowSystem_t* self) {
    self->window_count = 0; self->back_buffer = 0; 
}

static void gui_create_window(WindowSystem_t* self, char* title, int x, int y, int w, int h, int type) {
    if(self->window_count >= 5) return;
    Window* win = &self->windows[self->window_count++];
    win->title = title; win->x = x; win->y = y; win->width = w; win->height = h;
    win->is_visible = 1; win->is_minimized = 0; win->is_active = 0; win->window_type = type;
}

static int is_dragging_g = 0; static int dragged_win_idx_g = -1;
static int offset_x_g = 0; static int offset_y_g = 0;

static void gui_refresh(WindowSystem_t* self) {
    if (graphics_back_buffer == 0 || vga_lineer_buffer == 0) return;

    int mx = cursor_pos_x;
    int my = cursor_pos_y;
    int bar_y = SCREEN_HEIGHT - 40;

    /* Tıklama ve Etkileşim Algoritmaları Denetimi */
    if (mouse_left_button) {
        if (!was_mouse_pressed) {
            was_mouse_pressed = 1; 

            /* Başlat Butonu Sınır Kontrolü */
            if (mx >= 10 && mx <= 90 && my >= bar_y + 6 && my <= bar_y + 34) {
                is_start_menu_open = !is_start_menu_open;
            }
            /* Başlat Menüsü Dahili Buton Kontrolleri */
            else if (is_start_menu_open && mx >= 10 && mx <= 210 && my >= (bar_y - 205) && my <= bar_y) {
                int menu_y = bar_y - 200 - 5;
                
                if (my >= menu_y + 35 && my <= menu_y + 65) {
                    for(int k=0; k<self->window_count; k++) {
                        if(self->windows[k].window_type == 0) { self->windows[k].is_visible = 1; break; }
                    }
                    is_start_menu_open = 0; 
                }
                else if (my >= menu_y + 75 && my <= menu_y + 105) {
                    for(int k=0; k<self->window_count; k++) {
                        if(self->windows[k].window_type == 1) { self->windows[k].is_visible = 1; break; }
                    }
                    is_start_menu_open = 0;
                }
                else if (my >= menu_y + 115 && my <= menu_y + 145) {
                    for(int k=0; k<self->window_count; k++) {
                        if(self->windows[k].window_type == 2) { self->windows[k].is_visible = 1; break; }
                    }
                    is_start_menu_open = 0;
                }
                else if (my >= menu_y + 155 && my <= menu_y + 185) {
                    is_start_menu_open = 0;
                    sys_shutdown();
                }
            }
            /* Standart Pencere Alanı Odaklanma ve Sürükleme Kontrolleri */
            else {
                for (int i = self->window_count - 1; i >= 0; i--) {
                    Window* w = &self->windows[i];
                    if (!w->is_visible || w->is_minimized) continue;

                    /* Akıllı Odaklanma: Pencere yüzey sınırlarının kontrolü */
                    if (mx >= w->x && mx < (w->x + w->width) && my >= w->y && my < (w->y + w->height)) {
                        
                        /* Kapatma Butonu Denetimi (Sağ Üst Köşe) */
                        int btn_x = w->x + w->width - 24; int btn_y = w->y + 6;
                        if (mx >= btn_x && mx < (btn_x + 18) && my >= btn_y && my < (btn_y + 18)) {
                            w->is_visible = 0; 
                            break;
                        }

                        /* Katman Önceliği Ayarı (Z-Order Güncellemesi) */
                        if (i < self->window_count - 1) {
                            Window temp = self->windows[i];
                            for (int j = i; j < self->window_count - 1; j++) {
                                self->windows[j] = self->windows[j + 1];
                            }
                            self->windows[self->window_count - 1] = temp;
                            i = self->window_count - 1;
                        }

                        /* Sürükleme Kilitlemesi Denetimi (Başlık Barı - 30px) */
                        Window* top_w = &self->windows[self->window_count - 1];
                        if (my >= top_w->y && my < (top_w->y + 30)) {
                            is_dragging_g = 1; 
                            dragged_win_idx_g = self->window_count - 1;
                            offset_x_g = mx - top_w->x; 
                            offset_y_g = my - top_w->y;
                        }
                        
                        if (is_start_menu_open) is_start_menu_open = 0;
                        break;
                    }
                }
                if (my < bar_y && is_start_menu_open) is_start_menu_open = 0;
            }
        } else if (dragged_win_idx_g != -1) {
            /* Sürükleme Konum Güncellemesi */
            Window* w = &self->windows[dragged_win_idx_g];
            w->x = mx - offset_x_g; w->y = my - offset_y_g;
            if (w->x < 0) w->x = 0; 
            if (w->x + w->width > SCREEN_WIDTH) w->x = SCREEN_WIDTH - w->width;
            if (w->y < 0) w->y = 0; 
            if (w->y + w->height > bar_y) w->y = bar_y - w->height;
        }
    } else {
        was_mouse_pressed = 0; 
        is_dragging_g = 0; 
        dragged_win_idx_g = -1;
    }

    // --- ARKA TAMPON ÇİZİM AŞAMALARI ---
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) graphics_back_buffer[i] = 0x1A1B20; 

    for (int i = 0; i < self->window_count; i++) {
        Window* w = &self->windows[i];
        if (!w->is_visible || w->is_minimized) continue;

        /* Pencere Tipi Tabanlı Arka Plan Seçimi */
        if (w->window_type == 2) draw_rect(w->x, w->y, w->width, w->height, 0x050505);
        else draw_rect(w->x, w->y, w->width, w->height, 0xEEEEEE);

        if (w->window_type == 2) draw_rect(w->x, w->y, w->width, 30, 0x1F2421);
        else draw_rect(w->x, w->y, w->width, 30, 0x393E46);

        draw_string(w->x + 10, w->y + 11, w->title, 0xFFFFFF);

        /* Pencere İçerik Metinlerinin Yazdırılması */
        if (w->window_type == 0) {
            draw_string(w->x + 15, w->y + 50, "OBA SYSTEM PANEL V1", 0x222831);
            draw_rect(w->x + 15, w->y + 70, w->width - 30, 1, 0xCCCCCC);
        } else if (w->window_type == 1) {
            draw_string(w->x + 15, w->y + 50, "STATUS: FILES READY.", 0x00ADB5);
        } else if (w->window_type == 2) {
            int start_line_y = w->y + 45;
            draw_string(w->x + 15, start_line_y, "OBA Kernel Shell v1.0.4 - Welcome manet", 0xAAAAAA);
            draw_string(w->x + 15, start_line_y + 20, "manet@OBA:~$ ls", 0x00FF00);
            draw_string(w->x + 15, start_line_y + 40, "kernel.bin   system/   fs/   oba.txt", 0x00ADB5);
            draw_string(w->x + 15, start_line_y + 60, "manet@OBA:~$ _", 0x00FF00);
        }

        /* Kapatma Butonu Grafik Çıktısı */
        draw_rect(w->x + w->width - 24, w->y + 6, 18, 18, 0xD63031);
        draw_char(w->x + w->width - 19, w->y + 11, 'X', 0xFFFFFF);

        if (w->window_type == 2) draw_rect_outline(w->x, w->y, w->width, w->height, 0x00FF00);
        else draw_rect_outline(w->x, w->y, w->width, w->height, 0x000000);
    }

    draw_taskbar_graphic(self);
    draw_graphic_mouse(mx, my);

    /* Arka Tampon Verisinin Doğrudan Donanım Ekran Belleğine Aktarılması */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) vga_lineer_buffer[i] = graphics_back_buffer[i];
}

/* GUI Arabirim Yapısının Fonksiyon İşaretçileri ile Eşleştirilmesi */
WindowSystem_t GuiManager = {
    .window_count = 0, .init = gui_init, .create_window = gui_create_window, .refresh = gui_refresh
};