#include "kernel.h"

/* Linker script (linker.ld) içinde tanımladığımız sembol. 
   Kernel'ın bittiği bellek adresini gösterir. */
extern unsigned int _kernel_end;

/* Bellek tahsisatının başlayacağı adres. */
unsigned int placement_address = (unsigned int)&_kernel_end;

unsigned int *memory_bitmap = 0;
unsigned int total_frames = 0;

#define FRAME_SIZE 4096 // 4KB'lık sayfalar
#define HEAP_MAGIC 0x19283746

/* --- DİNAMİK HEAP MİMARİSİ VERİ YAPILARI --- */
typedef struct heap_chunk {
    unsigned int magic;
    unsigned int size;
    unsigned char is_free;
    struct heap_chunk* next;
} __attribute__((packed)) heap_chunk_t;

static heap_chunk_t* heap_start_node = 0;
static unsigned char is_heap_initialized = 0;

/**
 * Fiziksel Bellek Yönetimini Başlatır (PMM)
 */
void init_pmm(struct multiboot_info* mbi) {
    // Multiboot'tan gelen bellek miktarını alıyoruz (KB cinsinden gelir)
    unsigned int mem_kb = mbi->mem_upper + mbi->mem_lower;
    
    // Toplam 4KB'lık blok (frame) sayısı
    total_frames = mem_kb / 4;

    if (placement_address % 4) {
        placement_address = (placement_address & 0xFFFFFFFC) + 4;
    }
}

/**
 * Canlı Çekirdek Heap Alanını İlklendirir
 * Sayfalama açıldıktan sonra kernel.c içinden çağrılmalıdır.
 */
void init_kernel_heap(unsigned int heap_base_addr, unsigned int heap_size) {
    heap_start_node = (heap_chunk_t*)heap_base_addr;
    heap_start_node->magic = HEAP_MAGIC;
    heap_start_node->size = heap_size - sizeof(heap_chunk_t);
    heap_start_node->is_free = 1;
    heap_start_node->next = 0;
    
    is_heap_initialized = 1;
}

/**
 * Gelişmiş Dinamik Bellek Ayırma Fonksiyonu
 */
void* kmalloc_aligned(unsigned int size, int align) {
    /* 1. Aşama: Eğer gelişmiş heap henüz kurulmadıysa (Boot anı), eski Placement sistemini kullan */
    if (!is_heap_initialized) {
    if (align && (placement_address % 4096)) {
        placement_address = (placement_address & 0xFFFFF000) + 4096;
    }
    // Ekleme: Her halükarda veriyi 4-byte sınırına hizala
    if (placement_address % 4) {
        placement_address = (placement_address & 0xFFFFFFFC) + 4;
    }
    unsigned int tmp = placement_address;
    placement_address += size;
    return (void*)tmp;
}

    /* 2. Aşama: Sayfalama sonrası dinamik Linked List sistemini kullan */
    heap_chunk_t* curr = heap_start_node;
    
    // 4-byte hizalama garantisi
    if (size % 4) size = (size & 0xFFFFFFFC) + 4;

    while (curr != 0) {
        if (curr->is_free && curr->size >= size) {
            // Bloğu bölmeye (split) değer mi? (Header + minimum veri alanı kontrolü)
            if (curr->size > size + sizeof(heap_chunk_t) + 16) {
                heap_chunk_t* new_chunk = (heap_chunk_t*)((unsigned int)curr + sizeof(heap_chunk_t) + size);
                new_chunk->magic = HEAP_MAGIC;
                new_chunk->size = curr->size - size - sizeof(heap_chunk_t);
                new_chunk->is_free = 1;
                new_chunk->next = curr->next;

                curr->size = size;
                curr->next = new_chunk;
            }
            curr->is_free = 0;
            return (void*)((unsigned int)curr + sizeof(heap_chunk_t));
        }
        curr = curr->next;
    }
    
    return 0; // Çekirdek Bellek Taşması (OOM)
}

void* kmalloc(unsigned int size) {
    return kmalloc_aligned(size, 0);
}

/**
 * Dinamik Çekirdek Bellek Serbest Bırakma Motoru
 * VFS düğümleri ve GUI pencereleri imha edilirken sızıntıları önler.
 */
void kfree(void* ptr) {
    if (!ptr || !is_heap_initialized) return;

    heap_chunk_t* chunk = (heap_chunk_t*)((unsigned int)ptr - sizeof(heap_chunk_t));
    
    if (chunk->magic != HEAP_MAGIC) {
        return; // Geçersiz veya bozuk bellek göstergesi emniyet kilidi
    }

    chunk->is_free = 1;

    /* Parçalanmayı Önleyici Birleştirme Motoru (Coalescing) */
    heap_chunk_t* curr = heap_start_node;
    while (curr != 0 && curr->next != 0) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(heap_chunk_t) + curr->next->size;
            curr->next = curr->next->next;
            // Birleşen bloktan dolayı döngü indeksini bozmamak için aynı düğümü tekrar kontrol et
            continue;
        }
        curr = curr->next;
    }
}

/* Bitmap fiziksel çerçeve yönetim fonksiyonları */
void set_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) memory_bitmap[idx] |= (1 << off);
}

void clear_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) memory_bitmap[idx] &= ~(1 << off);
}

unsigned int test_frame(unsigned int frame_addr) {
    unsigned int frame = frame_addr / FRAME_SIZE;
    unsigned int idx = frame / 32;
    unsigned int off = frame % 32;
    if (memory_bitmap) return (memory_bitmap[idx] & (1 << off));
    return 0;
}