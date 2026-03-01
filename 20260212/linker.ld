/* linker.ld */
ENTRY(_start)                /* Giriş noktamız boot.s içindeki _start etiketi */

SECTIONS
{
    /* Çekirdeğimizi 1MB (0x100000) adresine yerleştiriyoruz. 
       Bu, x86 dünyasında çekirdekler için standart başlangıç noktasıdır. */
    . = 1M;

    /* Önce Multiboot başlığı gelmeli, yoksa önyükleyici bizi tanımaz! */
    .text :
    {
        *(.multiboot)
        *(.text)
    }

    /* Okunabilir veriler (sabitler vb.) */
    .rodata :
    {
        *(.rodata)
    }

    /* Başlatılmış veriler */
    .data :
    {
        *(.data)
    }

    /* Başlatılmamış veriler ve yığın alanı */
    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
