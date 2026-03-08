#pragma once
#include "settings.h"
#include "avr_util.h"

// Специальные параметры пользовательского приложения.
//
// Как и все другие файлы custom_*, этот файл должен быть адаптирован к конкретному приложению.
// Приведенный пример относится к инжектору с нажатием кнопки спортивного режима для 981/Cayman.
namespace custom_defs
{
    // true для контрольной суммы LIN V2 (расширенная). false для контрольной суммы LIN версии 1.
#if (LIN_VERSION == 1)
    const boolean kUseLinChecksumVersion2 = false;
#elif (LIN_VERSION == 2)
    const boolean kUseLinChecksumVersion2 = true;
#else
#error ВЫБЕРИ ВЕРНЫЙ НОМЕР ВЕРСИИ LIN | 1 или 2!!!
#endif

    // Скорость передачи данных по шине LIN в секунду.
    // Поддерживаемый диапазон скоростей от 1000 до 20000. Если за пределами диапазона, используется тихое значение по умолчанию
    // бод 9600.
    const uint16 kLinSpeed = LIN_SPEED;

} // namepsace custom_defs
