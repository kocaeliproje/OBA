; src/boot.s - Yeni Giriş Alanı
section .multiboot
align 4
    MULTIBOOT_MAGIC    equ 0x1BADB002
    MULTIBOOT_FLAGS    equ 0x00000004  ; BIT 2: Çekirdeğe grafik modu desteği istediğimizi belirtir
    MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

    ; Multiboot Grafik Bilgileri (Mode Type, Width, Height, Depth)
    dd 0x00000000  ; 0: Grafik modu istiyoruz (1 metin modu olurdu)
    dd 320         ; Genişlik (Width)
    dd 200         ; Yükseklik (Height)
    dd 8           ; Renk Derinliği (Bits per pixel - 256 renk için 8-bit)

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


extern keyboard_handler
global keyboard_handler_stub

keyboard_handler_stub:
    pusha          ; Tüm genel kayıtçıları yığına it (EAX, ECX, EDX vb.)
    call keyboard_handler
    popa           ; Kayıtçıları geri yükle
    iret           ; Kesmeden geri dön (Çok kritik!)

extern mouse_handler
global mouse_handler_stub

mouse_handler_stub:
    pusha          ; regs_t yapınla uyumlu olsun
    call mouse_handler
    popa
    iret


set_vga_mode:
    ; Real Mode'da olduğumuz varsayılarak (veya GRUB ayarıyla)
    ; VBE veya VGA 0x13 modu seçilir. 
    ; Şimdilik kodumuzu Protected Mode'da piksel boyamaya odaklayalım.

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]  ; C'den gelen gdt_ptr adresini al
    lgdt [eax]          ; GDT'yi işlemciye yükle (Load GDT)

    ; Segment kayıtçılarını yeni verilerle güncelle (0x10 veri segmentidir)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Uzak zıplama (Far Jump) ile kod segmentini (0x08) aktif et
    jmp 0x08:.flush
.flush:
    ret

global idt_flush
idt_flush:
    mov eax, [esp + 4]  ; C'den gelen idt_ptr adresini al
    lidt [eax]          ; IDT'yi işlemciye yükle (Load IDT)
    ret

extern timer_handler
global timer_handler_stub

; Zamanlayıcı Kesmesi (IRQ0)
timer_handler_stub:
    push 0          ; Sahte hata kodu
    push 32         ; Kesme numarası
    pusha           ; Genel kayıtçılar
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp        ; regs_t* r parametresi
    call timer_handler
    mov esp, eax    ; C'den dönen yeni ESP değerini yükle (HAYATİ ÖNEMDE)
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
    mov cr3, eax        ; CR3 kayıtçısına directory adresini yükle
    ret

global switch_to_task
switch_to_task:
    ; C'den gelen yeni task'ın ESP adresini al
    mov esp, [esp + 4]
    popa           ; Yeni görevin kayıtçılarını geri yükle
    iret           ; Yeni göreve zıpla

global switch_to_stack
switch_to_stack:
    mov eax, [esp + 4]    ; Yeni görevden gelen ESP değerini al
    mov esp, eax          ; İşlemcinin yığın işaretçisini (ESP) değiştir
    popa                  ; Yeni görevin kayıtçılarını geri yükle
    iret                  ; Kesmeden dönerek yeni görevin EIP'sine zıpla

extern exception_handler
global common_exception_stub

; Genel Hata Yakalayıcı (Exceptions)
common_exception_stub:
    pusha
    push ds
    push es
    push fs         ; Struct ile eşitlemek için eklendi
    push gs         ; Struct ile eşitlemek için eklendi
    push esp        ; regs_t* r olarak gönder
    call exception_handler
    ; Genelde exception_handler içinde hlt olduğu için buradan dönülmez
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret                 


; Bazı yaygın hatalar için giriş noktaları
global isr0                  ; Sıfıra bölme
isr0:
    push 0                   ; Sahte hata kodu
    push 0                   ; Kesme numarası
    jmp common_exception_stub

global isr13                 ; General Protection Fault
isr13:
    push 13                  ; Kesme numarası (hata kodu yığındadır)
    jmp common_exception_stub

global isr14                 ; Page Fault
isr14:
    push 14                  ; Kesme numarası
    jmp common_exception_stub

global enable_paging:
enable_paging:
    mov eax, cr0
    or eax, 0x80000000  ; CR0'ın 31. bitini (PG biti) 1 yap
    mov cr0, eax
    ret


    
.hang:
    hlt                        ; C'den çıkılırsa işlemciyi durdur
    jmp .hang                  ; Güvenlik için sonsuz döngü

section .bss
align 16
stack_bottom:
resb 131072                     ; 128 KB yığın alanı ayır
stack_top:


section .note.GNU-stack noalloc noexec nowrite progbits