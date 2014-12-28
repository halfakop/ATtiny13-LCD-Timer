#include <avr/interrupt.h>
#include "eertos.h"

// очередь таймеров
volatile QITEM queue[QUEUE_SIZE];


// установка задачи по таймеру
void task_assign(TASKS type, TPTR task, uint16_t time) {
  QITEM *p = (QITEM *) &queue[type];
  // иначе ищем первый пустой слот и заполняем его
  p->task = task;
  p->time = time;
  p->init = time;
}


// диспетчер задач, проверяет задачи в очереди, если находит реальную
// задачу, у которой счётчик дошёл до нуля, то выполняет её
inline void task_manager(void) {
  cli();
  for (uint8_t i=0; i<QUEUE_SIZE; i++) {
    QITEM *p = (QITEM *) &queue[i];
    if (p->time == 0) {
      (p->task)();
      p->time = p->init;
    }
  }
  sei();
}


// обработчик прерывания по совпадению таймера, срабатывает с частотой в ~50Гц,
// реализует службу таймеров ядра, здесь уменьшаем счётчик каждого таймера
ISR(TIM0_COMPA_vect) {
  for (uint8_t i=0; i<QUEUE_SIZE; i++) {
    QITEM *p = (QITEM *) &queue[i];
    if (p->time > 0) {
      p->time--;
    }
  }
}


// запуск системного таймера
inline void rtos_run(void) {
  // предполагая F_CPU = 9600000/CKDIV8 = 1200000
  // используем делитель 64 (CS=011), 
  // OC0n отключены от ног,
  // режим CTC (WGM=010)
  TCCR0A = (1<<WGM01) | (0<<WGM00);
  TCCR0B = (0<<CS02) | (1<<CS01) | (1<<CS00) | (0 << WGM02);
  // f_cpu / (2.0 * prescaler * ticks) - 1 даёт 49.87Гц
  TCNT0 = 0;
  OCR0A = 187; 
  // настраиваем прерывание по сравнению для канала A
  TIMSK0 = (0<<OCIE0B) | (1<<OCIE0A) |  (0<<TOIE0);
  // запускаем прерывания
  sei();
}
