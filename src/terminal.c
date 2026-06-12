/* * src/terminal.c - OBA-32 Bağımsız Gelişmiş Terminal ve Kabuk Yönetim Modülü
 * Dinamik pencere boyutlandırma (Resizing) esnasında metinlerin taşmasını önleyen
 * esnek word-wrap (otomatik alt satıra geçiş) motoru entegre edilmiştir.
 */

#include "terminal.h"
#include "fs.h"
#include "hal.h"
#include "fat32.h"
#include <stdint.h>

/* --- TÜM PROTOTİPLER --- */
void terminal_parse_command(char* cmd);
void terminal_init(void);
void terminal_write_line(const char* text, uint32_t color);
void terminal_render(int win_x, int win_y, int win_w, int win_h);
void terminal_handle_char(char c);

extern volatile unsigned int timer_ticks;
extern volatile int cursor_pos_x;
extern volatile int cursor_pos_y;
extern volatile int mouse_left_button;
extern volatile int mouse_scroll_direction;

extern void draw_string(int start_x, int start_y, const char* str, uint32_t color);
extern void draw_rect(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_rect_outline(int start_x, int start_y, int w, int h, uint32_t color);

static Terminal_t TerminalInstance;
static int was_channel_clicked = 0;
static VFS_Node_t* current_directory = 0;
static int user_scrolled_back = 0; // ✨ TİTREMEYİ ÖNLEYEN KONTROL BAYRAĞI

static void term_strcpy(char* dest, const char* src);
static void terminal_push_history(const char* cmd);
static void terminal_execute(const char* cmd);

static void term_strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0' && i < 63) { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

static void terminal_push_history(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (TerminalInstance.cmd_history_count > 0) {
        int last_idx = (TerminalInstance.cmd_history_count - 1) % COMMAND_HISTORY_MAX;
        int match = 1; int i = 0;
        while (cmd[i] != '\0' || TerminalInstance.cmd_history[last_idx][i] != '\0') {
            if (cmd[i] != TerminalInstance.cmd_history[last_idx][i]) { match = 0; break; }
            i++;
        }
        if (match) return;
    }
    term_strcpy(TerminalInstance.cmd_history[TerminalInstance.cmd_history_count % COMMAND_HISTORY_MAX], cmd);
    TerminalInstance.cmd_history_count++;
}

void terminal_init(void) {
    for (int r = 0; r < VIRTUAL_HISTORY_MAX; r++) {
        TerminalInstance.grid[r][0] = '\0'; TerminalInstance.colors[r] = 0xFFFFFF;
    }
    TerminalInstance.total_lines = 0; TerminalInstance.scroll_offset = 0;
    TerminalInstance.input_idx = 0; TerminalInstance.input_buffer[0] = '\0';
    TerminalInstance.is_scrollbar_dragging = 0; TerminalInstance.scrollbar_y_offset = 0;
    was_channel_clicked = 0;
    user_scrolled_back = 0;
    current_directory = vfs_get_root();
    
    TerminalInstance.cmd_history_count = 0;
    TerminalInstance.cmd_history_select = -1;
    for (int i = 0; i < COMMAND_HISTORY_MAX; i++) {
        TerminalInstance.cmd_history[i][0] = '\0';
    }
    
    terminal_write_line("OBA Kernel Shell v1.0.4 - Welcome manet", 0xAAAAAA);
}

void terminal_write_line(const char* text, uint32_t color) {
    if (TerminalInstance.total_lines < VIRTUAL_HISTORY_MAX) {
        int c = 0;
        while (text[c] != '\0' && c < TERM_COLS - 1) {
            TerminalInstance.grid[TerminalInstance.total_lines][c] = text[c];
            c++;
        }
        TerminalInstance.grid[TerminalInstance.total_lines][c] = '\0';
        TerminalInstance.colors[TerminalInstance.total_lines] = color;
        TerminalInstance.total_lines++;
        
        /* Yeni bir çıktı basıldığında ekranı otomatik olarak en alta çek */
        user_scrolled_back = 0;
    }
}

static void terminal_execute(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (current_directory == 0) { current_directory = vfs_get_root(); }

    if (cmd[0] == 't' && cmd[1] == 'o' && cmd[2] == 'u' && cmd[3] == 'c' && cmd[4] == 'h' && cmd[5] == ' ') {
        terminal_parse_command((char*)cmd);
        return;
    }

    if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && cmd[4] == '\0') {
        terminal_write_line("Commands: help, ls, cd, cat, mkdir, rm, clear, shutdown, touch", 0x00ADB5);
    }
    else if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r' && cmd[5] == '\0') {
        for (int r = 0; r < VIRTUAL_HISTORY_MAX; r++) TerminalInstance.grid[r][0] = '\0';
        TerminalInstance.total_lines = 0; TerminalInstance.scroll_offset = 0;
        user_scrolled_back = 0;
    }
    else if (cmd[0] == 's' && cmd[1] == 'h' && cmd[2] == 'u' && cmd[3] == 't' && cmd[4] == 'd' && cmd[5] == 'o' && cmd[6] == 'w' && cmd[7] == 'n' && cmd[8] == '\0') {
        sys_shutdown();
    }
    else if (cmd[0] == 'l' && cmd[1] == 's' && cmd[2] == '\0') {
        if (current_directory->child_count == 0) {
            terminal_write_line("[Empty Directory]", 0xAAAAAA);
        } else {
            char list_buffer[256] = {0}; int l_idx = 0;
            for (int i = 0; i < current_directory->child_count; i++) {
                VFS_Node_t* child = current_directory->children[i]; int c_idx = 0;
                while (child->name[c_idx] != '\0' && l_idx < 230) list_buffer[l_idx++] = child->name[c_idx++];
                if (child->type == FS_TYPE_DIRECTORY) list_buffer[l_idx++] = '/';
                list_buffer[l_idx++] = ' '; list_buffer[l_idx++] = ' ';
            }
            list_buffer[l_idx] = '\0'; terminal_write_line(list_buffer, 0x00ADB5);
        }
    }
    else if (cmd[0] == 'c' && cmd[1] == 'd') {
        if (cmd[2] == '.' && cmd[3] == '.') {
            current_directory = current_directory->parent;
            vfs_set_current_dir(current_directory);
        } else if (cmd[2] == ' ') {
            const char* target_name = &cmd[3];
            VFS_Node_t* child = vfs_find_child(current_directory, target_name);
            if (child != 0 && child->type == FS_TYPE_DIRECTORY) {
                current_directory = child; vfs_set_current_dir(current_directory);
            } else {
                terminal_write_line("cd: no such directory.", 0xD63031);
             }
        }        
    }
    else if (cmd[0] == 'm' && cmd[1] == 'k' && cmd[2] == 'd' && cmd[3] == 'i' && cmd[4] == 'r' && cmd[5] == ' ') {
        if (vfs_create_directory(current_directory, &cmd[6]) != 0) terminal_write_line("Directory created.", 0x00FF00);
        else terminal_write_line("Error creating directory.", 0xD63031);
    }
    else if (cmd[0] == 'r' && cmd[1] == 'm' && cmd[2] == ' ') {
        if (vfs_delete_node(current_directory, &cmd[3])) terminal_write_line("Node deleted.", 0x00FF00);
        else terminal_write_line("Error: Node not found.", 0xD63031);
    }
    else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == ' ') {
        const char* target_name = &cmd[4];
        VFS_Node_t* child = vfs_find_child(current_directory, target_name);
        
        if (child != 0 && child->type == FS_TYPE_FILE) {
            static uint8_t file_read_buffer[4096];
            
            if (fat32_read_file(target_name, file_read_buffer)) {
                file_read_buffer[2047] = '\0'; 
                terminal_write_line((const char*)file_read_buffer, 0xEEEEEE);
            } else {
                if (child->content != 0) terminal_write_line(child->content, 0xEEEEEE);
                else terminal_write_line("[ERROR] Physical disk read failed.", 0xD63031);
            }
        } else {
            terminal_write_line("cat: no such file.", 0xD63031);
        }
    }
    else {
        char err_msg[128] = "oba: command not found: "; int idx = 24;
        for (int i = 0; cmd[i] != '\0' && idx < 120; i++) err_msg[idx++] = cmd[i];
        err_msg[idx] = '\0'; terminal_write_line(err_msg, 0xD63031);
    }
}

