#include "kernel.h"

/* Linker script (linker.ld) i�inde tan�mlad���m�z sembol. 
   Kernel'�n bitti�i bellek adresini g�sterir. */
extern unsigned int _kernel_end;

/* Bellek tahsisat�n�n ba�layaca�� adres. 
   Kernel'�n hemen bitti�i yerden ba�l�yoruz. */
unsigned int placement_address = (unsigned int)&_kernel_end;

unsigned int *memory_bitmap = 0;
unsigned int total_frames = 0;

#define FRAME_SIZE 4096 // 4KB'l�k sayfalar

/**
 * Fiziksel Bellek Y�netimini Ba�lat�r (PMM)
 * kernel.c i�inden �a�r�l�r.
 */
void init_pmm(struct multiboot_info* mbi) {
    // Multiboot'tan gelen bellek miktar�n� al�yoruz (KB cinsinden gelir)
    unsigned int mem_kb = mbi->mem_upper + mbi->mem_lower;
    
    // Toplam 4KB'l�k blok (frame) say�s�n� hesapla
    total_frames = mem_kb / 4;

    // Basit bir ba�lang�� i�in �imdilik sadece placement_address'i
    // 4 byte hizalayarak haz�r tutuyoruz.
    if (placement_address % 4) {
        placement_address = (placement_address & 0xFFFFFFFC) + 4;
    }
}

/**
 * Basit Bellek Ay�rma Fonksiyonu (Placement Allocator)
 * Kernel ba�lat�l�rken dinamik veri yap�lar� i�in yer ay�r�r.
 */
void* kmalloc_aligned(unsigned int size, int align) {
    if (align && (placement_address % 4096)) {
        // Adresi bir sonraki 4096'nın (4KB) katına hizala
        placement_address = (placement_address & 0xFFFFF000) + 4096;
    }
    unsigned int tmp = placement_address;
    placement_address += size;
    return (void*)tmp;
}

// Eski kodlarla uyumluluk için standart kmalloc
void* kmalloc(unsigned int size) {
    return kmalloc_aligned(size, 0);
}

/**
 * Belirli bir frame'i bitmap �zerinde "dolu" olarak i�aretler.
 */
void set_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) {
        memory_bitmap[idx] |= (1 << off);
    }
}

/**
 * Belirli bir frame'i bitmap �zerinde "bo�" olarak i�aretler.
 */
void clear_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) {
        memory_bitmap[idx] &= ~(1 << off);
    }
}

/**
 * Bitmap �zerinde bir frame'in dolu olup olmad���n� kontrol eder.
 */
unsigned int test_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) {
        return (memory_bitmap[idx] & (1 << off));
    }
    return 0;
}