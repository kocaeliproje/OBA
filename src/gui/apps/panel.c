/* * src/gui/apps/panel.c - OBA-32 Kontrol Paneli Uygulama Modülü
 */

#include "gui/gui.h"
#include "kernel.h"
#include <stdint.h>

extern volatile unsigned int timer_ticks;
extern uint32_t gui_desktop_background_color;

extern void draw_rect(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_rect_outline(int start_x, int start_y, int w, int h, uint32_t color);
extern void draw_string(int start_x, int start_y, const char* str, uint32_t color);

void panel_render(Window_t* self) {
    draw_string(self->x + 15, self->y + 42, "OBA CONTROL CENTER v1.1", 0x00ADB5);
    draw_rect(self->x + 15, self->y + 56, self->width - 30, 1, 0xDDDDDD);

    int group_w = self->width - 30;

    int g1_y = self->y + 68;
    draw_rect_outline(self->x + 15, g1_y, group_w, 60, 0x393E46); 
    draw_rect(self->x + 25, g1_y - 4, 85, 10, 0xEEEEEE);           
    draw_string(self->x + 28, g1_y - 4, "[ MONITORING ]", 0x00ADB5);

    unsigned int total_seconds = timer_ticks / 100;
    unsigned int hours = total_seconds / 3600;
    unsigned int minutes = (total_seconds % 3600) / 60;
    unsigned int seconds = total_seconds % 60;
    char uptime_str[32] = "Uptime: 00:00:00";
    uptime_str[8]  = '0' + (hours / 10);   uptime_str[9]  = '0' + (hours % 10);
    uptime_str[11] = '0' + (minutes / 10); uptime_str[12] = '0' + (minutes % 10);
    uptime_str[14] = '0' + (seconds / 10); uptime_str[15] = '0' + (seconds % 10);
    draw_string(self->x + 25, g1_y + 12, uptime_str, 0x222831);

    draw_string(self->x + 25, g1_y + 32, "RAM: 4.2MB/32MB", 0x555555);
    int bar_start_x = self->x + 145;
    int bar_max_w = (self->x + self->width - 25) - bar_start_x;
    if (bar_max_w > 15) {
        draw_rect(bar_start_x, g1_y + 32, bar_max_w, 12, 0xDDDDDD);
        draw_rect(bar_start_x, g1_y + 32, (bar_max_w * 13) / 100, 12, 0x27AE60); 
    }

    int g2_y = self->y + 138;
    draw_rect_outline(self->x + 15, g2_y, group_w, 52, 0x393E46);
    draw_rect(self->x + 25, g2_y - 4, 98, 10, 0xEEEEEE);
    draw_string(self->x + 28, g2_y - 4, "[ TASK MANAGER ]", 0x00ADB5);

    draw_string(self->x + 25, g2_y + 12, "o PID 0: IDLE TASK    [Active]", 0x27AE60);
    draw_string(self->x + 25, g2_y + 28, "o PID 1: DESKTOP GUI  [Active]", 0x27AE60);

    int g3_y = self->y + 200;
    draw_rect_outline(self->x + 15, g3_y, group_w, 55, 0x393E46);
    draw_rect(self->x + 25, g3_y - 4, 135, 10, 0xEEEEEE);
    draw_string(self->x + 28, g3_y - 4, "[ DESKTOP BACKGROUND ]", 0x00ADB5);

    int b1_x = self->x + 20;
    int b2_x = self->x + 105;
    int b3_x = self->x + 190;

    draw_rect(b1_x, g3_y + 18, 75, 22, 0x1A1B20);
    draw_rect_outline(b1_x, g3_y + 18, 75, 22, 0x00ADB5);
    draw_string(b1_x + 21, g3_y + 23, "DARK", 0xFFFFFF);

    draw_rect(b2_x, g3_y + 18, 75, 22, 0x7F8C8D);
    draw_rect_outline(b2_x, g3_y + 18, 75, 22, 0xFFFFFF);
    draw_string(b2_x + 17, g3_y + 23, "LIGHT", 0xFFFFFF);

    draw_rect(b3_x, g3_y + 18, 85, 22, 0x0F4C5C);
    draw_rect_outline(b3_x, g3_y + 18, 85, 22, 0x00ADB5);
    draw_string(b3_x + 22, g3_y + 23, "OCEAN", 0xFFFFFF);
}

void panel_handle_click(Window_t* self, int local_mx, int local_my) {
    /* Unused parameter uyarısını engellemek amacıyla mühürlenmiştir */
    (void)self; 
    
    int g3_y = 200; 
    int b1_x = 20;
    int b2_x = 105;
    int b3_x = 190;

    if (local_my >= g3_y + 18 && local_my <= g3_y + 18 + 22) {
        if (local_mx >= b1_x && local_mx <= b1_x + 75) {
            gui_desktop_background_color = 0x1A1B20; 
        } else if (local_mx >= b2_x && local_mx <= b2_x + 75) {
            gui_desktop_background_color = 0x7F8C8D; 
        } else if (local_mx >= b3_x && local_mx <= b3_x + 85) {
            gui_desktop_background_color = 0x0F4C5C; 
        }
    }
}