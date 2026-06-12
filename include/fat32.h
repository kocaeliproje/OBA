/* * include/fat32.h - OBA-32 FAT32 Dosya Sistemi Yapı Tanımlamaları
 */

#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#define FAT32_EOF             0x0FFFFFF8
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20

/* FAT32 Extended BIOS Parameter Block (EBPB) Yapısı */
typedef struct {
    uint8_t  bootstrap_jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;      
    uint8_t  sectors_per_cluster;   
    uint16_t reserved_sector_count; 
    uint8_t  num_fats;              
    uint16_t root_entry_count;      
    uint16_t total_sectors_16;      
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;    
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;      

    uint32_t sectors_per_fat_32;    
    uint16_t drive_flags;
    uint16_t file_system_version;
    uint32_t root_cluster;          
    uint16_t fs_info_sector;        
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    
    uint8_t  drive_number;
    uint8_t  reserved_win_nt;
    uint8_t  boot_signature;        
    uint32_t volume_id;
    char     volume_label[11];
    char     file_system_type[8];   
} __attribute__((packed)) FAT32_BPB_t;

/* FAT32 Standart 32-Bayt Dizin Giriş Yapısı */
typedef struct {
    uint8_t  filename[8];     /* --- DÜZELTİLDİ: 'char' yerine 'uint8_t' olarak mühürlendi --- */
    uint8_t  extension[3];    /* --- DÜZELTİLDİ: 'char' yerine 'uint8_t' olarak mühürlendi --- */
    uint8_t  attributes;
    uint8_t  reserved_win_nt;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;    
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster_low;     
    uint32_t filesize;              
} __attribute__((packed)) FAT32_DirEntry_t;

uint8_t init_fat32(void);
uint32_t fat32_get_next_cluster(uint32_t current_cluster);
uint8_t fat32_read_cluster(uint32_t cluster, uint8_t* buffer);
uint32_t fat32_find_file(const char* name);
uint8_t fat32_read_file(const char* name, uint8_t* target_buffer);
uint32_t fat32_allocate_cluster(void); // Bu prototipin varlığını doğrulayın
uint8_t fat32_create_dir_entry(const char* name, const char* ext, uint32_t first_cluster, uint32_t filesize);
uint8_t fat32_write_file(const char* name, const uint8_t* buffer, uint32_t size);


#endif