void terminal_handle_char(char c) {
    int active_history_count = (TerminalInstance.cmd_history_count > COMMAND_HISTORY_MAX) ? COMMAND_HISTORY_MAX : TerminalInstance.cmd_history_count;
    if ((unsigned char)c == 0xE0) { 
        if (active_history_count > 0) {
            if (TerminalInstance.cmd_history_select == -1) TerminalInstance.cmd_history_select = TerminalInstance.cmd_history_count - 1;
            else if (TerminalInstance.cmd_history_select > (TerminalInstance.cmd_history_count - active_history_count)) TerminalInstance.cmd_history_select--;
            term_strcpy(TerminalInstance.input_buffer, TerminalInstance.cmd_history[TerminalInstance.cmd_history_select % COMMAND_HISTORY_MAX]);
            int len = 0; while (TerminalInstance.input_buffer[len] != '\0') len++; TerminalInstance.input_idx = len;
        }
        return;
    }
    if ((unsigned char)c == 0xE1) { 
        if (TerminalInstance.cmd_history_select != -1) {
            if (TerminalInstance.cmd_history_select < TerminalInstance.cmd_history_count - 1) {
                TerminalInstance.cmd_history_select++;
                term_strcpy(TerminalInstance.input_buffer, TerminalInstance.cmd_history[TerminalInstance.cmd_history_select % COMMAND_HISTORY_MAX]);
            } else {
                TerminalInstance.cmd_history_select = -1; TerminalInstance.input_buffer[0] = '\0';
            }
            int len = 0; while (TerminalInstance.input_buffer[len] != '\0') len++; TerminalInstance.input_idx = len;
        }
        return;
    }
    if (c == '\n') {
        if (current_directory == 0) { current_directory = vfs_get_root(); }
        char echo_line[128] = "manet@OBA:"; int idx = 10; int d_idx = 0;
        while(current_directory->name[d_idx] != '\0' && idx < 40) echo_line[idx++] = current_directory->name[d_idx++];
        echo_line[idx++] = '$'; echo_line[idx++] = ' ';
        for (int i = 0; i < TerminalInstance.input_idx; i++) echo_line[idx++] = TerminalInstance.input_buffer[i];
        echo_line[idx] = '\0'; terminal_write_line(echo_line, 0x00FF00);
        
        terminal_push_history(TerminalInstance.input_buffer);
        terminal_execute(TerminalInstance.input_buffer);
        TerminalInstance.input_idx = 0; TerminalInstance.input_buffer[0] = '\0'; TerminalInstance.cmd_history_select = -1; 
        
        /* Komut işletildiğinde ekranı tekrar canlı takibe al */
        user_scrolled_back = 0;
    } 
    else if (c == '\b') {
        if (TerminalInstance.input_idx > 0) { TerminalInstance.input_idx--; TerminalInstance.input_buffer[TerminalInstance.input_idx] = '\0'; }
    } 
    else if (TerminalInstance.input_idx < 35 && c >= ' ' && c <= '~') {
        TerminalInstance.input_buffer[TerminalInstance.input_idx++] = c; TerminalInstance.input_buffer[TerminalInstance.input_idx] = '\0';
    }
}

