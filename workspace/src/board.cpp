#include "stepper_synth/board.hpp"

#include <assert.h>

#include "stepper_synth/common.hpp"
#include "stepper_synth/config.hpp"
#include "stm32f407xx.h"

#define SYNTH_SYSCLK_FREQUENCY (SYNTH_SYSCLK_FREQUENCY_MHZ * MHZ)
#define SYNTH_HSE_FREQUENCY (SYNTH_HSE_FREQUENCY_MHZ * MHZ)

static void errata_2_2_13(const volatile uint32_t* reg, uint32_t pre_msk,
                          uint32_t pre_pos) {
  assert(reg);

  uint8_t hpre = (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
  uint8_t ppre = (RCC->CFGR & pre_msk) >> pre_pos;
  uint32_t cycles = 1 + hpre / ppre;

  static volatile uint32_t dummy;
  for (uint8_t i = 0; i < cycles; ++i) dummy = *reg;
}

/// @brief Select HSE as PLL input, set up PLL factors, set SYSCLK=PLLCLK
/// @param m pre-division factor
/// @param n multiplication factor
/// @param p post-division factor for SYSCLK
/// @param q post-division factor for PLL48CLK
static void pllSetup(uint32_t m, uint32_t n, uint32_t p, uint32_t q) {
  assert(p % 2 == 0);
  assert(2 <= p && p <= 8);
  assert(2 <= m && m <= 63);
  assert(50 <= n && n <= 432);
  assert(2 <= q && q <= 15);

  RCC->CR &= ~RCC_CR_PLLON;         // Disable PLL
  while (RCC->CR & RCC_CR_PLLRDY);  //

  RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN |  // Set factors
                    RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ);  //
                                                           //
  RCC->PLLCFGR |= (p / 2 - 1) << RCC_PLLCFGR_PLLP_Pos;     //
  RCC->PLLCFGR |= m << RCC_PLLCFGR_PLLM_Pos;               //
  RCC->PLLCFGR |= n << RCC_PLLCFGR_PLLN_Pos;               //
  RCC->PLLCFGR |= q << RCC_PLLCFGR_PLLQ_Pos;               //

  RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC_Msk;  // Select HSE as PLL source
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;   //

  RCC->CR |= RCC_CR_HSEON;             // Enable HSE
  while (!(RCC->CR & RCC_CR_HSERDY));  //

  RCC->CR |= RCC_CR_PLLON;             // Enable PLL
  while (!(RCC->CR & RCC_CR_PLLRDY));  //

  RCC->CFGR |= RCC_CFGR_SW_PLL;  // Select PLL as SYSCLK
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);
}

Clocks boardInit() {
  assert(SYNTH_SYSCLK_FREQUENCY_MHZ <= 168);
  Clocks clk;

  // PWR Setup
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);
  PWR->CR |= (1 << PWR_CR_VOS_Pos);  // RM0090 3.5.1

  // Flash setup
  FLASH->ACR |= (FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN);
  FLASH->ACR &= ~FLASH_ACR_LATENCY_Msk;
  FLASH->ACR |= FLASH_ACR_LATENCY_5WS;  // RM0090 3.5.1

  // Prescalers
  // APB1CLK = SYSCLK / 4 <= 42 MHz
  RCC->CFGR &= ~RCC_CFGR_PPRE1;
  RCC->CFGR |= 0b101u << RCC_CFGR_PPRE1_Pos;
  clk.apb1 = SYNTH_SYSCLK_FREQUENCY / 4;
  clk.apb1tim = SYNTH_SYSCLK_FREQUENCY / 2;

  // APB2CLK = SYSCLK / 2 <= 84 MHz
  RCC->CFGR &= ~RCC_CFGR_PPRE2;
  RCC->CFGR |= 0b100u << RCC_CFGR_PPRE2_Pos;
  clk.apb2 = SYNTH_SYSCLK_FREQUENCY / 2;
  clk.apb2tim = SYNTH_SYSCLK_FREQUENCY;

  // No AHB prescalers
  clk.ahb1 = SYNTH_SYSCLK_FREQUENCY;
  clk.ahb2 = SYNTH_SYSCLK_FREQUENCY;

  // Setup SYSCLK=SYNTH_SYSCLK_FREQUENCY
  pllSetup(SYNTH_HSE_FREQUENCY / MHZ,           // -> vco_in   = 1 MHz
           SYNTH_SYSCLK_FREQUENCY / MHZ * 2,    // -> vco_out  = 2 * SYSCLK
           2,                                   // -> PLLCLK   = SYSCLK
           SYNTH_SYSCLK_FREQUENCY / MHZ / 24);  // -> PLL48CLK = 48 MHz
  clk.sys = SYNTH_SYSCLK_FREQUENCY;

  // Manually set SystemCoreClock instead of
  // calling SystemCoreClockUpdate() to avoid
  // defining HSE_VALUE in CMake
  // It is not required since HAL is not used,
  // but I'll leave it here just in case
  SystemCoreClock = SYNTH_SYSCLK_FREQUENCY;

  // AHB1 periphery clock enable (GPIOA, GPIOC)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  errata_2_2_13(&RCC->AHB1ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // AHB2 periphery clock enable (OTGFS)
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  errata_2_2_13(&RCC->AHB2ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // APB1 periphery clock enable (TIM2)
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);

  return clk;
}

void boardEnableIRQ() {
  __DSB();  // Make sure all initialization is complete
  NVIC_EnableIRQ(TIM2_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);
}