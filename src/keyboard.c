/* src/keyboard.c */
#include "kernel.h"
#include "hal.h"

extern volatile char last_pressed_key;
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;

// Kombinasyonlar için Ctrl bayrağı
volatile int is_ctrl_pressed = 0; 

char input_buffer[256];
int input_ptr = 0;

unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
  '9', '0', '-', '=', '\b',
  '\t',
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* 29 - Left Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0, /* Left Shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', ',', '.', '/',   0, /* Right Shift */
  '*',
    0, /* Alt */
  ' ', /* Space bar */
    0, /* Caps lock */
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0
};

void keyboard_handler() {
    unsigned char scancode = inb(0x60);

    // Ctrl tuşu basılma ve bırakılma takibi
    if (scancode == 0x1D) {
        is_ctrl_pressed = 1;
    } else if (scancode == 0x9D) {
        is_ctrl_pressed = 0;
    }

    if (!(scancode & 0x80)) {
        if (scancode == 0x18) {
            last_pressed_key = 'o';
        }
        if (scancode == 0x16) {
            last_pressed_key = 'u';
        }

        char c = keyboard_map[scancode];
        if (c != 0) {
            // DÜZELTİLDİ: Fareyi bozan WASD if blokları tamamen kaldırıldı!
            if (c == '\n') {
                input_buffer[input_ptr] = '\0';
                input_ptr = 0;
            } else if (c == '\b' && input_ptr > 0) {
                input_ptr--;
            } else if (input_ptr < 255 && c != 'o' && c != 'u') {
                input_buffer[input_ptr++] = c;
            }
        }
    }
    outb(0x20, 0x20);
}