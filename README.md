# stepper-synth

**FreeRTOS**-based STM32F407 polyphonic synthesizer that produces sound using **stepper motors** — the
motors' step frequency is driven to play musical notes. Accepts **USB-MIDI** input,
so it can be played from any MIDI source.

A personal project focused on doing everything close to the metal: manual clock
setup, timer-driven synthesis, FreeRTOS integration and a USB device stack — no vendor HAL for the
core logic.

## Highlights

- **Bare-metal aspects of STM32F407** — manual startup, custom linker script, no HAL for the core logic.
- **Timer-driven synthesis** — 4-channel PWM Generation via a single hardware timer in output compare mode.
- **USB-MIDI** via **TinyUSB** — custom USB descriptors, MIDI class device.
- **FreeRTOS** — multitasking
- **Manual clock configuration** — PLL set up by hand.

## Stack

C++ · CMSIS · FreeRTOS · TinyUSB (USB-MIDI) · CMake · arm-none-eabi-gcc

## Layout

- `workspace/` — synth core, MIDI parsing, USB descriptors, FreeRTOS config & hooks, `main`
- `devices/` — linker script
- `common/` — syscalls / sysmem stubs
