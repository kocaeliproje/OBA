/* include/gui.h */
#ifndef GUI_H
#define GUI_H


typedef struct {
    int x, y;
    int width, height;
    char* title;
    int is_visible;
    int is_minimized;  // Simge durumunda mı?
    int is_active;     // Odaklanmış aktif pencere mi?
    int window_type;   // 0: Sistem, 1: Klasör/Dosya, 2: Uygulama
} Window;

typedef struct WindowSystem {
    unsigned char *back_buffer; // Sabit dizi yerine sadece bir pointer (Çekirdeği hafifletir)
    Window windows[5];
    int window_count;
    
    // OOP Metotları
    void (*init)(struct WindowSystem* self);
    void (*create_window)(struct WindowSystem* self, char* title, int x, int y, int w, int h, int type);
    void (*refresh)(struct WindowSystem* self);
} WindowSystem_t;

extern WindowSystem_t GuiManager;

#endif