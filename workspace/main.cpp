#include <stdarg.h>
#include <stdio.h>

#include <memory>
#include <queue>

#include "class/midi/midi_device.h"
#include "synth.hpp"
#include "tusb.h"

// Lower values provide better frequency granularity,
// but significantly affect performance
#define TIM2_DT_US 50  //

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

extern "C" void OTG_FS_IRQHandler(void) { tud_int_handler(0); }

static void errata_2_2_13(const volatile uint32_t* reg, uint32_t pre_msk,
                          uint32_t pre_pos) {
  assert(reg);

  uint8_t hpre = (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos;
  uint8_t ppre = (RCC->CFGR & pre_msk) >> pre_pos;
  uint32_t cycles = 1 + hpre / ppre;

  static volatile uint32_t dummy;
  for (uint8_t i = 0; i < cycles; ++i) dummy = *reg;
}

void checkPllFactors(size_t m, size_t n, size_t p, size_t q) {
  assert(p % 2 == 0);
  assert(2 <= p && p <= 8);
  assert(2 <= m && m <= 63);
  assert(50 <= n && n <= 432);
  assert(2 <= q && q <= 15);
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

  size_t pllm = 8;         // vco_in = 1 MHz
  size_t plln = 336;       // vco_out = 336 MHz
  size_t pllp = 2;         // pllp_out = 168 MHz
  size_t pllq = 336 / 48;  // pllq_out = 48 MHz
  checkPllFactors(pllm, plln, pllp, pllq);

  RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP |
                    RCC_PLLCFGR_PLLQ);
  RCC->PLLCFGR |= (pllp / 2 - 1) << RCC_PLLCFGR_PLLP_Pos;  // Set factors
  RCC->PLLCFGR |= pllm << RCC_PLLCFGR_PLLM_Pos;            //
  RCC->PLLCFGR |= plln << RCC_PLLCFGR_PLLN_Pos;            //
  RCC->PLLCFGR |= pllq << RCC_PLLCFGR_PLLQ_Pos;            //

  RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC_Msk;  // Select HSE as PLL source
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;   //

  RCC->CR |= RCC_CR_HSEON;             // Enable HSE
  while (!(RCC->CR & RCC_CR_HSERDY));  //

  RCC->CR |= RCC_CR_PLLON;             // Enable PLL
  while (!(RCC->CR & RCC_CR_PLLRDY));  //

  RCC->CFGR |= RCC_CFGR_SW_PLL;  // Select PLL as SYSCLK
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);

  // AHB1 periphery clock enable (GPIOA)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  errata_2_2_13(&RCC->AHB1ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // AHB2 periphery clock enable (OTGFS)
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

  errata_2_2_13(&RCC->AHB2ENR, RCC_CFGR_HPRE_Msk, RCC_CFGR_HPRE_Pos);

  // APB1 periphery clock enable (TIM2)
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  errata_2_2_13(&RCC->APB1ENR, RCC_CFGR_PPRE1_Msk, RCC_CFGR_PPRE1_Pos);

  // TIM2 configuration
  // PLL 168MHz -> /168 -> TIM2CLK = 1 MHz
  TIM2->PSC = 168;
  TIM2->ARR = TIM2_DT_US;
  TIM2->CR1 |= TIM_CR1_CEN;
  TIM2->DIER |= TIM_DIER_UIE;

  // USB Pin AF Configuration (PA11, PA12)
  GPIOA->MODER |= 0b10 << GPIO_MODER_MODE11_Pos;  // Set AF Mode
  GPIOA->MODER |= 0b10 << GPIO_MODER_MODE12_Pos;  //
  GPIOA->AFR[1] |= 10 << GPIO_AFRH_AFSEL11_Pos;   // Select AF10 (OTG)
  GPIOA->AFR[1] |= 10 << GPIO_AFRH_AFSEL12_Pos;   //

  GPIOA->OSPEEDR |= (3 << GPIO_OSPEEDR_OSPEED11_Pos) |  // Set high speed
                    (3 << GPIO_OSPEEDR_OSPEED12_Pos);   //

  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD11 | GPIO_PUPDR_PUPD12);  // Disable pulls

  // Disable VBUS (IMPORTANT)
  USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
  USB_OTG_FS->GCCFG &= ~(USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_VBUSASEN);

  // Enable all the IRQs
  NVIC_EnableIRQ(TIM2_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);
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

extern "C" int dbg_showstr(const char* fmt, ...) {
  char buf[256];

  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  static const char* volatile str = buf;

  return len;
}

struct MidiEventHandler {
 private:
  struct DriveData {
    uint8_t id;
    uint8_t note;
  };

  std::vector<DriveData> m_queue;

 public:
  MidiEventHandler(uint8_t num_drives) {
    for (uint8_t i = 0; i < num_drives; ++i) m_queue.push_back({i, 0});
  }

 public:
  void processEvent(StepperSynth& synth, const uint8_t packet[4]) {
    uint8_t status = packet[1] & 0xF0;
    uint8_t note = packet[2] & 0x7F;
    uint8_t vel = packet[3] & 0x7F;

    if (status == 0x80 || ((status == 0x90) && vel == 0))
      for (int i = 0; i < m_queue.size(); ++i)
        if (m_queue[i].note == note) {
          synth.playNote(m_queue[i].id, 0);
          m_queue.push_back({m_queue[i].id, 0});
          m_queue.erase(m_queue.begin() + i);
          break;
        }
    if (status == 0x90 && vel != 0) {
      synth.playNote(m_queue[0].id, note);
      m_queue.push_back({m_queue[0].id, note});
      m_queue.erase(m_queue.begin());
    }
  }
};

int main(void) {
  setup();
  tusb_init();
  tud_init(0);

  std::vector<StepperSynth::Pins> pins{{{GPIOA, 0}, {GPIOA, 1}},
                                       {{GPIOA, 2}, {GPIOA, 3}}};

  synth  = std::make_unique<StepperSynth>(pins);
  pindbg = std::make_unique<GPIOPinOut>(PinBase{GPIOA, 4});

  bool fx = true;
  synth->fx(0).stutter_period_us = fx ? 1200 : 0;
  synth->fx(0).stutter_duration_ms = fx ? 60 : 0;
  synth->fx(1).stutter_period_us = fx ? 1200 : 0;
  synth->fx(1).stutter_duration_ms = fx ? 60 : 0;

  size_t i = 0;
  uint8_t prev_note = 0;

  MidiEventHandler handler(pins.size());

  while (1) {
    tud_task();

    if (tud_midi_mounted()) {
      uint8_t packet[4];
      auto av = tud_midi_available();

      if (av >= 4) {
        assert(tud_midi_packet_read(packet));

        handler.processEvent(*synth, packet);
      }
    }

    bool gitr[] = {true, true, false, false, true, true, false, false};
    bool bass[] = {false, true, true, false, false, true, true, false};
    bool vibr[] = {false, false, false, false, true, true, true, false};

    // shoveit(gitr[i % 8], bass[i % 8], vibr[i % 8]);

    i++;
  }
}