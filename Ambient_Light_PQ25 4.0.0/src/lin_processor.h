#pragma once
#include "avr_util.h"
#include "lin_frame.h"

// Использует
// * Timer2 - используется для генерации битовых тиков.
// * OC2B (PD3) - тики выхода таймера. Для отладки. При необходимости можно изменить
// чтобы не использовать этот вывод.
// * PD2 - вход LIN RX.
// * PC0, PC1, PC2, PC3 - отладочные выходы. Подробнее см. в файле .cpp.
namespace lin_processor
{
    // Вызов один раз в настройках программы.
    extern void setup();

    // Попытка прочитать следующий доступный кадр rx. Если доступно, верните true и установите
    // заданный буфер. В противном случае верните false и оставьте *buffer без изменений.
    // Байты синхронизации, идентификатора и контрольной суммы кадра, а также общий байт
    // количество не проверено.
    extern boolean readNextFrame(LinFrame *buffer);

    // Маски байтов ошибок для отдельных битов ошибок.
    namespace errors
    {
        static const uint8 FRAME_TOO_SHORT = (1 << 0);
        static const uint8 FRAME_TOO_LONG = (1 << 1);
        static const uint8 START_BIT = (1 << 2);
        static const uint8 STOP_BIT = (1 << 3);
        static const uint8 SYNC_BYTE = (1 << 4);
        static const uint8 BUFFER_OVERRUN = (1 << 5);
        static const uint8 OTHER = (1 << 6);
    }

    // Получить текущий флаг ошибки и очистить его.
    extern uint8 getAndClearErrorFlags();

    // Напечатать в sio список флагов ошибок.
    // extern void printErrorFlags(uint8 lin_errors);
}
