#pragma once

#include <array>
#include <cassert>
#include <cstdlib>

#include "stm32f407xx.h"

void sleep(size_t ticks);

struct PinBase {
  GPIO_TypeDef* const m_gpio;
  const uint8_t m_pin;
};

struct GPIOPinOut final : private PinBase {
 public:
  GPIOPinOut(PinBase pin, bool value = false);

  void write(bool value);
  void toggle();
};

template <typename T, size_t N>
struct RingBuffer final {
 private:
  size_t m_head = 0;
  size_t m_tail = 0;
  size_t m_size = 0;
  std::array<T, N> m_array;

 public:
  RingBuffer() = default;

  size_t size() const { return m_size; }
  static constexpr size_t capacity() { return N; }
  bool empty() const { return m_size == 0; }
  bool full() const { return m_size == N; }

  /// @todo transaction
  bool push(T&& value) {
    if (full()) return false;

    m_size++;
    m_array[m_head] = std::move(value);
    m_head = (m_head + 1) % N;

    return true;
  }

  bool push(const T& value) {
    T tmp{value};
    return push(std::move(tmp));
  }

  /// @todo transaction
  bool pop(T& dest) {
    if (empty()) return false;

    dest = std::move(m_array[m_tail]);
    m_tail = (m_tail + 1) % N;
    m_size--;

    return true;
  }
};