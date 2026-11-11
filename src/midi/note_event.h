// SPDX-License-Identifier: MIT

// The decoding of note-ons and note-offs, for the MIDI reader and TrackBuilder::Retime. The header is private and isn't
// installed.

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "seqlib/midi/encoder.h"

namespace seqlib::detail
{

// A note-on or a note-off. A note-on of velocity 0 counts as a note-off.
struct NoteEvent
{
    bool on = false;
    int channel = 0;
    int key = 0;
    int velocity = 0;
};

// Returns the note-on or the note-off that the event `bytes` is, or none for any other event. The bytes start with the
// status byte. A note-on or a note-off has both of its data bytes.
inline std::optional<NoteEvent> AsNoteEvent(std::span<const uint8_t> bytes)
{
    const int kind = bytes[0] & 0xF0;
    if (kind != midi::status::kNoteOff && kind != midi::status::kNoteOn)
    {
        return std::nullopt;
    }

    const int velocity = bytes[2];
    return NoteEvent{kind == midi::status::kNoteOn && velocity > 0, bytes[0] & 0x0F, bytes[1], velocity};
}

} // namespace seqlib::detail
