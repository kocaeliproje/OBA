/* * src/kernel.c - OBA-32 Çekirdek Ana Giriş Noktası ve Çekirdek Döngüsü Yönetimi
 */

#include "kernel.h"
#include "hal.h"
#include "fs.h"
#include "gui.h"
#include "sched.h"
#include "ata.h"
#include "fat32.h"
#include <stdint.h>

/* --- KÜRESEL SİSTEM DURUM VE KOORDİNAT DEĞİŞKENLERİ --- */
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

/* Harici fonksiyon bildirimleri */
extern void init_gdt(void);
extern void init_idt(void);
extern void gorev1(void);
extern void terminal_init(void);
extern WindowSystem_t GuiManager; 
extern Scheduler_t Sched;
extern void load_page_directory(unsigned int*);
extern void enable_paging();

/**
 * Kesmeler kapalıyken (CLI modunda) PIT kanal 0 donanım sayacını 
 * doğrudan porttan okuyarak milisaniye seviyesinde milimetrik gecikme sağlar.
 */
void pit_wait_hardware(unsigned int ms) {
    // PIT her 10 milisaniyede bir (100Hz frekansta) 11931 çevrim sayar.
    unsigned int total_ticks_to_wait = ms / 10;

    /* Kesmeler kapalıyken timer_ticks artmayacağı için port gürültüsünü 
       ve PIT'in durum yazmacını (Status Byte) elle turluyoruz. */
    for (unsigned int i = 0; i < total_ticks_to_wait; i++) {
        // PIT Kanal 0 durumunu mandallat (Latch Command)
        outb(0x43, 0x00); 
        uint8_t low = inb(0x40);
        uint8_t high = inb(0x40);
        uint16_t counter = (high << 8) | low;

        // Sayaç bir tur atana kadar (PIT çevrimi kadar) mikro gecikme döngüsü
        volatile uint16_t last_counter = counter;
        while (last_counter >= counter) {
            last_counter = counter;
            outb(0x43, 0x00);
            low = inb(0x40);
            high = inb(0x40);
            counter = (high << 8) | low;
            asm volatile("pause"); // İşlemci boru hattını rahatlatır
        }
    }
}

void gorev_grafik_masaustu(void) {
    while(1) {
        if (graphics_back_buffer != 0) {
            GuiManager.refresh(&GuiManager); 
        }
        Sched.sleep(&Sched, 16); // ~60 FPS tazeleme oranı emniyeti
    }
}

void init_timer(unsigned int frequency) {
    unsigned int divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096))); 
unsigned int heap_page_table[1024] __attribute__((aligned(4096)));  
unsigned int lfb_page_table[1024] __attribute__((aligned(4096)));   

void init_paging(void) {
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; 
    }
    for(int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 3; 
    }
    page_directory[0] = ((unsigned int)first_page_table) | 3;

    unsigned int heap_base = 0x00A00000;
    unsigned int heap_pd_idx = heap_base >> 22; 
    for(int i = 0; i < 1024; i++) {
        heap_page_table[i] = (0x00800000 + (i * 4096)) | 3; 
    }
    page_directory[heap_pd_idx] = ((unsigned int)heap_page_table) | 3;

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

typedef struct {
    unsigned int gs, fs, es, ds;      
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; 
    unsigned int int_no, err_code;    
    unsigned int eip, cs, eflags, useresp, ss; 
} regs_t;

unsigned int timer_handler(regs_t *r) {
    timer_ticks++;
    outb(0x20, 0x20); 

    if (graphics_back_buffer == 0 || Sched.current_task == -1) {
        return (unsigned int)r;
    }

    unsigned int current_esp = (unsigned int)r;
    Sched.schedule(&Sched, &current_esp);
    return current_esp;
}

void exception_handler(regs_t *r) {
    (void)r;
    while(1) { asm volatile("cli; hlt"); }
}

void gorev1() { while(1) { Sched.sleep(&Sched, 500); } }

void kernel_sleep(unsigned int milliseconds) {
    unsigned int ticks_to_wait = milliseconds / 10; 
    unsigned int start_ticks = timer_ticks;
    while ((timer_ticks - start_ticks) < ticks_to_wait) {
        asm volatile("hlt"); 
    }
}

