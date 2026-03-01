/* kernel.c - Gelişmiş Versiyon */
extern void load_page_directory(unsigned int*);
extern void enable_paging();

#include "kernel.h"
extern void load_page_directory(unsigned int*);

extern void* kmalloc(unsigned int size);// Dışarıdan erişilecek kmalloc fonksiyonu

void check_scroll();
void init_gdt();
void init_idt();

// Dışarıdan gelecek tuş verisi (keyboard.c veya kernel.c'den)
 extern volatile char last_pressed_key;

 // Diğer dosyalardan extern ile erişilecek ana değişkenler
volatile char last_pressed_key; 
extern char input_buffer[128]; 
extern int input_ptr;

#define HEAP_LIMIT 0x00800000 // 8MB sınırı

#define BLOCK_SIZE 4096
#define BLOCKS_PER_BYTE 8

// kernel.c - Global Koordinatlar
volatile int cursor_pos_x = 40; // Ekranın ortası
volatile int cursor_pos_y = 12;

// Yaklaşık 128MB RAM desteği için bitmap boyutu
unsigned char memory_bitmap[4096];  // 4096 byte'lık bitmap, 128MB RAM'i (32768 blok) yönetebilir.
unsigned int total_blocks;
unsigned int second_page_table[1024] __attribute__((aligned(4096)));

/* Bitmap Yardımcı Fonksiyonları */
// Belirli bir bloğu dolu olarak işaretle
void mmap_set(int bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8));
}

// Belirli bir bloğu boş olarak işaretle
void mmap_unset(int bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Bir bloğun dolu olup olmadığını kontrol et
int mmap_test(int bit) {
    return memory_bitmap[bit / 8] & (1 << (bit % 8));
}

/* Multiboot bilgi yapısı */
struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length; // Bellek haritası uzunluğu
    unsigned int mmap_addr;   // Bellek haritası adresi
};

struct multiboot_mmap_entry {
    unsigned int size;
    unsigned int base_addr_low;
    unsigned int base_addr_high;
    unsigned int length_low;
    unsigned int length_high;
    unsigned int type; // 1 = Kullanılabilir RAM, diğerleri = Ayrılmış
} __attribute__((packed));


struct process_control_block {
    unsigned int esp;        // İşlemin yığın adresi
    unsigned int ebp;        // Base pointer
    unsigned int eip;        // Instruction pointer (nerede kalmıştı?)
    unsigned int page_dir;   // Kendi sayfa dizini
    int state;               // 0: Hazır, 1: Çalışıyor, 2: Bekliyor
};

// Her görevin tüm kayıtçılarını saklayacağı yapı
typedef struct {
    unsigned int gs, fs, es, ds;      // En son bunlar push ediliyor
    unsigned int edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; // pusha sırası
    unsigned int int_no, err_code;    // Kesme bilgileri
    unsigned int eip, cs, eflags, useresp, ss; // IRET çerçevesi
} regs_t;

// pcb_t yapısını güncelleyelim
typedef struct {
    unsigned int esp;        // Mevcut yığın adresi (regs_t işaretçisi)
    unsigned int state;      
} pcb_t;


pcb_t* task_list[10];        // Şimdilik en fazla 10 görev
int current_task = 0;
int num_tasks = 0;

typedef struct {
    int x, y, w, h;
    int is_dragging;
    int is_visible; // 1 ise çiz, 0 ise gizle
} window_t;

/* 2. Global Değişken Tanımı */
window_t win1 = {10, 5, 30, 10, 0, 1};

/* Fiziksel Bellek Yönetimi Sabitleri */
#define BLOCK_SIZE 4096
#define BLOCKS_PER_BYTE 8


/* İlk boş bloğu bulan fonksiyon */
int first_free_block() {
    for (int i = 0; i < 4096 * 8; i++) {
        if (!mmap_test(i)) return i;
    }
    return -1; // Boş yer kalmadı
}

/* Bir blok tahsis eden fonksiyon (Basit malloc başlangıcı) */
void *alloc_block() {
    int free_block = first_free_block();
    if (free_block == -1) return (void*)0;
    mmap_set(free_block);
    return (void*)(free_block * BLOCK_SIZE);
}

// Ekran boyutları (Standart VGA metin modu)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Ekran belleği başlangıcı
volatile char *video_memory = (char*) 0xB8000;

