#include "fs.h"
#include "kernel.h"

extern void* kmalloc(unsigned int size); // mm.c'den kullanacağız
file_t file_table[MAX_FILES];

void init_fs() {
    for(int i = 0; i < MAX_FILES; i++) {
        file_table[i].is_used = 0;
    }
}

// Basit bir string karşılaştırma (strcmp yerine)
int str_equal(char* s1, char* s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *s1 == *s2;
}

int create_file(char* name, char* content) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(!file_table[i].is_used) {
            // İsim kopyala
            int j = 0;
            while(name[j] && j < MAX_FILENAME-1) {
                file_table[i].name[j] = name[j];
                j++;
            }
            file_table[i].name[j] = '\0';

            // İçerik için bellek ayır ve kopyala
            int len = 0;
            while(content[len]) len++;
            
            file_table[i].size = len;
            file_table[i].data = (unsigned char*)kmalloc(len + 1);
            
            for(int k = 0; k <= len; k++) {
                file_table[i].data[k] = content[k];
            }

            file_table[i].is_used = 1;
            return i;
        }
    }
    return -1;
}

unsigned char* read_file(char* name) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(file_table[i].is_used && str_equal(file_table[i].name, name)) {
            return file_table[i].data;
        }
    }
    return 0; // Dosya bulunamadı
}

void list_files() {
    print("\n--- DOSYA LISTESI ---\n", 0x0B);
    for(int i = 0; i < MAX_FILES; i++) {
        if(file_table[i].is_used) {
            print(file_table[i].name, 0x0F);
            print("  -  Dosya bulundu\n", 0x07); 
        }
    }
}