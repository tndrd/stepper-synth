#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

// FreeRTOS includes
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

// Application includes
#include "stepper_synth/12tet.hpp"
#include "stepper_synth/board.hpp"
#include "stepper_synth/midi.hpp"
#include "stepper_synth/note_scheduler.hpp"
#include "stepper_synth/pwm_generator.hpp"
#include "stepper_synth/usb.hpp"

static PWMGenerator* volatile sp_pwmGen = nullptr;
static volatile bool* sp_connectedFlag = nullptr;

extern "C" void tud_mount_cb(void) {
  // Interrupt is enabled after sp_connectedFlag initialization
  assert(sp_connectedFlag);
  *sp_connectedFlag = true;
}
extern "C" void tud_suspend_cb(bool) {
  // Interrupt is enabled after sp_connectedFlag initialization
  assert(sp_connectedFlag);
  *sp_connectedFlag = false;
}

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

  uint8_t packet[MIDI::kPacketLen];

  while (1) {
    tud_task();

    if (!tud_midi_mounted()) continue;

    while (tud_midi_available() >= MIDI::kPacketLen) {
      assert(tud_midi_packet_read(packet));
      MIDI::NoteEvent event = MIDI::parsePacket(packet);

      if (event.kind == MIDI::NoteEvent::kNone) continue;

      BaseType_t ok = xQueueSend(ctx->eventQueue, &event, 0);
      assert(ok = pdPASS);
    }
  }
}

struct NoteHandlerTaskContext {
  PWMGenerator* pwmGenerator;
  QueueHandle_t eventQueue;
  QueueHandle_t snapshotQueue;
  TimerHandle_t idleTimer;
  volatile bool* idleFlag;
  // GPIOPinOut* enablePin;
};

static void noteHandlerTask(void* param) {
  assert(param);
  auto ctx = static_cast<NoteHandlerTaskContext*>(param);

  assert(ctx->pwmGenerator);
  assert(ctx->idleFlag);
  // assert(ctx->enablePin);

  NoteScheduler noteScheduler;
  MIDI::NoteEvent event;

  xTimerStart(ctx->idleTimer, portMAX_DELAY);

  while (1) {
    auto ok = xQueueReceive(ctx->eventQueue, &event, portMAX_DELAY);
    assert(ok == pdPASS);

    *ctx->idleFlag = false;
    xTimerStop(ctx->idleTimer, portMAX_DELAY);
    // ctx->enablePin->write(1);

    if (event.kind == event.kNoteOff) {
      auto result = noteScheduler.handleNoteOff(event.note);
      if (result.outcome != NoteScheduler::Outcome::kNoVoice)
        ctx->pwmGenerator->setChannelPeriod(result.voice_index, 0);
    }

    else if (event.kind == event.kNoteOn) {
      auto result = noteScheduler.handleNoteOn(event.note);
      uint32_t period = notePeriodUs12tet(event.note);
      ctx->pwmGenerator->setChannelPeriod(result.voice_index, period);
    }

    auto snapshot = noteScheduler.getSnapshot();
    xQueueOverwrite(ctx->snapshotQueue, &snapshot);

    if (noteScheduler.allFree()) xTimerStart(ctx->idleTimer, portMAX_DELAY);
  }
}

struct IdleTimerContext {
  volatile bool* idleFlag;
  // also a
  // GPIOPinOut* enablePin;
};

static void idleTimerCallback(TimerHandle_t timer) {
  auto ctx = static_cast<IdleTimerContext*>(pvTimerGetTimerID(timer));
  assert(ctx);
  assert(ctx->idleFlag);
  // assert(ctx->enablePin)

  *ctx->idleFlag = true;
  // ctx->enablePin->write(0)
}

struct DisplayTaskContext {
  QueueHandle_t snapshotQueue;
  const volatile bool* connectedFlag;
  const volatile bool* idleFlag;
};

struct {
  volatile bool connected;
  volatile bool idle;
  decltype(MIDI::Notes::A4) notes[SYNTH_NUM_VOICES];
} g_display_mock;

