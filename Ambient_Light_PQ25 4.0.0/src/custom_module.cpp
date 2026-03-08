#include "custom_module.h"
#include "action_led.h"

namespace custom_module
{
  bool door_states;
  uint8 light_states;
  bool team_to_sleep;
  bool team_to_sleep_was;
  bool door_message_flag;
  bool light_message_flag;

  void setup()
  {
    door_states = false;
    light_states = RED_BRIGH_OFF;
    team_to_sleep = false;
    door_message_flag = false;
    light_message_flag = false;
    action_led::setup();
  }

  void loop()
  {
    action_led::loop();
  }

  // -------Пользовательские сигналы-------
  void custom_signals(const LinFrame &frame)
  {
    const uint8 id = frame.get_byte(0);

    if (id == SERVICE_ID)
    {
      if (frame.num_bytes() == (SUM_8_BITS))
      {
        if (frame.get_byte(1) == 0x00)
        {
          team_to_sleep = true;
        }
      }
      return;
    }

    // id где хранятся данные о положении дверей
    if (id == DOOR_ID)
    {
      if (frame.num_bytes() == (SUM_6_BITS))
      {
        door_message_flag = true;
#if (PLAFON == 1)
        door_states = frame.get_byte(2) & H(3); // 0x08 |	0 0 0 0 1 0 0 0
#elif (PLAFON == 2)
        door_states = frame.get_byte(2) & H(7); // 0x80	| 1 0 0 0 0 0 0 0
#elif (PLAFON == 3)
        door_states = frame.get_byte(3) & H(3); // 0x08 |	0 0 0 0 1 0 0 0
#elif (PLAFON == 4)
        door_states = frame.get_byte(3) & H(7); // 0x80	| 1 0 0 0 0 0 0 0
#else
#error ВЫБЕРИ ВЕРНЫЙ НОМЕР ПЛАФОНА  | 1,2,3 или 4!!!
#endif
      }
      return;
    }

    // id где хранятся данные о положении включателя света
    /*0x14 - минимальная яркость 0x5a - максимальная*/
    if (id == LIGHT_ID)
    {
      if (frame.num_bytes() == (SUM_4_BITS))
      {
        light_message_flag = true;
        const uint8 light_Brightness = frame.get_byte(1);
        if (light_Brightness > DATA_LIN_MIN_BRIGHT)
        {
          light_states = map(light_Brightness, DATA_LIN_MIN_BRIGHT + 1, DATA_LIN_MAX_BRIGHT, RED_BRIGH_MIN, RED_BRIGH_MAX);
        }
        else
        {
          light_states = RED_BRIGH_OFF;
        }
        light_states = constrain(light_states, RED_BRIGH_OFF, RED_BRIGH_MAX);
      }
    }
  }

  void frameArrived(const LinFrame &frame)
  {
    custom_signals(frame);
    if (door_message_flag == false || light_message_flag == false)
    {
      return;
    }
    action_led::action_states(door_states, light_states);
  }

  void no_frameArrived()
  {
    door_states = false;
    light_states = RED_BRIGH_OFF;
    action_led::action_states(door_states, light_states);
  }

}