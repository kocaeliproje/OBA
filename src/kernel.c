#include "kernel.h"
#include "hal.h"
#include "fs.h"
#include "gui.h"
#include "sched.h"

// --- Global Paylaşılan Değişkenler ---
volatile char last_pressed_key = 0; 
volatile int cursor_pos_x = 40; 
volatile int cursor_pos_y = 12;
volatile unsigned int timer_ticks = 0;

// Ekran Boyutları ve Video Belleği
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
volatile char *video_memory = (char*) 0xB8000;
int cursor_x = 0;
int cursor_y = 0;

// --- 1. EKRAN VE YAZDIRMA FONKSİYONLARI ---

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        video_memory[i] = ' ';     
        video_memory[i+1] = 0x07;  
    }
    cursor_x = 0;
    cursor_y = 0;
}

void put_char(char c, char color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        int index = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        video_memory[index] = ' ';
        video_memory[index + 1] = color;
    } else {
        int index = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        video_memory[index] = c;
        video_memory[index + 1] = color;
        cursor_x++;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // Ekran kaydırma (scrolling) kontrolü
    if (cursor_y >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i++) {
            video_memory[i] = video_memory[i + VGA_WIDTH * 2];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i < VGA_HEIGHT * VGA_WIDTH * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i+1] = 0x07;
        }
        cursor_y = VGA_HEIGHT - 1;
    }
}

// Linker'ın fs.c içinde bulamadığı kritik fonksiyon!
void print(char *str, char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        put_char(str[i], color);
    }
}

void print_hex(unsigned int n) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];
    buffer[0] = '0'; buffer[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buffer[i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    buffer[10] = '\0';
    print(buffer, 0x0E); 
}

// --- 2. DONANIM İLKLENDİRME GÖVDELERİ ---

void init_keyboard(void) {
    // Klavye kesme haritalaması idt.c'de yapılıyor, linker için boş gövde yeterli.
}

void init_timer(unsigned int frequency) {
    unsigned int divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

// Paging Sürücüsü Altyapısı
extern void load_page_directory(unsigned int*);
extern void enable_paging();

// Sayfalama dizinleri (Hizalanmış)
unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));

void init_paging(void) {
    // 1. Tüm sayfa dizinini "Mevcut Değil (Not Present)" olarak ayarla
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Kernel modu, Read/Write, Not Present
    }

    // 2. İlk 4 MB'lık alanı birebir (Identity Map) haritala
    // Bu sayede hem kernel kodları (1MB), hem yığın, hem de VGA Belleği (0xA0000) korunur.
    for(int i = 0; i < 1024; i++) {
        // i * 4096 adresi üretir (0x00000000'dan 0x003FF000'a kadar)
        first_page_table[i] = (i * 4096) | 3; // Present, Read/Write, Supervisor
    }

    // 3. İlk sayfa tablosunu sayfa dizininin 0. indeksine bağla (0x00000000 - 0x003FFFFF)
    page_directory[0] = ((unsigned int)first_page_table) | 3;

    // 4. Kontrol kayıtçılarını doldur ve sayfolamayı ateşle
    load_page_directory(page_directory);
    enable_paging();
}

// --- 3. GÖREV VE ZAMANLAYICI YAPILARI ---

// Her görevin tüm kayıtçılarını saklayacağı yapı (boot.s'teki pusha/popa sırasıyla uyumlu)
typedef struct {
    unsigned int gs, fs, es, ds;      
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; 
    unsigned int int_no, err_code;    
    unsigned int eip, cs, eflags, useresp, ss; 
} regs_t;

// boot.s içindeki timer_handler_stub burayı tetikler
// src/kernel.c içindeki eski timer_handler'ı tamamen bununla değiştirin:
unsigned int timer_handler(regs_t *r) {
    timer_ticks++;
    
    // Master PIC'e kesme bitti sinyali (EOI) gönder
    outb(0x20, 0x20); 

    // GUI katmanını ve ekran tamponunu yenile
    GuiManager.refresh(&GuiManager);

    // Eğer Scheduler (Zamanlayıcı) henüz başlatılmadıysa veya görev yoksa mevcut yığınla devam et
    if (Sched.current_task == -1) {
        return (unsigned int)r;
    }

    // Mevcut görevin yığın adresini (`esp`) sakla
    unsigned int current_esp = (unsigned int)r;

    // Zamanlayıcıyı çağır ve sıradaki görevin yığın adresini al
    Sched.schedule(&Sched, &current_esp);

    // Yeni görevin ESP adresini boot.s'e geri döndür
    return current_esp;
}

