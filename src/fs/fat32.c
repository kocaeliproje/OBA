/* * src/fs/fat32.c - OBA-32 FAT32 Dosya Sistemi Yönetim Sürücüsü
 * Küme zincirleme, dosya arama ve sektörel okuma motoru gerçeklenmiştir.
 */

#include "fat32.h"
#include "ata.h"
#include <stdint.h>

extern void terminal_write_line(const char* text, uint32_t color);

static FAT32_BPB_t bpb32_instance;
static uint32_t fat_start_sector = 0;
static uint32_t data_start_sector = 0;

uint8_t init_fat32(void) {
    uint16_t sector_buffer[256];

    if (!ata_read_sector(0, sector_buffer)) {
        terminal_write_line("[FAT32] Critical Error: Unable to read Sector 0", 0xD63031);
        return 0;
    }

    uint8_t* src = (uint8_t*)sector_buffer;
    uint8_t* dest = (uint8_t*)&bpb32_instance;
    for (uint32_t i = 0; i < sizeof(FAT32_BPB_t); i++) {
        dest[i] = src[i];
    }

    if (bpb32_instance.file_system_type[0] != 'F' || bpb32_instance.file_system_type[1] != 'A') {
        terminal_write_line("[FAT32] Mismatch: Volume is not formatted as FAT32", 0xD63031);
        return 0;
    }

    fat_start_sector = bpb32_instance.reserved_sector_count;
    data_start_sector = fat_start_sector + (bpb32_instance.num_fats * bpb32_instance.sectors_per_fat_32);

    terminal_write_line("[FAT32] Storage Volume Initialization Completed.", 0x27AE60);
    return 1;
}

/* Mantıksal küme numarasını fiziksel ATA LBA sektör adresine dönüştürür */
static uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    return data_start_sector + ((cluster - 2) * bpb32_instance.sectors_per_cluster);
}

/* File Allocation Table (FAT) üzerinden bir sonraki küme indeksini okur */
uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    uint16_t sector_buffer[256];
    
    /* İlgili kümenin hangi FAT sektöründe ve kaçıncı ofsette olduğunun tespiti */
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / bpb32_instance.bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb32_instance.bytes_per_sector;

    if (!ata_read_sector(fat_sector, sector_buffer)) {
        return FAT32_EOF;
    }

    uint32_t* table = (uint32_t*)sector_buffer;
    uint32_t next_cluster = table[ent_offset / 4] & 0x0FFFFFFF;

    return (next_cluster >= 0x0FFFFFF8) ? FAT32_EOF : next_cluster;
}

/* Belirtilen kümenin tüm sektörlerini hedef belleğe transfer eder */
uint8_t fat32_read_cluster(uint32_t cluster, uint8_t* buffer) {
    uint32_t lba = fat32_cluster_to_lba(cluster);
    uint16_t* sector_ptr = (uint16_t*)buffer;

    for (uint8_t i = 0; i < bpb32_instance.sectors_per_cluster; i++) {
        if (!ata_read_sector(lba + i, sector_ptr)) {
            return 0;
        }
        sector_ptr += 256; /* 512 bayt ilerleme (256 Word) */
    }
    return 1;
}

/* Kök dizin içerisinde 11 karakterlik (8.3) FAT standardına göre dosya arar */
uint32_t fat32_find_file(const char* name) {
    uint8_t cluster_buffer[4096]; 
    uint32_t current_cluster = bpb32_instance.root_cluster;

    while (current_cluster != FAT32_EOF) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            return 0;
        }

        FAT32_DirEntry_t* entries = (FAT32_DirEntry_t*)cluster_buffer;
        uint32_t entries_per_cluster = (bpb32_instance.sectors_per_cluster * bpb32_instance.bytes_per_sector) / sizeof(FAT32_DirEntry_t);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].filename[0] == 0x00) return 0; 
            if (entries[i].filename[0] == 0xE5) continue; /* Veri tipi uint8_t olduğu için güvenle çalışır */
            if (entries[i].attributes == 0x0F)  continue; 

            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (name[k] == '\0' || entries[i].filename[k] != (uint8_t)name[k]) {
                    if (!(name[k] == '\0' && entries[i].filename[k] == ' ')) {
                        match = 0; break;
                    }
                }
            }

            if (match) {
                return ((uint32_t)entries[i].first_cluster_high << 16) | entries[i].first_cluster_low;
            }
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return 0;
}

/* Dosya adına göre tüm küme zincirini ardışık olarak okur ve hedef arabelleğe yazar */
uint8_t fat32_read_file(const char* name, uint8_t* target_buffer) {
    uint32_t cluster = fat32_find_file(name);
    if (cluster == 0) return 0; /* Dosya bulunamadı */

    uint8_t* write_ptr = target_buffer;
    while (cluster != FAT32_EOF) {
        if (!fat32_read_cluster(cluster, write_ptr)) {
            return 0;
        }
        write_ptr += (bpb32_instance.sectors_per_cluster * bpb32_instance.bytes_per_sector);
        cluster = fat32_get_next_cluster(cluster);
    }
    return 1;
}

/* Harici Sanal Dosya Sistemi düğüm üretim fonksiyonu */
extern void vfs_create_file(const char* name, const char* content, uint32_t size);

void fat32_mount_to_vfs(void) {
    uint8_t cluster_buffer[4096];
    uint32_t current_cluster = bpb32_instance.root_cluster;

    if (current_cluster == FAT32_EOF) return;

    /* Kök dizinin ilk kümesini tampon belleğe yükle */
    if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
        return;
    }

    FAT32_DirEntry_t* entries = (FAT32_DirEntry_t*)cluster_buffer;
    uint32_t entries_per_cluster = (bpb32_instance.sectors_per_cluster * bpb32_instance.bytes_per_sector) / sizeof(FAT32_DirEntry_t);

    for (uint32_t i = 0; i < entries_per_cluster; i++) {
        if (entries[i].filename[0] == 0x00) break;    /* Dizin sonu */
        if (entries[i].filename[0] == 0xE5) continue; /* Silinmiş kayıt */
        if (entries[i].attributes == 0x0F)  continue; /* Uzun dosya adı maskesi */
        
        /* Klasör yapıları şimdilik es geçilir, öncelik dosyalardadır */
        if (entries[i].attributes & FAT32_ATTR_DIRECTORY) continue;

        /* FAT 8.3 isimlendirmesini VFS uyumlu temiz karakter dizisine dönüştürme */
        static char clean_name[13];
        int p = 0;
        for (int k = 0; k < 8; k++) {
            if (entries[i].filename[k] != ' ' && entries[i].filename[k] != 0) {
                clean_name[p++] = (char)entries[i].filename[k];
            }
        }
        
        /* Uzantı ekleme lojistiği */
        if (entries[i].extension[0] != ' ' && entries[i].extension[0] != 0) {
            clean_name[p++] = '.';
            for (int k = 0; k < 3; k++) {
                if (entries[i].extension[k] != ' ' && entries[i].extension[k] != 0) {
                    clean_name[p++] = (char)entries[i].extension[k];
                }
            }
        }
        clean_name[p] = '\0';

        /* Dosya içeriğini diskten dinamik okumak üzere bellek alanı tahsisi */
        /* Çekirdek kmalloc sınırları dahilinde güvenli statik tampon ayrılmıştır */
        static uint8_t content_load_buffer[2048];
        
        /* Dosya adının 8 karakterlik ham halini arama motoruna gönder */
        char search_name[9];
        for(int m=0; m<8; m++) search_name[m] = (char)entries[i].filename[m];
        search_name[8] = '\0';

        if (fat32_read_file(search_name, content_load_buffer)) {
            content_load_buffer[entries[i].filesize < 2047 ? entries[i].filesize : 2047] = '\0';
            /* VFS tablosuna canlı kaydın yapılması */
            vfs_create_file(clean_name, (const char*)content_load_buffer, entries[i].filesize);
        }
    }
    terminal_write_line("[VFS] FAT32 Root Directory Synchronized into UI.", 0x00FF00);
}

/* FAT tablosunda boş bir küme arar ve bulduğunda onu rezerve eder */
uint32_t fat32_allocate_cluster(void) {
    uint32_t total_fat_sectors = bpb32_instance.sectors_per_fat_32;
    uint32_t fat_entries_per_sector = bpb32_instance.bytes_per_sector / 4;
    uint16_t sector_buffer[256];

    /* FAT tablosunun tamamını tara (Sektör 2'den başla) */
    for (uint32_t i = 0; i < total_fat_sectors; i++) {
        if (!ata_read_sector(fat_start_sector + i, sector_buffer)) {
            return 0; /* Okuma hatası */
        }

        uint32_t* table = (uint32_t*)sector_buffer;
        for (uint32_t j = 0; j < fat_entries_per_sector; j++) {
            /* 0x00000000 değeri boş küme demektir */
            if (table[j] == 0x00000000) {
                uint32_t found_cluster = (i * fat_entries_per_sector) + j;
                
                /* Kümeyi rezerve et (FAT32_EOF ile işaretle) */
                table[j] = FAT32_EOF; 
                
                /* Güncellenmiş FAT sektörünü diske geri mühürle */
                ata_write_sector(fat_start_sector + i, sector_buffer);
                
                return found_cluster;
            }
        }
    }
    return 0; /* Disk dolu */
}

