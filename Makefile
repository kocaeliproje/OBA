# Derleyici ayarları
CC = gcc
AS = nasm
LD = ld

# Klasör yolları
SRC_DIR = src
INC_DIR = include
OBJ_DIR = build

# Derleme bayrakları (-Iinclude ile header dosyalarını otomatik bulur)
CFLAGS = -m32 -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(INC_DIR)
LDFLAGS = -m elf_i386 -T linker.ld

# Nesne dosyaları listesi (build klasörüne gidecekler)
OBJS = $(OBJ_DIR)/boot.o \
       $(OBJ_DIR)/hal.o \
       $(OBJ_DIR)/gdt.o \
       $(OBJ_DIR)/idt.o \
       $(OBJ_DIR)/mm.o \
       $(OBJ_DIR)/fs.o \
       $(OBJ_DIR)/gui.o \
       $(OBJ_DIR)/sched.o \
       $(OBJ_DIR)/mouse.o \
       $(OBJ_DIR)/keyboard.o \
       $(OBJ_DIR)/kernel.o

all: prepare $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJS)

# Build klasörünü oluşturur
prepare:
	mkdir -p $(OBJ_DIR)

# Assembly derleme (boot.s dosyan src/ içinde olmalı)
$(OBJ_DIR)/boot.o: $(SRC_DIR)/boot.s
	$(AS) -f elf32 $< -o $@

# C dosyalarını otomatik derleme kuralı
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

run:
	qemu-system-i386 -kernel kernel.bin -vga std

clean:
	rm -rf $(OBJ_DIR) kernel.bin