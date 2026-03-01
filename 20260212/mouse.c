/* mouse.c */
#include "kernel.h" // outb ve inb prototipleri için

// Global fare durum değişkenleri
int mouse_x = 40;
int mouse_y = 12;
unsigned char mouse_cycle = 0;
char mouse_byte[3];

// Fareye komut göndermek için yardımcı fonksiyon
void mouse_wait(unsigned char type) {
    unsigned int _time_out = 100000;
    if (type == 0) {
        while (_time_out--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (_time_out--) {
            if ((inb(0x64) & 2) == 0) return;
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

    // Fareyi aktif et
    mouse_wait(1);
    outb(0x64, 0xA8);

    // Komut byte'ını al
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    _status = (inb(0x60) | 2);

    // Komut byte'ını güncelle (Kesmeleri aç)
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, _status);

    // Varsayılan ayarları yükle ve veri akışını başlat
    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();
}

void mouse_handler() {
    unsigned char status = inb(0x64);
    
    // Veri gerçekten fareden mi geliyor kontrolü
    if (!(status & 1) || !(status & 0x20)) {
        outb(0x20, 0x20);
        outb(0xA0, 0x20);
        return;
    }

    unsigned char data = inb(0x60);

    // Eğer veri yoksa veya fareden gelmiyorsa hemen çık
    if (!(status & 0x01)) {
        outb(0xA0, 0x20); outb(0x20, 0x20);
        return;
    }
    mouse_byte[mouse_cycle++] = data;


    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        // Byte 0: Tuşlar ve Flagler, Byte 1: X Değişimi, Byte 2: Y Değişimi
        // X ve Y değerlerini mevcut koordinatlara ekle
        int x_rel = mouse_byte[1];
        int y_rel = mouse_byte[2];

        // 8. ve 9. bitler (Sign bit) kontrolü ile negatif sayı dönüşümü
        if (mouse_byte[0] & 0x10) x_rel |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) y_rel |= 0xFFFFFF00;

        extern volatile int cursor_pos_x;
        extern volatile int cursor_pos_y;

        cursor_pos_x += x_rel / 2; // Hassasiyeti ayarlamak için bölme
        cursor_pos_y -= y_rel / 2; // Fare Y ekseni VGA'nın tersidir

        // Ekran sınırlarını koru
        if (cursor_pos_x < 0) cursor_pos_x = 0;
        if (cursor_pos_x > 79) cursor_pos_x = 79;
        if (cursor_pos_y < 0) cursor_pos_y = 0;
        if (cursor_pos_y > 24) cursor_pos_y = 24;
    }

    // Kesme bitti sinyallerini (EOI) gönder
    outb(0xA0, 0x20); // Slave PIC'e gönder (Fare Slave olduğu için önce bu)
    outb(0x20, 0x20); // Master PIC'e gönder
}