// Ekranda o an hangi satır ve sütunda olduğumuzu takip edelim
int cursor_x = 0;
int cursor_y = 0;

// Ekranı temizleyen fonksiyon
void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        video_memory[i] = ' ';     // Boşluk karakteri
        video_memory[i+1] = 0x07;  // Standart gri renk
    }
    cursor_x = 0;
    cursor_y = 0;
}

void put_char(char c, char color) {
    // Enter (Yeni Satır) Kontrolü
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } 
    // Backspace (Geri Silme) Kontrolü
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        // Mevcut yerdeki karakteri sil (boşluk bas)
        int index = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        video_memory[index] = ' ';
        video_memory[index + 1] = color;
    }
    // Normal Karakter Basımı
    else {
        int index = (cursor_y * VGA_WIDTH + cursor_x) * 2;
        video_memory[index] = c;
        video_memory[index + 1] = color;

        cursor_x++;
    }

    // Ekranın sonuna gelindiyse başa/alt satıra dön
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        check_scroll(); // Ekran kaydırma kontrolü
    }
    
    // İsteğe bağlı: Ekran kaydırma (scrolling) buraya eklenebilir
}

void check_scroll() {
    if (cursor_y >= VGA_HEIGHT) {
        // Tüm satırları bir yukarı kopyala
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i++) {
            video_memory[i] = video_memory[i + VGA_WIDTH * 2];
        }
        // En alt satırı temizle
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH * 2; i < VGA_HEIGHT * VGA_WIDTH * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i+1] = 0x07;
        }
        cursor_y = VGA_HEIGHT - 1;
    }
}


// Ekrana tüm bir metni (string) basan fonksiyon
void print(char *str, char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        put_char(str[i], color);
    }
}



// PORT ile konuşmak için (İşlemcinin dış dünyaya açılan kapıları)

// İşlemciden dış dünyaya (Porta) veri gönderir
void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Dış dünyadan (Porttan) işlemciye veri okur
unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

volatile unsigned int timer_ticks = 0;

// Zamanlayıcıyı belirli bir frekansta (Hz) başlatan fonksiyon
void init_timer(unsigned int frequency) {
    // PIT frekansı standart olarak 1193180 Hz'dir
    unsigned int divisor = 1193180 / frequency;

    // Komut register'ına (0x43) 0x36 göndererek zamanlayıcıyı başlatıyoruz
    outb(0x43, 0x36);

    // Divisor değerini düşük ve yüksek byte olarak gönderiyoruz
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

// Basit bir uyku fonksiyonu (ms cinsinden)
void sleep_ms(unsigned int ms) {
    unsigned int start_ticks = timer_ticks;
    // 100 Hz frekansta her tik 10ms'dir (1000ms / 100 = 10)
    unsigned int ticks_to_wait = ms / 10;
    while (timer_ticks < start_ticks + ticks_to_wait);
}

int str_compare(char *s1, char *s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 1;
        i++;
    }
    return 0;
}

void process_command(char *cmd) {
    print("\n", 0x07);
    if (str_compare(cmd, "selam")) {
        print("Cekirdekten size de selam!\n", 0x0B);
    } else if (str_compare(cmd, "temizle")) {
        clear_screen();
    } else {
        print("Bilinmeyen komut: ", 0x0C);
        print(cmd, 0x0C);
        print("\n", 0x07);
    }
    print("> ", 0x0A);
}

void print_hex(unsigned int n) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        buffer[i] = hex_chars[n & 0xF];
        n >>= 4;
    }
    buffer[10] = '\0';
    print(buffer, 0x0E); // Sayıları sarı renkte basalım
}

void init_pmm(struct multiboot_info* mbi) {
    // 1. Tüm belleği dolu (reserved) işaretle
    for (int i = 0; i < 4096; i++) memory_bitmap[i] = 0xFF;

    // 2. Multiboot haritasını tara ve "Kullanılabilir" (Type 1) alanları "Boş" (0) yap
    struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mbi->mmap_addr;
    
    while((unsigned int)mmap < mbi->mmap_addr + mbi->mmap_length) {
        if (mmap->type == 1) { 
            unsigned int start_block = mmap->base_addr_low / BLOCK_SIZE;
            unsigned int num_blocks = mmap->length_low / BLOCK_SIZE;
            
            for (unsigned int i = 0; i < num_blocks; i++) {
                mmap_unset(start_block + i);
            }
        }
        mmap = (struct multiboot_mmap_entry*)((unsigned int)mmap + mmap->size + sizeof(mmap->size));
    }
    
    // 3. Çekirdeğin kendi kullandığı alanı (ilk 1-2 MB) tekrar dolu işaretle
    // Kendi kodumuzun üzerine veri yazmamak için bu şart.
    for (int i = 0; i < 512; i++) mmap_set(i); 
}


