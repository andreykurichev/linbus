#include "avr_util.h"

namespace avr_util_private
{
  // Использование поиска, чтобы избежать смещения отдельных битов. Это быстрее
  // чем (1 << n) или оператором switch/case.
  const byte kBitMaskArray[] = {
      H(0),
      H(1),
      H(2),
      H(3),
      H(4),
      H(5),
      H(6),
      H(7),
  };

} // пространство имен avr_util_private