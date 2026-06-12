/* * include/ata.h - OBA-32 ATA PIO Sabit Disk Sürücüsü Başlık Dosyası
 * Sektörel okuma/yazma arabirimlerini ve durum bayraklarını tanımlar.
 */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE       512

/* ATA Primary Bus G/Ç Port Adresleri */
#define ATA_DATA_PORT         0x1F0
#define ATA_ERROR_PORT        0x1F1
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA_LOW_PORT      0x1F3
#define ATA_LBA_MID_PORT      0x1F4
#define ATA_LBA_HIGH_PORT     0x1F5
#define ATA_DRIVE_SELECT_PORT 0x1F6
#define ATA_COMMAND_PORT      0x1F7
#define ATA_STATUS_PORT       0x1F7

/* ATA Komut Seti */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

/* Durum Yazmacı (Status Register) Maskeleri */
#define ATA_STATUS_ERR        0x01
#define ATA_STATUS_DRQ        0x08
#define ATA_STATUS_SRV        0x10
#define ATA_STATUS_DF         0x20
#define ATA_STATUS_RDY        0x40
#define ATA_STATUS_BSY        0x80

/* Sürücü Fonksiyon Prototipleri */
uint8_t ata_identify(uint16_t* target_buffer);
uint8_t ata_read_sector(uint32_t lba, uint16_t* target_buffer);
uint8_t ata_write_sector(uint32_t lba, const uint16_t* source_buffer);

#endif