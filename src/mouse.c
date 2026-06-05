/* src/mouse.c */
#include "kernel.h" 
#include "hal.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button; 

static unsigned char mouse_cycle = 0;
static char mouse_byte[3];

// DÜZELTİLDİ: "asm volatile" ile derleyicinin bu kritik donanım bekleme döngülerini silmesi engellendi!
void mouse_wait(unsigned char type) {
    unsigned int _time_out = 100000;
    if (type == 0) {
        while (_time_out--) {
            if ((inb(0x64) & 1) == 1) return;
            asm volatile("nop"); // Derleyici bu döngüye artık dokunamaz
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
    
    cursor_pos_x = 400;
    cursor_pos_y = 300;
    mouse_left_button = 0;
}

void mouse_handler() {
    unsigned char status = inb(0x64);
    
    if ((status & 0x01) && (status & 0x20)) {
        unsigned char data = inb(0x60);
        
        if (mouse_cycle == 0 && !(data & 0x08)) {
            return; 
        }

        mouse_byte[mouse_cycle++] = data;

        if (mouse_cycle == 3) {
            mouse_cycle = 0;

            mouse_left_button = (mouse_byte[0] & 0x01);

            int x_rel = mouse_byte[1];
            int y_rel = mouse_byte[2];

            if (mouse_byte[0] & 0x10) x_rel |= 0xFFFFFF00;
            if (mouse_byte[0] & 0x20) y_rel |= 0xFFFFFF00;

            cursor_pos_x += x_rel;
            cursor_pos_y -= y_rel; 

            if (cursor_pos_x < 0) cursor_pos_x = 0;
            if (cursor_pos_x >= SCREEN_WIDTH) cursor_pos_x = SCREEN_WIDTH - 1; 
            
            if (cursor_pos_y < 0) cursor_pos_y = 0;
            if (cursor_pos_y >= SCREEN_HEIGHT) cursor_pos_y = SCREEN_HEIGHT - 1;
        }
    }

    outb(0xA0, 0x20); 
    outb(0x20, 0x20); 
}