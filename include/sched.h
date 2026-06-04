#ifndef SCHED_H
#define SCHED_H

typedef struct {
    unsigned int esp, ebp, eip, page_dir;
    unsigned int sleep_ticks; // Uyuma süresi (Tick cinsinden)
    int state;                // 0: Boş, 1: Hazır, 2: Beklemede (Uyuyor)
} Task_t;

typedef struct Scheduler {
    Task_t tasks[3];
    int current_task;
    
    void (*init)(struct Scheduler* self);
    void (*create_task)(struct Scheduler* self, int index, void (*entry_point)());
    void (*schedule)(struct Scheduler* self, unsigned int* current_esp);
    void (*sleep)(struct Scheduler* self, unsigned int ms);
} Scheduler_t;

extern Scheduler_t Sched;

#endif