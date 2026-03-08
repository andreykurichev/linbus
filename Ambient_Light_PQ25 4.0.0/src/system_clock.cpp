#include "system_clock.h"
#include <arduino.h>
#include "avr_util.h"
#include "hardware_clock.h"

namespace system_clock
{
  static const uint16 kTicksPerMilli = hardware_clock::kTicksPerMilli;
  static const uint16 kTicksPer10Millis = 10 * kTicksPerMilli;

  static uint16 accounted_ticks = 0;
  static uint32 time_millis = 0;

  void loop()
  {
    const uint16 current_ticks = hardware_clock::ticksForNonIsr();

    // Эта 16-битная беззнаковая арифметика хорошо работает и в случае переполнения таймера.
    // Предположим, что на каждый цикл таймера приходится не менее двух циклов.
    uint16 delta_ticks = current_ticks - accounted_ticks;

    // Цикл увеличения курса на случай, если у нас большой интервал обновления. Улучшает
    // время выполнения одного миллицикла обновления ниже.
    while (delta_ticks >= kTicksPer10Millis)
    {
      delta_ticks -= kTicksPer10Millis;
      accounted_ticks += kTicksPer10Millis;
      time_millis += 10;
    }

    // Цикл обновления одного миллисекунды.
    while (delta_ticks >= kTicksPerMilli)
    {
      delta_ticks -= kTicksPerMilli;
      accounted_ticks += kTicksPerMilli;
      time_millis++;
    }
  }

  uint32 timeMillis()
  {
    return time_millis;
  }

} // пространство имен system_clock
