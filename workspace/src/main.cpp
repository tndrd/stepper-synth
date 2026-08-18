#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "stepper_synth/12tet.hpp"
#include "stepper_synth/board.hpp"
#include "stepper_synth/midi.hpp"
#include "stepper_synth/note_scheduler.hpp"
#include "stepper_synth/pwm_generator.hpp"
#include "stepper_synth/usb.hpp"
#include "task.h"

PWMGenerator* volatile s_pwm_gen_ptr = nullptr;

struct AppContext {
  PWMGenerator* gen;
};

extern "C" void TIM2_IRQHandler(void) {
  // Interrupt is enabled after s_pwm_gen initialization
  assert(s_pwm_gen_ptr != nullptr);
  s_pwm_gen_ptr->isr();
}

void handleMidiEvent(PWMGenerator& gen, NoteScheduler& sched,
                     const MIDI::NoteEvent& event) {
  if (event.kind == event.kNoteOff) {
    auto result = sched.handleNoteOff(event.note);
    if (result.outcome != NoteScheduler::Outcome::kNoVoice)
      gen.setPWMPeriod(result.voice_index, 0);
  }

  else if (event.kind == event.kNoteOn) {
    auto result = sched.handleNoteOn(event.note);
    uint32_t period = notePeriodUs12tet(event.note);
    gen.setPWMPeriod(result.voice_index, period);
  }
}

static void midiTask(void* param) {
  assert(param);
  AppContext* ctx = static_cast<AppContext*>(param);
  assert(ctx->gen);
  
  PWMGenerator& gen = *ctx->gen;
  NoteScheduler sched;

  uint8_t packet[4];

  for (;;) {
    tud_task();

    if (tud_midi_mounted() && tud_midi_available()) {
      if (tud_midi_packet_read(packet)) {
        auto event = MIDI::parsePacket(packet);
        handleMidiEvent(gen, sched, event);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

int main(void) {
  // Clocks, PWR, Flash, SysTick
  // Interrupts are disabled
  Clocks clk = boardInit();

  // Configure periphery and initialize vars
  // that are shared with ISRs
  usbInit();

  PWMGenerator::Pin pins[SYNTH_NUM_VOICES] = {
      {GPIOA, 0, 1},  // TIM2_CH1
      {GPIOA, 1, 1},  // TIM2_CH2
      {GPIOA, 2, 1},  // TIM2_CH3
      {GPIOA, 3, 1}   // TIM2_CH4
  };

  static PWMGenerator s_pwm_gen{TIM2, clk.apb1tim, pins, SYNTH_NUM_VOICES};
  s_pwm_gen_ptr = &s_pwm_gen;

  NVIC_SetPriorityGrouping(0);
  NVIC_SetPriority(TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY - 2);
  NVIC_SetPriority(OTG_FS_IRQn,
                   configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY - 1);

  // Enable NVIC
  boardEnableIRQ();

  // Start periphery
  s_pwm_gen.start();

  AppContext ctx = {s_pwm_gen_ptr};
  // Start application task
  BaseType_t ok = xTaskCreate(
      midiTask,              // тело
      "midi",                // имя (для отладки/stack overflow hook)
      512,                   // размер стека в СЛОВАХ (=2KB) — с запасом, потом ужмёшь по high-water-mark
      &ctx,                // параметр
      3,                     // приоритет (из configMAX_PRIORITIES=5)
      nullptr                // handle не нужен пока
  );
  configASSERT(ok == pdPASS);

  NoteScheduler sched;
  uint8_t packet[4];

  vTaskStartScheduler();
  configASSERT(0 && "Scheduler failed to start");
}