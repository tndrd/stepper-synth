#include "stepper_synth/note_scheduler.hpp"

NoteScheduler::Result NoteScheduler::handleNoteOn(uint8_t note) {
  assert(note != kFreeNote);

  // Retrigger & update timestamp
  for (uint8_t i = 0; i < SYNTH_NUM_VOICES; ++i) {
    if (m_voices[i].note == note) {
      m_voices[i].update_ts = ++m_counter;
      return {Outcome::kRetrigger, i};
    }
  }

  // Greedily search for voices with the smallest timestamps,
  // simultaneously for both free and busy voices
  uint8_t lr_free_index = 0xFF;
  uint32_t lr_free_ts = std::numeric_limits<uint32_t>::max();
  uint8_t lr_busy_index = 0;
  uint32_t lr_busy_ts = std::numeric_limits<uint32_t>::max();

  for (uint8_t i = 0; i < SYNTH_NUM_VOICES; ++i) {
    const uint32_t ts = m_voices[i].update_ts;
    if (m_voices[i].note == kFreeNote) {
      if (ts < lr_free_ts) {
        lr_free_ts = ts;
        lr_free_index = i;
      }
    } else {
      if (ts < lr_busy_ts) {
        lr_busy_ts = ts;
        lr_busy_index = i;
      }
    }
  }

  // If there is indeed a free voice then allocate it
  // Otherwise, return a busy voice
  bool has_free = (lr_free_index != kMaxIndex);
  uint8_t chosen_voice_index = has_free ? lr_free_index : lr_busy_index;
  Outcome outcome = has_free ? Outcome::kAllocated : Outcome::kStolen;

  m_voices[chosen_voice_index].note = note;
  m_voices[chosen_voice_index].update_ts = ++m_counter;

  return {outcome, chosen_voice_index};
}

NoteScheduler::Result NoteScheduler::handleNoteOff(uint8_t note) {
  assert(note != kFreeNote);

  // Greedily search for a voice with the smallest timestamp
  uint8_t chosen_voice_index = kMaxIndex;
  uint32_t chosen_voice_ts = std::numeric_limits<uint32_t>::max();

  for (uint8_t i = 0; i < SYNTH_NUM_VOICES; ++i) {
    if (m_voices[i].note == note && m_voices[i].update_ts < chosen_voice_ts) {
      chosen_voice_ts = m_voices[i].update_ts;
      chosen_voice_index = i;
    }
  }

  // If found
  if (chosen_voice_index != kMaxIndex) {
    m_voices[chosen_voice_index].note = kFreeNote;
    return {Outcome::kFreed, chosen_voice_index};
  }
  return {Outcome::kNoVoice, kMaxIndex};
}

NoteScheduler::Snapshot NoteScheduler::getSnapshot() const {
  Snapshot s;
  for (uint8_t i = 0; i < SYNTH_NUM_VOICES; ++i) s.notes[i] = m_voices[i].note;

  return s;
}
