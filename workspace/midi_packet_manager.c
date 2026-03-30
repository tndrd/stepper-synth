#include <stdint.h>
#include <stdbool.h>

#define NOTE_ON 0x90
#define NOTE_OFF 0x80
#define BASE_NOTE_FQ 440.0L
#define BASE_NOTE_N 69

typedef enum {
    MIDI_EVT_NONE = 0,
    MIDI_EVT_NOTE_ON,
    MIDI_EVT_NOTE_OFF
} midi_evt_type_t;

typedef struct {
    midi_evt_type_t type;
    uint8_t note;
    uint8_t velocity;
} midi_note_evt_t;

/*
 * packet[0] = USB-MIDI header
 * packet[1] = MIDI status byte
 * packet[2] = data byte 1
 * packet[3] = data byte 2
 */
bool midi_parse_usb_packet(const uint8_t packet[4], midi_note_evt_t *out)
{
    if (packet == 0 || out == 0) {
        return false;
    }

    uint8_t status = packet[1] & 0xF0;
    uint8_t note   = packet[2] & 0x7F;
    uint8_t vel    = packet[3] & 0x7F;

    out->type = MIDI_EVT_NONE;
    out->note = note;
    out->velocity = vel;

    if (status == NOTE_ON) {
        if (vel == 0) {
            out->type = MIDI_EVT_NOTE_OFF;
        } else {
            out->type = MIDI_EVT_NOTE_ON;
        }
        return true;
    }

    if (status == NOTE_OFF) {
        out->type = MIDI_EVT_NOTE_OFF;
        return true;
    }

    return false;
}

inline uint32_t note_to_period(midi_note_evt_t src) {
  return (uint32_t)(1000000.0L / BASE_NOTE_FQ * powl(2, (BASE_NOTE_N - src.note)));
}