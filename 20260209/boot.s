; boot.s - Çekirdek Giriş Noktası

section .multiboot
align 4
    dd 0x1BADB002              ; Multiboot 'magic number' (Önyükleyici bunu arar)
    dd 0x00                    ; Flags (Şimdilik özellik istemiyoruz)
    dd -(0x1BADB002 + 0x00)    ; Checksum (Doğrulama için toplam)

section .text
global _start
extern kernel_main             ; C dosyamızdaki ana fonksiyonu çağıracağız

_start:
    cli                        ; Kesmeleri (Interrupts) kapat
    mov esp, stack_top         ; Yığın (Stack) işaretçisini ayarla
    ; Multiboot bilgilerini yığına it (C parametreleri olarak)
    push ebx    ; Multiboot bilgi yapısının adresi    ; Bu, mbi parametresine karşılık gelir (ikinci parametre)
    push eax    ; Magic number                        ; Bu, magic parametresine karşılık gelir (birinci parametre)
    call kernel_main           ; C fonksiyonuna zıpla

extern keyboard_handler
global keyboard_handler_stub

keyboard_handler_stub:
    pusha          ; Tüm genel kayıtçıları yığına it (EAX, ECX, EDX vb.)
    call keyboard_handler
    popa           ; Kayıtçıları geri yükle
    iret           ; Kesmeden geri dön (Çok kritik!)


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
resb 16384                     ; 16 KB yığın alanı ayır
stack_top:


