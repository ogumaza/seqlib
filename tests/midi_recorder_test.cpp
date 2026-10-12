// SPDX-License-Identifier: MIT

// Golden tests for seqlib/midi/recorder.h. The expected events come from two earlier recorders that followed these
// rules. Each ran the same calls with one of the two sets of options below. A test of one option compares a setting of
// each set.

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "seqlib/midi/recorder.h"
#include "test.h"

namespace
{

using seqlib::midi::ClampNoteOffs;
using seqlib::midi::ControlKind;
using seqlib::midi::CutAt;
using seqlib::midi::EventKind;
using seqlib::midi::IsSetting;
using seqlib::midi::PartSwitch;
using seqlib::midi::Performance;
using seqlib::midi::PresetIndex;
using seqlib::midi::Recorder;
using seqlib::midi::RecorderOptions;
using seqlib::midi::TrackEvent;
using seqlib::midi::TrackOutput;

// Settings before every event but a sound off, replacing only in the run of settings before them, dropping a part's
// values at a switch, frames before tick 0, and a pitch tolerance.
RecorderOptions FirstOptions()
{
    RecorderOptions options;
    options.settings_before_all_events = true;
    options.pitch_tolerance = 0.0005f;

    return options;
}

// Settings before notes and presets, replacing anywhere in the tick, sending the old part's values again at a switch,
// frames from tick 0, and every change of pitch.
RecorderOptions SecondOptions()
{
    RecorderOptions options;
    options.replace_anywhere_in_tick = true;
    options.resend_on_switch = true;
    options.frame_ticks_from_zero = true;

    return options;
}

// A recorder with a performance and 16 tracks, each on the MIDI track of its number.
struct Recording
{
    explicit Recording(const RecorderOptions& options) : recorder(options)
    {
        for (std::size_t t = 0; t < outputs.size(); t++)
        {
            outputs[t].midi_track = static_cast<uint8_t>(t);
        }
    }