// Her Page Directory 1024 giriş içerir
unsigned int page_directory[1024] __attribute__((aligned(4096)));
// İlk 4MB'ı haritalayacak ilk sayfa tablomuz
unsigned int first_page_table[1024] __attribute__((aligned(4096)));

void init_paging() {
    for(int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002;
    }

    for(int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 3;
    }

    for(int i = 0; i < 1024; i++) {
        second_page_table[i] = (0x400000 + (i * 4096)) | 3;
    }

    page_directory[0] = ((unsigned int)first_page_table) | 3;
    page_directory[1] = ((unsigned int)second_page_table) | 3;

    // VGA Video RAM (0xA0000) haritalama
    for(int i = 0xA0; i < 0xBF; i++) {
        first_page_table[i] = (i * 4096) | 3;
    } // <-- BU PARANTEZ EKSİKTİ

    load_page_directory(page_directory);
    enable_paging();
} 

/* kernel.c - Basit Heap Yönetimi */

// Çekirdek için basit bir heap başlangıç adresi (Örn: 4MB sonrası)
unsigned int heap_current = 0x00400000; 


// Basit bir görev oluşturma fonksiyonu (Çok basit, sadece bir fonksiyon çalıştıracak şekilde)
void create_task(void (*func)()) {
    pcb_t* new_task = (pcb_t*) kmalloc(sizeof(pcb_t));
    unsigned int stack_base = (unsigned int) kmalloc(4096); 
    
    // Yığının en üstünü (high memory) byte olarak hesaplayıp struct'a dönüştürün
    regs_t* r = (regs_t*)(stack_base + 4096 - sizeof(regs_t));

    // Manuel olarak tüm struct'ı sıfırlayın
    unsigned int *ptr = (unsigned int*)r;
    for(unsigned int i = 0; i < sizeof(regs_t)/4; i++) ptr[i] = 0;

    r->gs = r->fs = r->es = r->ds = 0x10;
    r->eip = (unsigned int)func;
    r->cs = 0x08;
    r->eflags = 0x202; // Kesmeler açık (Interrupt Flag)

    new_task->esp = (unsigned int)r;
    new_task->state = 1;
    task_list[num_tasks++] = new_task;
}


extern void switch_to_stack(unsigned int new_esp);

// Dikkat: Bu fonksiyon artık 'regs_t *r' parametresi alacak
// Zamanlayıcı kesmesi geldiğinde çağrılacak fonksiyon

unsigned int timer_handler(regs_t *r) {
    timer_ticks++;
    outb(0x20, 0x20); // Master PIC EOI

    if (num_tasks < 2) return (unsigned int)r;

    // Mevcut yığını mevcut görevin PCB'sine kaydet
    task_list[current_task]->esp = (unsigned int)r;

    // Bir sonraki göreve geç
    current_task = (current_task + 1) % num_tasks;

    // Yeni görevin yığınını döndür (boot.s bu değeri ESP'ye yükleyecek)
    return task_list[current_task]->esp;
}


void gorev1() {
    volatile char *video = (char*)0xB8000 + (10 * 80 * 2); 
    while(1) {
        video[0] = '1'; video[1] = 0x0A; // Yeşil 1
        sleep_ms(500); // Yarım saniye açık
        video[0] = ' ';
        sleep_ms(500); // Yarım saniye kapalı
    }
}

void gorev2() {
    volatile char *video = (char*)0xB8000 + (15 * 80 * 2); 
    while(1) {
        video[0] = '2'; video[1] = 0x0C; // Kırmızı 2
        sleep_ms(700); // Farklı bir hızda yanıp sönsün
        video[0] = ' ';
        sleep_ms(700);
    }
}

