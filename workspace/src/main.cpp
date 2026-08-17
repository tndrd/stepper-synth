#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "stepper_synth/12tet.hpp"
#include "stepper_synth/board.hpp"
#include "stepper_synth/midi.hpp"
#include "stepper_synth/note_scheduler.hpp"
#include "stepper_synth/shared.hpp"
#include "stepper_synth/usb.hpp"

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

int main(void) {
  // Clocks, PWR, Flash, SysTick
  // Interrupts are disabled
  Clocks clk = boardInit();

  // Configure periphery and initialize vars
  // that are shared with ISRs
  usbInit();

  PWMGenerator::Pin pins [SYNTH_NUM_VOICES] = {
    {GPIOA, 0, 1}, // TIM2_CH1
    {GPIOA, 1, 1}, // TIM2_CH2
    {GPIOA, 2, 1}, // TIM2_CH3
    {GPIOA, 3, 1}  // TIM2_CH4
  };

  static PWMGenerator s_pwm_gen{TIM2, clk.apb1tim, pins, SYNTH_NUM_VOICES};
  s_pwm_gen_ptr = &s_pwm_gen;
  g_ms_ticks = 0;

  // Enable NVIC
  boardEnableIRQ();

  // Start periphery and SysTick
  s_pwm_gen.start();
  boardSystickStart();

  // Start application task
  NoteScheduler sched;
  uint8_t packet[4];

  while (1) {
    tud_task();

    if (tud_midi_mounted() && tud_midi_available()) {
      assert(tud_midi_packet_read(packet));

      auto event = MIDI::parsePacket(packet);
      handleMidiEvent(s_pwm_gen, sched, event);
    }
  }
}