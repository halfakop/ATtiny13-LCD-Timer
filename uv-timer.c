#include <avr/io.h>
#include "eertos.h"

#define BITS                8 // количество бит в байте :)
#define HALFBYTE            4
#define DIGITS              3 // количество отображаемых символов
#define BIT_DATA            PB0
#define BIT_CLOCK           PB1
#define BIT_LATCH           PB2
#define BIT_RELAY           PB5
#define BTN_MODE_PRESSED    bit_is_clear(PINB, PB3)
#define BTN_SET_PRESSED     bit_is_clear(PINB, PB4)
#define DEBOUNCING          4
#define FREQ_BTN            1
#define FREQ_LCD            1
#define FREQ_TIMER          50


// возможные состояния кнопок: не нажата, нажата MODE, нажата SET
typedef enum {CLEAR, MODE, SET} BUTTON;
// возможные состояния приложения, см. главный цикл
typedef enum {M_LOW, S_HIGH, S_LOW, IDLE, READY, RUN, PAUSE} STATE;

// вспомогательная структура для настройки стартового значения таймера
typedef struct { 
  uint8_t multiplier; 
  uint8_t limit; 
} PAIR;

// список настроек для состояний M_LOW, S_HIGH и S_LOW
const PAIR g_data[] = {{60, 9}, {10, 5}, {1, 9}};
// шрифт для 7-ми сегментного индикатора: 0123456789
const uint8_t digits[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 
                          0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// глобальные переменные
volatile BUTTON g_button = CLEAR;   // инфа о нажатой кнопке
volatile uint16_t g_seconds = 0;    // счётчик таймера
volatile STATE g_state = IDLE;      // состояние приложения


// определения функций
void timer_setup(STATE new_state);
void task_display(void);
void task_buttons(void);
void task_timer(void);


// Задача на вывод информации на дисплей и его периодическое обновление.
inline void task_display(void) {
  static uint16_t last = 0;
  uint8_t min, sec, buf[DIGITS];

  min = g_seconds / 60;
  sec = g_seconds % 60;
  if (last != g_seconds) {
    buf[0] = digits[min&0x0F];
    buf[1] = digits[sec>>HALFBYTE];
    buf[2] = digits[sec&0x0F];
    // мигаем точкой ежесекундно
    if (g_seconds % 2) buf[1] | 0x80;
    // запоминаем
    last = g_seconds;

    // прячем ненужные цифры в режимах настройки
    switch (g_state) {
    case M_LOW:
      buf[2] = buf[3] = 0;
      break;
    case S_HIGH:
      buf[1] = buf[3] = 0;
      break;
    case S_LOW:
      buf[1] = buf[2] = 0;
      break;
    }
  }

  // инвертируем биты каждый раз из-за особенности работы ЖКИ
  buf[0] = ~buf[0];
  buf[1] = ~buf[1];
  buf[2] = ~buf[2];
  
  // выводим побитно софтовый буфер в буфер 4094
  for (uint8_t i=0; i<DIGITS; i++) {
    uint8_t byte = buf[i];
    for (uint8_t j=0; j<BITS; j++) {
      PORTB &= ~(1<<BIT_DATA);
      PORTB |= (byte>>(7-j)) & (1<<BIT_DATA);
      asm("nop");
      asm("nop");
      PORTB |= (1<<BIT_CLOCK);
      asm("nop");
      asm("nop");
      PORTB &= ~(1<<BIT_CLOCK);
    }
  }

  // применяем буфер, т.е. выводим данные на экран
  PORTB |= (1<<BIT_LATCH);
  asm("nop");
  asm("nop");
  PORTB &= ~(1<<BIT_LATCH);
}


// Задача на уменьшение значения таймера.
inline void task_timer(void) {
  if (0 < g_seconds) {
    g_seconds--;
  } else {
    g_state = IDLE;
  }
}


// Задача на опрос состояния кнопок, вызывается с частотой FREQ_BTN.
// Обеспечивает защиту от дребезга контактов. Информация о нажатой
// кнопке помещается в глобальную переменную g_button.
inline void task_buttons(void) {
  static uint8_t debounce_mode = 0;
  static uint8_t debounce_set = 0;

  if (BTN_MODE_PRESSED) {
    if (debounce_mode < DEBOUNCING) {
      debounce_mode++;
    } else {
      g_button = MODE;
      debounce_mode = 0;
    }
  }

  if (BTN_SET_PRESSED) {
    if (debounce_set < DEBOUNCING) {
      debounce_set++;
    } else {
      g_button = SET;
      debounce_set = 0;
    }
  }
}


// Функция для настройки стартового значения таймера.
void timer_setup(STATE state) {
  static uint8_t value = 0;

  PAIR *d = (PAIR *) &g_data[state];

  switch (g_button) {
  case MODE:
    g_seconds += value * d->multiplier;
    g_state = state;
    break;
  case SET:
    value++;
    if (d->limit < value) value = 0;
  default:
    break;
  }
}


// Точка входа в программу.
int __attribute__ ((OS_main)) main(void) {
  // настройка порта
  // PB5 - выход на управление нагрузкой
  // PB4 - вход с кнопки SET
  // PB3 - вход с кнопки MODE
  // PB2 - выход на LATCH
  // PB1 - выход на CLOCK
  // PB0 - выход на DATA
  DDRB = (1<<DDB5) | (0<<DDB4) |(0<<DDB3) | (1<<DDB2) | (1<<DDB1) | (1<<DDB0);

  // описываем задачи для RTOS
  task_assign(BUTTON_THREAD, task_buttons, FREQ_BTN);
  task_assign(DISPLAY_THREAD, task_display, FREQ_LCD);
  task_assign(TIMER_THREAD, task_timer, FREQ_TIMER);

  // запускаем выполнение задач
  rtos_run();

  while(1) {
    // выполняем обработку задач
    task_manager();

    // логика обработки кнопок
    switch (g_state) {
    case IDLE:
      if (MODE == g_button) {
        g_seconds = 0;
        g_state = M_LOW;
      }
      PORTB &= ~(1<<BIT_RELAY); // отключаем нагрузку
      break;
    case M_LOW:
      timer_setup(S_HIGH);
      break;
    case S_HIGH:
      timer_setup(S_LOW);
      break;
    case S_LOW:
      timer_setup(READY);
      break;
    case READY:
      if (SET == g_button) g_state = RUN;
      break;
    case RUN:
      if (MODE == g_button) g_state = IDLE;
      if (SET == g_button)  g_state = PAUSE;
      PORTB |= (1<<BIT_RELAY); // включаем нагрузку
      break;
    case PAUSE:
      if (MODE == g_button) g_state = IDLE;
      if (SET == g_button)  g_state = RUN;
      break;
    }
    g_button = CLEAR;
  }
  return 0;
}
