#include "action_led.h"
#include "custom_module.h"
#include "avr_util.h"
#include "passive_timer.h"
#include <microLED.h>
#include <avr/power.h>

namespace action_led
{
  microLED<1, RGB_PIN, MLED_NO_CLOCK, LED_WS2812, ORDER_GRB, CLI_AVER> rgb_leds;
  Possible_States current_state;

  uint8 White_Brigh;
  uint8 Red_Brigh;
  uint8 light_Brigh;

  boolean Red_States_Flag;
  boolean door_states;
  boolean repeat_write_flag;
  boolean repeat_red_flag;

  void setup()
  {
    DDRD |= H(PWR_RGB_PIN);
    PORTD |= H(PWR_RGB_PIN);
    DDRD |= H(WHITE_LED_PIN);
    PORTD &= ~H(WHITE_LED_PIN);
    White_Brigh = WHITE_BRIGH_OFF;
    Red_Brigh = RED_BRIGH_OFF;
    light_Brigh = RED_BRIGH_OFF;
    Red_States_Flag = false;
    door_states = false;
  }

  // Периодически вызывается из основного цикла() для выполнения переходов между состояниями.
  void loop()
  {
    action();
    led_switch();
  }

  void action_states(boolean door_message, uint8 light_message)
  {
    door_states = door_message;
    light_Brigh = light_message;
  }

  void action()
  {
    if (door_states == false && light_Brigh == RED_BRIGH_OFF) // все офф
    {
      if (White_Brigh == WHITE_BRIGH_OFF && Red_Brigh == RED_BRIGH_OFF)
      {
        current_state = Possible_States::ALL_OFF;
        return;
      }
    }

    if (door_states == true) // дверь открыта
    {
      if (Red_Brigh > RED_BRIGH_OFF)
      {
        current_state = Possible_States::RED_FAST_OFF;
        return;
      }
      if (White_Brigh < WHITE_BRIGH_MAX)
      {
        current_state = Possible_States::WRITE_ON;
        return;
      }
      current_state = Possible_States::REPEAT_WRITE; // NO_ACTION;
      return;
    }

    if (door_states == false) // дверь закрыта
    {
      if (White_Brigh > WHITE_BRIGH_OFF)
      {
        current_state = Possible_States::WRITE_OFF;
        return;
      }

      if (light_Brigh >= RED_BRIGH_MIN)
      {
        if (Red_Brigh < light_Brigh)
        {
          if (Red_States_Flag == false)
          {
            current_state = Possible_States::RED_ON;
            return;
          }

          else
          {
            current_state = Possible_States::RED_FAST_UP;
            return;
          }
        }

        else if (Red_Brigh > light_Brigh)
        {
          current_state = Possible_States::RED_FAST_DOWN;
          return;
        }

        else
        {
          current_state = Possible_States::REPEAT_RED;
          return;
        }
      }

      else if (light_Brigh == RED_BRIGH_OFF)
      {
        if (Red_Brigh > RED_BRIGH_OFF)
        {
          current_state = Possible_States::RED_OFF;
        }
      }
    }
  }

