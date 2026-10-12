// SPDX-License-Identifier: MIT

// A performance: the tempo, the notes, the controllers and the presets that a player plays on a sequence's tracks.
// performance_writer.h writes it as MIDI.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace seqlib::midi
{

// Event types, with the fields used by each. A kind above the named ones sets a controller (see ControlKind). A switch
// over EventKind can't name such a kind as a case without a -Wswitch warning. Test for it in the default branch.
enum class EventKind : uint8_t
{
    kNoteOn,     // key, value: velocity
    kNoteOff,    // key
    kPreset,     // preset: a SoundFont preset's index, sent as CC0 = preset / 128 and program = preset % 128
    kVolume,     // value: 0-127 (CC7)
    kExpression, // value: 0-127 (CC11)
    kPan,        // value: 0-127, 64 centre (CC10)
    kPitch,      // semitones: the pitch bend
    kModulation, // value: 0-127 (CC1)
    kReverb,     // value: reverb send (CC91)
    kChorus,     // value: chorus send (CC93)
    kSoundOff,   // cut every sound on the channel at once (CC120)
};

// The first kind that sets a controller.
constexpr uint8_t kFirstControlKind = 128;

// Returns the kind of an event that sets controller `controller` (0-127) to its value. These kinds sort after the named
// ones, in the order of their controllers.
constexpr EventKind ControlKind(uint8_t controller)
{
    return static_cast<EventKind>(kFirstControlKind + (controller & 0x7f));
}

// Returns true if events of `kind` set a controller.
constexpr bool IsControlKind(EventKind kind)
{
    return static_cast<uint8_t>(kind) >= kFirstControlKind;
}

// Returns the controller that events of `kind` set, for a kind that ControlKind() returns.
constexpr uint8_t ControllerOf(EventKind kind)
{
    return static_cast<uint8_t>(static_cast<uint8_t>(kind) - kFirstControlKind);
}

// An event on a track's timeline.
//
// MIDI has one set of controllers per channel. A player's channels can keep other settings than the track's later
// notes, such as a release tail that the track has detached. Such notes go on another part of the track. The MIDI
// writer gives each part that plays a note another channel while channels last. The writer drops the events of a part
// without notes. The track's changes go to the part that its new notes go to. A note's events go to its part.
//
// An event belongs to the player's frame that its tick is processed in (Performance::frame_ticks). An event between
// ticks, such as a pitch that a sweep reaches in a frame between them, comes `after` its tick by the fraction of a tick
// at which its frame starts. An event after the last tick comes as many ticks after it as its frame starts, at the last
// tempo.
struct TrackEvent
{
    uint32_t tick = 0;
    EventKind kind = EventKind::kNoteOn;
    uint8_t key = 0; // 0-127
    uint8_t value = 0;
    uint32_t preset = 0;
    float semitones = 0.0f;
    uint32_t note = 0; // NoteOn/NoteOff ID, starting at 1; distinguishes overlapping notes of the same key
    uint8_t part = 0;  // the part of the track, 0 for its first
    double after = 0;  // 0, between 0 and 1 for a frame between ticks, or more after the last tick
};

// A performance's tempo, its tracks' events, its main loop and its length.
struct Performance
{
    // A tempo change.
    struct Tempo
    {
        uint32_t tick = 0;
        double bpm = 120.0; // quarter notes (`timebase` ticks) per minute
    };

    // The main loop, in ticks.
    struct Loop
    {
        uint32_t start = 0, end = 0;
    };

    uint32_t timebase = 48; // ticks per quarter note
    std::vector<Tempo> tempo;
    std::array<std::vector<TrackEvent>, 16> tracks;

    // Each track's name for its MIDI tracks. An empty name gives "Track <n>".
    std::array<std::string, 16> track_names;

    // The start of each tick's frame, in fractional ticks. MIDI events go at these positions. Without an entry, an
    // event goes at its tick.
    std::vector<double> frame_ticks;

    uint32_t end_tick = 0;
    std::optional<Loop> loop; // if it loops
};

// Returns the number of notes that `perf` plays.
uint32_t NoteCount(const Performance& perf);

} // namespace seqlib::midi