/* ✨ TİTREMESİZ, AKICI VE İZOLASYONLU RENDER MOTORU ✨ */
void terminal_render(int win_x, int win_y, int win_w, int win_h) {
    if (current_directory == 0) { current_directory = vfs_get_root(); }
    
    int start_y = win_y + 45;
    int bar_x = win_x + win_w - 20; 
    int bar_y = start_y;
    int bar_w = 12;
    int bar_h = win_h - 60; 

    draw_rect(bar_x, bar_y, bar_w, bar_h, 0x1F2421);

    int dynamic_term_rows = bar_h / 16;
    if (dynamic_term_rows < 3) dynamic_term_rows = 3;

    int max_chars_per_line = (win_w - 35) / 8;
    if (max_chars_per_line < 10) max_chars_per_line = 10;
    if (max_chars_per_line > TERM_COLS - 1) max_chars_per_line = TERM_COLS - 1;

    int max_history_display_rows = dynamic_term_rows - 1;

    /* SARMAL SATIR ÖLÇÜM MATRİSİ */
    int total_wrapped_rows = 0;
    for (int i = 0; i < TerminalInstance.total_lines; i++) {
        char* line_ptr = TerminalInstance.grid[i];
        int len = 0; while(line_ptr[len] != '\0') len++;
        if (len == 0) { total_wrapped_rows++; continue; }
        int char_idx = 0;
        while (char_idx < len) {
            int chunk_len = 0;
            while (line_ptr[char_idx] != '\0' && chunk_len < max_chars_per_line) { char_idx++; chunk_len++; }
            total_wrapped_rows++;
        }
    }

    int max_offset = (total_wrapped_rows > max_history_display_rows) ? (total_wrapped_rows - max_history_display_rows) : 0;
    
    /* ✨ TİTREME ENGELLEYİCİ JITTER KONTROLÜ ✨
       Eğer kullanıcı geçmişe bakmıyorsa (fare/scrollbar ile yukarı gitmediyse) otomatik en alta odakla */
    if (!TerminalInstance.is_scrollbar_dragging && !user_scrolled_back) {
        TerminalInstance.scroll_offset = max_offset;
    }

    /* Etkileşim durumunda sınır koruması */
    if (TerminalInstance.scroll_offset < 0) TerminalInstance.scroll_offset = 0;
    if (TerminalInstance.scroll_offset > max_offset) TerminalInstance.scroll_offset = max_offset;

    int thumb_h = 30;
    int max_travel = bar_h - thumb_h;
    int thumb_y = bar_y;
    if (max_offset > 0) {
        thumb_y += (TerminalInstance.scroll_offset * max_travel) / max_offset;
    }

    /* Fare ve Scroll Etkileşim Yönetimi */
    int mx = cursor_pos_x; int my = cursor_pos_y;
    if (mouse_left_button) {
        if (!TerminalInstance.is_scrollbar_dragging && !was_channel_clicked) {
            if (mx >= bar_x && mx <= bar_x + bar_w && my >= thumb_y && my <= thumb_y + thumb_h) {
                TerminalInstance.is_scrollbar_dragging = 1;
                TerminalInstance.scrollbar_y_offset = my - thumb_y;
                user_scrolled_back = 1; // Kullanıcı serbest moda geçti
            }
            else if (mx >= bar_x && mx <= bar_x + bar_w && my >= bar_y && my <= bar_y + bar_h) {
                was_channel_clicked = 1;
                user_scrolled_back = 1;
                if (my < thumb_y) {
                    TerminalInstance.scroll_offset = (TerminalInstance.scroll_offset >= dynamic_term_rows) ? TerminalInstance.scroll_offset - dynamic_term_rows : 0;
                } else if (my > thumb_y + thumb_h) {
                    TerminalInstance.scroll_offset = (TerminalInstance.scroll_offset + dynamic_term_rows <= max_offset) ? TerminalInstance.scroll_offset + dynamic_term_rows : max_offset;
                }
            }
        } else if (TerminalInstance.is_scrollbar_dragging) {
            int new_thumb_y = my - TerminalInstance.scrollbar_y_offset;
            if (new_thumb_y < bar_y) new_thumb_y = bar_y;
            if (new_thumb_y > bar_y + max_travel) new_thumb_y = bar_y + max_travel;
            if (max_offset > 0) {
                TerminalInstance.scroll_offset = ((new_thumb_y - bar_y) * max_offset) / max_travel;
            }
        }
    } else { TerminalInstance.is_scrollbar_dragging = 0; was_channel_clicked = 0; }

    /* ✨ FARE TEKERLEĞİ (MOUSE SCROLL) KİLİDİNİN ÇÖZÜLMESİ ✨ */
    if (mx >= win_x && mx <= win_x + win_w && my >= win_y && my <= win_y + win_h) {
        if (mouse_scroll_direction != 0 && max_offset > 0) {
            user_scrolled_back = 1; // 1. Tekerlek döndüğü an serbest modu kilitle!
            
            if (mouse_scroll_direction == 1 && TerminalInstance.scroll_offset > 0) { 
                TerminalInstance.scroll_offset--; 
            }
            else if (mouse_scroll_direction == -1 && TerminalInstance.scroll_offset < max_offset) { 
                TerminalInstance.scroll_offset++; 
                /* Eğer kullanıcı tekerlekle tekrar en alta indiyse, otomatik takibe geri dön */
                if (TerminalInstance.scroll_offset >= max_offset) {
                    user_scrolled_back = 0;
                }
            }
            mouse_scroll_direction = 0;
        }
    }

    draw_rect(bar_x + 1, thumb_y, bar_w - 2, thumb_h, 0x00ADB5);

    /* GERÇEK METİN ÇİZİM ALGORİTMASI */
    int current_wrapped_row_index = 0;
    int rendered_rows = 0;

    for (int r = 0; r < TerminalInstance.total_lines && rendered_rows < max_history_display_rows; r++) {
        char* line_ptr = TerminalInstance.grid[r];
        int len = 0; while(line_ptr[len] != '\0') len++;

        if (len == 0) {
            if (current_wrapped_row_index >= TerminalInstance.scroll_offset) { rendered_rows++; }
            current_wrapped_row_index++;
            continue;
        }

        int char_idx = 0;
        while (char_idx < len && rendered_rows < max_history_display_rows) {
            char chunk[128];
            int chunk_len = 0;
            while (line_ptr[char_idx] != '\0' && chunk_len < max_chars_per_line) { chunk[chunk_len++] = line_ptr[char_idx++]; }
            chunk[chunk_len] = '\0';

            if (current_wrapped_row_index >= TerminalInstance.scroll_offset) {
                draw_string(win_x + 15, start_y + (rendered_rows * 16), chunk, TerminalInstance.colors[r]);
                rendered_rows++;
            }
            current_wrapped_row_index++;
        }
    }

    /* Girdi İstemi (Prompt) Sabit Çizimi */
    char prompt_buffer[128] = "manet@OBA:";
    int p_idx = 10; int d_idx = 0;
    while(current_directory->name[d_idx] != '\0' && p_idx < 40) { prompt_buffer[p_idx++] = current_directory->name[d_idx++]; }
    prompt_buffer[p_idx++] = '$'; prompt_buffer[p_idx++] = ' ';
    for (int i = 0; i < TerminalInstance.input_idx; i++) { prompt_buffer[p_idx++] = TerminalInstance.input_buffer[i]; }
    if ((timer_ticks / 50) % 2 == 0) { prompt_buffer[p_idx++] = '_'; }
    else { prompt_buffer[p_idx++] = ' '; }
    prompt_buffer[p_idx] = '\0';

    int final_prompt_row = rendered_rows;
    if (final_prompt_row >= dynamic_term_rows) final_prompt_row = dynamic_term_rows - 1;

    char* p_ptr = prompt_buffer;
    int p_char_idx = 0;
    while (p_ptr[p_char_idx] != '\0' && final_prompt_row < dynamic_term_rows) {
        char p_chunk[128]; int p_chunk_len = 0;
        while (p_ptr[p_char_idx] != '\0' && p_chunk_len < max_chars_per_line) { p_chunk[p_chunk_len++] = p_ptr[p_char_idx++]; }
        p_chunk[p_chunk_len] = '\0';
        draw_string(win_x + 15, start_y + (final_prompt_row * 16), p_chunk, 0x00FF00);
        final_prompt_row++;
    }
}

void terminal_parse_command(char* cmd) {
    if (cmd[0] == 't' && cmd[1] == 'o' && cmd[2] == 'u' && cmd[3] == 'c' && cmd[4] == 'h') {
        char* filename = &cmd[6]; 
        uint32_t cluster = fat32_allocate_cluster();
        if (cluster != 0) {
            if (fat32_create_dir_entry(filename, "TXT", cluster, 12)) {
                const char* initial_data = "EMPTY_FILE  ";
                if (fat32_write_file(filename, (const uint8_t*)initial_data, 12)) {
                    vfs_create_file(current_directory, filename, initial_data); 
                    terminal_write_line("[SHELL] File created and synchronized with VFS.", 0x00FF00);
                } else {
                    terminal_write_line("[SHELL] Error: Failed to write initial data.", 0xD63031);
                }
            } else {
                terminal_write_line("[SHELL] Error: Dir entry creation failed.", 0xD63031);
            }
        } else {
            terminal_write_line("[SHELL] Error: No free cluster on disk.", 0xD63031);
        }
    }
}