void kernel_main(unsigned int magic, multiboot_info_t* mbi) {
    (void)magic; 

    init_gdt();
    init_idt();

    if (mbi->flags & (1 << 12)) { 
        vga_lineer_buffer = (uint32_t*)(uintptr_t)mbi->framebuffer_addr_lower;
        screen_width = mbi->framebuffer_width;
        screen_height = mbi->framebuffer_height;
    } else {
        while(1); 
    }

    /* --- BELLEK VE SAYFALAMA KATMANI --- */
    init_pmm(mbi);
    init_paging(); 

    /* --- DİNAMİK HEAP ALANININ KURULMASI --- */
    extern void init_kernel_heap(unsigned int heap_base_addr, unsigned int heap_size);
    init_kernel_heap(0x00A00000, 4 * 1024 * 1024);

    /* ARKA TAMPON ALANINI ALLOCATE EDİYORUZ */
    graphics_back_buffer = (uint32_t*)kmalloc(screen_width * screen_height * sizeof(uint32_t));

    /* --- DONANIM SÜRÜCÜLERİNİN İLKLENDİRİLMESİ --- */
    extern void init_mouse(void);
    init_mouse(); 

    extern void ata_dma_discover(void);
    ata_dma_discover(); 

    /* --- SİSTEM ALTYAPILARININ ÖN HAZIRLIĞI --- */
    GuiManager.init(&GuiManager);
    Sched.init(&Sched);
    terminal_init();

    /* --- GUI PENCERELERİNİN OLUŞTURULMASI --- */
    GuiManager.create_window(&GuiManager, "PANEL", 50, 50, 300, 400, 0); 
    GuiManager.create_window(&GuiManager, "FILES", 400, 100, 350, 250, 1);
    GuiManager.create_window(&GuiManager, "TERMINAL", 200, 250, 450, 280, 2);

    /* VFS Altyapısının İlklendirilmesi */
    init_fs();   

    /* KESMELER KAPALIYKEN DISK SÜRÜCÜ VERİFİKASYONU */
    uint16_t disk_test_write_buffer[256];
    uint16_t disk_test_read_buffer[256];
    for(int i = 0; i < 256; i++) { disk_test_write_buffer[i] = 0x4142; }

    if(ata_write_sector(5, disk_test_write_buffer)) {
        if(ata_read_sector(5, disk_test_read_buffer)) {
            if(disk_test_read_buffer[0] == 0x4142) {
                extern void terminal_write_line(const char* text, uint32_t color);
                terminal_write_line("[STORAGE] ATA HDD Sector 5 Verified", 0x00FF00);
            }
        }
    }

    if (init_fat32()) {
        extern void fat32_mount_to_vfs(void);
        fat32_mount_to_vfs(); 
    }

    /* --- ÇOKLU GÖREV MOTORUNUN KURULMASI --- */
    Sched.create_task(&Sched, 1, gorev1);
    Sched.create_task(&Sched, 2, gorev_grafik_masaustu);

    /* Ana çekirdek akışını zamanlayıcıya 0. görev olarak kaydediyoruz */
    Sched.tasks[0].state = 1; 
    Sched.current_task = 0;   

    /* Splash Screen logosunu doğrudan ekrana basıyoruz */
    extern void draw_splash_screen();
    draw_splash_screen();

    /* 🌟 OPTİMİZASYON: QEMU zamanlamasını kalibre etmek için bekleme süresini 
       1500ms'den 400ms'ye çekiyoruz. Böylece emülatör içinde tam kararında (~1 sn) kalacaktır. */
    pit_wait_hardware(400);

    /* AGRESİF FARE TAMPONU TEMİZLEME MOTORU */
    for (volatile int flush = 0; flush < 1000; flush++) {
        if (inb(0x64) & 1) {
            inb(0x60);
        }
    }

    /* --- ZAMANLAYICIYI AÇ VE KESMELERİ ATEŞLE --- */
    init_timer(100); 
    asm volatile("sti"); 

    /* kernel_main artık kararlı bir görev olduğu için güvenle uyuyabilir */
    kernel_sleep(1000); 

    // Ana akışı sonsuza kadar askıya alıyoruz. Kontrol tamamen GUI'de!
    while(1) {
        Sched.sleep(&Sched, 10000);
    }
}