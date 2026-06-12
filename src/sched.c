#include "sched.h"
#include "kernel.h"

extern void* kmalloc_aligned(unsigned int size, int align);
extern volatile unsigned int timer_ticks;

static void sched_init(Scheduler_t* self) {
    self->current_task = -1;
    for(int i = 0; i < 3; i++) self->tasks[i].state = 0;
}

/* src/sched.c - Sadece sched_create_task fonksiyonunu bu kararlı sürümle değiştirin */
static void sched_create_task(Scheduler_t* self, int index, void (*entry_point)()) {
    Task_t* t = &self->tasks[index];
    unsigned int* stack = (unsigned int*) kmalloc_aligned(4096, 1); 
    unsigned int* stack_top = stack + 1024;

    /* --- REGS_T VE BOOT.S IRET ÇERÇEVESİ KUSURSUZ SİMETRİSİ --- */
    stack_top--; *stack_top = 0x0202;                     // EFLAGS (Kesmeleri açar, Bit 9 = 1)
    stack_top--; *stack_top = 0x08;                       // CS (Kernel Code)
    stack_top--; *stack_top = (unsigned int)entry_point;  // EIP (Görevin adresi)
    
    stack_top--; *stack_top = 32;                         // int_no (IRQ0)
    stack_top--; *stack_top = 0;                          // err_code

    // pusha Alanı (EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX)
    stack_top--; *stack_top = 0;                          // edi
    stack_top--; *stack_top = 0;                          // esi
    stack_top--; *stack_top = 0;                          // ebp
    stack_top--; *stack_top = 0;                          // esp_dummy
    stack_top--; *stack_top = 0;                          // ebx
    stack_top--; *stack_top = 0;                          // edx
    stack_top--; *stack_top = 0;                          // ecx
    stack_top--; *stack_top = 0;                          // eax

    // boot.s pop sırasına göre segment mühürleri
    stack_top--; *stack_top = 0x10;                       // ds
    stack_top--; *stack_top = 0x10;                       // es
    stack_top--; *stack_top = 0x10;                       // fs
    stack_top--; *stack_top = 0x10;                       // gs

    t->esp = (unsigned int)stack_top;
    t->state = 1; // TASK_READY
}

static void sched_sleep(Scheduler_t* self, unsigned int ms) {
    if(self->current_task == -1) return;
    // Ticks hesapla (PIT 100Hz ayarlandığı için her tick 10ms'dir)
    self->tasks[self->current_task].sleep_ticks = ms / 10;
    self->tasks[self->current_task].state = 2; // TASK_SLEEPING
    
    // Başka göreve geçilmesi için işlemciyi boşa çıkar (Zamanlayıcı kesmesini bekle veya tetikle)
    asm volatile("int $0x20"); 
}

static void sched_schedule(Scheduler_t* self, unsigned int* current_esp) {
    int ready_count = 0;
    for(int i = 0; i < 3; i++) {
        if(self->tasks[i].state == 1) ready_count++; // TASK_READY
    }
    if(ready_count == 0) return; // Hazır görev yoksa zamanlamayı es geç

    // Önce uyuyan görevlerin sürelerini güncelle
    for(int i = 0; i < 3; i++) {
        if(self->tasks[i].state == 2) {
            if(self->tasks[i].sleep_ticks > 0) {
                self->tasks[i].sleep_ticks--;
            } else {
                self->tasks[i].state = 1; // Süre bitti, tekrar hazır
            }
        }
    }

    if(self->current_task != -1) {
        self->tasks[self->current_task].esp = *current_esp;
    }

    // Sıradaki hazır görevi bul (Round Robin)
    int next_task = self->current_task;
    while(1) {
        next_task = (next_task + 1) % 3;
        if(self->tasks[next_task].state == 1) break; 
        if(next_task == self->current_task && self->tasks[next_task].state != 1) {
            // Eğer hiçbir görev hazır değilse ana döngüye veya boş göreve dön
            return;
        }
    }

    self->current_task = next_task;
    *current_esp = self->tasks[self->current_task].esp;
}

Scheduler_t Sched = {
    .current_task = -1,
    .init = sched_init,
    .create_task = sched_create_task,
    .schedule = sched_schedule,
    .sleep = sched_sleep
};