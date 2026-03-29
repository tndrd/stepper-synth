#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void sleep(size_t ticks)
{
  for (size_t i = 0; i < ticks; i++);
}

#if defined(STM32F407xx)
#include "stm32f407xx.h"
#define LED_PIN 1

void main(void)
{

  RCC->AHB1ENR |= (1 << RCC_AHB1ENR_GPIOAEN_Pos);
  
  // Errata
  volatile uint32_t dummy;
  dummy = RCC->AHB1ENR;
  dummy = RCC->AHB1ENR;

  GPIOA->MODER |= (1 << GPIO_MODER_MODER1_Pos);
  
  while(1)
  {
    for(int i = 0; i < 10; ++i)
    {
      GPIOA->ODR ^= (1 << LED_PIN);
      sleep(150000);
    }
    sleep(1500000);
  }
}

#elif defined(STM32G431xx)
#include "stm32g431xx.h"
#define LED_PIN 0

void main(void)
{

  RCC->AHB2ENR |= (1 << RCC_AHB2ENR_GPIOAEN_Pos);
  
  // Errata
  volatile uint32_t dummy;
  dummy = RCC->AHB2ENR;
  dummy = RCC->AHB2ENR;

  GPIOA->MODER &= ~(0b11 << (2 * LED_PIN));
  GPIOA->MODER |= (0b01 << (2 * LED_PIN));
  
  while(1)
  {
    for(int i = 0; i < 10; ++i)
    {
      GPIOA->ODR ^= (1 << LED_PIN);
      sleep(150000);
    }
    sleep(1500000);
  }
}
#endif