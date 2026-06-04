# Değişim Günlüğü (Change Log)

Bu projedeki tüm önemli değişiklikler bu dosyada belgelenecektir.
Format [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) standartlarına dayanmaktadır
ve bu proje [Semantic Versioning](https://semver.org/spec/v2.0.0.html) kurallarına uymaya çalışır.

## [1.0.4] - 2026-03-01

### Eklendi
- Bellek yönetimi (`mm.c`) için ilk mimari taslak oluşturuldu.

## [1.0.3] - 2026-02-28

### Eklendi
- Dinamik bellek genişletme (Dynamic Memory Expansion) özelliği eklendi.
- Grafiksel arayüz için temel masaüstü bileşenleri tamamlandı.

### Değiştirildi
- Klavye ve Fare sürücüleri daha stabil çalışacak şekilde güncellendi.

## [1.0.2] - 2026-02-13

### Eklendi
- Grafik modu denemeleri kapsamında ilk masaüstü (desktop) prototipi oluşturuldu.

## [1.0.1] - 2026-02-12

### Eklendi
- İlk klavye sürücüsü (`keyboard.c`) sisteme entegre edildi.
- Fare (Mouse) sürücüsü (`mouse.c`) eklendi.

## [1.0.0] - 2026-02-09

### Eklendi
- **Kernel:** Temel işletim sistemi çekirdeği (`kernel.c`) oluşturuldu.
- **Boot:** İlk bootloader (`boot.s`) tasarlandı ve başarıyla test edildi.
- **Mimari:** GDT (Global Descriptor Table) ve IDT (Interrupt Descriptor Table) temel yapıları kuruldu.
- **Sistem:** Basit bir "Hello, World!" uygulaması (`main.c`) ve derleme sürecini otomatize eden `Makefile` eklendi.

---
*Not: Bu sürüm projenin ilk kararlı temel sürümüdür.*