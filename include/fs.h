/* * include/fs.h - OBA-32 Sanal Dosya Sistemi (VFS) ve Klasör Ağacı Başlık Dosyası
 */

#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_NAME_MAX 32
#define FS_MAX_CHILDREN 16

typedef enum {
    FS_TYPE_FILE,
    FS_TYPE_DIRECTORY
} FS_NodeType;

typedef struct VFS_Node {
    char name[FS_NAME_MAX];
    FS_NodeType type;
    uint32_t size;
    char* content;
    
    struct VFS_Node* parent;
    struct VFS_Node* children[FS_MAX_CHILDREN];
    int child_count;
} VFS_Node_t;

void init_fs(void);
VFS_Node_t* vfs_get_root(void);
VFS_Node_t* vfs_create_file(VFS_Node_t* parent, const char* name, const char* content);
VFS_Node_t* vfs_create_directory(VFS_Node_t* parent, const char* name);
VFS_Node_t* vfs_find_child(VFS_Node_t* parent, const char* name);
int vfs_delete_node(VFS_Node_t* parent, const char* name); /* ✨ YENİ: Dinamik Silme Protokolü */

VFS_Node_t* vfs_get_current_dir(void);
void vfs_set_current_dir(VFS_Node_t* dir);

#endif