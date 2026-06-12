/* include/terminal.h */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

#define TERM_ROWS 12
#define TERM_COLS 128           /* ✨ 50'den 128'e yükseltildi: Geniş pencereleri desteklemek için */
#define VIRTUAL_HISTORY_MAX 100 
#define COMMAND_HISTORY_MAX 10  

typedef struct {
    char grid[VIRTUAL_HISTORY_MAX][TERM_COLS];
    uint32_t colors[VIRTUAL_HISTORY_MAX];
    int total_lines;        
    int scroll_offset;      
    
    char input_buffer[64];
    int input_idx;

    int is_scrollbar_dragging;
    int scrollbar_y_offset;

    char cmd_history[COMMAND_HISTORY_MAX][64];
    int cmd_history_count;   
    int cmd_history_select;  
} Terminal_t;

void terminal_init(void);
void terminal_write_line(const char* text, uint32_t color);
void terminal_handle_char(char c);
void terminal_render(int win_x, int win_y, int win_w, int win_h);

#endif