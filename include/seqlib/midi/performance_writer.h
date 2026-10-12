// SPDX-License-Identifier: MIT

// Standard MIDI File output for a Performance.

#pragma once

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "seqlib/midi/performance.h"

namespace seqlib::midi
{

// MIDI channel allocation, bend ranges and conversion warnings.
struct MidiReport
{
    int channels = 0;                      // MIDI channels used
    std::array<int, 16> bend_ranges{};     // each track's pitch bend range (RPN 0), in semitones
    uint32_t cut_notes = 0;                // overlapping notes cut short for lack of channels
    std::set<uint32_t> channel_10_presets; // presets that MIDI channel 10 plays
    std::vector<std::string> warnings;
};

// Where an event goes in MIDI time, given its tick's frame (Performance::frame_ticks).
enum class FrameTiming
{
    // At its frame's start, but never after its tick. A loop marker moves an event at or after it no earlier than the
    // marker.
    kNotAfterTick,

    // At its frame's start, even after its tick, for a player that runs a tick at the start of a later frame. A loop
    // marker also moves an event before it no later than the marker.
    kFrameStart,
};

// Options for WriteMidi().
struct MidiOptions
{
    FrameTiming frame_timing = FrameTiming::kNotAfterTick;
};

// Writes Format 1 MIDI: a conductor track for tempo and loop markers, plus a track for each sequence track/channel
// pair. Uses at least 7680 ticks per quarter note, with an integer number of MIDI ticks per performance tick. Places
// events at the start of their frame (Performance::frame_ticks). SoundFont preset i uses CC0 = i / 128 and
// program = i % 128. Fills `report` with allocation details and warnings.
//
// Each part of a track (TrackEvent::part) that plays a note gets a channel of its own while channels last. A part with
// notes but no channel plays on the channels of the track's first part. The events of a part without notes are dropped.
// A MIDI channel supports one active note per key. Repeated strikes that overlap therefore get extra channels (layers)
// with matching controllers, up to the 16-channel limit. Allocates in track order, and uses channel 10 (index 9,
// General MIDI drums) last. Synths use bank 128 for that channel regardless of bank select. Records channel 10's
// presets in `report`. sf2::AddChannel10Copy then copies each of them to that bank.
std::vector<uint8_t> WriteMidi(const Performance& perf, const std::string& title, MidiReport& report,
                               const MidiOptions& options = {});

} // namespace seqlib::midi
