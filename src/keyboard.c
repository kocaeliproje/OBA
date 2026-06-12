/* * src/keyboard.c - OBA-32 Donanımsal PS/2 Klavye Sürücüsü (Ok Tuşları Entegrasyonu)
 * Bu modül; iki baytlık genişletilmiş scancode verilerini çözerek dairesel
 * kuyruğa benzersiz kontrol karakterleri enjekte eder.
 */

#include "kernel.h"
#include "hal.h"

extern volatile char last_pressed_key;
volatile int is_ctrl_pressed = 0;
static int is_extended_scancode = 0; /* ✨ Ok tuşları ön ek durum takibi */

#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_fifo_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int fifo_write_ptr = 0;
static volatile int fifo_read_ptr = 0;

static const unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
  '9', '0', '-', '=', '\b',
  '\t',
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0, 
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', ',', '.', '/',   0, 
  '*',
    0, 
  ' ', 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void fifo_push(char c) {
    int next = (fifo_write_ptr + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != fifo_read_ptr) {
        keyboard_fifo_buffer[fifo_write_ptr] = c;
        fifo_write_ptr = next;
    }
}

char keyboard_getchar(void) {
    if (fifo_read_ptr == fifo_write_ptr) return 0;
    char c = keyboard_fifo_buffer[fifo_read_ptr];
    fifo_read_ptr = (fifo_read_ptr + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);

    /* Eğer donanım 0xE0 gönderdiyse, bir sonraki bayt bir yön tuşudur */
    if (scancode == 0xE0) {
        is_extended_scancode = 1;
        outb(0x20, 0x20);
        return;
    }

    if (is_extended_scancode) {
        is_extended_scancode = 0;
        if (scancode == 0x48) {      /* Yukarı Ok Basıldı */
            fifo_push((char)0xE0);   /* Benzersiz Yukarı Belirteci */
            outb(0x20, 0x20);
            return;
        } else if (scancode == 0x50) { /* Aşağı Ok Basıldı */
            fifo_push((char)0xE1);   /* Benzersiz Aşağı Belirteci */
            outb(0x20, 0x20);
            return;
        }
    }

    if (scancode == 0x1D) is_ctrl_pressed = 1;
    else if (scancode == 0x9D) is_ctrl_pressed = 0;

    if (!(scancode & 0x80)) {
        char c = keyboard_map[scancode];
        if (c != 0) {
            last_pressed_key = c;
            fifo_push(c);
        }
    }
    outb(0x20, 0x20);
}

void init_keyboard(void) {
    fifo_write_ptr = 0; fifo_read_ptr = 0; last_pressed_key = 0; is_ctrl_pressed = 0; is_extended_scancode = 0;
}