    Recorder recorder;
    Performance perf;
    std::array<TrackOutput, 16> outputs{};
};

void CheckEvents(const std::vector<TrackEvent>& actual, const std::vector<TrackEvent>& expected)
{
    SEQLIB_CHECK(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size() && i < expected.size(); i++)
    {
        seqlib::test::SetContext("event " + std::to_string(i));
        const TrackEvent& a = actual[i];
        const TrackEvent& e = expected[i];
        SEQLIB_CHECK(a.tick == e.tick);
        SEQLIB_CHECK(a.kind == e.kind);
        SEQLIB_CHECK(a.key == e.key);
        SEQLIB_CHECK(a.value == e.value);
        SEQLIB_CHECK(a.preset == e.preset);
        SEQLIB_CHECK(a.semitones == e.semitones);
        SEQLIB_CHECK(a.note == e.note);
        SEQLIB_CHECK(a.part == e.part);
        SEQLIB_CHECK(a.after == e.after);
    }
    seqlib::test::SetContext("");
}

// Notes and settings in three ticks, with a note-off at the second and a note-off a fraction after the third.
Performance SettingsAfterNotes(const RecorderOptions& options)
{
    Recording r(options);
    Recorder& rec = r.recorder;
    TrackOutput& out = r.outputs[0];
    rec.NoteOn(r.perf, out, 0, 60, 100);
    rec.SendSetting(r.perf, out, 0, EventKind::kVolume, 100);
    rec.SendSetting(r.perf, out, 0, EventKind::kPan, 64);
    rec.SendPreset(r.perf, out, 0, 3);
    rec.NoteOn(r.perf, out, 0, 62, 90);
    rec.SendSetting(r.perf, out, 0, EventKind::kVolume, 90);
    rec.NoteOff(r.perf, 0, 1, 0, 60, 1);
    rec.SendSetting(r.perf, out, 1, EventKind::kExpression, 120);
    rec.NoteOff(r.perf, 0, 2, 0, 62, 2, 0.5);
    rec.SendSetting(r.perf, out, 2, EventKind::kModulation, 10);

    return r.perf;
}

SEQLIB_TEST(midi_recorder, SettingsBeforeAllEvents)
{
    // A setting goes before the note-ons and presets at the end of its tick. The setting replaces the tick's earlier
    // value.
    const std::vector<TrackEvent> kTick0 = {
        {.tick = 0, .kind = EventKind::kVolume, .value = 90},
        {.tick = 0, .kind = EventKind::kPan, .value = 64},
        {.tick = 0, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
        {.tick = 0, .kind = EventKind::kPreset, .preset = 3},
        {.tick = 0, .kind = EventKind::kNoteOn, .key = 62, .value = 90, .note = 2},
    };

    // True: before the note-offs too, at the tick or after it.
    std::vector<TrackEvent> expected = kTick0;
    expected.push_back({.tick = 1, .kind = EventKind::kExpression, .value = 120});
    expected.push_back({.tick = 1, .kind = EventKind::kNoteOff, .key = 60, .note = 1});
    expected.push_back({.tick = 2, .kind = EventKind::kModulation, .value = 10});
    expected.push_back({.tick = 2, .kind = EventKind::kNoteOff, .key = 62, .note = 2, .after = 0.5});
    CheckEvents(SettingsAfterNotes(FirstOptions()).tracks[0], expected);

    // False: after them.
    expected = kTick0;
    expected.push_back({.tick = 1, .kind = EventKind::kNoteOff, .key = 60, .note = 1});
    expected.push_back({.tick = 1, .kind = EventKind::kExpression, .value = 120});
    expected.push_back({.tick = 2, .kind = EventKind::kNoteOff, .key = 62, .note = 2, .after = 0.5});
    expected.push_back({.tick = 2, .kind = EventKind::kModulation, .value = 10});
    CheckEvents(SettingsAfterNotes(SecondOptions()).tracks[0], expected);
}

// Two values of a setting with a sound off between them, and two with a note between them.
Performance Barriers(bool before_all, bool anywhere)
{
    RecorderOptions options;
    options.settings_before_all_events = before_all;
    options.replace_anywhere_in_tick = anywhere;
    Recording r(options);
    Recorder& rec = r.recorder;
    rec.SendSetting(r.perf, r.outputs[0], 0, EventKind::kVolume, 100);
    rec.Emit(r.perf, r.outputs[0], 0, {.kind = EventKind::kSoundOff});
    rec.SendSetting(r.perf, r.outputs[0], 0, EventKind::kVolume, 90);
    rec.SendSetting(r.perf, r.outputs[1], 0, EventKind::kVolume, 100);
    const uint32_t id = rec.NoteOn(r.perf, r.outputs[1], 0, 50, 60);
    rec.NoteOff(r.perf, 1, 0, 0, 50, id);
    rec.SendSetting(r.perf, r.outputs[1], 0, EventKind::kVolume, 90);

    return r.perf;
}

SEQLIB_TEST(midi_recorder, ReplaceAnywhereInTick)
{
    const TrackEvent kVolume100 = {.kind = EventKind::kVolume, .value = 100};
    const TrackEvent kVolume90 = {.kind = EventKind::kVolume, .value = 90};
    const TrackEvent kSoundOffEvent = {.kind = EventKind::kSoundOff};
    const TrackEvent kNoteOnEvent = {.kind = EventKind::kNoteOn, .key = 50, .value = 60, .note = 1};
    const TrackEvent kNoteOffEvent = {.kind = EventKind::kNoteOff, .key = 50, .note = 1};

    // True: the earlier value goes, past the sound off and the note.
    for (const bool before_all : {true, false})
    {
        const Performance perf = Barriers(before_all, true);
        CheckEvents(perf.tracks[0], {kVolume90, kSoundOffEvent});
        CheckEvents(perf.tracks[1], {kVolume90, kNoteOnEvent, kNoteOffEvent});
    }

    // False: a setting in the tick's last run of settings is replaced. One before a sound off or a note isn't. A
    // setting that goes before every event but a sound off joins the run before the note.
    Performance perf = Barriers(true, false);
    CheckEvents(perf.tracks[0], {kVolume100, kSoundOffEvent, kVolume90});
    CheckEvents(perf.tracks[1], {kVolume90, kNoteOnEvent, kNoteOffEvent});
    perf = Barriers(false, false);
    CheckEvents(perf.tracks[0], {kVolume100, kSoundOffEvent, kVolume90});
    CheckEvents(perf.tracks[1], {kVolume100, kNoteOnEvent, kNoteOffEvent, kVolume90});
}

SEQLIB_TEST(midi_recorder, SettingSentOnlyOnChange)
{
    for (const bool seeded : {true, false})
    {
        Recording r(seeded ? FirstOptions() : SecondOptions());
        Recorder& rec = r.recorder;
        TrackOutput& out = r.outputs[0];
        if (seeded)
        {
            // A part that starts with a value has no event for it.
            out.sent[{0, ControlKind(103)}] = 0;
        }

        SEQLIB_CHECK(rec.SendSetting(r.perf, out, 0, EventKind::kVolume, 100));
        SEQLIB_CHECK(!rec.SendSetting(r.perf, out, 0, EventKind::kVolume, 100));
        SEQLIB_CHECK(rec.SendSetting(r.perf, out, 0, ControlKind(103), 0) != seeded);
        SEQLIB_CHECK(rec.SendSetting(r.perf, out, 0, ControlKind(104), 5));
        SEQLIB_CHECK(!rec.SendSetting(r.perf, out, 1, EventKind::kVolume, 100));
        SEQLIB_CHECK(rec.SendSetting(r.perf, out, 1, EventKind::kVolume, 80));
        SEQLIB_CHECK(rec.SendSetting(r.perf, out, 1, EventKind::kVolume, 100));
        SEQLIB_CHECK(rec.SendSetting(r.perf, r.outputs[1], 2, EventKind::kReverb, 0));
        SEQLIB_CHECK(!rec.SendSetting(r.perf, r.outputs[1], 2, EventKind::kReverb, 0));

        // The change at tick 1 and its return replace each other.
        std::vector<TrackEvent> expected = {{.tick = 0, .kind = EventKind::kVolume, .value = 100}};
        if (!seeded)
        {
            expected.push_back({.tick = 0, .kind = ControlKind(103), .value = 0});
        }
        expected.push_back({.tick = 0, .kind = ControlKind(104), .value = 5});
        expected.push_back({.tick = 1, .kind = EventKind::kVolume, .value = 100});
        CheckEvents(r.perf.tracks[0], expected);
        CheckEvents(r.perf.tracks[1], {{.tick = 2, .kind = EventKind::kReverb, .value = 0}});
    }
}

SEQLIB_TEST(midi_recorder, SendPreset)
{
    Recording r(FirstOptions());
    TrackOutput& out = r.outputs[2];
    SEQLIB_CHECK(r.recorder.SendPreset(r.perf, out, 4, 300));
    SEQLIB_CHECK(!r.recorder.SendPreset(r.perf, out, 4, 300));
    SEQLIB_CHECK(r.recorder.SendPreset(r.perf, out, 5, 7));
    CheckEvents(r.perf.tracks[2], {{.tick = 4, .kind = EventKind::kPreset, .preset = 300},
                                   {.tick = 5, .kind = EventKind::kPreset, .preset = 7}});
    SEQLIB_CHECK((out.sent.at({0, EventKind::kPreset}) == 7));
}

// Settings, a pitch and a preset on part 0, a switch to part 1, the same values again, and a switch back.
Performance SwitchParts(const RecorderOptions& options, std::vector<PartSwitch>& switches)
{
    Recording r(options);
    Recorder& rec = r.recorder;
    TrackOutput& out = r.outputs[0];
    rec.SendSetting(r.perf, out, 0, EventKind::kVolume, 100);
    rec.SendSetting(r.perf, out, 0, EventKind::kPan, 30);
    rec.SendPitch(r.perf, out, 0, 2.0f);
    rec.SendPreset(r.perf, out, 0, 5);
    const uint32_t first = rec.NoteOn(r.perf, out, 0, 60, 100);
    SEQLIB_CHECK(rec.Switch(r.perf, out, 1, [](uint8_t part) { return part == 0; }));
    SEQLIB_CHECK(out.part == 1);
    rec.SendSetting(r.perf, out, 1, EventKind::kVolume, 100);
    rec.SendSetting(r.perf, out, 1, EventKind::kPan, 40);
    rec.SendPitch(r.perf, out, 1, 2.0f);
    rec.SendPreset(r.perf, out, 1, 5);
    rec.NoteOn(r.perf, out, 1, 64, 100);
    rec.SendSetting(r.perf, out, 1, EventKind::kVolume, 90, 0); // the part that the track left
    rec.NoteOff(r.perf, 0, 2, 0, 60, first);
    SEQLIB_CHECK(rec.Switch(r.perf, out, 2, [](uint8_t) { return false; }));
    SEQLIB_CHECK(out.part == 0);
    switches = rec.PartSwitches();
    SEQLIB_CHECK((rec.PartsUsed()[0] == 2));

    return r.perf;
}

SEQLIB_TEST(midi_recorder, ResendOnSwitch)
{
    const std::vector<TrackEvent> kFirstTicks = {
        {.tick = 0, .kind = EventKind::kVolume, .value = 100},
        {.tick = 0, .kind = EventKind::kPan, .value = 30},
        {.tick = 0, .kind = EventKind::kPitch, .semitones = 2.0f},
        {.tick = 0, .kind = EventKind::kPreset, .preset = 5},
        {.tick = 0, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
        {.tick = 1, .kind = EventKind::kVolume, .value = 100, .part = 1},
        {.tick = 1, .kind = EventKind::kPan, .value = 40, .part = 1},
        {.tick = 1, .kind = EventKind::kPitch, .semitones = 2.0f, .part = 1},
        {.tick = 1, .kind = EventKind::kVolume, .value = 90},
        {.tick = 1, .kind = EventKind::kPreset, .preset = 5, .part = 1},
        {.tick = 1, .kind = EventKind::kNoteOn, .key = 64, .value = 100, .note = 2, .part = 1},
        {.tick = 2, .kind = EventKind::kNoteOff, .key = 60, .note = 1},
    };
    const std::vector<PartSwitch> kExpectedSwitches = {{1, 0, 1}, {2, 0, 0}};

    // False: part 1's values are dropped. The track's same values go out there again. The switch back sends nothing.
    std::vector<PartSwitch> switches;
    CheckEvents(SwitchParts(FirstOptions(), switches).tracks[0], kFirstTicks);
    SEQLIB_CHECK(switches == kExpectedSwitches);

    // True: part 1 is sent part 0's values at once. The pan changes. The same values don't go out again. The switch
    // back sends part 1's values to part 0.
    std::vector<TrackEvent> expected = kFirstTicks;
    expected.push_back({.tick = 2, .kind = EventKind::kVolume, .value = 100});
    expected.push_back({.tick = 2, .kind = EventKind::kPan, .value = 40});
    expected.push_back({.tick = 2, .kind = EventKind::kPitch, .semitones = 2.0f});
    expected.push_back({.tick = 2, .kind = EventKind::kPreset, .preset = 5});
    CheckEvents(SwitchParts(SecondOptions(), switches).tracks[0], expected);
    SEQLIB_CHECK(switches == kExpectedSwitches);
}

SEQLIB_TEST(midi_recorder, PartLimits)
{
    for (const bool resend : {false, true})
    {
        Recording r(resend ? SecondOptions() : FirstOptions());
        Recorder& rec = r.recorder;
        TrackOutput& out = r.outputs[3];
        std::array<uint8_t, 16> limits;
        limits.fill(16);
        limits[3] = 2;
        rec.SetPartLimits(limits);
        SEQLIB_CHECK((rec.PartsUsed() == std::array<uint8_t, 16>{}));
        rec.SendSetting(r.perf, out, 4, EventKind::kVolume, 50);
        SEQLIB_CHECK(rec.Switch(r.perf, out, 4, [](uint8_t) { return false; }));
        SEQLIB_CHECK(!rec.Switch(r.perf, out, 4, [](uint8_t part) { return part == 0; }));
        SEQLIB_CHECK(!rec.Switch(r.perf, out, 4, [](uint8_t) { return true; }));
        rec.NoteOn(r.perf, out, 4, 40, 20);
        SEQLIB_CHECK((rec.PartSwitches() == std::vector<PartSwitch>{{4, 3, 1}}));
        SEQLIB_CHECK(rec.PartsUsed()[3] == 2 && rec.PartsUsed()[0] == 0);

        std::vector<TrackEvent> expected = {{.tick = 4, .kind = EventKind::kVolume, .value = 50}};
        if (resend)
        {
            expected.push_back({.tick = 4, .kind = EventKind::kVolume, .value = 50, .part = 1});
        }
        expected.push_back({.tick = 4, .kind = EventKind::kNoteOn, .key = 40, .value = 20, .note = 1, .part = 1});
        CheckEvents(r.perf.tracks[3], expected);
    }
}

// Bends at a tick and between ticks, small moves and a return to 0.
Performance Bends(const RecorderOptions& options)
{
    Recording r(options);
    Recorder& rec = r.recorder;
    TrackOutput& out = r.outputs[0];
    SEQLIB_CHECK(!rec.SendPitch(r.perf, out, 0, 0.0f));
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 0, 1.0f));
    SEQLIB_CHECK(!rec.SendPitch(r.perf, out, 0, 1.0f, -1, 0.25));
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 0, 1.5f, -1, 0.5));
    rec.SendPitch(r.perf, out, 0, 1.5004f, -1, 0.75);
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 1, 0.0003f));
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 1, 0.0f));
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 1, -2.0f));
    rec.NoteOn(r.perf, out, 1, 60, 100);
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 1, -1.0f));

    return r.perf;
}

