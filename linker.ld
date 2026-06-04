/* linker.ld */
ENTRY(_start)

SECTIONS
{
    /* Çekirdek 1MB (0x100000) adresine yüklenir */
    . = 0x100000;

    .text : AT(ADDR(.text))
    {
        *(.multiboot)    /* ÖNEMLİ: İlk olarak multiboot başlığını sabitle */
        *(.text)
        *(.rodata*)
    }

    .data : AT(ADDR(.data))
    {
        *(.data)
    }

    .bss : AT(ADDR(.bss))
    {
        _bss_start = .;
        *(.bss)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;
}