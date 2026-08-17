#pragma once

#include <assert.h>
#include <stdint.h>

#include <limits>

#include "stepper_synth/config.hpp"

class NoteScheduler {
 public:
  struct Snapshot {
    uint8_t notes[SYNTH_NUM_VOICES];
  };

 private:
  struct Voice {
    uint8_t note = kFreeNote;
    uint32_t update_ts = 0;
  };

 public:
  static constexpr uint8_t kFreeNote = 0xFF;
  static constexpr uint8_t kMaxIndex = 0xFF;

  enum class Outcome: uint8_t {
      kNoVoice,
      kStolen,
      kRetrigger,
      kAllocated,
      kFreed
  };

  struct Result {
    Outcome outcome;
    uint8_t voice_index;
  };

 private:
  Voice m_voices[SYNTH_NUM_VOICES];
  uint32_t m_counter = 0;

 public:
  /// @brief Allocate a voice for the given note.
  /// @return voice index in [0, SYNTH_NUM_VOICES).
  /// @note Resolution order:
  ///  1. If there is a voice that plays the same note,
  ///     then that voice's index would be returned (aka 'Retrigger').
  ///  2. If there is a free voice, then the LR (least recently)
  ///     used free voice's index would be returned.
  ///  3. Otherwise, the LR used busy voice would be returned (aka 'Stealing').
  Result handleNoteOn(uint8_t note);

  /// @brief Release the voice playing the given note.
  /// @returns voice index in [0, SYNTH_NUM_VOICES), or kNoVoice if the note is
  /// not playing.
  /// @note If multiple voices somehow play the same note, the LRU one wins.
  Result handleNoteOff(uint8_t note);

  /// @returns snapshot suitable for publishing to other tasks.
  Snapshot getSnapshot() const;
};
