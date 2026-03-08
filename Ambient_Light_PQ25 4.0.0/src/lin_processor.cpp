#include "lin_processor.h"

#include "Settings.h"
#include "avr_util.h"
#include "custom_defs.h"
#include "hardware_clock.h"

// ----- Параметры, связанные со скоростью передачи данных. ---

// Если указана скорость вне допустимого диапазона, используйте эту.
static const uint16 kDefaultBaud = 9600;

// Подождать не более N битов с конца стопового бита предыдущего байта
// к начальному биту следующего байта.
//
static const uint8 kMaxSpaceBits = 6;

// Определяем входной пин с быстрым доступом. Использование макроса делает
// не увеличивать время доступа к выводу по сравнению с прямой манипуляцией битами.
// Пин настроен с активным подтягиванием.
#define DEFINE_INPUT_PIN(name, port_letter, bit_index) \
  namespace name                                       \
  {                                                    \
    static const uint8 kPinMask = H(bit_index);        \
    static inline void setup()                         \
    {                                                  \
      DDR##port_letter &= ~kPinMask;                   \
      PORT##port_letter |= kPinMask;                   \
    }                                                  \
    static inline uint8 isHigh()                       \
    {                                                  \
      return PIN##port_letter & kPinMask;              \
    }                                                  \
  }

// Определяем выходной пин с быстрым доступом. Использование макроса делает
// не увеличивать время доступа к выводу по сравнению с прямой манипуляцией битами.
#define DEFINE_OUTPUT_PIN(name, port_letter, bit_index, initial_value) \
  namespace name                                                       \
  {                                                                    \
    static const uint8 kPinMask = H(bit_index);                        \
    static inline void setHigh()                                       \
    {                                                                  \
      PORT##port_letter |= kPinMask;                                   \
    }                                                                  \
    static inline void setLow()                                        \
    {                                                                  \
      PORT##port_letter &= ~kPinMask;                                  \
    }                                                                  \
    static inline void setup()                                         \
    {                                                                  \
      DDR##port_letter |= kPinMask;                                    \
      (initial_value) ? setHigh() : setLow();                          \
    }                                                                  \
  }

namespace lin_processor
{

  class Config
  {
  public:
    // Инициализируется для заданной скорости передачи данных.
    void setup()
    {
      // Если скорость передачи данных вне допустимого диапазона, используйте скорость по умолчанию.
      uint16 baud = custom_defs::kLinSpeed;
      if (baud < 1000 || baud > 20000)
      {
        baud = kDefaultBaud;
      }
      baud_ = baud;
      prescaler_x64_ = baud < 8000;
      const uint8 prescaling = prescaler_x64_ ? 64 : 8;
      counts_per_bit_ = (((16000000L / prescaling) / baud));
      // Добавление двух счетчиков для компенсации программной задержки.
      counts_per_half_bit_ = (counts_per_bit_ / 2) + 2;
      clock_ticks_per_bit_ = (hardware_clock::kTicksPerMilli * 1000) / baud;
      clock_ticks_per_half_bit_ = clock_ticks_per_bit_ / 2;
      clock_ticks_per_until_start_bit_ = clock_ticks_per_bit_ * kMaxSpaceBits;
    }

    inline uint16 baud() const
    {
      return baud_;
    }

    inline boolean prescaler_x64()
    {
      return prescaler_x64_;
    }

    inline uint8 counts_per_bit() const
    {
      return counts_per_bit_;
    }
    inline uint8 counts_per_half_bit() const
    {
      return counts_per_half_bit_;
    }
    inline uint8 clock_ticks_per_bit() const
    {
      return clock_ticks_per_bit_;
    }
    inline uint8 clock_ticks_per_half_bit() const
    {
      return clock_ticks_per_half_bit_;
    }
    inline uint8 clock_ticks_per_until_start_bit() const
    {
      return clock_ticks_per_until_start_bit_;
    }

  private:
    uint16 baud_;
    // Ложь -> x8, истина -> x64.
    // TODO: timer2 также имеет масштабирование x32l Мог бы использовать его получше
    // точность в среднем диапазоне бодов.
    boolean prescaler_x64_;
    uint8 counts_per_bit_;
    uint8 counts_per_half_bit_;
    uint8 clock_ticks_per_bit_;
    uint8 clock_ticks_per_half_bit_;
    uint8 clock_ticks_per_until_start_bit_;
  };

  // Фактическая конфигурация. Инициализируется в setup() на основе скорости передачи данных.
  Config config;

  // ----- Контакты цифрового ввода/вывода
  //
  // ПРИМЕЧАНИЕ: мы используем прямой доступ к регистру вместо абстракций в io_pins.h.
  // Таким образом, мы сэкономим несколько циклов ISR.

  // ЛИН-интерфейс.

  DEFINE_INPUT_PIN(rx_pin, D, RX_PIN);

  // D3 не используется
  DEFINE_OUTPUT_PIN(tx1_pin, D, TX_PIN, 1);
  DEFINE_OUTPUT_PIN(sleep_pin, D, SLEEP_PIN, 1);

  // Вызывается во время инициализации.
  static inline void setupPins()
  {
    sleep_pin::setup();
  }

  // ----- Кольцевые буферы ISR RX -----

  // Размер очереди буфера кадра.
  static const uint8 kMaxFrameBuffers = 8;

  // Очередь буферов RX Frame. Чтение/запись только ISR.
  static LinFrame rx_frame_buffers[kMaxFrameBuffers];

  // Индекс [0, kMaxFrameBuffers) текущего буфера кадра
  // написано (самое новое). Чтение/запись только ISR.
  static uint8 head_frame_buffer;

  // Индекс [0, kMaxFrameBuffers) следующего считываемого кадра (самого старого).
  // Если равно head_frame_buffer, то доступного фрейма нет.
  // Чтение/запись только ISR.
  static uint8 tail_frame_buffer;

  // Вызывается один раз из main.
  static inline void setupBuffers()
  {
    head_frame_buffer = 0;
    tail_frame_buffer = 0;
  }

  // Вызывается из ISR или из main с отключенными прерываниями.
  static inline void incrementTailFrameBuffer()
  {
    if (++tail_frame_buffer >= kMaxFrameBuffers)
    {
      tail_frame_buffer = 0;
    }
  }

  // Вызывается из ISR. Если вы наступаете на хвостовой буфер, вызывающая сторона должна
  // приращение поднять ошибку переполнения кадра.
  static inline void incrementHeadFrameBuffer()
  {
    if (++head_frame_buffer >= kMaxFrameBuffers)
    {
      head_frame_buffer = 0;
    }
  }

  // ----- ISR для основной передачи данных -----

  // Увеличение на ISR, чтобы указать основной программе, когда ISR возвращается.
  // Это используется для отложенного отключения прерываний до тех пор, пока ISR не завершится до
  // уменьшить время джиттера ISR.
  static volatile uint8 isr_marker;

  // Должен вызываться только из main.
  static inline void waitForIsrEnd()
  {
    const uint8 value = isr_marker;
    // Подождите, пока не завершится следующая ISR.
    while (value == isr_marker)
    {
    }
  }

  // Общедоступно. Вызывается из основного. См. описание в .h.
  boolean readNextFrame(LinFrame *buffer)
  {
    boolean result = false;
    waitForIsrEnd();
    cli();
    if (tail_frame_buffer != head_frame_buffer)
    {
      // Это копирует структуру буфера запроса.
      *buffer = rx_frame_buffers[tail_frame_buffer];
      incrementTailFrameBuffer();
      result = true;
    }
    sei();
    return result;
  }

  // ----- Декларация конечного автомата -----

  // То же, что enum, но только 8 бит.
  namespace states
  {
    static const uint8 DETECT_BREAK = 1;
    static const uint8 READ_DATA = 2;
  }
  static uint8 state;

  class StateDetectBreak
  {
  public:
    static inline void enter();
    static inline void handleIsr();

  private:
    static uint8 low_bits_counter_;
  };

  class StateReadData
  {
  public:
    // Должен вызываться после обнаружения стопового бита прерывания.
    static inline void enter();
    static inline void handleIsr();

  private:
    // Количество полных байтов, прочитанных на данный момент. Включает все байты, даже
    // синхронизация, идентификатор и контрольная сумма.
    static uint8 bytes_read_;

    // Количество битов, прочитанных на данный момент в текущем байте. Включает стартовый бит,
    // 8 бит данных и один стоповый бит.
    static uint8 bits_read_in_byte_;

    // Буфер для текущего байта мы собираем.
    static uint8 byte_buffer_;

    // При сборе битов данных происходит переход от (1 << 0) к (1 << 7). Мог
    // вычисляется как (1 << (bits_read_in_byte_ - 1)). Мы используем это кешированное значение
    // отменить вычисление ISR.
    static uint8 byte_buffer_bit_mask_;
  };

  // ----- Флаг ошибки. -----

  // Написано из ISR. Чтение/запись из основного. Битовая маска ожидающих ошибок.
  static volatile uint8 error_flags;

  // Частный. Вызывается из ISR и из установки (до запуска ISR).
  static inline void setErrorFlags(uint8 flags)
  {
    // Неатомарно при вызове из setup(), но должно быть нормально, так как ISR еще не запущен.
    error_flags |= flags;
  }

  // Вызывается из основного. Общественный. Предполагается, что прерывания разрешены.
  // Не звонить из ISR.
  uint8 getAndClearErrorFlags()
  {
    // Отключение прерываний на короткое время для атомарности. Нужно обратить внимание на
    // Дрожание ISR из-за отключенных прерываний.
    cli();
    const uint8 result = error_flags;
    error_flags = 0;
    sei();
    return result;
  }

  struct BitName
  {
    const uint8 mask;
    const char *const name;
  };

  static const BitName kErrorBitNames[] PROGMEM = {
      {errors::FRAME_TOO_SHORT, "SHRT"},
      {errors::FRAME_TOO_LONG, "LONG"},
      {errors::START_BIT, "STRT"},
      {errors::STOP_BIT, "STOP"},
      {errors::SYNC_BYTE, "SYNC"},
      {errors::BUFFER_OVERRUN, "OVRN"},
      {errors::OTHER, "OTHR"},
  };

  // ----- Инициализация -----

  static void setupTimer()
  {
    // Режим быстрой ШИМ, выход OC2B активен на высоком уровне.
    TCCR2A = L(COM2A1) | L(COM2A0) | H(COM2B1) | H(COM2B0) | H(WGM21) | H(WGM20);
    const uint8 prescaler = config.prescaler_x64() ? (H(CS22) | L(CS21) | L(CS20)) : (L(CS22) | H(CS21) | L(CS20)); // x64 // x8
    // Пределитель: X8. Должен соответствовать определению kPreScaler;
    TCCR2B = L(FOC2A) | L(FOC2B) | H(WGM22) | prescaler;
    // Очистить счетчик.
    TCNT2 = 0;
    // Определяет скорость передачи данных.
    OCR2A = config.counts_per_bit() - 1;
    // Короткий 8-тактовый импульс на OC2B в конце каждого цикла,
    // непосредственно перед запуском ISR.
    OCR2B = config.counts_per_bit() - 2;
    // Прерывание при совпадении A.
    TIMSK2 = L(OCIE2B) | H(OCIE2A) | L(TOIE2);
    // Очистить ожидающие прерывания сравнения A.
    TIFR2 = L(OCF2B) | H(OCF2A) | L(TOV2);
  }

  // Вызываем один раз из main в начале программы.
  void setup()
  {
    // Это следует сделать в первую очередь, так как от этого зависят некоторые из приведенных ниже шагов.
    config.setup();

    setupPins();
    setupBuffers();
    StateDetectBreak::enter();
    setupTimer();
    error_flags = 0;
    sleep_pin::setHigh();
  }

  // ----- Вспомогательные функции ISR -----

  // Установить значение таймера на ноль.
  static inline void resetTickTimer()
  {
    TCNT2 = 0;
    // Очистить ожидающие прерывания сравнения A.
    TIFR2 = L(OCF2B) | H(OCF2A) | L(TOV2);
  }

