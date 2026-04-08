#include "common.hpp"

void sleep(size_t ticks) { for (size_t i = 0; i < ticks; i++); }

GPIOPinOut::GPIOPinOut(PinBase pin, bool value) : PinBase{pin} {
  assert(m_gpio);

  // Setup pin as output
  m_gpio->MODER |= (1 << (m_pin * 2));

  write(value);
}

void GPIOPinOut::write(bool value) {
  uint8_t bssr_pos = m_pin + (value ? 0 : 16);
  m_gpio->BSRR = 1 << bssr_pos;
}

void GPIOPinOut::toggle() { m_gpio->ODR ^= (1 << m_pin); }