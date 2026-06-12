/* * src/gui/apps/files.c - OBA-32 Dosya Gezgini Uygulama Modülü
 */

#include "gui/gui.h"
#include "fs.h"
#include <stdint.h>

extern volatile unsigned int timer_ticks;

/* ✨ MÜHÜRLENDİ: Bağlayıcı aşamasındaki referans hatalarını önlemek amacıyla eklenen extern tanımları */
extern int gui_selected_file_idx;
extern const char* notepad_file_content_pointer;

/* Harici grafik ve pencere sistemi referansları */
extern void draw_rect(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_rect_outline(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_string(int start_x, int start_y, const char* str, uint32_t color);
extern void gui_create_window(WindowSystem_t* self, char* title, int x, int y, int w, int h, int type);

static unsigned int last_click_tick = 0;
static int last_clicked_item_idx = -1;

static void draw_folder_icon(int x, int y, const char* name, uint32_t text_color) {
    draw_rect(x, y + 4, 32, 22, 0xE0A96D);     
    draw_rect(x, y, 14, 5, 0xDE8F55);          
    draw_rect(x + 2, y + 8, 28, 16, 0xF4CC70); 
    draw_string(x - 8, y + 28, name, text_color);
}

static void draw_file_icon(int x, int y, const char* name, uint32_t text_color) {
    draw_rect(x + 2, y, 26, 26, 0xFFFFFF);       
    draw_rect_outline(x + 2, y, 26, 26, 0x999999); 
    draw_rect(x + 6, y + 6, 18, 2, 0x00ADB5);
    draw_rect(x + 6, y + 12, 14, 2, 0xAAAAAA);
    draw_rect(x + 6, y + 18, 18, 2, 0xAAAAAA);
    draw_string(x - 8, y + 28, name, text_color);
}

void files_render(Window_t* self) {
    draw_rect(self->x, self->y + 30, self->width, 22, 0xEAEAEA);
    int btn_x = self->x + 10; int btn_y = self->y + 33;
    draw_rect(btn_x, btn_y, 22, 16, 0x393E46); 
    draw_rect_outline(btn_x, btn_y, 22, 16, 0x00ADB5); 
    draw_string(btn_x + 4, btn_y + 4, "<-", 0x00ADB5);   

    char path_buffer[64] = "Location: /"; int p_ptr = 11;
    VFS_Node_t* active_dir = vfs_get_current_dir();
    if (active_dir != 0 && active_dir->name[0] != '/') {
        int n_ptr = 0; while(active_dir->name[n_ptr] != '\0' && p_ptr < 60) path_buffer[p_ptr++] = active_dir->name[n_ptr++];
    }
    path_buffer[p_ptr] = '\0';
    draw_string(self->x + 40, self->y + 36, path_buffer, 0x222831);
    draw_rect(self->x, self->y + 52, self->width, 1, 0xDDDDDD);

    if (active_dir != 0) {
        int start_icon_x = self->x + 25; int start_icon_y = self->y + 75;
        int row_count = 0; int col_count = 0;
        int max_cols = (self->width - 30) / 75; if (max_cols < 1) max_cols = 1;

        for (int c = 0; c < active_dir->child_count; c++) {
            VFS_Node_t* item = active_dir->children[c];
            int icon_x = start_icon_x + (col_count * 75);
            int icon_y = start_icon_y + (row_count * 65);

            if (icon_x + 40 < self->x + self->width && icon_y + 50 < self->y + self->height) {
                if (gui_selected_file_idx == c) {
                    draw_rect(icon_x - 6, icon_y - 4, 44, 46, 0xCEE5D0); 
                    draw_rect_outline(icon_x - 6, icon_y - 4, 44, 46, 0x00ADB5);  
                }
                if (item->type == FS_TYPE_DIRECTORY) { draw_folder_icon(icon_x, icon_y, item->name, 0x222831); }
                else { draw_file_icon(icon_x, icon_y, item->name, 0x222831); }
            }
            col_count++; if (col_count >= max_cols) { col_count = 0; row_count++; }
        }
    }
}



void files_handle_click(Window_t* self, int local_mx, int local_my) {
    VFS_Node_t* active_dir = vfs_get_current_dir();
    
    // Geri butonu kontrolü (<-)
    if (local_mx >= 10 && local_mx <= 32 && local_my >= 33 && local_my <= 49) {
        if (active_dir != 0 && active_dir->parent != active_dir) {
            vfs_set_current_dir(active_dir->parent);
            gui_selected_file_idx = -1;
        }
        return;
    }

    // İkon alanına tıklandı mı kontrolü
    if (active_dir != 0 && local_my >= 52) {
        int start_icon_x = 25; int start_icon_y = 75;
        int row_count = 0; int col_count = 0; int hit_any_icon = 0;
        int max_cols = (self->width - 30) / 75; if (max_cols < 1) max_cols = 1;

        for (int c = 0; c < active_dir->child_count; c++) {
            int icon_x = start_icon_x + (col_count * 75);
            int icon_y = start_icon_y + (row_count * 65);

            // Fare bu ikonun sınırları içinde mi?
            if (local_mx >= icon_x - 5 && local_mx <= icon_x + 37 && local_my >= icon_y - 5 && local_my <= icon_y + 45) {
                hit_any_icon = 1; 
                unsigned int current_tick = timer_ticks;
                
                // 1. Tıklanan öğeyi seçili yap ve ekrana yansıt
                gui_selected_file_idx = c;
                
                // 2. ÇİFT TIKLAMA KONTROLÜ
                if (last_clicked_item_idx == c && (current_tick - last_click_tick) < 35) {
                    VFS_Node_t* target_node = active_dir->children[c];
                    if (target_node->type == FS_TYPE_DIRECTORY) {
                        vfs_set_current_dir(target_node); 
                        gui_selected_file_idx = -1;
                    } else {
                        // İçerik göstericisini güncelle
                        notepad_file_content_pointer = target_node->content;
                        
                        // Sistemde zaten bir NOTEPAD penceresi var mı kontrol et
                        int notepad_exists = 0;
                        for (int k = 0; k < GuiManager.window_count; k++) {
                            if (GuiManager.windows[k].window_type == 3) {
                                GuiManager.windows[k].title = target_node->name;
                                GuiManager.windows[k].is_visible = 1;
                                notepad_exists = 1;
                                break;
                            }
                        }
                        
                        // İlk defa açılıyorsa oluştur
                        if (!notepad_exists) {
                            gui_create_window(&GuiManager, target_node->name, 250, 150, 400, 250, 3);
                            // Yeni oluşturulan pencereyi görünür yapmayı unutmayalım
                            for (int k = 0; k < GuiManager.window_count; k++) {
                                if (GuiManager.windows[k].window_type == 3) {
                                    GuiManager.windows[k].is_visible = 1;
                                    break;
                                }
                            }
                        }
                    }
                    last_clicked_item_idx = -1; // Durumu sıfırla
                } else {
                    // ✨ DÜZELTME: İlk tıklamayı kaydet ki bir sonraki tık çift tıklama sayılabilsin!
                    last_clicked_item_idx = c;
                    last_click_tick = current_tick;
                }
                
                return; // İkon bulundu, fonksiyonu sonlandır
            }
            col_count++; if (col_count >= max_cols) { col_count = 0; row_count++; }
        }
        
        // Boş bir yere tıklandıysa seçimi kaldır
        if (!hit_any_icon) gui_selected_file_idx = -1;
    }
}