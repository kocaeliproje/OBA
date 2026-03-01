/* kernel.h */
#ifndef KERNEL_H
#define KERNEL_H

// Port Girdi/Çıktı Fonksiyonları (kernel.c içinde tanımlı)
void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

// Ekran Fonksiyonları
void print(char *str, char color);

// Fare Fonksiyonları (mouse.c içinde tanımlı)
void init_mouse();
void mouse_handler();

#endif