void exception_handler(regs_t *r) {
    clear_screen();
    print("!!! SISTEM KRITIK HATASI !!!\n", 0x4F);
    print("Hata Numarasi: ", 0x0F);
    print_hex(r->int_no);  // Artık tam olarak 0x0 göreceksin!
    print("\nAdres (EIP): ", 0x0F);
    print_hex(r->eip);
    
    if (r->int_no == 0) print("\nTur: Sifira Bolme Hatasi!", 0x0E);
    if (r->int_no == 14) print("\nTur: Page Fault", 0x0E);

    print("\nSistem Durduruldu.", 0x0C);
    asm volatile("cli; hlt");
}


/* kernel.c - Grafik Fonksiyonları */
volatile unsigned char *vga_mem = (unsigned char*) 0xA0000;

void draw_pixel(int x, int y, unsigned char color) {
    // 320x200 modunda adres = y * genişlik + x
    vga_mem[y * 320 + x] = color;
}

void draw_rect(int x, int y, int w, int h, unsigned char color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void gorev_cizim() {
    int old_x = cursor_pos_x;
    int old_y = cursor_pos_y;
    
    while(1) {
        // 1. Eğer konum değiştiyse eskiyi sil
        if (old_x != cursor_pos_x || old_y != cursor_pos_y) {
            volatile char *old_ptr = (char*)0xB8000 + (old_y * 80 + old_x) * 2;
            old_ptr[0] = ' '; // Eski yeri temizle
            
            old_x = cursor_pos_x;
            old_y = cursor_pos_y;
        }

        // 2. Yeni konuma imleci (X karakteri) çiz
        volatile char *new_ptr = (char*)0xB8000 + (cursor_pos_y * 80 + cursor_pos_x) * 2;
        new_ptr[0] = 'X';    // İmleç karakteri
        new_ptr[1] = 0x0E;   // Sarı renk

        sleep_ms(20); // Çok hızlı dönüp CPU'yu yormasın
    }
}



/* kernel.c - Double Buffering Hazırlığı */

// 80x25 ekran için 4000 byte'lık bir arka tampon ayırıyoruz (80*25*2)
unsigned char back_buffer[80 * 25 * 2];

void finalize_render() {
    // RAM'deki tamponu tek seferde video belleğine kopyala
    for (int i = 0; i < 80 * 25 * 2; i++) {
        video_memory[i] = back_buffer[i];
    }
}

// Artık clear_screen, draw_window gibi fonksiyonlar video_memory yerine 
// back_buffer üzerine yazmalı.
void clear_back_buffer() {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        back_buffer[i] = ' ';
        back_buffer[i+1] = 0x07;
    }
}

/* kernel.c - Masaüstü Görselleştirmesi */

/* kernel.c - Masaüstü Görselleştirme Fonksiyonu */
void draw_desktop() {
    // Tüm ekranı dolaş (80x25 karakter modu)
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT * 2; i += 2) {
        // Eğer hücre boşsa veya üzerinde bir karakter yoksa arka planı boya
        // 0x1F: Mavi Arka Plan (1), Beyaz Yazı (F)
        video_memory[i + 1] = 0x1F; 
    }

    // Basit bir görev çubuğu çiz (En alt satır)
    for (int x = 0; x < 80; x++) {
        int index = (24 * 80 + x) * 2;
        video_memory[index] = ' ';
        video_memory[index + 1] = 0x70; // Gri arka plan
    }
}

/* kernel.c - RAM üzerine arka plan çizimi */
void draw_desktop_buffered() {
    // Tüm ekranı dolaş (80x25 karakter modu)
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        // back_buffer üzerine mavi arka plan (0x1) ve beyaz yazı (0xF) bas
        back_buffer[i] = ' ';
        back_buffer[i + 1] = 0x1F; 
    }

    // Görev çubuğu (En alt satır)
    for (int x = 0; x < 80; x++) {
        int index = (24 * 80 + x) * 2;
        back_buffer[index] = ' ';
        back_buffer[index + 1] = 0x70; // Gri arka plan
    }
}


