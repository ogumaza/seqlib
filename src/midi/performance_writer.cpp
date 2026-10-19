// SPDX-License-Identifier: MIT

// Standard MIDI File output for a Performance (see performance_writer.h).

#include "seqlib/midi/performance_writer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <optional>
#include <utility>

#include "seqlib/midi/encoder.h"

namespace seqlib::midi
{
namespace
{

// General MIDI drum channel (channel 10).
constexpr int kDrumChannel = 9;

// The order in which tracks get MIDI channels. The drum channel comes last.
constexpr int kChannelOrder[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, kDrumChannel};

// Minimum MIDI ticks per quarter note. Events are rounded to the nearest MIDI tick at the start of a player's frame
// (see EventTick). At 120 BPM the error is at most 1/30 ms, about one sample at 32 kHz. This preserves the phase
// relationship of rapid repeated notes in players with exact event timing.
//
// FluidSynth also rounds its position to a tick at each tempo change, then restarts its clock there. Each change can
// advance the rest of the file by half a tick. At the sequence's original 48 or 96 ticks per quarter note, the error
// can be about 5 ms per change at 120 BPM. During gradual tempo changes, the errors add up to over a second.
constexpr uint32_t kMinDivision = 7680;

// Returns the smallest integer MIDI ticks per performance tick that meets kMinDivision.
uint32_t TickScale(const Performance& perf)
{
    const uint32_t timebase = std::max<uint32_t>(perf.timebase, 1);
    return (kMinDivision + timebase - 1) / timebase;
}

// Returns true if `perf` has a loop that the file marks: one that ends after it starts. The writer ignores any other
// loop, in the markers and in the timing of events.
bool MarksLoop(const Performance& perf)
{
    return perf.loop && perf.loop->end > perf.loop->start;
}

// Maps performance tick `tick` to a MIDI tick. A player processes ticks within its frames, and its changes sound from a
// frame boundary. Places MIDI events at the start of their tick's frame (Performance::frame_ticks), or of their frame
// between ticks (TrackEvent::after), rounded to the nearest MIDI tick. `timing` sets whether a frame's start may come
// after its tick. Clamps at loop markers to keep events in the correct loop. An event before a marker goes at least one
// MIDI tick before it. A player that loops at the marker can skip an event on the marker's tick.
//
// Uses 64 bits: MIDI tick positions can exceed 32 bits if a sequence starts with a small timebase and later increases
// it.
uint64_t EventTick(const Performance& perf, FrameTiming timing, uint32_t tick, double after)
{
    const uint64_t scale = TickScale(perf);
    uint64_t midi_tick = tick * scale;
    if (after > 0)
    {
        midi_tick = static_cast<uint64_t>(std::llround((tick + after) * static_cast<double>(scale)));
    }
    else if (tick < perf.frame_ticks.size())
    {
        const uint64_t frame_tick = static_cast<uint64_t>(std::llround(perf.frame_ticks[tick] * scale));
        midi_tick = timing == FrameTiming::kFrameStart ? frame_tick : std::min(midi_tick, frame_tick);
    }

    if (MarksLoop(perf))
    {
        for (const uint32_t marker : {perf.loop->start, perf.loop->end})
        {
            if (tick >= marker)
            {
                midi_tick = std::max(midi_tick, marker * scale);
            }
            else
            {
                // Here the marker is above `tick`, and so above 0. The subtraction can't wrap.
                midi_tick = std::min(midi_tick, marker * scale - 1);
            }
        }
    }

    return midi_tick;
}

// The channels of a track: for each of its parts (TrackEvent::part), the channel of each layer. The list is empty for a
// part that gets none. Such a part plays on the first part's channels if it has notes (see PartsThatPlay).
using PartChannels = std::vector<std::vector<int>>;

// The number of parts track `t` of `perf` uses: one more than the highest.
std::size_t PartCount(const Performance& perf, int t)
{
    std::size_t parts = 1;
    for (const TrackEvent& e : perf.tracks[t])
    {
        parts = std::max<std::size_t>(parts, e.part + std::size_t{1});
    }

    return parts;
}

// Returns whether each part of track `t` of `perf` plays a note. The events of a part without notes have no note to
// affect. On the first part's channels, they could change the sounds that play there. The writer drops them.
std::vector<bool> PartsThatPlay(const Performance& perf, int t)
{
    std::vector<bool> plays(PartCount(perf, t), false);
    for (const TrackEvent& e : perf.tracks[t])
    {
        if (e.kind == EventKind::kNoteOn)
        {
            plays[e.part] = true;
        }
    }

    return plays;
}

// Counts the notes of `part` per layer with no channel limit. Layer i holds notes whose key is already sounding on
// layers 0..i-1.
std::vector<uint32_t> LayerDemand(const std::vector<TrackEvent>& events, uint8_t part)
{
    std::vector<uint32_t> demand;
    std::array<std::vector<uint32_t>, 128> held{}; // per key: note id per layer, 0: free
    std::map<uint32_t, std::pair<uint8_t, std::size_t>> where;
    for (const TrackEvent& e : events)
    {
        if (e.part != part)
        {
            continue;
        }

        const uint8_t key = e.key;
        if (e.kind == EventKind::kNoteOn)
        {
            std::vector<uint32_t>& layers = held[key];
            std::size_t l = 0;
            while (l < layers.size() && layers[l] != 0)
            {
                l++;
            }
            if (l == layers.size())
            {
                layers.push_back(0);
            }

            layers[l] = e.note;
            where[e.note] = {key, l};

            if (demand.size() <= l)
            {
                demand.resize(l + 1, 0);
            }
            demand[l]++;
        }
        else if (e.kind == EventKind::kNoteOff)
        {
            const auto it = where.find(e.note);
            if (it != where.end())
            {
                held[it->second.first][it->second.second] = 0;
                where.erase(it);
            }
        }
    }

    return demand;
}

// Gives the first part of each active track one MIDI channel, then each of its other parts with notes, track by track.
// Stops when the channels run out. Allocates remaining channels one at a time to the part whose next layer would save
// the most notes from being cut short.
std::array<PartChannels, 16> AssignChannels(const Performance& perf)
{
    std::array<std::vector<std::vector<uint32_t>>, 16> demand; // per track, per part
    std::array<PartChannels, 16> channels;
    for (int t = 0; t < 16; t++)
    {
        demand[t].resize(PartCount(perf, t));
        channels[t].resize(demand[t].size());
        for (std::size_t p = 0; p < demand[t].size(); p++)
        {
            demand[t][p] = LayerDemand(perf.tracks[t], static_cast<uint8_t>(p));
        }
    }

    int next = 0;
    for (int t = 0; t < 16; t++)
    {
        auto plays = [](const std::vector<uint32_t>& d)
        {
            return !d.empty();
        };
        if (std::ranges::any_of(demand[t], plays))
        {
            channels[t][0].push_back(kChannelOrder[next++]);
        }
    }

    for (int t = 0; t < 16; t++)
    {
        for (std::size_t p = 1; p < demand[t].size() && next < 16; p++)
        {
            if (!demand[t][p].empty())
            {
                channels[t][p].push_back(kChannelOrder[next++]);
            }
        }
    }

    while (next < 16)
    {
        int best = -1;
        std::size_t best_part = 0;
        uint32_t best_demand = 0;
        for (int t = 0; t < 16; t++)
        {
            for (std::size_t p = 0; p < demand[t].size(); p++)
            {
                const std::size_t l = channels[t][p].size();
                if (l > 0 && l < demand[t][p].size() && demand[t][p][l] > best_demand)
                {
                    best = t;
                    best_part = p;
                    best_demand = demand[t][p][l];
                }
            }
        }
        if (best < 0)
        {
            break;
        }

        channels[best][best_part].push_back(kChannelOrder[next++]);
    }

    return channels;
}

// Writes the conductor track: title, tempo and loop markers. CC111 marks loop start for RPG Maker players; "loopStart"
// and "loopEnd" serve other players.
TrackEncoder ConductorTrack(const Performance& perf, const std::string& title,
                            const std::array<PartChannels, 16>& channels)
{
    const uint64_t scale = TickScale(perf);
    TrackEncoder conductor;
    conductor.Meta(0, 0x03, title);

    int first_channel = -1;
    for (const PartChannels& c : channels)
    {
        if (!c.empty() && !c[0].empty() && (first_channel < 0 || c[0][0] < first_channel))
        {
            first_channel = c[0][0];
        }
    }

    // Writes the tempo changes before tick `until`.
    std::size_t tempo_index = 0;
    auto tempo_before = [&](uint64_t until)
    {
        while (tempo_index < perf.tempo.size() && perf.tempo[tempo_index].tick < until)
        {
            conductor.Tempo(perf.tempo[tempo_index].tick * scale, perf.tempo[tempo_index].bpm);
            tempo_index++;
        }
    };

    if (MarksLoop(perf))
    {
        const uint64_t start = perf.loop->start;
        tempo_before(start);
        conductor.Meta(start * scale, 0x06, "loopStart");
        if (first_channel >= 0)
        {
            conductor.Event(start * scale, {static_cast<uint8_t>(0xb0 | first_channel), 111, 0});
        }

        // A player that loops goes back to the event after the marker, or after CC111. A tempo change on the loop's
        // first tick goes after both. Every pass then starts with it.
        tempo_before(start + 1);

        tempo_before(uint64_t{perf.loop->end} + 1);
        conductor.Meta(perf.loop->end * scale, 0x06, "loopEnd");
    }

    tempo_before(UINT64_MAX);
    conductor.End(perf.end_tick * scale);

    return conductor;
}

// Selects track `t`'s pitch bend range (RPN 0), up to MIDI's 127-semitone limit. The wheel has 8192 downward steps but
// only 8191 upward steps. The range must therefore exceed the largest positive bend. Warns in `report` if the track
// requires more. The parts in `plays` that play no note don't count (PartsThatPlay).
int BendRange(const Performance& perf, int t, const std::vector<bool>& plays, MidiReport& report)
{
    double needed = 0.0;
    for (const TrackEvent& e : perf.tracks[t])
    {
        if (e.kind == EventKind::kPitch && plays[e.part])
        {
            needed = std::max(needed, e.semitones > 0.0f ? e.semitones * 8192.0 / 8191.0 : -e.semitones);
        }
    }

    int range = 2;
    if (needed > 2.0)
    {
        range = static_cast<int>(std::ceil(needed - 1e-4));
    }
    if (range > 127)
    {
        report.warnings.push_back("track " + std::to_string(t) + " bends up to " + std::to_string(range) +
                                  " semitones; MIDI allows 127");
        range = 127;
    }

    return range;
}

// The controller that a setting of `kind` sets to the event's value: CC7, CC11, CC10, CC1, CC91 or CC93 for a named
// kind, or the controller of a ControlKind(). None for any other kind.
std::optional<uint8_t> ControllerFor(EventKind kind)
{
    switch (kind)
    {
    case EventKind::kVolume:
        return 7;
    case EventKind::kExpression:
        return 11;
    case EventKind::kPan:
        return 10;
    case EventKind::kModulation:
        return 1;
    case EventKind::kReverb:
        return 91;
    case EventKind::kChorus:
        return 93;
    default:
        return IsControlKind(kind) ? std::optional<uint8_t>(ControllerOf(kind)) : std::nullopt;
    }
}

// The layer among `mine` for a note-on of `key`: the first whose key is free, or else the one whose note of the key
// started first. `sounding` holds each layer's note on each key, 0 for a free key. `since` holds the tick that it
// started at.
std::size_t LayerForNote(const std::vector<std::size_t>& mine, const std::vector<std::array<uint32_t, 128>>& sounding,
                         const std::vector<std::array<uint32_t, 128>>& since, uint8_t key)
{
    const auto free = std::ranges::find_if(mine, [&](std::size_t l) { return sounding[l][key] == 0; });
    if (free != mine.end())
    {
        return *free;
    }

    std::size_t oldest = mine[0];
    for (const std::size_t l : mine)
    {
        if (since[l][key] < since[oldest][key])
        {
            oldest = l;
        }
    }

    return oldest;
}

// Writes one MIDI track per channel of a sequence track, part by part and layer by layer. Assigns a note to the first
// layer of its part with a free key. If all of them have that key in use, cuts that key's oldest note and records the
// loss in `report`. A part with notes but no channel plays on the first part's channels. Drops the events of a part
// without notes (PartsThatPlay). Records in `report` the track's bend range and the presets that it sends to the drum
// channel.
std::vector<TrackEncoder> TrackLayers(const Performance& perf, const MidiOptions& options, int t,
                                      const PartChannels& parts, MidiReport& report)
{
    std::vector<TrackEncoder> layers;
    std::vector<int> channels;
    std::vector<std::vector<std::size_t>> part_layers(parts.size());
    for (std::size_t p = 0; p < parts.size(); p++)
    {
        for (const int channel : parts[p])
        {
            const std::size_t l = layers.size();
            layers.emplace_back();
            const std::string& name = perf.track_names[static_cast<std::size_t>(t)];
            layers.back().Meta(0, 0x03,
                               (name.empty() ? "Track " + std::to_string(t) : name) +
                                   (l ? " (layer " + std::to_string(l + 1) + ")" : ""));
            channels.push_back(channel);
            part_layers[p].push_back(l);
        }
    }

    // The layers of part `part`.
    auto of_part = [&](uint8_t part) -> const std::vector<std::size_t>&
    {
        return part < part_layers.size() && !part_layers[part].empty() ? part_layers[part] : part_layers[0];
    };

    const std::vector<bool> plays = PartsThatPlay(perf, t);
    const int range = BendRange(perf, t, plays, report);
    report.bend_ranges[t] = range;

    // Sends a channel message to the layers `to` at MIDI tick `midi_tick`.
    auto send = [&](uint64_t midi_tick, const std::vector<std::size_t>& to, std::initializer_list<uint8_t> message)
    {
        for (const std::size_t l : to)
        {
            std::vector<uint8_t> b(message);
            b[0] = static_cast<uint8_t>((b[0] & 0xf0) | channels[l]);
            layers[l].Event(midi_tick, b);
        }
    };

    // Set RPN 0 to `range` semitones, then select the null RPN to prevent later data entry messages from changing it.
    std::vector<std::size_t> every(layers.size());
    for (std::size_t l = 0; l < layers.size(); l++)
    {
        every[l] = l;
    }
    send(0, every, {0xb0, 101, 0});
    send(0, every, {0xb0, 100, 0});
    send(0, every, {0xb0, 6, static_cast<uint8_t>(range)});
    send(0, every, {0xb0, 38, 0});
    send(0, every, {0xb0, 101, 127});
    send(0, every, {0xb0, 100, 127});

    std::vector<std::array<uint32_t, 128>> sounding(layers.size()); // note id per key, 0: free
    std::vector<std::array<uint32_t, 128>> since(layers.size());    // that note's start time
    std::map<uint32_t, std::size_t> layer_of;                       // note id -> layer
    for (const TrackEvent& e : perf.tracks[t])
    {
        if (!plays[e.part])
        {
            continue;
        }

        const uint8_t key = e.key;
        const std::vector<std::size_t>& mine = of_part(e.part);
        const uint64_t at = EventTick(perf, options.frame_timing, e.tick, e.after);
        switch (e.kind)
        {
        case EventKind::kNoteOn:
            {
                // A note-on whose layers all have its key in use cuts the oldest of those notes.
                const std::size_t l = LayerForNote(mine, sounding, since, key);
                if (sounding[l][key] != 0)
                {
                    layers[l].Event(at, {static_cast<uint8_t>(0x80 | channels[l]), key, 64});
                    layer_of.erase(sounding[l][key]);
                    report.cut_notes++;
                }

                sounding[l][key] = e.note;
                since[l][key] = e.tick;
                layer_of[e.note] = l;
                layers[l].Event(at, {static_cast<uint8_t>(0x90 | channels[l]), key, e.value});
                break;
            }

        case EventKind::kNoteOff:
            {
                auto it = layer_of.find(e.note);
                if (it == layer_of.end())
                {
                    break; // cut earlier
                }

                const std::size_t l = it->second;
                layer_of.erase(it);
                sounding[l][key] = 0;
                layers[l].Event(at, {static_cast<uint8_t>(0x80 | channels[l]), key, 64});
                break;
            }

        case EventKind::kPreset:
            {
                send(at, mine, {0xb0, 0, static_cast<uint8_t>((e.preset >> 7) & 0x7f)});
                send(at, mine, {0xb0, 32, 0});
                send(at, mine, {0xc0, static_cast<uint8_t>(e.preset & 0x7f)});

                // Synths play bank 128 on the drum channel. sf2::AddChannel10Copy copies the preset there.
                const auto on_drums = [&channels](std::size_t l)
                {
                    return channels[l] == kDrumChannel;
                };
                if (std::ranges::any_of(mine, on_drums))
                {
                    report.channel_10_presets.insert(e.preset);
                }
                break;
            }

        case EventKind::kSoundOff:
            send(at, mine, {0xb0, 120, 0});
            break;

        case EventKind::kPitch:
            {
                const int64_t v = std::clamp<int64_t>(8192 + std::llround(e.semitones / range * 8192.0), 0, 16383);
                send(at, mine, {0xe0, static_cast<uint8_t>(v & 0x7f), static_cast<uint8_t>(v >> 7)});
                break;
            }

        default:
            if (const std::optional<uint8_t> controller = ControllerFor(e.kind))
            {
                send(at, mine, {0xb0, *controller, e.value});
            }
            break;
        }
    }

    for (TrackEncoder& layer : layers)
    {
        layer.End(perf.end_tick * static_cast<uint64_t>(TickScale(perf)));
    }

    return layers;
}

} // namespace

std::vector<uint8_t> WriteMidi(const Performance& perf, const std::string& title, MidiReport& report,
                               const MidiOptions& options)
{
    const std::array<PartChannels, 16> channels = AssignChannels(perf);
    report.channels = 0;
    for (const PartChannels& parts : channels)
    {
        for (const std::vector<int>& c : parts)
        {
            report.channels += static_cast<int>(c.size());
        }
    }

    std::vector<TrackEncoder> tracks;
    tracks.push_back(ConductorTrack(perf, title, channels));
    for (int t = 0; t < 16; t++)
    {
        if (channels[t][0].empty())
        {
            continue;
        }

        for (TrackEncoder& x : TrackLayers(perf, options, t, channels[t], report))
        {
            tracks.push_back(std::move(x));
        }
    }

    std::vector<uint8_t> out;
    const uint16_t division = static_cast<uint16_t>(std::max<uint32_t>(perf.timebase, 1) * TickScale(perf));
    AppendHeader(out, 1, static_cast<uint16_t>(tracks.size()), division);
    for (const TrackEncoder& x : tracks)
    {
        x.AppendTo(out);
    }

    return out;
}

} // namespace seqlib::midi