  // Установите значение таймера на половину тика. Вызывается в начале
  // стартовый бит для генерации тиков выборки в середине следующего
  // 10 бит (старт, 8 * данные, стоп).
  static inline void setTimerToHalfTick()
  {
    // Добавляем 2, чтобы компенсировать задержку перед вызовом. Цель
    // чтобы следующая выборка данных ISR была в середине старта
    // кусочек.
    TCNT2 = config.counts_per_half_bit();
  }

  // Выполняем плотный цикл занятости до тех пор, пока RX не станет низким или заданное число
  // пройдено тактов часов (тайм-аут). Возвращает true, если все в порядке,
  // ложь, если тайм-аут. Сохраняет сброс таймера во время ожидания.
  // Вызывается только из ISR.
  static inline boolean waitForRxLow(uint16 max_clock_ticks)
  {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    for (;;)
    {
      // Держите тиковый таймер не тикающим (нет ISR).
      resetTickTimer();

      // Если rx низкий, мы закончили.
      if (!rx_pin::isHigh())
      {
        return true;
      }

      // Проверка тайм-аута.
      // Должно работать также в случае переполнения 16-битных часов.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= max_clock_ticks)
      {
        return false;
      }
    }
  }

  // То же, что и waitForRxLow, но с обратной полярностью.
  // Мы клонируем код для оптимизации времени.
  // Вызывается только из ISR.
  static inline boolean waitForRxHigh(uint16 max_clock_ticks)
  {
    const uint16 base_clock = hardware_clock::ticksForIsr();
    for (;;)
    {
      resetTickTimer();
      if (rx_pin::isHigh())
      {
        return true;
      }
      // Должно работать и в случае переполнения часов.
      const uint16 clock_diff = hardware_clock::ticksForIsr() - base_clock;
      if (clock_diff >= max_clock_ticks)
      {
        return false;
      }
    }
  }

  // ----- Реализация состояния обнаружения-разрыва -----

  uint8 StateDetectBreak::low_bits_counter_;

  inline void StateDetectBreak::enter()
  {
    state = states::DETECT_BREAK;
    low_bits_counter_ = 0;
  }

  // Возвращаем true, если достаточно времени для обслуживания запроса rx.
  inline void StateDetectBreak::handleIsr()
  {
    if (rx_pin::isHigh())
    {
      low_bits_counter_ = 0;
      return;
    }

    // Здесь RX низкий (активный)

    if (++low_bits_counter_ < 10)
    {
      return;
    }

    waitForRxHigh(280);

    // Идем обрабатывать данные
    StateReadData::enter();
  }

  // ----- Реализация состояния чтения данных -----

  uint8 StateReadData::bytes_read_;
  uint8 StateReadData::bits_read_in_byte_;
  uint8 StateReadData::byte_buffer_;
  uint8 StateReadData::byte_buffer_bit_mask_;

  // Вызывается при переходе от низкого уровня к высокому в конце разрыва.
  inline void StateReadData::enter()
  {
    state = states::READ_DATA;
    bytes_read_ = 0;
    bits_read_in_byte_ = 0;
    rx_frame_buffers[head_frame_buffer].reset();

    waitForRxLow(280);
    setTimerToHalfTick();
  }

