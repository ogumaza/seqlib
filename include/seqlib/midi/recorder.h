// SPDX-License-Identifier: MIT

// A recorder that puts a sequence player's output on a Performance's tracks: the notes, the settings and presets of
// each track's parts, the pitch, the tempo and the start of each tick's frame. The player keeps its time. The recorder
// takes ticks, and fractions of a tick (TrackEvent::after). Where sequence players record in different ways, the
// recorder takes an option (RecorderOptions).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "seqlib/midi/performance.h"

namespace seqlib::midi
{

// Returns true for the kinds of event that are settings: every kind but a note-on, a note-off, a preset and a sound
// off. A setting applies to its part's notes from its tick on.
bool IsSetting(EventKind kind);

// A switch of a track's new notes and changes to another part: the tick, the MIDI track and the new part.
using PartSwitch = std::tuple<uint32_t, int, uint8_t>;

// What one of a player's tracks has sent to MIDI: the MIDI track that the track's events go to, the part that the
// track's new notes and changes go to, and the last values that each part was sent. A player keeps one for each of its
// tracks. Several of the player's tracks may share a MIDI track. Give them different parts: `sent` and `pitch` hold one
// track's values. Another track's changes to the same part aren't in them.
struct TrackOutput
{
    uint8_t midi_track = 0; // 0-15, an index into Performance::tracks
    uint8_t part = 0;

    // Each part's last value of each setting, and its last preset, by part and kind. For a part without a value of a
    // kind, the next value of that kind always goes out.
    std::map<std::pair<uint8_t, EventKind>, int> sent;

    // Each part's last pitch in semitones. A part without an entry is at 0. For a part whose entry is empty, the next
    // pitch always goes out.
    std::map<uint8_t, std::optional<float>> pitch;
};

// The rules in which sequence players record differently.
struct RecorderOptions
{
    // Where a setting goes among the events of its tick. True: before the events at the end of the tick that are
    // neither settings nor sound offs. Their `after` doesn't matter. False: before the note-ons and presets at the end
    // of the tick whose `after` is 0.
    bool settings_before_all_events = false;

    // Which earlier event of its tick a setting replaces. That's an event of the same kind on the same part. True: the
    // latest such event whose `after` is 0, anywhere before the setting's place. False: only one in the run of settings
    // directly before that place. A setting that replaces none goes in as a new event.
    bool replace_anywhere_in_tick = false;

    // What a part switch does with the parts' last values. True: the old part's last settings, preset and pitch go to
    // the new part at once. They become the new part's last values. A kind that the old part has no value for keeps the
    // new part's value. False: the new part's last values are dropped. The new part's next setting, preset and pitch
    // then always go out.
    bool resend_on_switch = false;

    // Where the frame of a tick goes when the frame starts before the first tick's time. True: at tick 0. False: at a
    // fractional tick below 0.
    bool frame_ticks_from_zero = false;

    // How far a part's pitch has to move from its last pitch to go out, in semitones. 0 keeps every move. Above 0, a
    // move of this much or less is left out. A return to exactly 0 always goes out.
    float pitch_tolerance = 0.0f;
};

// Records a player's output on a Performance. The player passes the Performance, and the TrackOutput of the track
// concerned, to each call. A player's performance can therefore be a type derived from Performance. The recorder holds
// what the player's tracks share: the next note's id, the part switches, the parts that each MIDI track uses, the
// level's scale and peak, and the approximations and warnings. It holds no pointer or reference. A copy of a recorder
// records apart from the original.
class Recorder
{
public:
    explicit Recorder(const RecorderOptions& options = {});

    const RecorderOptions& Options() const;

    // Records event `e` at `tick`, on part `part` of the track with `output`, or with a negative `part` on the part
    // that the track's new notes go to. Emit sets e.tick and e.part. A setting whose `after` is 0 goes before events at
    // the end of the tick. The setting replaces an earlier value of the same setting in the tick. RecorderOptions gives
    // the events that the setting goes before, and the value that it replaces. Every other event goes after the events
    // recorded so far. Emit changes neither `sent`, `pitch`, the note ids nor PartsUsed(). A note-on recorded with Emit
    // keeps its e.note, 0 by default. CutAt treats a note-on with id 0 as no note.
    void Emit(Performance& perf, const TrackOutput& output, uint32_t tick, TrackEvent e, int part = -1) const;

    // Records setting `kind` at `value` for part `part` if the part's last value of the setting is another. The setting
    // goes in where Emit() puts it. Returns true if it was recorded. A value is a byte. The pitch goes through
    // SendPitch().
    bool SendSetting(Performance& perf, TrackOutput& output, uint32_t tick, EventKind kind, uint8_t value,
                     int part = -1) const;

    // Records preset `preset` for the part that the track's new notes go to, if that part's last preset is another.
    // Returns true if it was recorded.
    bool SendPreset(Performance& perf, TrackOutput& output, uint32_t tick, uint32_t preset) const;

