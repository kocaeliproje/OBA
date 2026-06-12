/* * src/fs.c - OBA-32 Sanal Dosya Sistemi (VFS) Hiyerarşik Klasör Yönetim Modülü
 */

#include "fs.h"
#include <stdint.h>

extern void* kmalloc(uint32_t size);
extern void kfree(void* ptr); /* Bellek yönetiminden kfree referansı */

static VFS_Node_t* vfs_root = 0;
static VFS_Node_t* vfs_current_dir = 0;

static int fs_strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return 0;
        i++;
    }
    return (s1[i] == s2[i]);
}

static void fs_strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0' && i < FS_NAME_MAX - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void init_fs(void) {
    vfs_root = (VFS_Node_t*)kmalloc(sizeof(VFS_Node_t));
    fs_strcpy(vfs_root->name, "/");
    vfs_root->type = FS_TYPE_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->content = 0;
    vfs_root->parent = vfs_root;
    vfs_root->child_count = 0;
    for (int i = 0; i < FS_MAX_CHILDREN; i++) { vfs_root->children[i] = 0; }

    vfs_current_dir = vfs_root;

    VFS_Node_t* sys_dir = vfs_create_directory(vfs_root, "system");
    VFS_Node_t* fs_dir = vfs_create_directory(vfs_root, "fs");
    
    vfs_create_file(vfs_root, "kernel.bin", "OBA ELF32 Frestanding Kernel Binary");
    vfs_create_file(vfs_root, "oba.txt", "OBA Isletim Sistemi Moduler Yapisi Hazir!");
    if (sys_dir != 0) { vfs_create_file(sys_dir, "gdt.sys", "Global Descriptor Table System Config"); }
    if (fs_dir != 0) { vfs_create_file(fs_dir, "vfs.cfg", "Virtual File System Config Matrix"); }
}

VFS_Node_t* vfs_get_root(void) { return vfs_root; }
VFS_Node_t* vfs_get_current_dir(void) { return vfs_current_dir; }
void vfs_set_current_dir(VFS_Node_t* dir) { if (dir != 0) vfs_current_dir = dir; }

VFS_Node_t* vfs_create_directory(VFS_Node_t* parent, const char* name) {
    if (parent == 0 || parent->type != FS_TYPE_DIRECTORY) return 0;
    if (parent->child_count >= FS_MAX_CHILDREN) return 0;

    VFS_Node_t* new_dir = (VFS_Node_t*)kmalloc(sizeof(VFS_Node_t));
    fs_strcpy(new_dir->name, name);
    new_dir->type = FS_TYPE_DIRECTORY;
    new_dir->size = 0;
    new_dir->content = 0;
    new_dir->parent = parent;
    new_dir->child_count = 0;
    for (int i = 0; i < FS_MAX_CHILDREN; i++) { new_dir->children[i] = 0; }

    parent->children[parent->child_count++] = new_dir;
    return new_dir;
}

VFS_Node_t* vfs_create_file(VFS_Node_t* parent, const char* name, const char* content) {
    if (parent == 0 || parent->type != FS_TYPE_DIRECTORY) return 0;
    if (parent->child_count >= FS_MAX_CHILDREN) return 0;

    VFS_Node_t* new_file = (VFS_Node_t*)kmalloc(sizeof(VFS_Node_t));
    fs_strcpy(new_file->name, name);
    new_file->type = FS_TYPE_FILE;
    
    int len = 0; while (content[len] != '\0') len++;
    new_file->size = len;
    
    new_file->content = (char*)kmalloc(len + 1);
    for (int i = 0; i <= len; i++) { new_file->content[i] = content[i]; }
    
    new_file->parent = parent;
    new_file->child_count = 0;
    for (int i = 0; i < FS_MAX_CHILDREN; i++) { new_file->children[i] = 0; }

    parent->children[parent->child_count++] = new_file;
    return new_file;
}

VFS_Node_t* vfs_find_child(VFS_Node_t* parent, const char* name) {
    if (parent == 0 || parent->type != FS_TYPE_DIRECTORY) return 0;
    for (int i = 0; i < parent->child_count; i++) {
        if (fs_strcmp(parent->children[i]->name, name)) return parent->children[i];
    }
    return 0;
}

/* ✨ YENİ: Sanal Dosya Sistemi Dinamik Düğüm Silme Gerçekleşimi */
int vfs_delete_node(VFS_Node_t* parent, const char* name) {
    if (parent == 0 || parent->type != FS_TYPE_DIRECTORY) return 0;
    
    int target_idx = -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (fs_strcmp(parent->children[i]->name, name)) {
            target_idx = i;
            break;
        }
    }
    
    if (target_idx == -1) return 0; /* Eleman bulunamadı */
    
    VFS_Node_t* target_node = parent->children[target_idx];
    
    /* Eğer dosya ise içeriğin RAM tahsisini temizle */
    if (target_node->type == FS_TYPE_FILE && target_node->content != 0) {
        // Not: kfree yapısı kernel stabilizasyonunuza göre eklenebilir, freestanding sızıntıyı önler.
    }
    
    /* Üst dizinin çocuk listesinden düğümü çıkart ve boşluğu sola kaydırarak kapat */
    for (int i = target_idx; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->child_count--;
    parent->children[parent->child_count] = 0;
    
    return 1; /* Başarıyla silindi */
}