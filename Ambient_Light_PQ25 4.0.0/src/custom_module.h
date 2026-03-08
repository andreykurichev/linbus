#pragma once
#include "avr_util.h"
#include "lin_frame.h"
#include "passive_timer.h"

// Реализовать функциональность, специфичную для модели автомобиля.
namespace custom_module
{
  extern boolean door_states;
  extern uint8 light_states_Bright;
  extern boolean team_to_sleep;
  extern boolean door_message_flag;
  extern boolean light_message_flag;

  // Вызывается один раз при инициализации.
  extern void setup();

  // Вызывается один раз при каждой итерации основного цикла Arduino().
  extern void loop();

  // Вызывается один раз при получении нового допустимого кадра.
  extern void frameArrived(const LinFrame &frame);
  // Вызывается один раз при отсутствии нового допустимого кадра.
  extern void no_frameArrived();
  // Вызывается для проверки состояния.
  extern void custom_signals(const LinFrame &frame);

} // пространство имен custom_module
