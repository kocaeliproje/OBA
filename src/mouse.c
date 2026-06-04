/* src/mouse.c */
#include "kernel.h" 
#include "hal.h"

extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
volatile int mouse_left_button = 0; 

static unsigned char mouse_cycle = 0;
static char mouse_byte[3];

void mouse_wait(unsigned char type) {
    unsigned int _time_out = 100000;
    if (type == 0) {
        while (_time_out--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (_time_out--) {
            if ((inb(0x64) & 2) == 0) return;
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

void init_mouse() {
    unsigned char _status;
    mouse_wait(1);
    outb(0x64, 0xA8); 
    mouse_wait(1);
    outb(0x64, 0x20); 
    mouse_wait(0);
    _status = (inb(0x60) | 2); 
    mouse_wait(1);
    outb(0x64, 0x60); 
    mouse_wait(1);
    outb(0x60, _status);
    mouse_write(0xF6); 
    mouse_read();
    mouse_write(0xF4); 
    mouse_read();
    
    // Başlangıçta sanal piksel koordinatlarını ekranın tam ortasına (320x200 evreni) kuruyoruz
    cursor_pos_x = 160;
    cursor_pos_y = 100;
}

void mouse_handler() {
    unsigned char status = inb(0x64);
    
    if ((status & 0x01) && (status & 0x20)) {
        unsigned char data = inb(0x60);
        mouse_byte[mouse_cycle++] = data;

        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            mouse_left_button = (mouse_byte[0] & 0x01);

            int x_rel = mouse_byte[1];
            int y_rel = mouse_byte[2];

            if (mouse_byte[0] & 0x10) x_rel |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) y_rel |= 0xFFFFFF00;

            // --- SANAL PİKSEL TABANLI HAREKET AKTARIMI ---
            // Ham donanım verilerini çarparak/bölmeden geniş sanal uzama ekliyoruz
            cursor_pos_x += x_rel;
            cursor_pos_y -= y_rel; 

            // Sanal Grafik Modu Sınır Koruması (320x200 piksel alanı)
            if (cursor_pos_x < 0) cursor_pos_x = 0;
            if (cursor_pos_x > 319) cursor_pos_x = 319; 
            if (cursor_pos_y < 0) cursor_pos_y = 0;
            if (cursor_pos_y > 199) cursor_pos_y = 199;
        }
    }

    outb(0xA0, 0x20); 
    outb(0x20, 0x20); 
}