  inline void StateReadData::handleIsr()
  {
    // Выборка бита данных как можно скорее, чтобы избежать джиттера.
    const uint8 is_rx_high = rx_pin::isHigh();

    // Обработка стартового бита.
    if (bits_read_in_byte_ == 0)
    {
      // Ошибка стартового бита.
      if (is_rx_high)
      {
        // Если в байте синхронизации, сообщить об ошибке синхронизации.
        setErrorFlags(bytes_read_ == 0 ? errors::SYNC_BYTE : errors::START_BIT);
        StateDetectBreak::enter();
        return;
      }
      // Стартовый бит в порядке.
      bits_read_in_byte_++;
      // Подготовить буфер и маску для сбора битов данных.
      byte_buffer_ = 0;
      byte_buffer_bit_mask_ = (1 << 0);
      return;
    }

    // Обработка следующего бита данных, 1 из 8.
    // Собираем текущий бит в byte_buffer_, сначала младший бит.
    if (bits_read_in_byte_ <= 8)
    {
      if (is_rx_high)
      {
        byte_buffer_ |= byte_buffer_bit_mask_;
      }
      byte_buffer_bit_mask_ = byte_buffer_bit_mask_ << 1;
      bits_read_in_byte_++;
      return;
    }

    // Здесь, когда в стоповом бите.
    bytes_read_++;
    bits_read_in_byte_ = 0;

    // Ошибка, если стоповый бит не высокий.
    if (!is_rx_high)
    {
      // Если в байте синхронизации, сообщить об ошибке синхронизации.
      setErrorFlags(bytes_read_ == 0 ? errors::SYNC_BYTE : errors::STOP_BIT);
      StateDetectBreak::enter();
      return;
    }

    // Здесь, когда мы только что закончили чтение байта.
    // bytes_read уже увеличен для этого байта.

    // Если это байт синхронизации, убедитесь, что он имеет ожидаемое значение.
    if (bytes_read_ == 1)
    {
      // Должно быть ровно 0x55. Мы не добавляем этот байт в буфер.
      if (byte_buffer_ != 0x55)
      {
        setErrorFlags(errors::SYNC_BYTE);
        StateDetectBreak::enter();
        return;
      }
    }
    else
    {
      // Если это байты идентификатора, данных или контрольной суммы, добавьте их в буфер кадра.
      // ПРИМЕЧАНИЕ: количество байтов принудительно установлено где-то в другом месте, поэтому здесь мы можем с уверенностью предположить, что это
      // не приведет к переполнению буфера.
      rx_frame_buffers[head_frame_buffer].append_byte(byte_buffer_);
    }

    // Ждем перехода от старшего к младшему начального бита следующего байта.
    const boolean has_more_bytes = waitForRxLow(config.clock_ticks_per_until_start_bit());

    // Обрабатываем случай, когда в этом фрейме больше нет байтов.
    if (!has_more_bytes)
    {
      // Проверить минимальное количество байтов.
      if (bytes_read_ < LinFrame::kMinBytes)
      {
        setErrorFlags(errors::FRAME_TOO_SHORT);
        StateDetectBreak::enter();
        return;
      }

      // Кадр пока выглядит нормально. Переход к следующему кадру в кольцевом буфере.
      // ПРИМЕЧАНИЕ: мы сбросим byte_count нового буфера кадра в следующий раз, когда войдем в состояние обнаружения данных.
      // ПРИМЕЧАНИЕ: проверка байта синхронизации, идентификатора, контрольной суммы и т. д. выполняется позже основным кодом, а не ISR.
      incrementHeadFrameBuffer();
      if (tail_frame_buffer == head_frame_buffer)
      {
        // Буфер кадра переполнен. Отбрасываем самый старый кадр и продолжаем с этим.
        setErrorFlags(errors::BUFFER_OVERRUN);
        incrementTailFrameBuffer();
      }

      StateDetectBreak::enter();
      return;
    }

    // Все готово для следующего байта. Поставьте галочку посередине
    // стартовый бит.
    setTimerToHalfTick();

    // Здесь, когда в этом фрейме есть хотя бы еще один байт. Ошибка, если у нас уже было
    // максимальное количество байт.
    if (rx_frame_buffers[head_frame_buffer].num_bytes() >= LinFrame::kMaxBytes)
    {
      setErrorFlags(errors::FRAME_TOO_LONG);
      StateDetectBreak::enter();
      return;
    }
  }

  // ----- Обработчик ISR -----

  // Прерывание по таймеру 2 A-match.
  ISR(TIMER2_COMPA_vect)
  {
    switch (state)
    {
    case states::DETECT_BREAK:
      StateDetectBreak::handleIsr();
      break;
    case states::READ_DATA:
      StateReadData::handleIsr();
      break;
    default:
      // setErrorFlags(errors::OTHER);
      StateDetectBreak::enter();
    }

    // Увеличиваем флаг isr, чтобы указать главному серверу, что ISR только что
    // завершено и прерывания могут быть временно отключены без причины ISR
    // дрожание.
    isr_marker++;
  }
} // пространство имен lin_processor
