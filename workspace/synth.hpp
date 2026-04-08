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

 private:
  struct Drive {
    GPIOPinOut m_stp;
    GPIOPinOut m_dir;

    uint16_t m_period_us;
    uint16_t m_counter;

    Drive(const Pins& pins);
  };

 private:
  uint8_t m_base_note;
  double m_base_freq;
  double m_fine_tune;

 private:
  int8_t m_transpose;
  double m_pitch_wheel;
  uint8_t m_velo_threshold;

 private:
  bool m_enable;
  std::vector<Drive> m_drives;

 public:
  StepperSynth(const std::vector<Pins>& pin_array);

  void enable(bool value);
  void update(uint16_t dt_us);
  void noteOn(uint8_t id, uint8_t note, uint8_t velocity);

 private:
  // 12TET conversion
  uint16_t getNotePeriodUs(uint8_t note);
};