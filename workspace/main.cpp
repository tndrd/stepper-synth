#include <memory>

#include "synth.hpp"

#define TIM2_DT_US 50       // notes up to 1Khz
#define HSE_VALUE 8000000ul  // 8 MHz

static std::unique_ptr<StepperSynth> synth;
static std::unique_ptr<GPIOPinOut> pindbg;

static volatile int64_t sleep_cnt;

void sleep(uint16_t ms) {
  __disable_irq();
  sleep_cnt = ms * 1000 / TIM2_DT_US;
  __enable_irq();

  while (sleep_cnt > 0);
}

extern "C" void TIM2_IRQHandler(void) {
  if (pindbg.get()) pindbg->toggle();
  if (synth.get()) synth->update(TIM2_DT_US);
  if (pindbg.get()) pindbg->toggle();

  sleep_cnt--;

  TIM2->SR &= ~TIM_SR_UIF;
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

void checkPllFactors(size_t m, size_t n, size_t p) {
  assert(p % 2 == 0);
  assert(2 <= p && p <= 8);
  assert(2 <= m && m <= 63);
  assert(50 <= n && n <= 99);
}

enum class MCOSel : uint8_t {
  SYSCLK = 0b00,
  PLLI2S = 0b01,
  HSE = 0b10,
  PLL = 0b11
};

void MCODebug(MCOSel source) {
  uint8_t src = static_cast<uint8_t>(source);
  assert(src < 4);

  RCC->CFGR &= ~(RCC_CFGR_MCO2_Msk | RCC_CFGR_MCO2PRE_Msk);
  RCC->CFGR |= (src << RCC_CFGR_MCO2_Pos);       // HSE
  RCC->CFGR |= (0b111 << RCC_CFGR_MCO2PRE_Pos);  // /5

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  errata_2_2_13(&RCC->AHB1ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  GPIOC->MODER |= (0b10 << GPIO_MODER_MODER9_Pos);
}

static void setup() {
  // PWR Setup
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);
  PWR->CR |= (1 << PWR_CR_VOS_Pos);  // RM0090 3.5.1

  // Flash setup
  FLASH->ACR |= (FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN);
  FLASH->ACR &= ~FLASH_ACR_LATENCY_Msk;
  FLASH->ACR |= FLASH_ACR_LATENCY_5WS;  // RM0090 3.5.1

  // PLL setup (fin = 8Mhz, fout = 168Mhz)
  RCC->CR &= ~RCC_CR_PLLON;         // Disable PLL
  while (RCC->CR & RCC_CR_PLLRDY);  //

  size_t pllm = 8 / 4;    // vco_in  = 4 MHz
  size_t plln = 168 / 2;  // vco_out = 336 MHz
  size_t pllp = 2;        // pll_out = 168 MHz
  checkPllFactors(pllm, plln, pllp);

  RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP);
  RCC->PLLCFGR |= (pllp / 2 - 1) << RCC_PLLCFGR_PLLP_Pos;  // Set factors
  RCC->PLLCFGR |= pllm << RCC_PLLCFGR_PLLM_Pos;            //
  RCC->PLLCFGR |= plln << RCC_PLLCFGR_PLLN_Pos;            //

  RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC_Msk;  // Select HSE as PLL source
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;   //

  RCC->CR |= RCC_CR_HSEON;             // Enable HSE
  while (!(RCC->CR & RCC_CR_HSERDY));  //

  RCC->CR |= RCC_CR_PLLON;             // Enable PLL
  while (!(RCC->CR & RCC_CR_PLLRDY));  //

  RCC->CFGR |= RCC_CFGR_SW_PLL;  // Select PLL as SYSCLK
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);

  // AHB1 periphery clock enable
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  errata_2_2_13(&RCC->AHB1ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // APB1 periphery clock enable
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);

  // TIM2 configuration
  // PLL 168MHz -> /168 -> TIM2CLK = 1 MHz
  TIM2->PSC = 168;
  TIM2->ARR = TIM2_DT_US;
  TIM2->CR1 |= TIM_CR1_CEN;
  TIM2->DIER |= TIM_DIER_UIE;

  NVIC_EnableIRQ(TIM2_IRQn);
}

static void shoveit(bool gtr, bool bass, bool fx) {
  assert(synth.get());

  using namespace MIDI::Notes;

  uint8_t notes1[] = {Db2, C3, Db3, Db2, C3, A2,  Db2, A2,
                      Db2, A2, Ab2, Db2, A2, Gb2, Gb2, Gb2};

  uint8_t notes2[] = {Gb1, Db2, Db2, Db2, Db2, C2,  C2,  C2,
                      C2,  A1,  A1,  A1,  A1,  Gb1, Gb1, Gb1};

  synth->fx(0).stutter_period_us = fx ? 100 : 0;
  synth->fx(0).stutter_duration_ms = fx ? 20 : 0;
  synth->fx(0).vibrato_depth = fx ? 0.03 : 0;
  synth->fx(0).vibrato_period_ms = 100;

  synth->fx(1).stutter_period_us = fx ? 1200 : 0;
  synth->fx(1).stutter_duration_ms = fx ? 60 : 0;

  for (uint8_t i = 0; i < sizeof(notes1); ++i) {
    if (gtr) synth->playNote(0, notes1[i] + 12);
    if (bass) synth->playNote(1, notes2[i] + 0);
    sleep(250);
  }

  synth->playNote(0, 0);
  synth->playNote(1, 0);
}

int main(void) {
  setup();

  std::vector<StepperSynth::Pins> pins{{{GPIOA, 0}, {GPIOA, 1}},
                                       {{GPIOA, 2}, {GPIOA, 3}}};

  synth = std::make_unique<StepperSynth>(pins);
  pindbg = std::make_unique<GPIOPinOut>(PinBase{GPIOA, 4});

  // synth->fx(0).vibrato_depth = 0.01;
  // synth->fx(0).vibrato_period_ms = 500;

  size_t i = 0;
  while (1) {
    bool gitr[] = {true, true, false, false, true, true, false, false};
    bool bass[] = {false, true, true, false, false, true, true, false};
    bool vibr[] = {false, false, false, false, true, true, true, false};

    shoveit(gitr[i % 8], bass[i % 8], vibr[i % 8]);
    //synth->playNote(1, MIDI::Notes::A0 + i % 72);
    //sleep(200);
    i++;
  }
}