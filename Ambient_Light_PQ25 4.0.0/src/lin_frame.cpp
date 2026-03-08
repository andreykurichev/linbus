#include "lin_frame.h"
#include "custom_defs.h"

// Вычисление контрольной суммы кадра. На данный момент используется только контрольная сумма LIN V2.
uint8 LinFrame::computeChecksum() const
{
  // Контрольная сумма LIN V2 включает байт ID, а V1 — нет.
  const uint8 startByteIndex = custom_defs::kUseLinChecksumVersion2 ? 0 : 1;
  const uint8 *p = &bytes_[startByteIndex];

  // Исключаем байт контрольной суммы в конце кадра.
  uint8 numBytesToChecksum = num_bytes_ - startByteIndex - 1;

  // Сумма байтов. У нас не должно быть 16-битного переполнения, так как размер кадра ограничен.
  uint16 sum = 0;
  while (numBytesToChecksum-- > 0)
  {
    sum += *(p++);
  }

  // Продолжайте добавлять старший и младший байты до тех пор, пока перенос не закончится.
  for (;;)
  {
    const uint8 highByte = (uint8)(sum >> 8);
    if (!highByte)
    {
      break;
    }
    // ПРИМЕЧАНИЕ: это может добавить дополнительный перенос.
    sum = (sum & 0xff) + highByte;
  }

  return (uint8)(~sum);
}

uint8 LinFrame::setLinIdChecksumBits(uint8 id)
{
  // Алгоритм оптимизирован для процессорного времени (избегая отдельных сдвигов для каждого идентификатора
  // кусочек). Использование регистров для двух битов контрольной суммы. P1 вычисляется в бите 7
  // p1_at_b7 и p0 вычисляются в бите 6 p0_at_b6.
  uint8 p1_at_b7 = ~0;
  uint8 p0_at_b6 = 0;

  // P1: id5, P0: id4
  uint8 shifter = id << 2;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  // P1: id4, P0: id3
  shifter += shifter;
  p1_at_b7 ^= shifter;

  // P1: id3, P0: id2
  shifter += shifter;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  // P1: id2, P0: id1
  shifter += shifter;
  p0_at_b6 ^= shifter;

  // P1: id1, P0: id0
  shifter += shifter;
  p1_at_b7 ^= shifter;
  p0_at_b6 ^= shifter;

  return (p1_at_b7 & 0b10000000) | (p0_at_b6 & 0b01000000) | (id & 0b00111111);
}

boolean LinFrame::isValid() const
{
  const uint8 n = num_bytes_;

  // Проверяем размер кадра.
  // Один байт идентификатора с дополнительными 1-8 байтами данных и 1 байтом контрольной суммы.
  if (n != 1 && (n < 3 || n > 10))
  {
    return false;
  }

  // Проверяем биты контрольной суммы байта идентификатора.
  const uint8 id_byte = bytes_[0];
  if (id_byte != setLinIdChecksumBits(id_byte))
  {
    return false;
  }

  // Если кадр не только ID, проверьте также общую контрольную сумму.
  if (n > 1)
  {
    if (bytes_[n - 1] != computeChecksum())
    {
      return false;
    }
  }
  // TODO: проверить защищенный идентификатор.
  return true;
}
