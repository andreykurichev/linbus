#include "settings.h"
#include "avr_util.h"
#include "custom_module.h"
#include "hardware_clock.h"
#include "lin_processor.h"
#include "system_clock.h"
#include "passive_timer.h"
#include "action_led.h"
#include "sleep_set.h"

// Функция настройки Arduino. Вызывается один раз во время инициализации.
void setup()
{
  // Использует Timer1, без прерываний.
  hardware_clock::setup();
  // Использует Timer2 с прерываниями и несколькими контактами ввода-вывода. Подробности смотрите в исходном коде.
  lin_processor::setup();
  custom_module::setup();
  // Включить глобальные прерывания. Мы ожидаем, что будут прерывания только от таймера 1
  // процессор lin для уменьшения джиттера ISR.
  sei();
}

// Метод Arduino loop(). Вызывается после установки(). Никогда не возвращается.
// Это быстрый цикл, который не использует delay() или другие занятые циклы или
// блокировка
void loop()
{
  for (;;)
  {
    // Периодические обновления.
    system_clock::loop();
    custom_module::loop();

    // Переход в режим сна если шина спит или пришла команда о переходе.
    static PassiveTimer sleep_timer;
    static bool bus_is_inactive;

    uint32 sleep_time = (custom_module::door_states == false) ? multiplication(SHORT_BEFORE_SLEEP, 1000) : multiplication(LONG_BEFORE_SLEEP, 60000);
    if (sleep_timer.timeMillis() >= sleep_time || custom_module::team_to_sleep == true)
    {
      bus_is_inactive = true;
      custom_module::door_message_flag = false;
      custom_module::light_message_flag = false;
      custom_module::no_frameArrived();
      sleep_timer.restart();
    }
    if (bus_is_inactive == true && action_led::White_Brigh == WHITE_BRIGH_OFF && action_led::Red_Brigh == RED_BRIGH_OFF)
    {
      sleep_set::go_sleep();
      bus_is_inactive = false;
    }

    lin_processor::getAndClearErrorFlags();

    // Обработка полученных кадров LIN.
    LinFrame frame;
    if (lin_processor::readNextFrame(&frame) && custom_module::team_to_sleep == false)
    {
      const boolean frameOk = frame.isValid();

      if (frameOk)
      {
        // Сообщаем пользовательской логике о входящем кадре.
        custom_module::frameArrived(frame);
      }
      sleep_timer.restart();
    }
  }
}