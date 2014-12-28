#ifndef EERTOS_H
#define EERTOS_H

#include <inttypes.h>

// указатель на выполняемую задачу
typedef void (*TPTR)(void);

typedef struct {
  TPTR task;
  uint16_t time;
  uint16_t init;
} QITEM;

// определяем типы задач
typedef enum {BUTTON_THREAD, DISPLAY_THREAD, TIMER_THREAD} TASKS;

// размер очереди таймеров
#define QUEUE_SIZE 3

void task_manager(void);

extern void task_assign(TASKS type, TPTR task, uint16_t value);

extern void rtos_init(void);
extern void rtos_run(void);

#endif
