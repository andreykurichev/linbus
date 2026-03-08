#pragma once
#include <arduino.h>
#include "custom_module.h"
#include "action_led.h"
#include "avr_util.h"
#include <avr/sleep.h>

namespace sleep_set
{
  extern void go_sleep()
  {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Устанавливаем режим сна
    cli();                                // Временно запрещаем обработку прерываний
    custom_module::team_to_sleep = false; // Сбрасываем флаг команды ко сну
    PORTD &= ~H(PWR_RGB_PIN);             // Выключаем питание RGB
    PORTD &= ~H(SLEEP_PIN);               // Выключаем лин трансивер
    sleep_enable();                       // Разрешаем сон для МК
    EICRA |= H(ISC11);                    // Прерывания восходящий фронт на ножке INT0
    EIMSK |= H(INT0);                     // Разрешить внешние прерывания INT0
    sei();                                // Разрешаем обработку прерываний
    sleep_cpu();                          // Переводим МК в спящий режим
                                          // Когда проснулись
    PORTD |= H(SLEEP_PIN);                // Запускаем лин трансивер
    PORTD |= H(PWR_RGB_PIN);              // Подаем питание на RGB
  }

  ISR(INT0_vect)
  {
    EIMSK &= ~H(INT0); // Запрещяем внешние прерывания INT0
    sleep_disable();   // Запрещяем сон для МК
  };
}