/* kernel.h */
#ifndef KERNEL_H
#define KERNEL_H

#include "hal.h" 

// Ekran Fonksiyonları
void print(char *str, char color);
void clear_screen(void);

// Fare Fonksiyonları
void init_mouse(void);
void mouse_handler(void);

// Multiboot yapıları
struct multiboot_mmap_entry {
    unsigned int size;
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type;
} __attribute__((packed));

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length;
    unsigned int mmap_addr;
} __attribute__((packed));

// --- Çekirdek İlklendirme Fonksiyonları ---
void init_gdt(void);
void init_idt(void);
void init_pmm(struct multiboot_info* mbi);
void init_paging(void);
void init_timer(unsigned int frequency);
void init_keyboard(void);

#endif