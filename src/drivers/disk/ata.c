/* * src/drivers/disk/ata.c - OBA-32 ATA PIO Sabit Disk Sürücü Motoru
 * Donanım portları üzerinden 28-bit LBA modunda sektörel transferleri yönetir.
 */

#include "ata.h"
#include "hal.h"
#include <stdint.h>

/* Denetleyicinin meşguliyet (Busy) durumunun kalkmasını bekler */
static void ata_wait_busy(void) {
    while (inb(ATA_STATUS_PORT) & ATA_STATUS_BSY);
}

/* Denetleyicinin veri transferine hazır olma (Data Request) durumunu bekler */
static void ata_wait_drq(void) {
    while (!(inb(ATA_STATUS_PORT) & ATA_STATUS_DRQ));
}

/* Donanımsal IDENTIFY komutu ile bağlı bulunan diskin temel özelliklerini doğrular */
uint8_t ata_identify(uint16_t* target_buffer) {
    outb(ATA_DRIVE_SELECT_PORT, 0xA0); /* Master Sürücü Seçimi */
    outb(ATA_SECTOR_COUNT_PORT, 0);
    outb(ATA_LBA_LOW_PORT, 0);
    outb(ATA_LBA_MID_PORT, 0);
    outb(ATA_LBA_HIGH_PORT, 0);
    
    outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS_PORT);
    if (status == 0) return 0; 

    ata_wait_busy();

    status = inb(ATA_STATUS_PORT);
    if (status & ATA_STATUS_ERR) return 0;

    ata_wait_drq();

    /* 512 baytlık aygıt bilgi bloğunun bellek alanına transferi */
    insw(ATA_DATA_PORT, target_buffer, 256);
    return 1;
}

/* LBA28 Protokolü uyarınca belirtilen sektör adresinden 512 bayt veri okur */
uint8_t ata_read_sector(uint32_t lba, uint16_t* target_buffer) {
    ata_wait_busy();

    outb(ATA_DRIVE_SELECT_PORT, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_SECTOR_COUNT_PORT, 1);
    
    outb(ATA_LBA_LOW_PORT,  (uint8_t)(lba));
    outb(ATA_LBA_MID_PORT,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    
    outb(ATA_COMMAND_PORT, ATA_CMD_READ_SECTORS);

    ata_wait_busy();
    
    uint8_t status = inb(ATA_STATUS_PORT);
    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return 0; 

    ata_wait_drq();

    /* Verinin G/Ç portundan hedef arabelleğe kopyalanması */
    insw(ATA_DATA_PORT, target_buffer, 256);
    return 1;
}

/* LBA28 Protokolü uyarınca belirtilen sektör adresine 512 bayt veri mühürler */
uint8_t ata_write_sector(uint32_t lba, const uint16_t* source_buffer) {
    ata_wait_busy();

    outb(ATA_DRIVE_SELECT_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, 1);
    
    outb(ATA_LBA_LOW_PORT,  (uint8_t)(lba));
    outb(ATA_LBA_MID_PORT,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    
    outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_SECTORS);

    ata_wait_busy();
    ata_wait_drq();

    /* 512 baytlık kaynak verinin G/Ç portu üzerinden diske iletilmesi */
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA_PORT, source_buffer[i]);
    }

    ata_wait_busy();
    
    uint8_t status = inb(ATA_STATUS_PORT);
    if (status & (ATA_STATUS_ERR | ATA_STATUS_DF)) return 0;

    return 1;
}