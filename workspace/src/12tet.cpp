#include "stepper_synth/12tet.hpp"

#include <math.h>

#include "stepper_synth/midi.hpp"
#include "stepper_synth/config.hpp"

static float g_note_powers_table[12] = {
    powf(2, 0.f / 12), powf(2, 1.f / 12),  powf(2, 2.f / 12),
    powf(2, 3.f / 12), powf(2, 4.f / 12),  powf(2, 5.f / 12),
    powf(2, 6.f / 12), powf(2, 7.f / 12),  powf(2, 8.f / 12),
    powf(2, 9.f / 12), powf(2, 10.f / 12), powf(2, 11.f / 12),
};

uint32_t notePeriodUs12tet(uint8_t note) {
  // 12TET: T = T_base * 2^{(base_note - note) / 12}
  // Algorithm:
  // Split the distance between notes into
  // a number of whole octaves and a remainder.
  // Assume note < base_note:
  //   base_note - note = 12*m + r
  // 12TET conversion:
  //   T = T_base * c_whole * c_sub, note < base_note:
  //   c_whole = 2 ^ {m} = 1 << dist / 12 - integer
  //   c_sub   = 2 ^ {r / 12}, r < 12     - tabulated function
  // In case note > base_note,
  //   T = T_base / (c_whole * c_sub)

  int16_t dist = MIDI::Notes::SYNTH_BASE_NOTE - note;

  uint32_t c_whole = 1 << abs(dist) / 12;
  float c_sub = g_note_powers_table[abs(dist) % 12];

  float t_us = 1e6f / SYNTH_BASE_FREQ;
  if (dist >= 0)
    t_us *= c_whole * c_sub;
  else
    t_us /= c_whole * c_sub;

  return t_us;
}