; --- OBA-32 Multiboot Grafik Yapılandırması ---
MULTIBOOT_PAGE_ALIGN    equ 1 << 0
MULTIBOOT_MEMORY_INFO   equ 1 << 1
MULTIBOOT_VIDEO_MODE    equ 1 << 2  ; Ekran kartından grafik modu istiyoruz!

MULTIBOOT_FLAGS         equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE
MULTIBOOT_MAGIC         equ 0x1BADB002
MULTIBOOT_CHECKSUM      equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM
    
    ; Multiboot standardı gereği grafik modu için rezerve alanlar
    dd 0, 0, 0, 0, 0
    dd 0            ; 0 = Linear Framebuffer (LFB) modu aktif
    dd 800          ; Ekran Genişliği (Width)
    dd 600          ; Ekran Yükseklik (Height)
    dd 32           ; Renk Derinliği (32-bit RGBA)

section .text
global _start
extern kernel_main             

_start:
    cli                        ; Kesmeleri kapat
    mov esp, stack_top         ; 128 KB'lık güvenli yığın işaretçisini ayarla

    ; Multiboot bilgilerini yığına it
    push ebx    ; mbi pointer
    push eax    ; magic number
    call kernel_main           ; C fonksiyonuna zıpla

infinite_loop:
    hlt
    jmp infinite_loop

extern keyboard_handler
global keyboard_handler_stub

keyboard_handler_stub:
    pusha          ; Tüm genel kayıtçıları yığına it
    call keyboard_handler
    popa           ; Kayıtçıları geri yükle
    iret           ; Kesmeden geri dön

extern mouse_handler
global mouse_handler_stub

mouse_handler_stub:
    pusha          
    call mouse_handler
    popa
    iret

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]  ; C'den gelen gdt_ptr adresini al
    lgdt [eax]          ; GDT'yi işlemciye yükle

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush
.flush:
    ret

global idt_flush
idt_flush:
    mov eax, [esp + 4]  ; C'den gelen idt_ptr adresini al
    lidt [eax]          ; IDT'yi işlemciye yükle
    ret

extern timer_handler
global timer_handler_stub

timer_handler_stub:
    push 0          
    push 32         
    pusha           
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp        
    call timer_handler
    mov esp, eax    ; C'den dönen yeni ESP değerini yükle
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

global load_page_directory
load_page_directory:
    mov eax, [esp + 4]
    mov cr3, eax        
    ret

global switch_to_task
switch_to_task:
    mov esp, [esp + 4]
    popa           
    iret           

global switch_to_stack
switch_to_stack:
    mov eax, [esp + 4]    
    mov esp, eax          
    popa                  
    iret                  

extern exception_handler
global common_exception_stub

common_exception_stub:
    pusha
    push ds
    push es
    push fs         
    push gs         
    push esp        
    call exception_handler
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret                 

global isr0                  
isr0:
    push 0                   
    push 0                   
    jmp common_exception_stub

global isr13                 
isr13:
    push 13                  
    jmp common_exception_stub

global isr14                 
isr14:
    push 14                  
    jmp common_exception_stub

global enable_paging
enable_paging:
    mov eax, cr0
    or eax, 0x80000000  
    mov cr0, eax
    ret

section .bss
align 16
stack_bottom:
    resb 131072         ; 128 KB yığın alanı tek bir yerde tanımlandı
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits