/* hal.h - Hardware Abstraction Layer */
#ifndef HAL_H
#define HAL_H


// Port Girdi/Çıktı Fonksiyonları (Düşük seviyeli donanım erişimi)
void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);


// Bu fonksiyonlar her mimari için (x86, ARM, RISC-V) özel olarak gerçeklenir
void display_putc(char c); // Ekrana tek bir karakter basar
void display_clear();
unsigned char device_read(int device_id);

#endif