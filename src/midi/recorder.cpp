// SPDX-License-Identifier: MIT

// A recorder of a player's output (see recorder.h).

#include "seqlib/midi/recorder.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace seqlib::midi
{
namespace
{

// Part `part`, or with a negative `part` the part that the track's new notes go to.
uint8_t PartOf(const TrackOutput& output, int part)
{
    return static_cast<uint8_t>(part >= 0 ? part : output.part);
}

// Stores `value` as part `part`'s last value of `kind` in `output`. Returns false, and stores nothing, when that's the
// last value already.
bool StoreSent(TrackOutput& output, uint8_t part, EventKind kind, int value)
{
    const auto sent = output.sent.find({part, kind});
    if (sent != output.sent.end() && sent->second == value)
    {
        return false;
    }

    output.sent[{part, kind}] = value;

    return true;
}

} // namespace

bool IsSetting(EventKind kind)
{
    switch (kind)
    {
    case EventKind::kNoteOn:
    case EventKind::kNoteOff:
    case EventKind::kPreset:
    case EventKind::kSoundOff:
        return false;
    default:
        return true;
    }
}

Recorder::Recorder(const RecorderOptions& options) : options_(options), level_scale_(1.0), level_peak_(0.0)
{
    part_limits_.fill(16);
}

const RecorderOptions& Recorder::Options() const
{
    return options_;
}

void Recorder::Emit(Performance& perf, const TrackOutput& output, uint32_t tick, TrackEvent e, int part) const
{
    e.tick = tick;
    e.part = PartOf(output, part);
    std::vector<TrackEvent>& events = perf.tracks[output.midi_track];
    if (!IsSetting(e.kind) || e.after != 0)
    {
        events.push_back(e);
        return;
    }

    // The setting's place: before the events at the end of the tick that it goes before.
    auto in_tick = [&](std::vector<TrackEvent>::iterator it)
    {
        return it != events.begin() && std::prev(it)->tick == tick;
    };
    auto goes_before = [&](const TrackEvent& later)
    {
        if (options_.settings_before_all_events)
        {
            return !IsSetting(later.kind) && later.kind != EventKind::kSoundOff;
        }

        return later.after == 0 && (later.kind == EventKind::kNoteOn || later.kind == EventKind::kPreset);
    };
    auto at = events.end();
    while (in_tick(at) && goes_before(*std::prev(at)))
    {
        --at;
    }

    // An earlier value of the same setting in the tick is replaced.
    for (auto it = at; in_tick(it); --it)
    {
        TrackEvent& earlier = *std::prev(it);
        if (!options_.replace_anywhere_in_tick && !IsSetting(earlier.kind))
        {
            break;
        }

        if (earlier.kind == e.kind && earlier.part == e.part &&
            (!options_.replace_anywhere_in_tick || earlier.after == 0))
        {
            earlier = e;
            return;
        }
    }

    events.insert(at, e);
}

bool Recorder::SendSetting(Performance& perf, TrackOutput& output, uint32_t tick, EventKind kind, uint8_t value,
                           int part) const
{
    const uint8_t on = PartOf(output, part);
    if (!StoreSent(output, on, kind, value))
    {
        return false;
    }

    TrackEvent e;
    e.kind = kind;
    e.value = value;
    Emit(perf, output, tick, e, on);

    return true;
}

bool Recorder::SendPreset(Performance& perf, TrackOutput& output, uint32_t tick, uint32_t preset) const
{
    if (!StoreSent(output, output.part, EventKind::kPreset, static_cast<int>(preset)))
    {
        return false;
    }

    TrackEvent e;
    e.kind = EventKind::kPreset;
    e.preset = preset;
    Emit(perf, output, tick, e);

    return true;
}

bool Recorder::SendPitch(Performance& perf, TrackOutput& output, uint32_t tick, float semitones, int part,
                         double after) const
{
    // The part's last pitch: 0 without an entry, and unknown with an empty one. A pitch goes out when the last one is
    // unknown, when it has moved by more than the tolerance, and when it returns to exactly 0.
    const uint8_t on = PartOf(output, part);
    const auto sent = output.pitch.find(on);
    const std::optional<float> last = sent == output.pitch.end() ? std::optional<float>(0.0f) : sent->second;
    if (last)
    {
        const bool moved = std::fabs(semitones - *last) > options_.pitch_tolerance;
        const bool back_to_zero = semitones == 0.0f && *last != 0.0f;
        if (!moved && !back_to_zero)
        {
            return false;
        }
    }

    output.pitch[on] = semitones;
    TrackEvent e;
    e.kind = EventKind::kPitch;
    e.semitones = semitones;
    e.after = after;
    Emit(perf, output, tick, e, on);

    return true;
}

void Recorder::SwitchTo(Performance& perf, TrackOutput& output, uint32_t tick, uint8_t part)
{
    const uint8_t old = output.part;
    output.part = part;
    part_switches_.emplace_back(tick, output.midi_track, part);
    MarkUsed(output.midi_track, part);
    if (!options_.resend_on_switch)
    {
        std::erase_if(output.sent, [part](const auto& sent) { return sent.first.first == part; });
        output.pitch[part] = std::nullopt;
        return;
    }

    // The old part's values go out in the order of their kinds, then its pitch.
    std::vector<std::pair<EventKind, int>> values;
    for (const auto& [key, value] : output.sent)
    {
        if (key.first == old)
        {
            values.emplace_back(key.second, value);
        }
    }

    for (const auto& [kind, value] : values)
    {
        output.sent[{part, kind}] = value;
        TrackEvent e;
        e.kind = kind;
        if (kind == EventKind::kPreset)
        {
            e.preset = static_cast<uint32_t>(value);
        }
        else
        {
            e.value = static_cast<uint8_t>(value);
        }
        Emit(perf, output, tick, e);
    }

    const auto pitch = output.pitch.find(old);
    if (pitch != output.pitch.end() && pitch->second)
    {
        output.pitch[part] = pitch->second;
        TrackEvent e;
        e.kind = EventKind::kPitch;
        e.semitones = *pitch->second;
        Emit(perf, output, tick, e);
    }
}

void Recorder::MarkUsed(uint8_t midi_track, uint8_t part)
{
    parts_used_[midi_track] = std::max<uint8_t>(parts_used_[midi_track], static_cast<uint8_t>(part + 1));
}

uint32_t Recorder::NextNote() const
{
    return next_note_;
}

uint32_t Recorder::NoteOn(Performance& perf, const TrackOutput& output, uint32_t tick, uint8_t key, uint8_t velocity)
{
    TrackEvent e;
    e.kind = EventKind::kNoteOn;
    e.key = key;
    e.value = velocity;
    e.note = next_note_++;
    Emit(perf, output, tick, e);
    MarkUsed(output.midi_track, output.part);

    return e.note;
}

void Recorder::NoteOff(Performance& perf, uint8_t midi_track, uint32_t tick, uint8_t part, uint8_t key, uint32_t note,
                       double after) const
{
    TrackEvent e;
    e.tick = tick;
    e.kind = EventKind::kNoteOff;
    e.key = key;
    e.note = note;
    e.part = part;
    e.after = after;
    perf.tracks[midi_track].push_back(e);
}

void Recorder::SetTempo(Performance& perf, uint32_t tick, double bpm) const
{
    if (bpm <= 0.0)
    {
        return;
    }

    if (!perf.tempo.empty() && perf.tempo.back().tick == tick)
    {
        perf.tempo.back().bpm = bpm;
    }
    else if (perf.tempo.empty() || perf.tempo.back().bpm != bpm)
    {
        perf.tempo.push_back({tick, bpm});
    }
}

void Recorder::RecordFrameStart(Performance& perf, std::span<const double> tick_times, uint32_t tick,
                                double start) const
{
    // The last tick at or before the start, and the fraction of the way to the tick after it.
    std::size_t k = tick_times.empty() ? 0 : tick_times.size() - 1;
    while (k > 0 && tick_times[k] > start)
    {
        k--;
    }

    double place = static_cast<double>(k);
    if (k + 1 < tick_times.size() && tick_times[k + 1] > tick_times[k])
    {
        place += (start - tick_times[k]) / (tick_times[k + 1] - tick_times[k]);
    }
    if (options_.frame_ticks_from_zero)
    {
        place = std::max(place, 0.0);
    }
    perf.frame_ticks.push_back(std::min(place, static_cast<double>(tick)));
}

void Recorder::SetPartLimits(const std::array<uint8_t, 16>& limits)
{
    part_limits_ = limits;
}

const std::array<uint8_t, 16>& Recorder::PartsUsed() const
{
    return parts_used_;
}

const std::vector<PartSwitch>& Recorder::PartSwitches() const
{
    return part_switches_;
}

uint8_t Recorder::Expression(double level) const
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(level * level_scale_)), 0, 127));
}