    // Records part `part`'s pitch in semitones, if it has moved from the part's last pitch (pitch_tolerance). A pitch
    // between ticks comes `after` its tick. Such a pitch goes after the events recorded so far. Returns true if the
    // pitch was recorded.
    bool SendPitch(Performance& perf, TrackOutput& output, uint32_t tick, float semitones, int part = -1,
                   double after = 0) const;

    // Switches the track's new notes and changes to the first part, below the MIDI track's part limit, other than the
    // part that they go to now and on which `sounds(part)` is false. The switch is recorded in PartSwitches(). The new
    // part is sent the old part's last values, or the new part's last values are dropped (resend_on_switch). Returns
    // false if no part was free.
    template <typename Sounds>
    bool Switch(Performance& perf, TrackOutput& output, uint32_t tick, Sounds sounds)
    {
        for (uint8_t part = 0; part < part_limits_[output.midi_track]; part++)
        {
            if (part != output.part && !sounds(part))
            {
                SwitchTo(perf, output, tick, part);
                return true;
            }
        }

        return false;
    }

    // Returns the id that the next note will have. The ids start at 1.
    uint32_t NextNote() const;

    // Records a note-on of `key` at `velocity`, on the part that the track's new notes go to. Returns the note's id.
    uint32_t NoteOn(Performance& perf, const TrackOutput& output, uint32_t tick, uint8_t key, uint8_t velocity);

    // Records the note-off of note `note` of `key` on part `part` of MIDI track `midi_track`, `after` its tick. The
    // note-off goes after the events recorded so far.
    void NoteOff(Performance& perf, uint8_t midi_track, uint32_t tick, uint8_t part, uint8_t key, uint32_t note,
                 double after = 0) const;

    // Records a tempo of `bpm` quarter notes a minute from `tick` on. A tempo at the tick of the last one replaces it.
    // Any other tempo goes in only if it differs from the last. A tempo of 0 or less is left out.
    void SetTempo(Performance& perf, uint32_t tick, double bpm) const;

    // Records where tick `tick`'s frame starts (Performance::frame_ticks). `start` becomes a fractional tick between
    // the two tick times around it, no later than `tick`. `tick_times` are the times of ticks 0 to `tick`, in the unit
    // of `start`. Each call appends one entry. Record each tick's frame once, in the order of the ticks.
    void RecordFrameStart(Performance& perf, std::span<const double> tick_times, uint32_t tick, double start) const;

    // The parts that each MIDI track may use (16 by default), and those that it has used: one more than the highest
    // part that a switch or a note has gone to.
    void SetPartLimits(const std::array<uint8_t, 16>& limits);
    const std::array<uint8_t, 16>& PartsUsed() const;

    // The part switches, in the order they were made.
    const std::vector<PartSwitch>& PartSwitches() const;

    // Returns the expression controller (CC11) for `level`: `level` times the level scale, rounded and limited to
    // 0-127. The level scale is 1 by default.
    uint8_t Expression(double level) const;
    void SetLevelScale(double scale);

    // The highest level that RaiseLevelPeak() has been given, 0 at first.
    void RaiseLevelPeak(double level);
    double LevelPeak() const;

    // Counts an approximation that MIDI or a SoundFont makes of the player's output, under the name `what`.
    void Approximate(const std::string& what);
    const std::map<std::string, uint32_t>& Approximations() const;

    // Adds a warning about the performance.
    void Warn(const std::string& text);
    const std::vector<std::string>& Warnings() const;

private:
    void SwitchTo(Performance& perf, TrackOutput& output, uint32_t tick, uint8_t part);

    RecorderOptions options_;
    uint32_t next_note_ = 1;
    std::vector<PartSwitch> part_switches_;
    std::array<uint8_t, 16> part_limits_;
    std::array<uint8_t, 16> parts_used_{};
    double level_scale_;
    double level_peak_;
    std::map<std::string, uint32_t> approximations_;
    std::vector<std::string> warnings_;
};

// Leaves out what the next pass of a performance started at `end_tick`, the tick that the pass begins with: every event
// after `end_tick`, and every event of a note that starts at `end_tick` or later. Returns the ids of the notes left
// out.
std::set<uint32_t> CutAt(Performance& perf, uint32_t end_tick);

// Moves every note-off after `end_tick` to `end_tick`.
void ClampNoteOffs(Performance& perf, uint32_t end_tick);

// Returns the index of `key` in `presets`. A key that isn't there yet is added at the end.
template <typename Key>
uint32_t PresetIndex(std::vector<Key>& presets, const Key& key)
{
    for (std::size_t i = 0; i < presets.size(); i++)
    {
        if (presets[i] == key)
        {
            return static_cast<uint32_t>(i);
        }
    }

    presets.push_back(key);

    return static_cast<uint32_t>(presets.size() - 1);
}

} // namespace seqlib::midi