// boot.s içindeki common_exception_stub'ın aradığı kritik fonksiyon!
void exception_handler(regs_t *r) {
    clear_screen();
    print("!!! OBA CEKIRDEK HATASI (EXCEPTION) !!!\n", 0x4F);
    print("Hata Numarasi: ", 0x0F);
    print_hex(r->int_no);  
    print("\nAdres (EIP): ", 0x0F);
    print_hex(r->eip);
    print("\nSistem guvenlik nedeniyle durduruldu.", 0x0C);
    asm volatile("cli; hlt");
}

// --- 4. SİSTEM GÖREVLERİ (THREADS) ---

void gorev1() {
    while(1) {
        Sched.sleep(&Sched, 500); // 500ms uyuma (CPU'yu kilitlemez)
    }
}

// Orijinal fs.h fonksiyonlarına erişmek için üstte extern bildirim yapalım
extern unsigned char* read_file(char* name);

void gorev_masaustu() {
    while(1) {
        // --- 1. SÜRÜKLEME VE PENCERE MANTIĞI ---
        // kernel.c içindeki update_desktop_logic() fonksiyonunu sildiyseniz bile 
        // doğrudan GuiManager üzerinden pencerelerin koordinatlarını güncel tutabiliriz.
        // (Şimdilik pencerelerimizin başlıklarını ve statik metinlerini görelim)

        // --- 2. DOSYA İÇERİĞİNİ PENCEREYE YAZDIRMA ---
        // "Dosya Yoneticisi" başlığına sahip olan penceremizin içine txt dosyasını basalım
        unsigned char* file_data = read_file("oba.txt");
        if (file_data) {
            // GuiManager.windows[1] bizim Dosya Yöneticimizdi (kernel_main içinde ekleme sırasına göre)
            Window* file_win = &GuiManager.windows[1];
            
            // Dosya içeriğini pencerenin sol üst gövdesinden (y+2, x+2) başlayarak yazalım
            int text_y = file_win->y + 2;
            int text_x = file_win->x + 2;
            
            for(int i = 0; file_data[i] != '\0'; i++) {
                int idx = (text_y * 80 + (text_x + i)) * 2;
                if(idx >= 0 && idx < 80 * 25 * 2 && (text_x + i) < (file_win->x + file_win->width - 2)) {
                    GuiManager.back_buffer[idx] = file_data[i];
                    GuiManager.back_buffer[idx+1] = 0x1E; // Mavi arka plan üzerine beyaz yazı
                }
            }
        }

        // Zamanlayıcıyı uyutarak CPU'yu rahatlat ve ekranı ~60 Hz tazele
        Sched.sleep(&Sched, 16); 
    }
}

// --- 5. ANA GİRİŞ NOKTASI ---

void kernel_main(unsigned int magic, struct multiboot_info* mbi) {
    (void)magic; // Unused warning'i susturmak için

    clear_screen();
    print("OBA Isletim Sistemi Yukleniyor...\n", 0x0E);

    // Donanım Katmanları
    init_gdt();
    init_idt();
    init_pmm(mbi);
    init_paging();

    // Modüllerin (OOP) İlklendirilmesi
    GuiManager.init(&GuiManager);
    Sched.init(&Sched);

    // Dosya Sisteminin Kurulması (Artık ölü kod değil, çalışıyor!)
    init_fs();
    create_file("oba.txt", "OBA Isletim Sistemi Moduler Yapisi Hazir!");

    // Örnek pencereleri oluştur
    GuiManager.create_window(&GuiManager, "PANEL", 5, 2, 25, 6, 0); 
    GuiManager.create_window(&GuiManager, "FILES", 35, 5, 35, 10, 1);

    // Çoklu Görevleri Kaydet (Index 0 ana akış için ayrılabilir, 1 ve 2'ye yazalım)
    Sched.create_task(&Sched, 1, gorev1);
    Sched.create_task(&Sched, 2, gorev_masaustu);

    // Sürücüleri ve Zamanlayıcıyı Çalıştır
    init_timer(100); // 100 Hz
    init_keyboard();
    init_mouse();
    
    print("Kesmeler aciliyor, GUI baslatiliyor...\n", 0x0A);
    asm volatile("sti"); // Donanımsal kesintileri (IRQs) serbest bırak

    // Çekirdek Boşta Kalma Döngüsü (Idle Thread)
    while(1) {
        asm volatile("hlt"); 
    }
}