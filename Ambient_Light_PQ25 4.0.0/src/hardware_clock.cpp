#include "hardware_clock.h"

#include <arduino.h>
#include "avr_util.h"

namespace hardware_clock
{

  void setup()
  {
    // Нормальный режим (свободная работа [0, ffff]).
    TCCR1A = L(COM1A1) | L(COM1A0) | L(COM1B1) | L(COM1B0) | L(WGM11) | L(WGM10);
    // Предварительный делитель: X64 (250 тактов в мс при 16 МГц). 2 ^ 16 тактовых циклов каждые ~ 260 мс.
    // Это также определяет максимальный интервал update(), чтобы не пропустить переполнение счетчика.
    TCCR1B = L(ICNC1) | L(ICES1) | L(WGM13) | L(WGM12) | L(CS12) | H(CS11) | H(CS10);
    // Очистить счетчик.
    TCNT1 = 0;
    // Сравните A. Не используется.
    OCR1A = 0;
    // Сравните B. Используется для вывода тактовых импульсов для отладки.
    OCR1B = 0;
    // Запрещенные прерывания.
    TIMSK1 = L(ICIE1) | L(OCIE1B) | L(OCIE1A) | L(TOIE1);
    TIFR1 = L(ICF1) | L(OCF1B) | L(OCF1A) | L(TOV1);
  }

} // пространство имен hardware_clock