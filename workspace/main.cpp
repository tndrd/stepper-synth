#include <memory>

#include "synth.hpp"

#define TIM2_DT_US 500  // notes up to 1Khz

static std::unique_ptr<StepperSynth> synth;

extern "C" void TIM2_IRQHandler(void) {
  if (synth.get()) synth->update(TIM2_DT_US);

  TIM2->SR &= ~TIM_SR_UIF;
}

static void shoveit(bool flip, bool bass) {
  assert(synth.get());

  using namespace MIDI::Notes;

  uint8_t notes1[] = {Db2, C3, Db3, Db2, C3, A2,  Db2, A2,
                      Db2, A2, Ab2, Db2, A2, Gb2, Gb2, Gb2};

  uint8_t notes2[] = {Gb1, Db2, Db2, Db2, Db2, C2,  C2,  C2,
                      C2,  A1,  A1,  A1,  A1,  Gb1, Gb1, Gb1};

  uint8_t velo = flip ? 255 : 1;

  for (uint8_t i = 0; i < sizeof(notes1); ++i) {
    synth->noteOn(0, notes1[i], velo);

    if (bass) synth->noteOn(1, notes2[i], velo);

    sleep(300000);
  }

  synth->noteOn(0, 0, 0);
  synth->noteOn(1, 0, 0);
}

static void errata_2_2_13(const volatile uint32_t* reg, uint32_t pre_msk,
                          uint32_t pre_pos) {
  assert(reg);

  uint8_t hpre = (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
  uint8_t ppre = (RCC->CFGR & pre_msk) >> pre_pos;
  uint32_t cycles = 1 + hpre / ppre;

  static volatile uint32_t dummy;
  for (uint8_t i = 0; i < cycles; ++i) dummy = *reg;
}

static void setup() {
  // AHB1 periphery clock enable
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  errata_2_2_13(&RCC->AHB1ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // APB1 periphery clock enable
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);

  // TIM2 configuration
  // HSI 16MHz -> /16 -> TIM2CLK = 1 MHz
  TIM2->PSC = 16;
  TIM2->ARR = TIM2_DT_US;
  TIM2->CR1 |= TIM_CR1_CEN;
  TIM2->DIER |= TIM_DIER_UIE;

  NVIC_EnableIRQ(TIM2_IRQn);
}

int main(void) {
  setup();

  std::vector<StepperSynth::Pins> pins{{{GPIOA, 0}, {GPIOA, 1}},
                                       {{GPIOA, 2}, {GPIOA, 3}}};

  synth = std::make_unique<StepperSynth>(pins);

  size_t i = 0;
  while (1) {
    shoveit(true, (i++) >= 2);
  }
}