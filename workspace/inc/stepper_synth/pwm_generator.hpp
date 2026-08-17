#pragma once
#include <assert.h>
#include <stdint.h>

#include "stm32f407xx.h"

struct PWMGenerator {
 public:
  struct Pin {
    GPIO_TypeDef* port;
    uint8_t pin;
    uint8_t af;
  };

 public:
  static constexpr uint8_t kMaxNumChannels = 4;

 private:
  TIM_TypeDef* m_tim;
  uint8_t m_num_channels;
  volatile uint32_t m_periods_us[kMaxNumChannels] = {};

 public:
  PWMGenerator(TIM_TypeDef* tim, uint32_t clock,
               const Pin (&pins)[kMaxNumChannels], uint8_t num_channels);

  void start();

  /// @brief Set PWM period on channel.
  /// @param ch channel
  /// @param period us
  /// @note If ```period``` equals zero, channel is disabled.
  ///       Providing a positive ```period``` to a disabled channel enables it.
  void setPWMPeriod(uint8_t ch, uint32_t period);

  void isr();
};
