# =============================================================================
# OBA-32 Çekirdek İnşa Otomasyon Yönetim Betiği (Makefile)
# Bu betik; assembly ve C kaynak kodlarının freestanding modda derlenmesi,
# alt dizin hiyerarşilerinin taranması ve ISO kalıbının QEMU üzerinde yürütülmesini yönetir.
# =============================================================================

# --- 1. ÇAPRAZ DERLEME VE BAĞLANTI ARAÇLARI ---
CC = gcc
AS = nasm
LD = ld

# --- 2. DİZİN VE HEDEF YAPILANDIRMALARI ---
SRC_DIR = src
INC_DIR = include
OBJ_DIR = build

# --- 3. DERLEME VE BAĞLANTI BAYRAKLARI ---
CFLAGS  = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(INC_DIR) -I$(INC_DIR)/gui
LDFLAGS = -m elf_i386 -T linker.ld

# --- 4. DERLEME ZİNCİRİNE DAHİL EDİLEN NESNE DOSYALARI (OBJS) ---
OBJS = $(OBJ_DIR)/boot.o \
       $(OBJ_DIR)/kernel.o \
       $(OBJ_DIR)/hal.o \
       $(OBJ_DIR)/gdt.o \
       $(OBJ_DIR)/idt.o \
       $(OBJ_DIR)/mm.o \
       $(OBJ_DIR)/fs.o \
       $(OBJ_DIR)/fat32.o \
       $(OBJ_DIR)/graphics.o \
       $(OBJ_DIR)/mouse.o \
       $(OBJ_DIR)/keyboard.o \
       $(OBJ_DIR)/sched.o \
       $(OBJ_DIR)/terminal.o \
       $(OBJ_DIR)/gui_manager.o \
       $(OBJ_DIR)/app_panel.o \
       $(OBJ_DIR)/app_files.o \
       $(OBJ_DIR)/ata_disk.o

# --- 5. ANA SİSTEM İNŞA HEDEFLERİ (TARGETS) ---
.PHONY: all prepare run clean

all: prepare kernel.bin

kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)

prepare:
	@mkdir -p $(OBJ_DIR)

# --- 6. KAYNAK KOD DERLEME KURALLARI (COMPILATION RULES) ---

$(OBJ_DIR)/boot.o: $(SRC_DIR)/boot.s | prepare
	$(AS) -f elf32 $< -o $@

$(OBJ_DIR)/kernel.o: $(SRC_DIR)/kernel.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/hal.o: $(SRC_DIR)/hal.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/gdt.o: $(SRC_DIR)/gdt.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/idt.o: $(SRC_DIR)/idt.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/mm.o: $(SRC_DIR)/mm.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/fs.o: $(SRC_DIR)/fs.c | prepare
	$(CC) $(CFLAGS) $< -o $@

# FAT32 Dosya Sistemi Modülü Derleme Kuralı
$(OBJ_DIR)/fat32.o: $(SRC_DIR)/fs/fat32.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/graphics.o: $(SRC_DIR)/graphics.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/mouse.o: $(SRC_DIR)/mouse.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/keyboard.o: $(SRC_DIR)/keyboard.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/sched.o: $(SRC_DIR)/sched.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/terminal.o: $(SRC_DIR)/terminal.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/gui_manager.o: $(SRC_DIR)/gui/gui_manager.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/app_panel.o: $(SRC_DIR)/gui/apps/panel.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/app_files.o: $(SRC_DIR)/gui/apps/files.c | prepare
	$(CC) $(CFLAGS) $< -o $@

$(OBJ_DIR)/ata_disk.o: $(SRC_DIR)/drivers/disk/ata.c | prepare
	$(CC) $(CFLAGS) $< -o $@

# --- 7. EMÜLASYON VE DAĞITIM KATMANI (DEPLOYMENT) ---

run: kernel.bin
	@mkdir -p build/iso/boot/grub
	@echo 'set timeout=0' > build/iso/boot/grub/grub.cfg
	@echo 'set default=0' >> build/iso/boot/grub/grub.cfg
	@echo 'menuentry "OBA OS v1.1.0" {' >> build/iso/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.bin' >> build/iso/boot/grub/grub.cfg
	@echo '    boot' >> build/iso/boot/grub/grub.cfg
	@echo '}' >> build/iso/boot/grub/grub.cfg
	@cp kernel.bin build/iso/boot/kernel.bin
	@grub-mkrescue -o oba.iso build/iso
	
	@# AKILLI MODELLEME: Disk imajı yoksa oluştur, varsa es geç!
	@if [ ! -f build/oba_disk.img ]; then \
		echo "[MAKE] Permanent disk image not found. Creating a new one..."; \
		qemu-img create -f raw build/oba_disk.img 40M; \
		mkfs.vfat -F 32 build/oba_disk.img; \
		echo "OBA-32 FAT32 subsystem data read transaction completed successfully." > build/testdata.txt; \
		mkdir -p build/mnt; \
		sudo mount -o loop build/oba_disk.img build/mnt; \
		sudo cp build/testdata.txt build/mnt/TESTDATA.TXT; \
		sudo umount build/mnt; \
	else \
		echo "[MAKE] Permanent disk image found. Preserving data layer..."; \
	fi

	qemu-system-i386 -cdrom oba.iso -hda build/oba_disk.img -boot d

clean:
	rm -rf $(OBJ_DIR) kernel.bin oba.iso