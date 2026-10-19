// SPDX-License-Identifier: MIT

// Standard MIDI File encoding: variable-length quantities, the header and track chunks, the bytes of meta and tempo
// events, and an encoder for a track's events in time order.

#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace seqlib::midi
{

// Appends `v` as a variable-length quantity: 7 bits a byte, most significant first, with the top bit set on all but the
// last.
void AppendVlq(std::vector<uint8_t>& out, uint32_t v);

// Appends the header chunk (MThd) of a file in `format` with `tracks` tracks, timed in ticks of 1/`division` of a
// quarter note.
void AppendHeader(std::vector<uint8_t>& out, uint16_t format, uint16_t tracks, uint16_t division);

// Appends a track chunk (MTrk) whose data is `body`.
void AppendTrack(std::vector<uint8_t>& out, std::span<const uint8_t> body);

// Returns the bytes of a meta event of `type` that holds `text`: 0xFF, the type, the text's length and the text.
std::vector<uint8_t> MetaEvent(uint8_t type, const std::string& text);

// Returns the bytes of a tempo event of `micros_per_quarter` microseconds per quarter note. The event holds only the
// value's low 24 bits.
std::vector<uint8_t> TempoEvent(uint32_t micros_per_quarter);

// Builds the data of a track chunk from events in time order, with delta times in MIDI ticks.
class TrackEncoder
{
public:
    void Event(uint64_t tick, std::initializer_list<uint8_t> bytes);

    // Adds an event whose bytes are already encoded, such as a stored meta event.
    void Event(uint64_t tick, std::span<const uint8_t> bytes);

    void Meta(uint64_t tick, uint8_t type, const std::string& text);

    // A tempo change. MIDI gives tempo in microseconds per quarter note.
    void Tempo(uint64_t tick, double bpm);

    // Ends the track at `tick`, or at the last event's tick when that comes later.
    void End(uint64_t tick);

    // Returns the track's data, without the chunk's ID and size.
    const std::vector<uint8_t>& Data() const;

    // Appends the track chunk: its ID, its size and its data.
    void AppendTo(std::vector<uint8_t>& out) const;

private:
    // Largest delta time that fits MIDI's four-byte variable-length encoding.
    static constexpr uint32_t kMaxDelta = 0x0fffffff;

    // Writes the delta from the previous event. Clamps a time before that event to the event's tick. A sequence that
    // jumps back and then finishes can end before its loop-end marker. Splits oversized deltas with empty text events.
    void Delta(uint64_t tick);

    std::vector<uint8_t> data_;
    uint64_t last_ = 0; // the last event's MIDI tick
};

} // namespace seqlib::midi