  void led_switch()
  {
    switch (current_state)
    {
    case Possible_States::ALL_OFF:
    {
      static PassiveTimer all_off_repeat_timer;
      if (all_off_repeat_timer.timeMillis() >= REPEAT_PERIOD)
      {
        all_off_repeat_timer.restart();
        White_Brigh = WHITE_BRIGH_OFF;
        Red_Brigh = RED_BRIGH_OFF;
        rgb_leds.clear();
        PORTD &= ~H(WHITE_LED_PIN);
        rgb_leds.show();
      }
      break;
    }

    case Possible_States::REPEAT_WRITE:
    {
      static PassiveTimer white_repeat_timer;
      if (white_repeat_timer.timeMillis() >= REPEAT_PERIOD)
      {
        white_repeat_timer.restart();
        uint8 bright = GammaCorrection_White(White_Brigh);
        rgb_leds.fill(mRGB(WHITE_COLOR));
        rgb_leds.setBrightness(bright);
        analogWrite(WHITE_LED_PIN, bright);
        rgb_leds.show();
      }
      break;
    }

    case Possible_States::REPEAT_RED:
    {
      static PassiveTimer red_repeat_timer;
      if (red_repeat_timer.timeMillis() >= REPEAT_PERIOD)
      {
        red_repeat_timer.restart();
        uint8 bright = GammaCorrection_Red(Red_Brigh);
        rgb_leds.fill(mRGB(RED_COLOR));
        rgb_leds.setBrightness(bright);
        PORTD &= ~H(WHITE_LED_PIN);
        rgb_leds.show();
      }
      break;
    }

    case Possible_States::WRITE_ON:
    {
      static PassiveTimer white_update_on_timer;
      if (white_update_on_timer.timeMillis() >= WHITE_UPDATE_PERIOD / WHITE_BRIGH_MAX)
      {
        white_update_on_timer.restart();
        White_Brigh++;
        uint8 bright = GammaCorrection_White(White_Brigh);
        rgb_leds.fill(mRGB(WHITE_COLOR));
        rgb_leds.setBrightness(bright);
        analogWrite(WHITE_LED_PIN, bright);
        rgb_leds.show();
        Red_States_Flag = false;
      }
      break;
    }

    case Possible_States::WRITE_OFF:
    {
      static PassiveTimer white_update_off_timer;
      if (white_update_off_timer.timeMillis() >= WHITE_UPDATE_PERIOD / WHITE_BRIGH_MAX)
      {
        white_update_off_timer.restart();
        White_Brigh--;
        uint8 bright = GammaCorrection_White(White_Brigh);
        rgb_leds.fill(mRGB(WHITE_COLOR));
        rgb_leds.setBrightness(bright);
        analogWrite(WHITE_LED_PIN, bright);
        rgb_leds.show();
      }
      break;
    }

    case Possible_States::RED_ON: // плавно включаем
    {
      static PassiveTimer red_update_on_timer;
      if (red_update_on_timer.timeMillis() >= RED_UPDATE_PERIOD / RED_BRIGH_MAX)
      {
        red_update_on_timer.restart();
        Red_Brigh++;
        uint8 bright = GammaCorrection_Red(Red_Brigh);
        rgb_leds.fill(mRGB(RED_COLOR));
        rgb_leds.setBrightness(bright);
        PORTD &= ~H(WHITE_LED_PIN);
        rgb_leds.show();
        if (Red_Brigh == light_Brigh)
        {
          Red_States_Flag = true;
        }
      }
      break;
    }

    case Possible_States::RED_OFF: // плавно выключаем
    {
      static PassiveTimer red_update_off_timer;
      if (red_update_off_timer.timeMillis() >= RED_UPDATE_PERIOD / RED_BRIGH_MAX)
      {
        red_update_off_timer.restart();
        Red_Brigh--;
        uint8 bright = GammaCorrection_Red(Red_Brigh);
        rgb_leds.fill(mRGB(RED_COLOR));
        rgb_leds.setBrightness(bright);
        PORTD &= ~H(WHITE_LED_PIN);
        rgb_leds.show();
        if (Red_Brigh == RED_BRIGH_OFF)
        {
          Red_States_Flag = false;
        }
      }
      break;
    }

    case Possible_States::RED_FAST_UP: // быстро прибовляем
    {
      Red_Brigh = light_Brigh;
      uint8 bright = GammaCorrection_Red(Red_Brigh);
      rgb_leds.fill(mRGB(RED_COLOR));
      rgb_leds.setBrightness(bright);
      PORTD &= ~H(WHITE_LED_PIN);
      rgb_leds.show();
      break;
    }

    case Possible_States::RED_FAST_DOWN: // быстро убовляем
    {
      Red_Brigh = light_Brigh;
      uint8 bright = GammaCorrection_Red(Red_Brigh);
      rgb_leds.fill(mRGB(RED_COLOR));
      rgb_leds.setBrightness(bright);
      PORTD &= ~H(WHITE_LED_PIN);
      rgb_leds.show();
      break;
    }

    case Possible_States::RED_FAST_OFF: // быстро выключаем
    {
      Red_Brigh = RED_BRIGH_OFF;
      rgb_leds.clear();
      PORTD &= ~H(WHITE_LED_PIN);
      rgb_leds.show();
      break;
    }
    }
  }

  // функция возвращает скорректированное по CRT значение
  byte GammaCorrection_White(byte val)
  {
    return pgm_read_byte(&(CRTgamma_White[val]));
  }

  // функция возвращает скорректированное по CRT значение
  byte GammaCorrection_Red(byte val)
  {
    return pgm_read_byte(&(CRTgamma_Red[val]));
  }
} // пространство имен action_led
