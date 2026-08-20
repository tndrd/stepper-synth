#include "stepper_synth/pwm_generator.hpp"

#include "stepper_synth/common.hpp"
#include "stm32f407xx.h"

static void configureAFPin(GPIO_TypeDef* port, uint8_t pin, uint8_t af) {
  // AFRx index of given pin
  uint8_t afr_ind = pin / 8;
  // AF field position of given pin
  uint8_t af_pos = 4 * (pin % 8);

  // Set given af
  port->AFR[afr_ind] &= ~(0b1111u << af_pos);
  port->AFR[afr_ind] |= af << af_pos;

  // Set low speed (sound frequencies)
  port->OSPEEDR &= ~(0b11u << pin * 2);

  // No PUPD
  port->PUPDR &= ~(0b11u << pin * 2);

  // Select AF mode
  port->MODER &= ~(0b11u << (pin * 2));
  port->MODER |= 0b10u << (pin * 2);
}

PWMGenerator::PWMGenerator(TIM_TypeDef* tim, uint32_t clock,
                           const Pin (&pins)[kMaxNumChannels],
                           uint8_t num_channels)
    : m_tim{tim}, m_num_channels{num_channels} {
  assert(tim);
  assert(m_num_channels <= kMaxNumChannels);

  // Set prescaler and reload value
  m_tim->PSC = clock / MHZ - 1;  // TIM period = 1 us
  m_tim->ARR = ~uint32_t(0);     // overflow in ~1 hr

  // Force update event
  // Otherwise, new prescaler value would
  // be applied 40s after start
  // (when counter wraps around for the first time)
  m_tim->EGR = TIM_EGR_UG;
  m_tim->SR = ~TIM_SR_UIF; 

  // Set CC channels to output toggle mode
  // without actually enabling them
  // CH1
  m_tim->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_CC1S);
  m_tim->CCMR1 |= 0b011u << TIM_CCMR1_OC1M_Pos;

  // CH2
  m_tim->CCMR1 &= ~(TIM_CCMR1_OC2M | TIM_CCMR1_CC2S);
  m_tim->CCMR1 |= (0b011u << TIM_CCMR1_OC2M_Pos);

  // CH3
  m_tim->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_CC3S);
  m_tim->CCMR2 |= (0b011u << TIM_CCMR2_OC3M_Pos);

  // CH4
  m_tim->CCMR2 &= ~(TIM_CCMR2_OC4M | TIM_CCMR2_CC4S);
  m_tim->CCMR2 |= (0b011u << TIM_CCMR2_OC4M_Pos);

  // Setup pins in AF mode
  for (uint8_t i = 0; i < m_num_channels; ++i)
    configureAFPin(pins[i].port, pins[i].pin, pins[i].af);
}

void PWMGenerator::start() {
  // Enable clock
  m_tim->CR1 |= TIM_CR1_CEN;
}

void PWMGenerator::setChannelPeriod(uint8_t ch, uint32_t period) {
  assert(ch < m_num_channels);

  // Compare event in toggle mode only flips the polarity.
  // Therefore, a single PWM period consists of two CC events
  // This division accounts for that
  period /= 2;

  // &TIM->CCRx
  volatile uint32_t* tim_ccrx_ptr = &(m_tim->CCR1) + ch;

  // TIM_CCER_CCxE_Msk
  uint32_t tim_ccer_ccxe_msk = 1 << (ch * 4);

  // TIM_DIER_CCxIE_Msk
  uint32_t tim_dier_ccxie_msk = 1 << (ch + 1);

  // TIM_SR_CCxIF_Msk
  uint32_t tim_sr_ccxif_msk = 1 << (ch + 1);

  // If channel is disabled and needs to be enabled
  if (m_periods_us[ch] == 0 && period != 0) {
    *tim_ccrx_ptr = m_tim->CNT + period;  // Update CCR (CNT == const)
    m_periods_us[ch] = period;            // Set new period
    m_tim->CCER |= tim_ccer_ccxe_msk;     // Enable CC
    m_tim->SR = ~tim_sr_ccxif_msk;        // Clear interrupt flag
    m_tim->DIER |= tim_dier_ccxie_msk;    // Enable interrupt

    return;
  }

  // If channel is enabled and needs to be disabled
  if (m_periods_us[ch] != 0 && period == 0) {
    m_tim->DIER &= ~tim_dier_ccxie_msk;  // Disable interrupt
    m_tim->CCER &= ~tim_ccer_ccxe_msk;   // Disable CC
    m_periods_us[ch] = 0;                // Set period to zero
    return;
  }

  // Update period if channel is active
  // Does nothing if channel remains inactive
  m_periods_us[ch] = period;  // Set new period
}

void PWMGenerator::isr() {
  for (int ch = 0; ch < m_num_channels; ++ch) {
    // TIM_SR_CCxIF_Msk
    uint32_t tim_sr_ccxif_msk = 1 << (ch + 1);

    // &TIM->CCRx
    volatile uint32_t* tim_ccrx_ptr = &(m_tim->CCR1) + ch;

    // If timer has counted up to CCR
    if (m_tim->SR & tim_sr_ccxif_msk) {
      // Add one target period to CCR
      *tim_ccrx_ptr += m_periods_us[ch];

      // Clear interrupt flag
      m_tim->SR = ~tim_sr_ccxif_msk;
    }
  }
}
