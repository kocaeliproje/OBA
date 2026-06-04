#ifndef FS_H
#define FS_H

#define MAX_FILES 32
#define MAX_FILENAME 16
#define MAX_FILE_SIZE 1024

// Dosya yapısı
typedef struct {
    char name[MAX_FILENAME];
    unsigned int size;
    unsigned char* data;
    int is_used;
} file_t;

// Dosya sistemi fonksiyonları
void init_fs();
int create_file(char* name, char* content);
unsigned char* read_file(char* name);
void list_files();

int str_equal(char* s1, char* s2);

#endif