void Recorder::SetLevelScale(double scale)
{
    level_scale_ = scale;
}

void Recorder::RaiseLevelPeak(double level)
{
    level_peak_ = std::max(level_peak_, level);
}

double Recorder::LevelPeak() const
{
    return level_peak_;
}

void Recorder::Approximate(const std::string& what)
{
    approximations_[what]++;
}

const std::map<std::string, uint32_t>& Recorder::Approximations() const
{
    return approximations_;
}

void Recorder::Warn(const std::string& text)
{
    warnings_.push_back(text);
}

const std::vector<std::string>& Recorder::Warnings() const
{
    return warnings_;
}

std::set<uint32_t> CutAt(Performance& perf, uint32_t end_tick)
{
    std::set<uint32_t> cut;
    for (std::vector<TrackEvent>& events : perf.tracks)
    {
        // A note-on with id 0 is no note (Emit). Such a note-on cuts no other event.
        std::set<uint32_t> late;
        for (const TrackEvent& e : events)
        {
            if (e.kind == EventKind::kNoteOn && e.note != 0 && e.tick >= end_tick)
            {
                late.insert(e.note);
            }
        }

        std::erase_if(events, [&](const TrackEvent& e) { return e.tick > end_tick || late.contains(e.note); });
        cut.insert(late.begin(), late.end());
    }

    return cut;
}

void ClampNoteOffs(Performance& perf, uint32_t end_tick)
{
    for (std::vector<TrackEvent>& events : perf.tracks)
    {
        for (TrackEvent& e : events)
        {
            if (e.kind == EventKind::kNoteOff && e.tick > end_tick)
            {
                e.tick = end_tick;
            }
        }
    }
}

} // namespace seqlib::midi
