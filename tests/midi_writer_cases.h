// SPDX-License-Identifier: MIT

// Synthetic performances for the performance writer's tests. The files in tests/data/midi hold the expected output of
// each case. They're fixed: a change to them is a change of seqlib's output.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "seqlib/midi/performance_writer.h"

namespace seqlib::test
{

// An event's kind. kFilter, kLowPass and kBiquad set controllers 102, 103 and 104.
enum class CaseKind
{
    kNoteOn,
    kNoteOff,
    kPreset,
    kVolume,
    kExpression,
    kPan,
    kPitch,
    kModulation,
    kReverb,
    kChorus,
    kSoundOff,
    kFilter,
    kLowPass,
    kBiquad,
};

struct CaseEvent
{
    int track = 0;
    uint32_t tick = 0;
    CaseKind kind = CaseKind::kNoteOn;
    uint8_t key = 0;
    uint8_t value = 0;
    uint32_t preset = 0;
    float semitones = 0.0f;
    uint32_t note = 0;
    uint8_t part = 0;
    double after = 0;
};

struct MidiCase
{
    std::string name;
    uint32_t timebase = 48;
    std::vector<std::pair<uint32_t, double>> tempo;
    std::vector<CaseEvent> events;
    std::array<std::string, 16> track_names;
    std::vector<double> frame_ticks;
    uint32_t end_tick = 0;
    bool loops = false;
    uint32_t loop_start = 0, loop_end = 0;
};

// Adds a note on and its note off.
inline void AddNote(MidiCase& c, int track, uint32_t on, uint32_t off, uint8_t key, uint8_t velocity, uint32_t note,
                    uint8_t part = 0)
{
    c.events.push_back({track, on, CaseKind::kNoteOn, key, velocity, 0, 0.0f, note, part});
    c.events.push_back({track, off, CaseKind::kNoteOff, key, 0, 0, 0.0f, note, part});
}

// Adds a note on, or a note off.
inline void AddKey(MidiCase& c, int track, uint32_t tick, CaseKind kind, uint8_t key, uint8_t velocity, uint32_t note,
                   uint8_t part = 0)
{
    c.events.push_back({track, tick, kind, key, velocity, 0, 0.0f, note, part});
}

// Adds an event that sets a value.
inline void AddValue(MidiCase& c, int track, uint32_t tick, CaseKind kind, uint8_t value, uint8_t part = 0)
{
    c.events.push_back({track, tick, kind, 0, value, 0, 0.0f, 0, part});
}

inline void AddPreset(MidiCase& c, int track, uint32_t tick, uint32_t preset, uint8_t part = 0)
{
    c.events.push_back({track, tick, CaseKind::kPreset, 0, 0, preset, 0.0f, 0, part});
}

inline void AddPitch(MidiCase& c, int track, uint32_t tick, float semitones, double after = 0)
{
    c.events.push_back({track, tick, CaseKind::kPitch, 0, 0, 0, semitones, 0, 0, after});
}

// Presets on two tracks, controllers, small bends and a tempo change.
inline MidiCase BasicCase()
{
    MidiCase c;
    c.name = "basic";
    c.tempo = {{0, 120.0}, {96, 150.0}};
    AddPreset(c, 0, 0, 5);
    AddValue(c, 0, 0, CaseKind::kVolume, 100);
    AddValue(c, 0, 0, CaseKind::kPan, 40);
    AddValue(c, 0, 0, CaseKind::kExpression, 120);
    AddKey(c, 0, 0, CaseKind::kNoteOn, 60, 100, 1);
    AddKey(c, 0, 24, CaseKind::kNoteOff, 60, 0, 1);
    AddPitch(c, 0, 24, 1.5f);
    AddValue(c, 0, 30, CaseKind::kModulation, 30);
    AddValue(c, 0, 30, CaseKind::kReverb, 40);
    AddKey(c, 0, 48, CaseKind::kNoteOn, 64, 90, 2);
    AddKey(c, 0, 96, CaseKind::kNoteOff, 64, 0, 2);
    AddPitch(c, 0, 96, 0.0f);
    AddKey(c, 0, 120, CaseKind::kNoteOn, 67, 80, 3);
    AddKey(c, 0, 180, CaseKind::kNoteOff, 67, 0, 3);
    AddPreset(c, 3, 0, 130);
    AddNote(c, 3, 0, 12, 36, 127, 4);
    AddNote(c, 3, 48, 60, 38, 64, 5);
    c.end_tick = 192;

    return c;
}

// Overlapping strikes of one key, a second part, and more tracks than channels: layers, the drum channel last, and
// notes cut short.
inline MidiCase LayersCase()
{
    MidiCase c;
    c.name = "layers";
    c.timebase = 96;
    c.tempo = {{0, 100.0}};
    AddPreset(c, 0, 0, 7);
    AddKey(c, 0, 0, CaseKind::kNoteOn, 60, 100, 1);
    AddKey(c, 0, 10, CaseKind::kNoteOn, 60, 90, 2);
    AddKey(c, 0, 20, CaseKind::kNoteOn, 60, 80, 3);
    AddValue(c, 0, 30, CaseKind::kVolume, 90, 1);
    AddKey(c, 0, 30, CaseKind::kNoteOn, 62, 70, 4, 1);
    AddKey(c, 0, 60, CaseKind::kNoteOff, 62, 0, 4, 1);
    AddKey(c, 0, 100, CaseKind::kNoteOff, 60, 0, 1);
    AddKey(c, 0, 110, CaseKind::kNoteOff, 60, 0, 2);
    AddKey(c, 0, 120, CaseKind::kNoteOff, 60, 0, 3);
    for (int t = 1; t < 14; t++)
    {
        const uint8_t key = static_cast<uint8_t>(40 + t);
        AddPreset(c, t, 0, static_cast<uint32_t>(t));
        AddNote(c, t, static_cast<uint32_t>(t * 8), static_cast<uint32_t>(t * 8 + 40), key, 100,
                static_cast<uint32_t>(10 + t));
    }
    c.end_tick = 384;

    return c;
}

// Bend ranges: over 2 semitones, a positive bend of exactly 2, and one past MIDI's 127-semitone limit.
inline MidiCase BendsCase()
{
    MidiCase c;
    c.name = "bends";
    c.tempo = {{0, 90.0}};
    AddKey(c, 0, 0, CaseKind::kNoteOn, 60, 100, 1);
    AddPitch(c, 0, 12, 7.5f);
    AddPitch(c, 0, 24, -12.0f);
    AddKey(c, 0, 48, CaseKind::kNoteOff, 60, 0, 1);
    AddKey(c, 1, 0, CaseKind::kNoteOn, 60, 100, 2);
    AddPitch(c, 1, 12, 130.0f);
    AddKey(c, 1, 48, CaseKind::kNoteOff, 60, 0, 2);
    AddKey(c, 2, 0, CaseKind::kNoteOn, 60, 100, 3);
    AddPitch(c, 2, 12, 2.0f);
    AddKey(c, 2, 48, CaseKind::kNoteOff, 60, 0, 3);
    c.end_tick = 96;

    return c;
}

// A loop, frames that start before and after their ticks, and events between frames and after the last tick. A frame
// that starts after its loop marker shows where the two frame timings differ.
inline MidiCase LoopFramesCase()
{
    MidiCase c;
    c.name = "loop_frames";
    c.tempo = {{0, 120.0}, {50, 133.25}};
    c.loops = true;
    c.loop_start = 48;
    c.loop_end = 144;
    for (uint32_t t = 0; t <= 160; t++)
    {
        c.frame_ticks.push_back(t % 2 == 0 ? t - 0.3 : t + 0.4);
    }
    c.frame_ticks[0] = 0.0;
    c.frame_ticks[143] = 144.2;
    AddKey(c, 0, 0, CaseKind::kNoteOn, 60, 100, 1);
    AddKey(c, 0, 47, CaseKind::kNoteOff, 60, 0, 1);
    AddKey(c, 0, 47, CaseKind::kNoteOn, 62, 100, 2);
    AddKey(c, 0, 49, CaseKind::kNoteOff, 62, 0, 2);
    AddPitch(c, 0, 60, 0.5f, 0.25);
    AddPitch(c, 0, 61, 1.0f, 0.75);
    AddValue(c, 0, 143, CaseKind::kVolume, 80);
    AddPitch(c, 0, 143, 0.25f, 1.5);
    AddKey(c, 0, 145, CaseKind::kNoteOn, 64, 100, 3);
    AddKey(c, 0, 150, CaseKind::kNoteOff, 64, 0, 3);
    AddPitch(c, 0, 150, -0.5f, 2.5);
    c.end_tick = 160;

    return c;
}

// A gap longer than the largest delta that a variable-length quantity holds.
inline MidiCase LongDeltaCase()
{
    MidiCase c;
    c.name = "long_delta";
    c.timebase = 1;
    c.tempo = {{0, 60.0}};
    AddNote(c, 0, 0, 40000, 60, 100, 1);
    c.end_tick = 40001;

    return c;
}

// The chorus send.
inline MidiCase ChorusCase()
{
    MidiCase c;
    c.name = "chorus";
    c.tempo = {{0, 120.0}};
    AddValue(c, 0, 0, CaseKind::kChorus, 50);
    AddNote(c, 0, 0, 48, 60, 100, 1);
    AddValue(c, 0, 24, CaseKind::kChorus, 0);
    c.end_tick = 96;

    return c;
}

// Sound off and other controllers.
inline MidiCase ControlsCase()
{
    MidiCase c;
    c.name = "controls";
    c.tempo = {{0, 120.0}};
    AddKey(c, 0, 0, CaseKind::kNoteOn, 60, 100, 1);
    AddValue(c, 0, 10, CaseKind::kFilter, 64);
    AddValue(c, 0, 10, CaseKind::kLowPass, 20);
    AddValue(c, 0, 10, CaseKind::kBiquad, 30);
    AddValue(c, 0, 40, CaseKind::kSoundOff, 0);
    AddKey(c, 0, 50, CaseKind::kNoteOff, 60, 0, 1);
    c.end_tick = 96;

    return c;
}

// Named tracks with a layer, and a note-off whose note never started.
inline MidiCase NamesCase()
{
    MidiCase c;
    c.name = "names";
    c.tempo = {{0, 120.0}};
    c.track_names[0] = "Flute";
    c.track_names[2] = "Bass";
    AddKey(c, 0, 0, CaseKind::kNoteOn, 72, 100, 1);
    AddKey(c, 0, 12, CaseKind::kNoteOn, 72, 100, 2);
    AddKey(c, 0, 24, CaseKind::kNoteOff, 72, 0, 99);
    AddKey(c, 0, 36, CaseKind::kNoteOff, 72, 0, 1);
    AddKey(c, 0, 48, CaseKind::kNoteOff, 72, 0, 2);
    AddNote(c, 1, 0, 48, 64, 100, 3);
    AddNote(c, 2, 0, 48, 40, 100, 4);
    c.end_tick = 96;

    return c;
}

// Every case, in the order of their files.
inline std::vector<MidiCase> MidiWriterCases()
{
    return {BasicCase(),     LayersCase(), BendsCase(),    LoopFramesCase(),
            LongDeltaCase(), ChorusCase(), ControlsCase(), NamesCase()};
}

// Returns WriteMidi's report as text, one field a line. The files in tests/data/midi hold reports in this form.
inline std::string ReportText(const midi::MidiReport& report)
{
    std::string text = "channels " + std::to_string(report.channels) + "\nbend_ranges";
    for (const int range : report.bend_ranges)
    {
        text += " " + std::to_string(range);
    }

    text += "\ncut_notes " + std::to_string(report.cut_notes) + "\nchannel_10_presets";
    for (const uint32_t preset : report.channel_10_presets)
    {
        text += " " + std::to_string(preset);
    }

    text += "\n";
    for (const std::string& warning : report.warnings)
    {
        text += "warning " + warning + "\n";
    }

    return text;
}

} // namespace seqlib::test