static void displayTask(void* param) {
  assert(param);
  auto ctx = static_cast<DisplayTaskContext*>(param);

  assert(ctx->connectedFlag);
  assert(ctx->idleFlag);

  NoteScheduler::Snapshot snapshot;

  while (1) {
    xQueuePeek(ctx->snapshotQueue, &snapshot, 0);

    // MOCK THE DISPLAY START
    memcpy(g_display_mock.notes, snapshot.notes, SYNTH_NUM_VOICES);
    g_display_mock.connected = ctx->connectedFlag;
    g_display_mock.idle = ctx->idleFlag;
    // MOCK THE DISPLAY END

    vTaskDelay(pdMS_TO_TICKS(SYNTH_DISPLAY_UPDATE_PERIOD_MS));
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

  static volatile bool s_connectedFlag = false;
  sp_connectedFlag = &s_connectedFlag;

  // Initialize GpioPinOut enablePin;

  // Configure interrupt priorities
  NVIC_SetPriorityGrouping(0);  // FreeRTOS requirement

  NVIC_SetPriority(TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY +
                                  SYNTH_PWM_TIM_IRQ_PRIORITY_REL);
  NVIC_SetPriority(OTG_FS_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY +
                                    SYNTH_USB_OTG_IRQ_PRIORITY_REL);

  // Enable interrupts
  boardEnableIRQ();

  // Start periphery after all lower-level initialization is done
  s_pwmGenerator.start();

  // Initialize vars shared between tasks
  QueueHandle_t eventQueue =
      xQueueCreate(SYNTH_NOTE_EVENT_QUEUE_SIZE, sizeof(MIDI::NoteEvent));

  QueueHandle_t snapshotQueue =
      xQueueCreate(1, sizeof(NoteScheduler::Snapshot));

  static volatile bool idleFlag = false;

  // Create timer
  static IdleTimerContext idleTimerCtx;
  idleTimerCtx.idleFlag = &idleFlag;
  // idleTimerCtx.enablePin = &enablePin;

  TimerHandle_t idleTimer =
      xTimerCreate("Idle timer", pdMS_TO_TICKS(1000 * SYNTH_IDLE_TIME_S),
                   pdFALSE, &idleTimerCtx, idleTimerCallback);

  // Create tasks
  BaseType_t ok;

  static NoteHandlerTaskContext noteHandlerCtx;

  noteHandlerCtx.pwmGenerator = &s_pwmGenerator;
  noteHandlerCtx.eventQueue = eventQueue;
  noteHandlerCtx.snapshotQueue = snapshotQueue;
  noteHandlerCtx.idleTimer = idleTimer;
  noteHandlerCtx.idleFlag = &idleFlag;
  // noteHandler.enablePin = &enablePin

  ok = xTaskCreate(noteHandlerTask, "Note handler",
                   SYNTH_NOTE_HANDLER_TASK_STACK_SIZE, &noteHandlerCtx,
                   SYNTH_NOTE_HANDLER_TASK_PRIORITY, nullptr);
  assert(ok == pdPASS);

  static UsbMidiTaskContext usbMidiCtx;
  usbMidiCtx.eventQueue = eventQueue;

  ok = xTaskCreate(usbMidiTask, "USB-MIDI", SYNTH_USB_MIDI_TASK_STACK_SIZE,
                   &usbMidiCtx, SYNTH_USB_MIDI_TASK_PRIORITY, nullptr);
  assert(ok == pdPASS);

  static DisplayTaskContext displayTaskCtx;
  displayTaskCtx.snapshotQueue = snapshotQueue;
  displayTaskCtx.connectedFlag = sp_connectedFlag;
  displayTaskCtx.idleFlag = &idleFlag;

  ok = xTaskCreate(displayTask, "Display task", SYNTH_DISPLAY_TASK_STACK_SIZE,
                   &displayTaskCtx, SYNTH_DISPLAY_TASK_PRIORITY, nullptr);
  assert(ok == pdPASS);

  // Launch scheduler
  vTaskStartScheduler();
  assert(0 && "Scheduler failed to start");
}