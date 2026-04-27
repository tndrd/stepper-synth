#include "synth.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

#include "midi.hpp"
#include "stm32f407xx.h"

StepperSynth::Drive::Drive(const Pins& pins)
    : m_stp{pins.stp}, m_dir{pins.dir}, m_note{0}, m_counter{0} {}

StepperSynth::StepperSynth(const std::vector<Pins>& pin_array)
    : m_base_note{MIDI::Notes::A4},
      m_base_freq{440.0},
      m_fine_tune{0},
      m_transpose{0},
      m_enable{true} {
  for (const auto& pins : pin_array) m_drives.emplace_back(pins);
  for (int i = 0; i < 12; ++i) m_note_coeffs[i] = pow(2, float(i) / 12);
}

void StepperSynth::enable(bool value) { m_enable = value; }

void StepperSynth::update(uint16_t dt_us) {
  assert(dt_us);

  for (auto& drive : m_drives) {
    if (!m_enable || drive.m_note == 0) {
      drive.m_stp.write(false);
      drive.m_counter++;
      continue;
    }
    
    if (drive.m_counter == ~uint32_t(0))
      drive.m_counter = 0;

    // Vibrato modulation
    float mod = drive.m_fx.vibrato_depth;
    if (drive.m_fx.vibrato_period_ms) {
      uint32_t period = (drive.m_fx.vibrato_period_ms * 1000) / dt_us;
      float rad = float(drive.m_counter % period) / float(period);
      mod *= sinf(2.f * M_PI * rad);
    }

    bool value;
    uint16_t period;
    
    if (drive.m_counter * dt_us < drive.m_fx.stutter_duration_ms * 1000)
    {
      period = drive.m_fx.stutter_period_us;
      value = ((drive.m_counter * dt_us) % period) < (period / 2);
      drive.m_dir.write(value);
    }
    
    period = getNotePeriodUs(drive.m_note, mod);
    value = ((drive.m_counter * dt_us) % period) < (period / 2);

    drive.m_stp.write(value);
    drive.m_counter += 1;
  }
}

void StepperSynth::playNote(uint8_t id, uint8_t note) {
  assert(id < m_drives.size());
  m_drives[id].m_note = note;
  m_drives[id].m_counter = ~uint32_t(0);
}

auto StepperSynth::fx(uint8_t id) -> FX& {
  assert(id < m_drives.size());
  return m_drives[id].m_fx;
}

uint16_t StepperSynth::getNotePeriodUs(uint8_t note, float mod) {
  float dt = 1e6l / (m_base_freq + m_fine_tune);

  uint8_t dist = m_base_note - note - m_transpose;
  assert(dist >= 0);
  float t_us = dt * (1 << dist / 12) * m_note_coeffs[dist % 12] * powf(2, mod / 12);;
  float freq = 1e6f / t_us;

  return t_us;
}
