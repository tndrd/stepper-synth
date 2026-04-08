#include "synth.hpp"

#include <cmath>
#include <vector>

#include "midi.hpp"
#include "stm32f407xx.h"

StepperSynth::Drive::Drive(const Pins& pins)
    : m_stp{pins.stp}, m_dir{pins.dir}, m_period_us{0}, m_counter{0} {}

StepperSynth::StepperSynth(const std::vector<Pins>& pin_array)
    : m_base_note{MIDI::Notes::A4},
      m_base_freq{440.0l},
      m_fine_tune{0},
      m_transpose{0},
      m_pitch_wheel{0},
      m_velo_threshold{100},
      m_enable{true} {
  for (const auto& pins : pin_array) m_drives.emplace_back(pins);
}

void StepperSynth::enable(bool value) { m_enable = value; }

void StepperSynth::update(uint16_t dt_us) {
  assert(dt_us);

  for (auto& drive : m_drives) {
    bool value = false;
    uint16_t period = drive.m_period_us / dt_us;

    if (m_enable && period != 0) {
      drive.m_counter += 1;
      drive.m_counter %= period;

      // 50% duty cycle
      value = drive.m_counter < (period / 2);
    }

    drive.m_stp.write(value);
  }
}

void StepperSynth::noteOn(uint8_t id, uint8_t note, uint8_t velocity) {
  assert(id < m_drives.size());
  auto& drive = m_drives[id];

  if (velocity == 0)
    drive.m_period_us = 0;
  else {
    if (velocity >= m_velo_threshold) drive.m_dir.toggle();
    drive.m_period_us = getNotePeriodUs(note);
  }
}

uint16_t StepperSynth::getNotePeriodUs(uint8_t note) {
  double power = double(m_base_note - note - m_transpose) + m_pitch_wheel;
  double dt = 1e6l / (m_base_freq + m_fine_tune);

  return dt * pow(2, power / 12);
}
