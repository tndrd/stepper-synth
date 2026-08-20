#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// Application includes
#include "stepper_synth/12tet.hpp"
#include "stepper_synth/board.hpp"
#include "stepper_synth/midi.hpp"
#include "stepper_synth/note_scheduler.hpp"
#include "stepper_synth/pwm_generator.hpp"
#include "stepper_synth/usb.hpp"

PWMGenerator* volatile sp_pwmGen = nullptr;

extern "C" void TIM2_IRQHandler(void) {
  // Interrupt is enabled after s_pwm_gen initialization
  assert(sp_pwmGen != nullptr);
  sp_pwmGen->isr();
}

struct UsbMidiTaskContext {
  QueueHandle_t eventQueue;
};

static void usbMidiTask(void* param) {
  assert(param);
  auto ctx = static_cast<UsbMidiTaskContext*>(param);

  QueueHandle_t eventQueue = ctx->eventQueue;

  uint8_t packet[MIDI::kPacketLen];

  for (;;) {
    tud_task();

    if (!tud_midi_mounted()) continue;
    if (!tud_midi_available()) continue;

    while (tud_midi_available() >= MIDI::kPacketLen) {
      assert(tud_midi_packet_read(packet));
      MIDI::NoteEvent event = MIDI::parsePacket(packet);

      if (event.kind == MIDI::NoteEvent::kNone) continue;

      BaseType_t ok = xQueueSend(eventQueue, &event, 0);
      assert(ok = pdPASS);
    }
  }
}

struct NoteHandlerTaskContext {
  QueueHandle_t eventQueue;
  PWMGenerator* pwmGenerator;
  NoteScheduler* noteScheduler;
};

static void noteHandlerTask(void* param) {
  assert(param);
  auto ctx = static_cast<NoteHandlerTaskContext*>(param);

  QueueHandle_t eventQueue = ctx->eventQueue;

  assert(ctx->pwmGenerator);
  PWMGenerator& pwmGenerator = *ctx->pwmGenerator;

  assert(ctx->noteScheduler);
  NoteScheduler& noteScheduler = *ctx->noteScheduler;

  MIDI::NoteEvent event;

  for (;;) {
    auto ok = xQueueReceive(eventQueue, &event, portMAX_DELAY);
    assert(ok == pdPASS);

    if (event.kind == event.kNoteOff) {
      auto result = noteScheduler.handleNoteOff(event.note);
      if (result.outcome != NoteScheduler::Outcome::kNoVoice)
        pwmGenerator.setChannelPeriod(result.voice_index, 0);
    }

    else if (event.kind == event.kNoteOn) {
      auto result = noteScheduler.handleNoteOn(event.note);
      uint32_t period = notePeriodUs12tet(event.note);
      pwmGenerator.setChannelPeriod(result.voice_index, period);
    }
  }
}

int main(void) {
  // Configure clocks, PWR, Flash
  // Interrupts remain disabled
  Clocks clk = boardInit();

  // Configure periphery and initialize vars
  // that are shared between tasks and/or ISRs
  // Interrupts remain disabled
  usbInit();

  PWMGenerator::Pin pins[SYNTH_NUM_VOICES] = {
      {GPIOA, 0, 1},  // TIM2_CH1
      {GPIOA, 1, 1},  // TIM2_CH2
      {GPIOA, 2, 1},  // TIM2_CH3
      {GPIOA, 3, 1}   // TIM2_CH4
  };

  static PWMGenerator s_pwmGenerator{TIM2, clk.apb1tim, pins, SYNTH_NUM_VOICES};
  sp_pwmGen = &s_pwmGenerator;

  static NoteScheduler s_noteScheduler;

  // Configure interrupt priorities
  NVIC_SetPriorityGrouping(0);  // FreeRTOS requirement

  NVIC_SetPriority(TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY +
                                  SYNTH_PWM_TIM_IRQ_PRIORITY_REL);
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY +
                                    SYNTH_USB_OTG_IRQ_PRIORITY_REL);

  // Enable interrupts
  boardEnableIRQ();

  // Start periphery after all initialization is done
  s_pwmGenerator.start();

  // Create tasks
  BaseType_t ok;

  QueueHandle_t eventQueue =
      xQueueCreate(SYNTH_NOTE_EVENT_QUEUE_SIZE, sizeof(MIDI::NoteEvent));

  static NoteHandlerTaskContext noteHandlerCtx;
  noteHandlerCtx.eventQueue = eventQueue;
  noteHandlerCtx.noteScheduler = &s_noteScheduler;
  noteHandlerCtx.pwmGenerator = &s_pwmGenerator;

  ok = xTaskCreate(noteHandlerTask, "Note handler",
                   SYNTH_NOTE_HANDLER_TASK_STACK_SIZE, &noteHandlerCtx,
                   SYNTH_NOTE_HANDLER_TASK_PRIORITY, nullptr);
  assert(ok == pdPASS);

  static UsbMidiTaskContext usbMidiCtx;
  usbMidiCtx.eventQueue = eventQueue;

  ok = xTaskCreate(usbMidiTask, "USB-MIDI", SYNTH_USB_MIDI_TASK_STACK_SIZE,
                   &usbMidiCtx, SYNTH_USB_MIDI_TASK_PRIORITY, nullptr);
  assert(ok == pdPASS);

  // Launch scheduler
  vTaskStartScheduler();
  assert(0 && "Scheduler failed to start");
}