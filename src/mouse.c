/* * src/mouse.c - OBA-32 Gelişmiş PS/2 Fare Sürücüsü (Kaydırma Tekerleği Entegrasyonu)
 * Bu modül; donanımı 4 baytlık IntelliMouse moduna geçirir, tekerlek hareketlerini
 * yakalar ve koordinat sınır filtrelemesini yürütür.
 */

#include "kernel.h" 
#include "hal.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button; 

/* ✨ Küresel Tekerlek Durum Değişkeni (0: Sabit, 1: Yukarı, -1: Aşağı) */
volatile int mouse_scroll_direction = 0;

static unsigned char mouse_cycle = 0;
static char mouse_byte[4]; /* Kaydırma modu için arabellek 4 bayta çıkarılmıştır */
static unsigned char mouse_mode_extended = 0;

void mouse_wait(unsigned char type) {
    unsigned int _time_out = 100000;
    if (type == 0) {
        while (_time_out--) {
            if ((inb(0x64) & 1) == 1) return;
            asm volatile("nop");
        }
    } else {
        while (_time_out--) {
            if ((inb(0x64) & 2) == 0) return;
            asm volatile("nop");
        }
    }
}

void mouse_write(unsigned char a) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a);
}

unsigned char mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

/* Donanımı IntelliMouse (Scroll) moduna geçiren sihirli sekans */
void init_mouse() {
    mouse_wait(1);
    outb(0x64, 0xA8); 

    mouse_wait(1);
    outb(0x64, 0x20); 
    mouse_wait(0);
    unsigned char _status = (inb(0x60) | 2); 
    
    mouse_wait(1);
    outb(0x64, 0x60); 
    mouse_wait(1);
    outb(0x60, _status);

    /* --- ✨ SİHİRLİ INTELLIMOUSE ENJEKSİYON SEKANSI ✨ --- */
    mouse_write(0xF3); mouse_read(); mouse_write(200); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(100); mouse_read();
    mouse_write(0xF3); mouse_read(); mouse_write(80);  mouse_read();
    
    /* Mod kontrolü: Donanım tekerleği onayladı mı? */
    mouse_write(0xF2); mouse_read();
    unsigned char device_id = mouse_read();
    if (device_id == 3) {
        mouse_mode_extended = 1; /* Tekerlek donanımsal olarak aktiftir */
    }

    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
    
    cursor_pos_x = 400;
    cursor_pos_y = 300;
    mouse_left_button = 0;
    mouse_scroll_direction = 0;
}

void mouse_handler() {
    unsigned char status = inb(0x64);
    
    if ((status & 0x01) && (status & 0x20)) {
        unsigned char data = inb(0x60);
        
        if (mouse_cycle == 0 && !(data & 0x08)) {
            return; 
        }

        mouse_byte[mouse_cycle++] = data;

        /* Genişletilmiş modda paket limiti 4, standart modda 3 bayttır */
        int packet_limit = (mouse_mode_extended) ? 4 : 3;

        if (mouse_cycle == packet_limit) {
            mouse_cycle = 0;

            mouse_left_button = (mouse_byte[0] & 0x01);

            int x_rel = mouse_byte[1];
            int y_rel = mouse_byte[2];

            if (mouse_byte[0] & 0x10) x_rel |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) y_rel |= 0xFFFFFF00;

            cursor_pos_x += x_rel;
            cursor_pos_y -= y_rel; 

            /* ✨ 4. Bayt Verisinin (Tekerlek Hareketi) Analiz Edilmesi */
            if (mouse_mode_extended) {
                char scroll_data = (mouse_byte[3] & 0x0F);
                if (scroll_data == 1) {
                    mouse_scroll_direction = 1;  /* Yukarı kaydırma sinyali */
                } else if (scroll_data == 15) {
                    mouse_scroll_direction = -1; /* Aşağı kaydırma sinyali */
                }
            }

            if (cursor_pos_x < 0) cursor_pos_x = 0;
            if (cursor_pos_x >= SCREEN_WIDTH) cursor_pos_x = SCREEN_WIDTH - 1; 
            if (cursor_pos_y < 0) cursor_pos_y = 0;
            if (cursor_pos_y >= SCREEN_HEIGHT) cursor_pos_y = SCREEN_HEIGHT - 1;
        }
    }
    outb(0xA0, 0x20); 
    outb(0x20, 0x20); 
}