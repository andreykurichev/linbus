#pragma once
#include <arduino.h>
#include "settings.h"
#include "avr_util.h"
#include "crt_gamma.h"

namespace action_led
{
  enum Possible_States
  {
    ALL_OFF,
    REPEAT_WRITE,
    REPEAT_RED,
    WRITE_ON,
    WRITE_OFF,
    RED_ON,
    RED_OFF,
    RED_FAST_UP,
    RED_FAST_DOWN,
    RED_FAST_OFF
  };

  extern uint8 White_Brigh;
  extern uint8 Red_Brigh;
  extern boolean Red_States_Flag;

  extern void setup();
  extern void loop();

  extern void action_states(boolean door_message, uint8 light_message);
  inline void action();
  void led_switch();
  byte GammaCorrection_White(byte val);
  byte GammaCorrection_Red(byte val);
}