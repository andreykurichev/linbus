#pragma once
#include <arduino.h>
#include "system_clock.h"

// Оболочка системных часов, обеспечивающая измерение времени в миллисекундах.
class PassiveTimer
{
public:
  PassiveTimer()
  {
    restart();
  }

  inline void restart()
  {
    start_time_millis_ = system_clock::timeMillis();
  }

  void copy(const PassiveTimer &other)
  {
    start_time_millis_ = other.start_time_millis_;
  }

  inline uint32 timeMillis() const
  {
    return system_clock::timeMillis() - start_time_millis_;
  }

private:
  uint32 start_time_millis_;
};
