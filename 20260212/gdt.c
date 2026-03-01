/* gdt.c */

/* gdt_flush fonksiyonunun dışarıda (assembly tarafında) olduğunu derleyiciye bildiriyoruz */
extern void gdt_flush(unsigned int);
// GDT Giriş Yapısı
struct gdt_entry_struct {
    unsigned short limit_low;           // Sınırın alt 16 biti
    unsigned short base_low;            // Başlangıç adresinin alt 16 biti
    unsigned char  base_middle;         // Başlangıç adresinin sonraki 8 biti
    unsigned char  access;              // Erişim hakları (Kod, Veri, Sistem vb.)
    unsigned char  granularity;
    unsigned char  base_high;           // Başlangıç adresinin son 8 biti
} __attribute__((packed));

// GDT İşaretçisi (İşlemciye bu tabloyu verirken kullanılan özel yapı)
struct gdt_ptr_struct {
    unsigned short limit;               // Tablonun boyutu
    unsigned int   base;                // Tablonun başlangıç adresi
} __attribute__((packed));

struct gdt_entry_struct gdt_entries[3];
struct gdt_ptr_struct   gdt_ptr;

// GDT'yi belleğe yazan fonksiyon
void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void init_gdt() {
    gdt_ptr.limit = (sizeof(struct gdt_entry_struct) * 3) - 1;
    gdt_ptr.base  = (unsigned int)&gdt_entries;

    // 1. Boş Giriş (GDT her zaman boş bir girişle başlamalıdır)
    gdt_set_gate(0, 0, 0, 0, 0);
    // 2. Kod Segmenti (Tüm belleği kapsar, çalışma izni var)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // 3. Veri Segmenti (Tüm belleği kapsar, okuma/yazma izni var)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // Assembly ile işlemciye bu tabloyu yükle (Bunu bir sonraki adımda yazacağız)
    gdt_flush((unsigned int)&gdt_ptr);
}
