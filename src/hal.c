/* * src/hal.c - OBA-32 Donanım Soyutlama Katmanı Fonksiyon Gerçekleşimi
 * Sadece inline olamayacak büyüklükteki donanımsal sistem prosedürlerini barındırır.
 */

#include "hal.h"
#include <stdint.h>

void sys_shutdown(void) {
    /* QEMU/ACPI donanımsal kapatma port sinyallerinin gönderilmesi */
    outw(0xB004, 0x2000); /* Eski QEMU sürümleri için ACPI kapatma yönergesi */
    outw(0x604, 0x2000);  /* Modern QEMU/ICH9 standartları için ACPI kapatma yönergesi */
    
    /* Donanım kapatma başarısız olursa işlemciyi güvenli askı moduna al */
    while(1) {
        asm volatile("cli; hlt");
    }
}