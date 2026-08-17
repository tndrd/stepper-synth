#include "stepper_synth/midi.hpp"

#include <assert.h>
namespace MIDI {
NoteEvent parsePacket(const uint8_t packet[kPacketLen]) {
  assert(packet);

  switch (packet[0] & kCinMask) {
    case kCinNoteOn: {
      uint8_t note = packet[2] & kDataMask;
      uint8_t velo = packet[3] & kDataMask;
      uint8_t event = velo ? NoteEvent::kNoteOn : NoteEvent::kNoteOff;

      if (velo == 0)
        return {NoteEvent::kNoteOff, note, velo};
      else
        return {NoteEvent::kNoteOn, note, velo};
    }
    case kCinNoteOff: {
      uint8_t note = packet[2] & kDataMask;
      uint8_t velo = packet[3] & kDataMask;

      return {NoteEvent::kNoteOff, note, velo};
    }
    default:
      return {NoteEvent::kNone, 0, 0};
  }
}
} // namespace MIDI