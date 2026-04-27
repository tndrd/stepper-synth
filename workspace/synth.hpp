#pragma once

#include <vector>

#include "common.hpp"
#include "midi.hpp"

struct StepperSynth final {
 public:
  struct Pins {
    PinBase stp;
    PinBase dir;
  };

  struct FX {
    float vibrato_depth = 0;        // In semitones
    uint16_t vibrato_period_ms = 0;  // Period

    uint16_t stutter_period_us = 0;
    uint16_t stutter_duration_ms = 0;
  };

 private:
  struct Drive {
    GPIOPinOut m_stp;
    GPIOPinOut m_dir;

    volatile uint8_t m_note;
    volatile uint32_t m_counter;

    FX m_fx;

    Drive(const Pins& pins);
  };

 private:
  uint8_t m_base_note;
  float m_base_freq;
  float m_fine_tune;

 private:
  int8_t m_transpose;

  // 2^{0/12}, 2^{1/12}, ..., 2^{11/12}
  std::array<float, 12> m_note_coeffs;

 private:
  bool m_enable;
  std::vector<Drive> m_drives;

 public:
  StepperSynth(const std::vector<Pins>& pin_array);

  void enable(bool value);
  void update(uint16_t dt_us);

  void playNote(uint8_t id, uint8_t note);

  FX& fx(uint8_t id);

 private:
  // 12TET conversion
  uint16_t getNotePeriodUs(uint8_t note, float mod);
};