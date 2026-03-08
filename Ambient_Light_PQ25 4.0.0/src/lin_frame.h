#pragma once
#include "settings.h"
#include "avr_util.h"

// Буфер для одного кадра.
class LinFrame
{
public:
  LinFrame()
  {
    reset();
  }

  // Количество байтов в самом коротком кадре. Это кадр с байтом идентификатора и без
  // Ответ подчиненного устройства (и, следовательно, никаких данных или контрольной суммы).
  static const uint8 kMinBytes = 1;

  // Количество байтов в самом длинном фрейме. Один байт идентификатора, 8 байтов данных, один байт контрольной суммы.
  static const uint8 kMaxBytes = SUM_8_BITS;

  // Вычислить контрольные биты [P1,P0] идентификатора lin в битах [5:0] и вернуть
  // [P1,P0][5:0] — проводное представление этого идентификатора.
  static uint8 setLinIdChecksumBits(uint8 id);

  boolean isValid() const;

  // Вычисление контрольной суммы кадра LIN. Предположим, что в буфере есть хотя бы один байт. Действительный
  // фрейм должен содержать один байт для id, 1-8 байт для данных, один байт для контрольной суммы.
  uint8 computeChecksum() const;

  inline void reset()
  {
    num_bytes_ = 0;
  }

  inline uint8 num_bytes() const
  {
    return num_bytes_;
  }

  // Получить байт кадра заданного индекса.
  // Байтовый индекс 0 — это байт идентификатора кадра, за которым следуют 1-8 байтов данных
  // за которым следует 1 байт контрольной суммы.
  //
  // Вызывающий должен убедиться, что index < num_bytes.
  inline uint8 get_byte(uint8 index) const
  {
    return bytes_[index];
  }

  // Вызывающий должен проверить, что num_bytes < kMaxBytes;
  inline void append_byte(uint8 value)
  {
    bytes_[num_bytes_++] = value;
  }

private:
  // Количество байтов в bytes_buffer. Максимум kMaxBytes.
  uint8 num_bytes_;

  // Полученные байты кадра. Включает идентификатор, данные и контрольную сумму. Не
  // включить байт синхронизации 0x55.
  uint8 bytes_[kMaxBytes];
};
