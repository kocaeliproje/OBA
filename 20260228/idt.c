/* idt.c */

// IDT ve dış fonksiyon tanımlamaları (DİKKAT: Bunları ekledik)
extern void outb(unsigned short port, unsigned char val);
extern void keyboard_handler_stub(); // Klavye fonksiyonunu tanıttık
extern void mouse_handler_stub();    // Fare fonksiyonunu tanıttık  
extern void idt_flush(unsigned int);
extern void timer_handler_stub();  //Zamanlayıcıyı tanıttık
extern void isr0();
extern void isr13();
extern void isr14();

// IDT Giriş Yapısı
struct idt_entry_struct {
    unsigned short base_low;    // Adresin alt 16 biti
    unsigned short sel;         // Kernel segment seçicisi (GDT'den 0x08)
    unsigned char  always0;     // Her zaman 0 olmalı
    unsigned char  flags;       // Erişim hakları ve kapı tipi
    unsigned short base_high;   // Adresin üst 16 biti
} __attribute__((packed));

// IDT İşaretçisi
struct idt_ptr_struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

// Assembly'den çağıracağımız yükleme fonksiyonu
extern void idt_flush(unsigned int);

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].always0 = 0;
    idt_entries[num].flags = flags;
}

void pic_remap() {
    outb(0x20, 0x11); // Master PIC başlatma
    outb(0xA0, 0x11); // Slave PIC başlatma
    outb(0x21, 0x20); // Master PIC için IDT başlangıç ofseti (32)
    outb(0xA1, 0x28); // Slave PIC için IDT başlangıç ofseti (40)
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    //outb(0x21, 0x0);  // Tüm kesmeleri aç
    //outb(0xA1, 0x0);
    // 0xFC göndererek hem Zamanlayıcıyı (IRQ0) hem Klavyeyi (IRQ1) aktif ediyoruz
    //outb(0x21, 0xFC);
   //outb(0xA1, 0xFF); // Slave PIC tamamen kapalı
    outb(0x21, 0xF8); // Master PIC'te sadece IRQ0 (Timer), IRQ1 (Keyboard) ve IRQ2 (Slave PIC) aktif
    outb(0xA1, 0xEF); // Slave PIC'te sadece IRQ12 (Mouse) aktif

}

void init_idt() {
    pic_remap();
    idt_ptr.limit = sizeof(struct idt_entry_struct) * 256 - 1;
    idt_ptr.base  = (unsigned int)&idt_entries;

    // Şimdilik tüm tabloyu sıfırla
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(32, (unsigned int)timer_handler_stub, 0x08, 0x8E); // Zamanlayıcı kesmesi için bir giriş ekleyelim (IRQ0 -> IDT 32)

    // mouse_handler yerine mouse_handler_stub kullanıyoruz
    idt_set_gate(44, (unsigned int)mouse_handler_stub, 0x08, 0x8E);

    // Şimdi klavye kesmesi için bir giriş ekleyelim (IRQ1 -> IDT 33)
    //idt_set_gate(33, (unsigned int)keyboard_handler, 0x08, 0x8E);

    // keyboard_handler yerine keyboard_handler_stub kullanıyoruz
    idt_set_gate(33, (unsigned int)keyboard_handler_stub, 0x08, 0x8E);

       // İşlemciye IDT'yi yükle
    idt_flush((unsigned int)&idt_ptr);

    // Standart Hataları Kaydet
    idt_set_gate(0, (unsigned int)isr0, 0x08, 0x8E);   // Division by Zero
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E); // General Protection Fault
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E); // Page Fault

    // Mevcut Zamanlayıcı ve Klavye girişlerin kalmalı
    idt_set_gate(32, (unsigned int)timer_handler_stub, 0x08, 0x8E);
    idt_set_gate(33, (unsigned int)keyboard_handler_stub, 0x08, 0x8E);

    idt_flush((unsigned int)&idt_ptr);
}