uint8_t fat32_write_cluster(uint32_t cluster, const uint8_t* buffer) {
    uint32_t lba = fat32_cluster_to_lba(cluster);
    const uint16_t* sector_ptr = (const uint16_t*)buffer;

    for (uint8_t i = 0; i < bpb32_instance.sectors_per_cluster; i++) {
        /* ATA PIO sürücüsü kullanılarak 512 baytlık (256 Word) veri yazımı */
        if (!ata_write_sector(lba + i, sector_ptr)) {
            return 0; /* Yazma hatası */
        }
        sector_ptr += 256; 
    }
    return 1; /* İşlem başarıyla mühürlendi */
}

/* Yeni bir dosya için dizin tablosunda giriş oluşturur */
uint8_t fat32_create_dir_entry(const char* name, const char* ext, uint32_t first_cluster, uint32_t filesize) {
    uint8_t cluster_buffer[4096];
    uint32_t current_cluster = bpb32_instance.root_cluster;

    while (current_cluster != FAT32_EOF) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) return 0;

        FAT32_DirEntry_t* entries = (FAT32_DirEntry_t*)cluster_buffer;
        uint32_t entries_per_cluster = (bpb32_instance.sectors_per_cluster * bpb32_instance.bytes_per_sector) / sizeof(FAT32_DirEntry_t);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            /* Boş veya silinmiş girişi bul (0x00 veya 0xE5) */
            if (entries[i].filename[0] == 0x00 || entries[i].filename[0] == 0xE5) {
                /* Girişi doldur */
                for(int k=0; k<8; k++) entries[i].filename[k] = (k < 8 && name[k] != '\0') ? (uint8_t)name[k] : ' ';
                for(int k=0; k<3; k++) entries[i].extension[k] = (k < 3 && ext[k] != '\0') ? (uint8_t)ext[k] : ' ';
                
                entries[i].attributes = FAT32_ATTR_ARCHIVE;
                entries[i].first_cluster_low = (uint16_t)(first_cluster & 0xFFFF);
                entries[i].first_cluster_high = (uint16_t)((first_cluster >> 16) & 0xFFFF);
                entries[i].filesize = filesize;

                /* Güncellenmiş dizin kümesini diske geri mühürle */
                return fat32_write_cluster(current_cluster, cluster_buffer);
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return 0; /* Dizin alanı dolu */
}

/* Veriyi küme zinciri boyunca fiziksel diske yazar */
uint8_t fat32_write_file(const char* name, const uint8_t* buffer, uint32_t size) {
    uint32_t cluster = fat32_find_file(name);
    if (cluster == 0) return 0; /* Dosya bulunamadı */

    uint32_t bytes_per_cluster = bpb32_instance.sectors_per_cluster * bpb32_instance.bytes_per_sector;
    const uint8_t* read_ptr = buffer;
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        /* Mevcut kümeye veriyi yaz */
        if (!fat32_write_cluster(cluster, read_ptr)) return 0;
        
        bytes_written += bytes_per_cluster;
        read_ptr += bytes_per_cluster;

        /* Eğer daha yazılacak veri varsa, yeni küme tahsis et */
        if (bytes_written < size) {
            uint32_t next_cluster = fat32_allocate_cluster();
            if (next_cluster == 0) return 0; /* Disk doldu */
            
            /* FAT tablosunda zinciri bağla */
            uint32_t fat_sector = fat_start_sector + ((cluster * 4) / bpb32_instance.bytes_per_sector);
            uint32_t fat_offset = (cluster * 4) % bpb32_instance.bytes_per_sector;
            uint16_t sector_buffer[256];
            ata_read_sector(fat_sector, sector_buffer);
            ((uint32_t*)sector_buffer)[fat_offset / 4] = next_cluster;
            ata_write_sector(fat_sector, sector_buffer);
            
            cluster = next_cluster;
        }
    }
    return 1;
}