/* kernel.h */
#ifndef KERNEL_H
#define KERNEL_H

#include "hal.h" 
#include <stdint.h>

// --- OBA Grafik Çözünürlük Tanımları ---
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

// kernel.c'de tanımlanan global buffer'lar
extern uint32_t* vga_lineer_buffer;
extern uint32_t* graphics_back_buffer;

// Fonksiyon prototipleri
void draw_pixel(int x, int y, uint32_t color);

// Ekran Fonksiyonları (Grafik uyumlu)
void print(char *str, char color);
void clear_screen(void);

// Sürücü Fonksiyonları
void init_mouse(void);
void mouse_handler(void);
void init_keyboard(void);
void init_timer(unsigned int frequency);

// Multiboot Hafıza Haritası Yapısı
struct multiboot_mmap_entry {
    unsigned int size;
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

// Buradaki eksik struct çakışmasını engellemek için doğrudan multiboot.h'ı dahil ediyoruz
#include "multiboot.h"

// --- Çekirdek İlklendirme Fonksiyonları ---
void init_gdt(void);
void init_idt(void);
void init_pmm(multiboot_info_t* mbi); // Güncellendi: multiboot_info_t kullanıyor
void init_paging(void);
void* kmalloc(unsigned int size); // mm.c içindeki bellek tahsis motoru

#endif