/* 3. Yardımcı Fonksiyonlar (gorev_masaustu'ndan önce gelmeli) */
void draw_window_buffered(window_t w) {
    for (int i = 0; i < w.h; i++) {
        for (int j = 0; j < w.w; j++) {
            int index = ((w.y + i) * 80 + (w.x + j)) * 2;
            if (index < 0 || index >= 80 * 25 * 2) continue;

            if (i == 0) { // Başlık Çubuğu
                back_buffer[index] = ' ';
                // Kapatma butonu alanı (Sağ üst köşe)
                if (j == w.w - 2) {
                    back_buffer[index] = 'X';
                    back_buffer[index + 1] = 0x4F; // Kırmızı arka plan, Beyaz X
                } else {
                    back_buffer[index + 1] = 0x70; // Gri başlık
                }
            } else { // Pencere Gövdesi
                back_buffer[index] = ' ';
                back_buffer[index + 1] = 0x1E; // Lacivert gövde
            }
        }
    }
}


void update_desktop_logic() {
    extern volatile int mouse_left_button;
    extern volatile int cursor_pos_x;
    extern volatile int cursor_pos_y;
    
    // Sürükleme sırasında farenin pencere içindeki konumunu korumak için
    static int offset_x = 0;
    static int last_button_state = 0;

    if (!win1.is_visible) return; // Eğer pencere gizliyse hiçbir işlem yapma

    // 1. TIKLAMA ANINI YAKALA (Edge Trigger)
    if (mouse_left_button && !last_button_state) {
        // Kapatma butonu (X) kontrolü - Sürüklemeden önce kontrol edilmeli
        if (cursor_pos_x == (win1.x + win1.w - 2) && cursor_pos_y == win1.y) {
            win1.x = -100; // Pencereyi "yok et"
            win1.y = -100;
            return; // Kapatılan pencere sürüklenemez
        }

        // Başlık çubuğuna tıklandı mı?
        if (cursor_pos_x >= win1.x && cursor_pos_x < (win1.x + win1.w) &&
            cursor_pos_y == win1.y) {
            win1.is_dragging = 1;
            offset_x = cursor_pos_x - win1.x; // Tıklanan noktanın pencere soluna uzaklığı
        }
    }    


    // 2. SÜRÜKLEME MANTIĞI
    if (mouse_left_button && win1.is_dragging) {
        win1.x = cursor_pos_x - offset_x; // Zıplama yapmadan sürükle
        win1.y = cursor_pos_y;

        // EKRAN SINIRLARI (80x25 moduna göre)
        if (win1.x < 0) win1.x = 0;
        if (win1.x + win1.w > 80) win1.x = 80 - win1.w;
        
        if (win1.y < 0) win1.y = 0;
        if (win1.y + win1.h > 24) win1.y = 24 - win1.h; // Görev çubuğu sınırı
    } else {
        win1.is_dragging = 0;
    }

    last_button_state = mouse_left_button;
}

void draw_mouse_buffered() {
    extern volatile int cursor_pos_x;
    extern volatile int cursor_pos_y;
    int mouse_index = (cursor_pos_y * 80 + cursor_pos_x) * 2;
    back_buffer[mouse_index] = 'X';     // Fare karakteri
    back_buffer[mouse_index + 1] = 0x0E; // Sarı renk
}


/* 4. Ana Çizim Görevi */
void gorev_masaustu() {
    
    while(1) {
        // --- 1. GİRİŞ KONTROLÜ (Input Handling) ---
        if (last_pressed_key == 'o') { 
            win1.is_visible = 1; 
            win1.x = 10; 
            win1.y = 5;
            last_pressed_key = 0; // Tuşu işledikten sonra sıfırla ki sürekli tetiklenmesin
        }

        // --- 2. MANTIK GÜNCELLEME (Physics/Logic) ---
        update_desktop_logic(); 

        // --- 3. ÇİZİM (Rendering - Katman Katman) ---
        clear_back_buffer();      // 1. Katman: Temizlik
        draw_desktop_buffered();  // 2. Katman: Duvar Kağıdı

        if (win1.is_visible) {
            draw_window_buffered(win1); // 3. Katman: Pencere (Arka planın üstünde)
        }

        draw_mouse_buffered();    // 4. Katman: Fare (Her şeyin üstünde)

        // --- 4. EKRANA YANSITMA ---
        finalize_render(); 

        sleep_ms(16); // ~60 FPS
    }
}

