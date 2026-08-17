# stepper-synth

Bare-metal STM32 synthesizer that produces sound using **stepper motors** — the
motors' step frequency is driven to play musical notes. Accepts **USB-MIDI** input,
so it can be played from any MIDI source.

A personal project focused on doing everything close to the metal: manual clock
setup, timer-driven synthesis, and a USB device stack — no vendor HAL for the
core logic.

## Highlights

- **Bare-metal on STM32F407** — manual startup, custom linker script, no vendor
  framework for the core synth logic.
- **Timer-driven synthesis** — a hardware timer interrupt is used as the synthesis
  tick that toggles the motor steps at the right frequency for each note.
- **USB-MIDI** via **TinyUSB** — custom USB descriptors, MIDI class device.
- **Manual clock configuration** — PLL set up by hand.

## Stack

C++ · bare-metal STM32F407 · TinyUSB (USB-MIDI) · CMake · arm-none-eabi-gcc

## Layout

- `workspace/` — synth core, MIDI parsing, USB descriptors, `main`
- `devices/` — linker script
- `common/` — syscalls / sysmem stubs
