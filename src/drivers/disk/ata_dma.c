#include "ata.h"
#include "hal.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

#define DMA_CMD_START       0x01
#define DMA_CMD_READ        0x08

/* Physical Region Descriptor (PRD) Yapısı */
typedef struct PRD_Entry {
    uint32_t physical_buffer_address;
    uint16_t byte_count;
    uint16_t reserved : 15;
    uint16_t eot : 1; // End of Table (Zincirin son halkası)
} __attribute__((packed)) prd_entry_t;

/* 4KB hizalı PRD Tablosu (Sayfalama sınırlarına dikkat edilmelidir) */
static prd_entry_t prdt[1] __attribute__((aligned(4096)));
static uint32_t bus_master_ide_bar4 = 0;

/**
 * PCI Konfigürasyon Uzayından 32-bit Veri Okur
 */
static uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((uint32_t)1 << 31) | 
                       ((uint32_t)bus << 16) | 
                       ((uint32_t)slot << 11) | 
                       ((uint32_t)func << 8) | 
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/**
 * PCI Bus üzerinde IDE Denetleyicisini ve BAR4 (DMA Taban Adresi) Kayıtçısını Bulur
 */
void ata_dma_discover(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_config_read_dword(bus, slot, 0, 0x00);
            if ((reg0 & 0xFFFF) == 0xFFFF) continue; // Cihaz yok

            uint32_t class_reg = pci_config_read_dword(bus, slot, 0, 0x08);
            uint8_t base_class = (class_reg >> 24) & 0xFF;
            uint8_t sub_class  = (class_reg >> 16) & 0xFF;

            // Mass Storage Controller (0x01) ve IDE Controller (0x01) kontrolü
            if (base_class == 0x01 && sub_class == 0x01) {
                // BAR4 (Offset 0x20) Bus Master IDE taban adresini barındırır
                uint32_t bar4 = pci_config_read_dword(bus, slot, 0, 0x20);
                if (bar4 & 0x01) { // I/O Space kontrolü
                    bus_master_ide_bar4 = bar4 & 0xFFFC;
                    
                   // PCI Command Register'dan Bus Mastering (Bit 2) özelliğini aktifleştir
                    uint32_t pci_cmd = pci_config_read_dword(bus, slot, 0, 0x04);
                    pci_cmd |= (1 << 2); // Bus Master bitini 1 yap (0x04)
                    // Donanıma geri mühürle (Offset 0x04, register'ın alt 16 bitidir, outl ile yazıyoruz)
                    uint32_t pci_address = (uint32_t)((uint32_t)1 << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | (0x04 & 0xFC);
                    outl(0xCF8, pci_address);
                    outl(0xCFC, pci_cmd);

                    extern void terminal_write_line(const char* text, uint32_t color);
                    terminal_write_line("[DMA] Bus Master IDE Controller Discovered on PCI.", 0x00ADB5);
                    return;
                }
            }
        }
    }
}

/**
 * DMA Sektör Okuma Motoru
 */
uint8_t ata_dma_read(uint32_t lba, uint8_t sector_count, void* target_buffer) {
    if (bus_master_ide_bar4 == 0) return 0; // DMA denetleyicisi bulunamadıysa PIO'ya düş

    uint32_t byte_count = sector_count * 512;

    // 1. PRD Tablosunu doldur (Hedef RAM adresini ve boyutunu mühürle)
    prdt[0].physical_buffer_address = (uint32_t)target_buffer;
    prdt[0].byte_count = byte_count;
    prdt[0].reserved = 0;
    prdt[0].eot = 1; // Tek bir prd girdisiyle transferi bitiriyoruz

    // 2. PRDT fiziksel adresini Bus Master Primary PRD Register'a (BAR4 + 0x04) yaz
    outl(bus_master_ide_bar4 + 0x04, (uint32_t)&prdt);

    // 3. IDE Komut kanalını temizle ve Yönü Ayarla (Bit 3: Read = 1, Write = 0)
    outb(bus_master_ide_bar4 + 0x00, DMA_CMD_READ);

    // 4. Klasik ATA Sektör Seçim Protokolünü Hazırla (LBA28)
    outb(0x1F2, sector_count);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));

    // 5. ATA DMA Okuma Komutunu Ateşle (0xC8 -> DMA READ)
    outb(0x1F7, 0xC8);

    // 6. SİHİRLİ DÜĞME: Bus Master DMA Motorunu Başlat (Bit 0 -> Start)
    outb(bus_master_ide_bar4 + 0x00, DMA_CMD_READ | DMA_CMD_START);

    // 7. Donanımsal durum kontrolü: Transfer bitene kadar CPU hafif döngüde bekler
    // Not: Gerçek preemptive sistemde burası IRQ14 kesmesini hlt modunda beklemelidir.
    while (1) {
        uint8_t status = inb(bus_master_ide_bar4 + 0x02); // Status Register
        uint8_t ata_status = inb(0x1F7);
        
        if (!(ata_status & 0x80)) { // BSY temizlendiyse
            if (status & 0x04) { // Interrupt biti donanım tarafından 1 yapıldıysa
                break;
            }
        }
        asm volatile("nop");
    }

    // 8. DMA Motorunu Durdur ve Durum Bayraklarını Temizle
    outb(bus_master_ide_bar4 + 0x00, 0x00);
    outb(bus_master_ide_bar4 + 0x02, 0x04); // Kesme bitini sıfırla

    return 1;
}