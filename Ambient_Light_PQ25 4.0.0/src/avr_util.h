#pragma once
#include <arduino.h>

// Избавьтесь от суффикса типа _t.
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

// Битовый индекс к битовой маске.
// Битовые индексы регистров AVR определены в файле iom328p.h.
#define H(x) (1 << (x))
#define L(x) (0 << (x))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

// Частные данные. Не использовать из других модулей.
namespace avr_util_private
{
  extern const byte kBitMaskArray[];
}

// Аналогично (1 << bit_index), но более эффективно для неконстантных значений. За
// константные маски используют H(n). Неопределенный результат, если it_index не в [0, 7].
inline byte bitMask(byte bit_index)
{
  return *(avr_util_private::kBitMaskArray + bit_index);
}