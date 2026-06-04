---
# OBA-32: x86 İşletim Sistemi Çekirdeği (Kernel)

OBA, x86 (32-bit) mimarisini hedefleyen, modüler tasarımı temel alan ve modern işletim sistemi teorilerini uygulamak amacıyla sıfırdan (From Scratch) geliştirilen bağımsız bir çekirdek (kernel) projesidir. 

Proje, donanım seviyesinde kararlı bir korumalı mod (Protected Mode) ortamı kurarak; çoklu görev (multitasking), sanal bellek (paging) ve olay tabanlı (event-driven) bir grafik masaüstü sunucusunu kendi mikrosisteminde çalıştırmayı başarmıştır.

---

<p align="center">
  <img src="assets/2026-06-04 223440.png" alt="OBA v1.0.4 Pencere Denemesi" width="600px">
  <br>
  <i>Görsel 1: OBA v1.0.3 - İlk Masaüstü ve Pencere Yönetimi Denemesi</i>
</p>
---


## 🛠️ Temel ve İleri Seviye Özellikler

OBA Çekirdeği, ilkel bir monolitik yapıdan ziyade nesne tabanlı (OOP in C) tasarım kalıplarını kullanarak aşağıdaki alt sistemleri barındırır:

* **Çoklu Görev Motoru (Round-Robin Scheduler):** Zamanlayıcı (PIT IRQ0) kesmesi ile çalışan, görevlerin CPU durumlarını (`EFLAGS`, `CS`, `EIP` ve genel amaçlı yazmaçlar) yığında (Stack) saklayıp değiştiren, `sleep` (uyuma) yeteneğine sahip kararlı çoklu görev motoru.
* **Identity Mapping & Kararlı Sayfalama (Paging):** İlk 4 MB'lık fiziksel bellek alanını (Kernel kodları, sistem yığını ve VGA video belleği dahil) birebir haritalayarak Page Fault ve hafıza taşması risklerini sıfırlayan sayfalama altyapısı.
* **Çift Tamponlu Masaüstü Sunucusu (Double-Buffered GUI):** Donanımsal VBE/VGA sınırlamalarından bağımsız, tüm pencereleri ve bileşenleri önce RAM'deki `back_buffer` alanında işleyen ve ardından tek bir döngüde video belleğine fırlatarak ekran yırtılmalarını (flickering) önleyen pencere yöneticisi.
* **Sanal Dosya Sistemi (VFS Prototipi):** Dinamik bellekten yer tahsis ederek dosya oluşturma (`create_file`), dosya okuma (`read_file`) ve dizin listeleme yeteneklerine sahip RAM tabanlı temel dosya sistemi.
* **Senkronize Donanım Sürücüleri:** * **Fare (PS/2 Mouse):** Ham donanımsal piksel verilerini sanal bir uzayda biriktirip ekran çözünürlüğüne oranlayan, emülatör sınırlarından taşmayan ve tıklama/sürükleme (drag & drop) destekleyen IRQ12 sürücüsü.
  * **Klavye (PS/2 Keyboard):** `Ctrl`, `Shift` gibi modifikasyon tuşlarının basılma/bırakılma durumlarını (Make/Break scancodes) anlık takip eden ve asenkron kısayol mekanizmasını besleyen IRQ1 sürücüsü. (Ctrl + o ve Ctrl +u test edildi.)

---

## 📂 Dosya Yapısı ve Mimarisi

```text
oba/
├── src/
│   ├── boot.s          # Multiboot imzası, kesme giriş noktaları (stubs) ve yığın yönetimi
│   ├── kernel.c        # Ana çekirdek döngüsü, ekran çıktıları ve donanım ilklendirme
│   ├── gdt.c / idt.c   # Global Descriptor ve Interrupt Descriptor tablolarının inşası
│   ├── mm.c            # PMM, Placement Allocator ve kmalloc/kmalloc_aligned motoru
│   ├── sched.c         # Task yapıları, Round-Robin algoritması ve context switching
│   ├── gui.c           # Çift tamponlu pencere çizim logic'i, görev çubuğu ve dijital saat
│   ├── mouse.c         # PS/2 fare sürücüsü ve sanal koordinat emniyet filtreleri
│   ├── keyboard.c      # Klavye tarama kod haritası (Scancode Map) ve terminal tamponu
│   ├── fs.c            # RAM tabanlı dosya arama, yazma ve string optimizasyonları
│   └── hal.c           # Donanım Soyutlama Katmanı (I/O port - inb/outb rutinleri)
├── include/            # Sistem bileşenlerine ait tüm `.h` başlık (header) dosyaları
├── linker.ld           # Multiboot imzasını ilk 8KB içine sabitleyen 1MB hizalama betiği
└── Makefile            # Otomasyon ve temiz derleme zinciri emirleri

##🚀 Derleme ve Çalıştırma
OBA çekirdeği, freestanding (bağımsız) modda derlenmekte olup herhangi bir standart C kütüphanesine (glibc) bağımlı değildir.

Gereksinimler
Sisteminizde nasm, gcc (32-bit desteği ile) ve qemu-system-i386 kurulu olmalıdır. Ubuntu/WSL üzerinde kurulum için:

Bash
sudo apt update
sudo apt install nasm gcc-multilib qemu-system-x86
Projeyi Derlemek
Derleme kalıntılarını temizlemek ve tüm alt sistemleri nesne dosyalarına (.o) dönüştürüp kernel.bin imajını bağlamak (link etmek) için:


1. **Derlemek için:**
   
   make

2. **QEMU ile test etmek için:**

    make run

---

<p align="center">
  <video width="600" autoplay muted loop controls>
    <source src="assets/oba_demo.mp4" type="video/mp4">
    Tarayıcınız video etiketini desteklemiyor.
  </video>
  <br>
  <i>Video 1: OBA v1.0.4 - Fare Senkronizasyonu ve Canlı Pencere Sürükleme Demosu</i>
</p>

---
📜 Lisans
Bu proje Apache License 2.0 ile lisanslanmıştır. Detaylar için LICENSE dosyasına göz atabilirsiniz.
