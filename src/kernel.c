/* * src/kernel.c - OBA-32 Çekirdek Ana Giriş Noktası ve Çekirdek Döngüsü Yönetimi
 * Bu modül; donanım tablolarının ilklendirilmesi, bellek yönetimi, sayfalama ve 
 * çekirdek görevlerinin (task) zamanlayıcıya kaydedilmesinden sorumludur.
 */

#include "kernel.h"
#include "hal.h"
#include "fs.h"
#include "gui.h"
#include "sched.h"
#include <stdint.h>

/* Global sistem durum ve koordinat değişkenleri */
volatile char last_pressed_key = 0; 
volatile int cursor_pos_x = 400; 
volatile int cursor_pos_y = 300; 
volatile int mouse_left_button = 0; 
volatile unsigned int timer_ticks = 0;

/* Grafik katmanı ve doğrusal çerçeve arabelleği (LFB) göstericileri */
uint32_t* vga_lineer_buffer = 0; 
uint32_t* graphics_back_buffer = 0;
uint32_t screen_width = 800;
uint32_t screen_height = 600;

/* Dış modüllerden aktarılan fonksiyon bildirimleri */
extern void init_gdt(void);
extern void init_idt(void);
extern void gorev1(void);

/* Grafik tabanlı masaüstü görevinin ana döngüsü */
void gorev_grafik_masaustu(void) {
    while(1) {
        Sched.sleep(&Sched, 16); 
    }
}

/* Küresel sistem nesne bildirimleri */
extern WindowSystem_t GuiManager; 
extern Scheduler_t Sched;

/* Eski metin modu (VGA Text Mode) tampon tanımlamaları */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
volatile char *video_memory = (char*) 0xB8000;
int cursor_x = 0;
int cursor_y = 0;

/* Metin modu ekran belleğini temizler */
void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        video_memory[i] = ' ';     
        video_memory[i+1] = 0x07;  
    }
    cursor_x = 0; cursor_y = 0;
}

/* Metin modu ekranına tek bir karakter basar */
void put_char(char c, char color) {
    if (c == '\n') { cursor_x = 0; cursor_y++; }
    else {
        int index = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        video_memory[index] = c;
        video_memory[index + 1] = color;
        cursor_x++;
    }
}

/* Metin modu ekranına karakter dizisi (string) yazdırır */
void print(char *str, char color) {
    for (int i = 0; str[i] != '\0'; i++) put_char(str[i], color);
}

/* Onaltılık (Hexadecimal) sayısal değerleri ekrana yazdırır */
void print_hex(unsigned int n) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11]; buffer[0] = '0'; buffer[1] = 'x';
    for (int i = 9; i >= 2; i--) { buffer[i] = hex_chars[n & 0xF]; n >>= 4; }
    buffer[10] = '\0'; print(buffer, 0x0E); 
}

/* Programlanabilir Kesme Zamanlayıcısı (PIT) frekans ayarlarını yürütür */
void init_timer(unsigned int frequency) {
    unsigned int divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

/* Assembly seviyesindeki sayfalama denetim fonksiyonları */
extern void load_page_directory(unsigned int*);
extern void enable_paging();

/* Çekirdek sayfa dizini ve sayfa tabloları yerleşimi */
unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));
unsigned int lfb_page_table[1024] __attribute__((aligned(4096))); 

/* Sanal bellek haritalama ve sayfalama (Paging) sistemini ilklendirir */
void init_paging(void) {
    for(int i = 0; i < 1024; i++) page_directory[i] = 0x00000002; 
    for(int i = 0; i < 1024; i++) first_page_table[i] = (i * 4096) | 3; 
    page_directory[0] = ((unsigned int)first_page_table) | 3;

    /* Doğrusal çerçeve arabelleği (LFB) için identity mapping haritalaması */
    if (vga_lineer_buffer != 0) {
        unsigned int lfb_base = (unsigned int)vga_lineer_buffer;
        unsigned int lfb_pd_idx = lfb_base >> 22;
        unsigned int page_frame = lfb_base & 0xFFC00000; 
        for(int i = 0; i < 1024; i++) {
            lfb_page_table[i] = (page_frame + (i * 4096)) | 3; 
        }
        page_directory[lfb_pd_idx] = ((unsigned int)lfb_page_table) | 3;
    }

    load_page_directory(page_directory);
    enable_paging();
}

/* İşlemci yazmaç (register) durum yapısı */
typedef struct {
    unsigned int gs, fs, es, ds;      
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; 
    unsigned int int_no, err_code;    
    unsigned int eip, cs, eflags, useresp, ss; 
} regs_t;

/* PIT (IRQ0) donanım kesme işleyicisi */
unsigned int timer_handler(regs_t *r) {
    timer_ticks++;
    outb(0x20, 0x20); 

    /* Grafik arka tamponu aktif ise arayüz tazelemeyi yürütür */
    if (graphics_back_buffer != 0) {
        GuiManager.refresh(&GuiManager);
    }

    /* * EMNİYET KONTROLÜ: Görev planlayıcı (Scheduler) henüz ilklendirilmediyse,
     * grafik arka tamponu henüz tahsis edilmediyse (logo aşamasındaysa)
     * veya aktif bir görev yoksa, bağlam değişimini (context switch) güvenli olarak es geç.
     */
    if (graphics_back_buffer == 0 || Sched.current_task == -1) {
        return (unsigned int)r;
    }

    unsigned int current_esp = (unsigned int)r;
    Sched.schedule(&Sched, &current_esp);
    return current_esp;
}

/* Çekirdek istisna (exception) işleyicisi */
void exception_handler(regs_t *r) {
    (void)r;
    while(1) { asm volatile("cli; hlt"); }
}

/* Örnek sistem görevi 1 */
void gorev1() { while(1) { Sched.sleep(&Sched, 500); } }

/* Belirtilen milisaniye cinsinden donanımsal gecikme yürütür */
void kernel_sleep(unsigned int milliseconds) {
    unsigned int ticks_to_wait = milliseconds / 10; /* PIT 100Hz ayarlı olduğundan her tık 10ms'dir */
    unsigned int start_ticks = timer_ticks;
    while ((timer_ticks - start_ticks) < ticks_to_wait) {
        asm volatile("hlt"); /* İşlemciyi bir sonraki kesmeye kadar uyku moduna alarak veri hattını korur */
    }
}

/* * Çekirdek Ana Giriş Fonksiyonu (Kernel Main Entry Point)
 * Multiboot önyükleyicisi sonrasında kontrolü devralan ana akış mimarisi.
 */
void kernel_main(unsigned int magic, multiboot_info_t* mbi) {
    (void)magic; 

    /* Donanım tanımlama tablolarının kurulması */
    init_gdt();
    init_idt();

    /* Multiboot yapısı üzerinden grafik arabellek adresinin tespiti */
    if (mbi->flags & (1 << 12)) { 
        vga_lineer_buffer = (uint32_t*)(uintptr_t)mbi->framebuffer_addr_lower;
        screen_width = mbi->framebuffer_width;
        screen_height = mbi->framebuffer_height;
    } else {
        while(1); 
    }

    /* Fiziksel bellek yönetimi ve sayfalama sisteminin devreye alınması */
    init_pmm(mbi);
    init_paging(); 

    /* * KRİTİK DONANIM GÜNCELLEMESİ: PS/2 Fare denetleyicisi donanım hat sinyallerinin 
     * çakışmasını engellemek amacıyla, kesmeler açılmadan ÖNCE güvenli alanda kurulur.
     */
    extern void init_mouse(void);
    init_mouse(); 

    /* Gecikme sayacının aktifleşmesi için donanım zamanlayıcısının ilklendirilmesi */
    init_timer(100); /* 100 Hz frekans doğrultusunda kurulur */
    asm volatile("sti"); /* Zamanlayıcı tıklarının sayılması amacıyla kesmeler dünyaya açılır */

    /* --- SİSTEM AÇILIŞ SEKANSI (SPLASH SCREEN) AŞAMASI --- */
    extern void draw_splash_screen();
    draw_splash_screen();

    /* Donanım tabanlı zamanlayıcı kesmesi ile tam olarak 1000 milisaniye (1 saniye) kararlı gecikme */
    kernel_sleep(1000); 
    /* --- SİSTEM AÇILIŞ SEKANSI SONLANDIRILDI --- */

    /* Çift tamponlama için dinamik bellek alanından arka tampon tahsisi */
    graphics_back_buffer = (uint32_t*)kmalloc(screen_width * screen_height * sizeof(uint32_t));

    /* GUI ve Görev Zamanlayıcı modüllerinin ilklendirilmesi */
    GuiManager.init(&GuiManager);
    Sched.init(&Sched);

    /* Başlangıç grafik pencerelerinin koordinat ve tip tanımlamaları */
    GuiManager.create_window(&GuiManager, "PANEL", 50, 50, 300, 400, 0); 
    GuiManager.create_window(&GuiManager, "FILES", 400, 100, 350, 250, 1);
    GuiManager.create_window(&GuiManager, "TERMINAL", 200, 250, 450, 280, 2);

    /* Sanal dosya sisteminin ilklendirilmesi ve örnek dosya üretimi */
    init_fs();
    create_file("oba.txt", "OBA Isletim Sistemi Moduler Yapisi Hazir!");

    /* Çekirdek görevlerinin zamanlayıcıya enjekte edilmesi */
    Sched.create_task(&Sched, 1, gorev1);
    Sched.create_task(&Sched, 2, gorev_grafik_masaustu);

    /* Ana çekirdek askı döngüsü */
    while(1) {
        asm volatile("hlt"); 
    }
}