void kernel_main(unsigned int magic, struct multiboot_info* mbi) {   
    init_gdt(); // 1. İşlemci segmentlerini ayarla
    init_idt();// 2. Kesme masasını kur    
    init_pmm(mbi); // 3. Fiziksel bellek haritasını çıkar (Multiboot'tan gelen bilgiyle)       
    init_paging(); // <-- Paging sistemini burada aktif ediyoruz

    // --- KMALLOC ŞİMDİ HAZIR ---
    
    // Örnek test kullanımı:
    int* test_dizi = (int*) kmalloc(sizeof(int) * 10);
    test_dizi[0] = 58; 
    
    print("Dinamik bellek ayrildi.\n", 0x0A);


    init_mouse(); // Fare desteği ekleyelim
    num_tasks = 0; // İndeksleri sıfırla
    

    // Ana döngüyü Task 2 olarak ekle (Zaten çalışan bu olduğu için yığınını kaydedecek yer lazım)
    // 1. ANA DÖNGÜYÜ (TASK 0) EN BAŞTA KAYDET
    pcb_t* main_pcb = (pcb_t*) kmalloc(sizeof(pcb_t));
    main_pcb->state = 1;
    task_list[num_tasks++] = main_pcb; 
    current_task = 0; // İlk görev ana döngü olsun


    // YENİ GÖREV: Masaüstü ve Pencere Yöneticisi
    create_task(gorev_masaustu);

    // Task 1: Hareketli Karakter (Gorev1 yerine bunu kullan)
    create_task(gorev_cizim);
   // create_task(gorev1); // Task 0
    create_task(gorev2); // Task 1


    init_timer(100); // 100 Hz (Saniyede 100 kez kesme)
    clear_screen(); // Önce BIOS'tan kalan yazıları temizle

    print("Sistem Hazir. Multitasking aktif.\n", 0x0A);

    // Şimdi istediğimiz her şeyi yazdırabiliriz!
    print("Isletim Sistemi Cekirdegi Baslatildi...", 0x0A); // 0x0A: Parlak Yeşil
    
    cursor_y = 2; // İki alt satıra geç
    cursor_x = 0;

    if (magic != 0x2BADB002) {
        print("Hata: Gecersiz Multiboot Sihirli Sayisi!", 0x0C);
        return;
    }

    print("Bellek Haritasi (Memory Map) Analizi:\n", 0x0B);

    // Multiboot yapısından mmap adresini alıyoruz
    struct multiboot_mmap_entry* mmap = (struct multiboot_mmap_entry*)mbi->mmap_addr;
    
    while((unsigned int)mmap < mbi->mmap_addr + mbi->mmap_length) {
        if (mmap->type == 1) { // Sadece kullanılabilir RAM'i yazdır
            print("Uygun RAM: ", 0x0A);
            print("Adres: ", 0x07);
            // Burada basitlik için sadece düşük adresleri yazdırıyoruz
            // Gerçek bir sistemde 64-bit adresleme için hex_print yazılmalıdır
            print("Baslangic: ", 0x07);
            // Örn: mmap->base_addr_low ve mmap->length_low değerlerini kontrol et
            print_hex(mmap->base_addr_low);
            print(" Uzunluk: ", 0x07);
            print_hex(mmap->length_low);
            print("\n", 0x07);
        }
        // Bir sonraki girişe atla
        mmap = (struct multiboot_mmap_entry*)((unsigned int)mmap + mmap->size + sizeof(mmap->size));
    }
   
    
    

     //  masaüstü görevini başlattık
    //gorev_masaustu();

    asm volatile("sti"); // Kesmeleri aç

    while(1){
        // Ekranın sağ üst köşesinde sürekli artan bir sayaç gösterelim
        // 0xB8000 + (satır_sayısı * sütun_sayısı * 2) - biraz_pay
        // Sağ üst köşe adresi yaklaşık 0xB8000 + 150
        
        volatile char *timer_display = (char*) 0xB8000 + 150;
        int seconds = timer_ticks / 100;
        
        // Basit bir saniye gösterimi (Sadece birler basamağı örneği)
        timer_display[0] = (seconds % 10) + '0';
        timer_display[1] = 0x4F; // Kırmızı arka plan, beyaz yazı

        asm volatile("hlt"); // İşlemciyi bir sonraki kesmeye kadar uyut (Verimlilik)
    }
    
    int* dinamik_dizi = (int*) kmalloc(sizeof(int) * 5);
    dinamik_dizi[0] = 1234;
    
    print("Dinamik veri adresi: ", 0x07);
    print_hex((unsigned int)dinamik_dizi); // Adresi gör
    print("\n", 0x07);
}