SEQLIB_TEST(midi_recorder, BendWithAfter)
{
    // A pitch between ticks comes `after` its tick. A pitch at a tick replaces the tick's earlier pitch, before the
    // note.
    std::vector<TrackEvent> expected = {
        {.tick = 0, .kind = EventKind::kPitch, .semitones = 1.0f},
        {.tick = 0, .kind = EventKind::kPitch, .semitones = 1.5f, .after = 0.5},
        {.tick = 1, .kind = EventKind::kPitch, .semitones = -1.0f},
        {.tick = 1, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
    };

    // A tolerance of 0.0005 leaves out the move of 0.0004.
    CheckEvents(Bends(FirstOptions()).tracks[0], expected);

    // A tolerance of 0 doesn't.
    expected.insert(expected.begin() + 2, {.tick = 0, .kind = EventKind::kPitch, .semitones = 1.5004f, .after = 0.75});
    CheckEvents(Bends(SecondOptions()).tracks[0], expected);
}

// A part's pitch after the track has left it, and the pitch of a part after a switch.
Performance PartPitches(const RecorderOptions& options)
{
    Recording r(options);
    Recorder& rec = r.recorder;
    TrackOutput& out = r.outputs[0];
    rec.SendPitch(r.perf, out, 0, 3.0f);
    rec.NoteOn(r.perf, out, 0, 60, 100);
    rec.Switch(r.perf, out, 1, [](uint8_t part) { return part == 0; });
    SEQLIB_CHECK(!rec.SendPitch(r.perf, out, 1, 3.0f, 0));
    SEQLIB_CHECK(rec.SendPitch(r.perf, out, 1, 2.0f, 0));
    rec.SendPitch(r.perf, out, 1, 0.0f);
    rec.SendPitch(r.perf, out, 1, 2.5f, 0, 0.5);
    TrackOutput& other = r.outputs[2];
    rec.SendSetting(r.perf, other, 2, EventKind::kPan, 64);
    rec.Switch(r.perf, other, 2, [](uint8_t) { return false; });
    rec.SendPitch(r.perf, other, 2, 0.0f);
    rec.SendSetting(r.perf, other, 2, EventKind::kPan, 64);

    return r.perf;
}

SEQLIB_TEST(midi_recorder, PartPitch)
{
    const TrackEvent kPitch3 = {.tick = 0, .kind = EventKind::kPitch, .semitones = 3.0f};
    const TrackEvent kNoteOnEvent = {.tick = 0, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1};
    const TrackEvent kPitch2 = {.tick = 1, .kind = EventKind::kPitch, .semitones = 2.0f};
    const TrackEvent kPitch0 = {.tick = 1, .kind = EventKind::kPitch, .semitones = 0.0f, .part = 1};
    const TrackEvent kPitch2Point5 = {.tick = 1, .kind = EventKind::kPitch, .semitones = 2.5f, .after = 0.5};
    const TrackEvent kPan0 = {.tick = 2, .kind = EventKind::kPan, .value = 64};
    const TrackEvent kPan1 = {.tick = 2, .kind = EventKind::kPan, .value = 64, .part = 1};

    // Dropped at the switch, the new part's pitch goes out at 0. So does its pan.
    Performance perf = PartPitches(FirstOptions());
    CheckEvents(perf.tracks[0], {kPitch3, kNoteOnEvent, kPitch2, kPitch0, kPitch2Point5});
    CheckEvents(perf.tracks[2], {kPan0, {.tick = 2, .kind = EventKind::kPitch, .semitones = 0.0f, .part = 1}, kPan1});

    // Sent again at the switch, part 0's pitch of 3 takes part 1 to 3. The return to 0 replaces it. A part without a
    // pitch sends none.
    perf = PartPitches(SecondOptions());
    CheckEvents(perf.tracks[0], {kPitch3, kNoteOnEvent, kPitch0, kPitch2, kPitch2Point5});
    CheckEvents(perf.tracks[2], {kPan0, kPan1});
}

SEQLIB_TEST(midi_recorder, Emit)
{
    Recording r(FirstOptions());
    const Recorder& rec = r.recorder;
    TrackOutput& out = r.outputs[5];
    out.part = 2;

    // An event takes the tick, and the part given or else the part of the track's new notes. A setting at the tick goes
    // before the note. A setting with `after` goes at the end.
    rec.Emit(r.perf, out, 7, {.kind = EventKind::kNoteOn, .key = 1, .value = 2, .note = 9});
    rec.Emit(r.perf, out, 7, {.kind = EventKind::kReverb, .value = 4}, 1);
    rec.Emit(r.perf, out, 7, {.kind = EventKind::kChorus, .value = 3, .after = 0.25});
    rec.Emit(r.perf, out, 8, {.kind = EventKind::kNoteOff, .key = 1, .note = 9, .part = 6});
    CheckEvents(r.perf.tracks[5], {{.tick = 7, .kind = EventKind::kReverb, .value = 4, .part = 1},
                                   {.tick = 7, .kind = EventKind::kNoteOn, .key = 1, .value = 2, .note = 9, .part = 2},
                                   {.tick = 7, .kind = EventKind::kChorus, .value = 3, .part = 2, .after = 0.25},
                                   {.tick = 8, .kind = EventKind::kNoteOff, .key = 1, .note = 9, .part = 2}});
}

SEQLIB_TEST(midi_recorder, IsSettingKinds)
{
    for (const EventKind kind : {EventKind::kNoteOn, EventKind::kNoteOff, EventKind::kPreset, EventKind::kSoundOff})
    {
        SEQLIB_CHECK(!IsSetting(kind));
    }

    for (const EventKind kind :
         {EventKind::kVolume, EventKind::kExpression, EventKind::kPan, EventKind::kPitch, EventKind::kModulation,
          EventKind::kReverb, EventKind::kChorus, ControlKind(0), ControlKind(102), ControlKind(127)})
    {
        SEQLIB_CHECK(IsSetting(kind));
    }
}

SEQLIB_TEST(midi_recorder, Options)
{
    RecorderOptions options;
    options.resend_on_switch = true;
    options.pitch_tolerance = 0.25f;
    const Recorder rec(options);
    SEQLIB_CHECK(rec.Options().resend_on_switch && !rec.Options().settings_before_all_events);
    SEQLIB_CHECK(!rec.Options().replace_anywhere_in_tick && !rec.Options().frame_ticks_from_zero);
    SEQLIB_CHECK(rec.Options().pitch_tolerance == 0.25f);
}

SEQLIB_TEST(midi_recorder, Notes)
{
    Recording r(SecondOptions());
    Recorder& rec = r.recorder;
    SEQLIB_CHECK(rec.NextNote() == 1);
    r.outputs[4].part = 3;
    SEQLIB_CHECK(rec.NoteOn(r.perf, r.outputs[4], 2, 70, 127) == 1);
    SEQLIB_CHECK(rec.NextNote() == 2);
    SEQLIB_CHECK(rec.NoteOn(r.perf, r.outputs[4], 2, 70, 1) == 2);
    rec.NoteOff(r.perf, 4, 3, 3, 70, 1, 0.75);
    rec.NoteOff(r.perf, 4, 3, 1, 70, 2);
    CheckEvents(r.perf.tracks[4],
                {{.tick = 2, .kind = EventKind::kNoteOn, .key = 70, .value = 127, .note = 1, .part = 3},
                 {.tick = 2, .kind = EventKind::kNoteOn, .key = 70, .value = 1, .note = 2, .part = 3},
                 {.tick = 3, .kind = EventKind::kNoteOff, .key = 70, .note = 1, .part = 3, .after = 0.75},
                 {.tick = 3, .kind = EventKind::kNoteOff, .key = 70, .note = 2, .part = 1}});
    SEQLIB_CHECK(rec.PartsUsed()[4] == 4);
}

SEQLIB_TEST(midi_recorder, SetTempo)
{
    for (const RecorderOptions& options : {FirstOptions(), SecondOptions()})
    {
        const Recorder rec(options);
        Performance perf;
        rec.SetTempo(perf, 0, 120.0);
        rec.SetTempo(perf, 0, 0.0);
        rec.SetTempo(perf, 0, 150.0);
        rec.SetTempo(perf, 3, 150.0);
        rec.SetTempo(perf, 4, -10.0);
        rec.SetTempo(perf, 5, 90.5);
        SEQLIB_CHECK(perf.tempo.size() == 2);
        SEQLIB_CHECK(perf.tempo[0].tick == 0 && perf.tempo[0].bpm == 150.0);
        SEQLIB_CHECK(perf.tempo[1].tick == 5 && perf.tempo[1].bpm == 90.5);
    }
}

SEQLIB_TEST(midi_recorder, FrameTicksFromZero)
{
    // Two ticks in a frame that starts before the first tick's time, two ticks at one time, and a frame after the last
    // tick.
    const std::vector<double> kTimes = {10.0, 12.0, 20.0, 35.0, 35.0, 50.0};
    const double kStarts[] = {4.0, 4.0, 18.0, 33.0, 33.0, 52.0};
    for (const bool from_zero : {false, true})
    {
        RecorderOptions options;
        options.frame_ticks_from_zero = from_zero;
        const Recorder rec(options);
        Performance perf;
        for (uint32_t tick = 0; tick < kTimes.size(); tick++)
        {
            rec.RecordFrameStart(perf, std::span(kTimes).first(tick + 1), tick, kStarts[tick]);
        }
        const std::vector<double> expected = {0.0, from_zero ? 0.0 : -3.0, 1.75, 2.0 + 13.0 / 15.0, 2.0 + 13.0 / 15.0,
                                              5.0};
        SEQLIB_CHECK(perf.frame_ticks == expected);
    }
}

SEQLIB_TEST(midi_recorder, CutAtNoteIdZero)
{
    // A note-on that Emit records with id 0 is no note. After the end tick, it goes, and the track's other events of
    // note 0 stay.
    Recording r(SecondOptions());
    Recorder& rec = r.recorder;
    rec.SendSetting(r.perf, r.outputs[0], 1, EventKind::kVolume, 70);
    rec.Emit(r.perf, r.outputs[0], 3, {.kind = EventKind::kNoteOn, .key = 60, .value = 100});

    const std::set<uint32_t> cut = CutAt(r.perf, 2);

    SEQLIB_CHECK(cut.empty());
    CheckEvents(r.perf.tracks[0], {{.tick = 1, .kind = EventKind::kVolume, .value = 70}});
}

SEQLIB_TEST(midi_recorder, CutAtPassEnd)
{
    // Notes before, across and at the end tick, a note-off a fraction after it, and a later note-off.
    Recording r(SecondOptions());
    Recorder& rec = r.recorder;
    const uint32_t a = rec.NoteOn(r.perf, r.outputs[0], 0, 60, 100);
    const uint32_t b = rec.NoteOn(r.perf, r.outputs[1], 2, 62, 100);
    rec.SendSetting(r.perf, r.outputs[0], 3, EventKind::kVolume, 70);
    const uint32_t c = rec.NoteOn(r.perf, r.outputs[0], 3, 64, 100);
    rec.NoteOff(r.perf, 1, 3, 0, 62, b);
    rec.NoteOff(r.perf, 0, 4, 0, 64, c);
    rec.NoteOff(r.perf, 0, 4, 0, 60, a, 0.5);
    SEQLIB_CHECK((CutAt(r.perf, 3) == std::set<uint32_t>{c}));
    rec.NoteOff(r.perf, 0, 5, 0, 60, a);
    ClampNoteOffs(r.perf, 3);
    CheckEvents(r.perf.tracks[0], {{.tick = 0, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
                                   {.tick = 3, .kind = EventKind::kVolume, .value = 70},
                                   {.tick = 3, .kind = EventKind::kNoteOff, .key = 60, .note = 1}});
    CheckEvents(r.perf.tracks[1], {{.tick = 2, .kind = EventKind::kNoteOn, .key = 62, .value = 100, .note = 2},
                                   {.tick = 3, .kind = EventKind::kNoteOff, .key = 62, .note = 2}});
}

SEQLIB_TEST(midi_recorder, Levels)
{
    Recorder rec;
    SEQLIB_CHECK(rec.Expression(100.4) == 100);
    SEQLIB_CHECK(rec.Expression(63.5) == 64);
    SEQLIB_CHECK(rec.Expression(127.6) == 127);
    SEQLIB_CHECK(rec.Expression(-3.0) == 0);
    rec.SetLevelScale(0.5);
    SEQLIB_CHECK(rec.Expression(200.0) == 100);
    SEQLIB_CHECK(rec.Expression(300.0) == 127);

    SEQLIB_CHECK(rec.LevelPeak() == 0.0);
    rec.RaiseLevelPeak(10.0);
    rec.RaiseLevelPeak(250.5);
    rec.RaiseLevelPeak(20.0);
    SEQLIB_CHECK(rec.LevelPeak() == 250.5);
}

SEQLIB_TEST(midi_recorder, ApproximationsAndWarnings)
{
    Recorder rec;
    SEQLIB_CHECK(rec.Approximations().empty() && rec.Warnings().empty());
    rec.Approximate("second LFO (omitted)");
    rec.Approximate("pan LFO (omitted)");
    rec.Approximate("second LFO (omitted)");
    rec.Warn("track 3 stopped");
    rec.Warn("track 3 stopped");
    SEQLIB_CHECK((rec.Approximations() ==
                  std::map<std::string, uint32_t>{{"pan LFO (omitted)", 1}, {"second LFO (omitted)", 2}}));
    SEQLIB_CHECK((rec.Warnings() == std::vector<std::string>{"track 3 stopped", "track 3 stopped"}));

    // A copy records apart from the original.
    Recorder copy = rec;
    copy.Approximate("pan LFO (omitted)");
    SEQLIB_CHECK(rec.Approximations().at("pan LFO (omitted)") == 1);
    SEQLIB_CHECK(copy.Approximations().at("pan LFO (omitted)") == 2);
}

SEQLIB_TEST(midi_recorder, PresetIndices)
{
    std::vector<std::string> presets;
    SEQLIB_CHECK(PresetIndex(presets, std::string("piano")) == 0);
    SEQLIB_CHECK(PresetIndex(presets, std::string("strings")) == 1);
    SEQLIB_CHECK(PresetIndex(presets, std::string("piano")) == 0);
    SEQLIB_CHECK((presets == std::vector<std::string>{"piano", "strings"}));
}

} // namespace
