/* * include/hal.h - OBA-32 Donanım Soyutlama Katmanı (HAL) Başlık Dosyası
 * İşlemci I/O port yönergelerini inline assembly seviyesinde soyutlar.
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/* Port üzerinden 8-bit ham veri okur */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Port üzerine 8-bit ham veri yazar */
static inline void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

/* Port üzerinden 16-bit ham veri okur */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Port üzerine 16-bit ham veri yazar */
static inline void outw(uint16_t port, uint16_t data) {
    asm volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

/* Sektör veri hattından ardışık 16-bitlik (Word) string veri okuması yapar */
static inline void insw(uint16_t port, void *addr, uint32_t count) {
    asm volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

/* Donanımsal ACPI/QEMU kapatma prosedürünü tetikler */
void sys_shutdown(void);

#endif