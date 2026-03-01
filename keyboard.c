/* keyboard.c */
#include "kernel.h"

// Dýþ deðiþkenleri tanýmlayalým (kernel.c'den gelecekler)
extern volatile char last_pressed_key;
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;

char input_buffer[256];
int input_ptr = 0;

// Standart Q klavye haritasý (Basit hali)
unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0,	/* F1-F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* End key */
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

void keyboard_handler() {
    unsigned char scancode = inb(0x60);

    // Tuþ býrakýlmadýysa (0x80 bit'i basýlý olmadýðýný gösterir)
    if (!(scancode & 0x80)) {
        
        // 'o' tuþu kontrolü (Pencere için)
        if (scancode == 0x18) {
            last_pressed_key = 'o';
        }

        char c = keyboard_map[scancode];
        if (c != 0) {
            // WASD ile imleç hareketi (Opsiyonel, test için kalabilir)
            if (c == 'w' && cursor_pos_y > 0) cursor_pos_y--;
            if (c == 's' && cursor_pos_y < 24) cursor_pos_y++;
            if (c == 'a' && cursor_pos_x > 0) cursor_pos_x--;
            if (c == 'd' && cursor_pos_x < 79) cursor_pos_x++;
            
            // Komut satýrý buffer mantýðý
            if (c == '\n') {
                input_buffer[input_ptr] = '\0';
                // process_command(input_buffer); // kernel.c'de tanýmlý olmalý
                input_ptr = 0;
            } else if (c == '\b' && input_ptr > 0) {
                input_ptr--;
            } else if (input_ptr < 255) {
                input_buffer[input_ptr++] = c;
            }
        }
    }
    outb(0x20, 0x20); // EOI
}