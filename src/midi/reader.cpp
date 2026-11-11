// SPDX-License-Identifier: MIT

// Reading Standard MIDI Files (see reader.h).

#include "seqlib/midi/reader.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <deque>
#include <map>
#include <optional>
#include <utility>

#include "bytes.h"
#include "midi/note_event.h"
#include "midi/values.h"
#include "seqlib/midi/encoder.h"
#include "seqlib/read_error.h"

namespace seqlib::midi
{
namespace
{

using detail::AsNoteEvent;
using detail::kMaxDataByte;
using detail::LoadBe16;
using detail::LoadBe32;
using detail::NoteEvent;

std::string TrackLabel(int track)
{
    return "track " + std::to_string(track);
}

// Reads a variable-length number at `p` and moves `p` past it. Throws ReadError if the number runs past `end` or is
// longer than four bytes.
uint32_t ReadVlq(std::span<const uint8_t> d, std::size_t& p, std::size_t end)
{
    const std::size_t start = p;
    uint32_t value = 0;
    for (int i = 0; i < 4; i++)
    {
        if (p >= end)
        {
            throw ReadError(start, "variable-length number runs past the end of its track");
        }

        const uint8_t b = d[p++];
        value = value << 7 | (b & 0x7f);
        if (!(b & 0x80))
        {
            return value;
        }
    }

    throw ReadError(start, "variable-length number longer than four bytes");
}

// Returns the number of data bytes that follow a channel message's status byte.
std::size_t DataBytes(uint8_t status_byte)
{
    const int kind = status_byte & 0xF0;
    return kind == status::kProgram || kind == status::kChannelPressure ? 1 : 2;
}

// What ReadTrack finds in a track that Problems() reports.
struct TrackFindings
{
    std::size_t running_events = 0; // events written with running status
    std::size_t first_running = 0;  // the byte of the first of them
    std::size_t high_data = 0;      // data bytes above 127
    bool ended = false;             // the track has an end-of-track event
    bool after_end = false;         // an event follows the end-of-track event
};

// Adds a line to `problems` for each problem that `found` shows in track `index`.
void AddTrackProblems(const TrackFindings& found, int index, std::vector<std::string>& problems)
{
    const std::string label = TrackLabel(index);
    if (found.running_events != 0)
    {
        problems.push_back(label + ": running status in " + std::to_string(found.running_events) +
                           " events, the first at byte " + std::to_string(found.first_running));
    }
    if (found.high_data != 0)
    {
        problems.push_back(label + ": " + std::to_string(found.high_data) + " data bytes above 127");
    }
    if (!found.ended)
    {
        problems.push_back(label + ": no end-of-track event");
    }
    else if (found.after_end)
    {
        problems.push_back(label + ": events after its end-of-track event");
    }
}

// Reads the events of track chunk `index`. The chunk's data runs from `begin` to `end`. Adds a line to `problems`, if
// it isn't null, for running status, a data byte above 127, and an end-of-track event that's missing or not last.
Track ReadTrack(std::span<const uint8_t> d, std::size_t begin, std::size_t end, int index,
                std::vector<std::string>* problems)
{
    Track track;
    uint64_t tick = 0;
    uint8_t running = 0; // the status that running status repeats, or 0 after a meta or system exclusive event
    TrackFindings found;
    std::size_t p = begin;
    while (p < end)
    {
        tick += ReadVlq(d, p, end);
        if (p >= end)
        {
            throw ReadError(p, "event runs past the end of its track");
        }

        found.after_end = found.after_end || found.ended;
        const std::size_t at = p;
        uint8_t status_byte = d[p];
        if (status_byte <= kMaxDataByte)
        {
            if (running == 0)
            {
                throw ReadError(at, "running status with no status before it");
            }

            if (found.running_events == 0)
            {
                found.first_running = at;
            }
            found.running_events++;
            status_byte = running;
        }
        else
        {
            p++;
        }

        if (status_byte == status::kMeta || status_byte == status::kSysEx || status_byte == status::kSysExEnd)
        {
            // A meta event has a type before its length. A meta or system exclusive event ends running status.
            const bool is_meta = status_byte == status::kMeta;
            if (is_meta && p >= end)
            {
                throw ReadError(at, "event runs past the end of its track");
            }

            const bool end_of_track = is_meta && d[p] == meta::kEndOfTrack;
            p += is_meta ? 1 : 0;
            const uint32_t length = ReadVlq(d, p, end);
            if (length > end - p)
            {
                throw ReadError(at, "event runs past the end of its track");
            }

            p += length;
            found.ended = found.ended || end_of_track;
            running = 0;
        }
        else if (status_byte >= status::kSysEx)
        {
            throw ReadError(at, "status byte " + std::to_string(status_byte) + " can't be in a file");
        }
        else
        {
            const std::size_t n = DataBytes(status_byte);
            if (n > end - p)
            {
                throw ReadError(at, "event runs past the end of its track");
            }

            found.high_data += static_cast<std::size_t>(std::count_if(d.begin() + static_cast<std::ptrdiff_t>(p),
                                                                      d.begin() + static_cast<std::ptrdiff_t>(p + n),
                                                                      [](uint8_t b) { return b > kMaxDataByte; }));
            p += n;
            running = status_byte;
        }

        Event e{tick, std::vector<uint8_t>(d.begin() + static_cast<std::ptrdiff_t>(at),
                                           d.begin() + static_cast<std::ptrdiff_t>(p))};
        if (e.bytes[0] <= kMaxDataByte)
        {
            e.bytes.insert(e.bytes.begin(), status_byte);
        }
        track.events.push_back(std::move(e));
    }
    track.end_tick = tick;

    if (problems != nullptr)
    {
        AddTrackProblems(found, index, *problems);
    }

    return track;
}

// Reads `d` as Read does, and adds a line to `problems`, if it isn't null, for each problem found while reading.
File Parse(std::span<const uint8_t> d, std::vector<std::string>* problems)
{
    if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4) != 0)
    {
        throw ReadError(0, "no MIDI header (MThd)");
    }

    const uint32_t header = LoadBe32(d.data() + 4);
    if (header < 6 || header > d.size() - 8)
    {
        throw ReadError(4, "header of " + std::to_string(header) + " bytes");
    }
    if (header != 6 && problems != nullptr)
    {
        problems->push_back("header of " + std::to_string(header) + " bytes, not 6");
    }

    File file;
    file.format = LoadBe16(d.data() + 8);
    const uint16_t count = LoadBe16(d.data() + 10);
    file.division = LoadBe16(d.data() + 12);

    // Read the track chunks, and skip any other chunk.
    std::size_t p = 8 + header;
    while (file.tracks.size() < count)
    {
        if (p > d.size() - 8)
        {
            throw ReadError(p, "the header gives " + std::to_string(count) + " tracks, but the data holds " +
                                   std::to_string(file.tracks.size()));
        }

        const uint32_t size = LoadBe32(d.data() + p + 4);
        const std::size_t begin = p + 8;
        if (size > d.size() - begin)
        {
            throw ReadError(p, "chunk runs past the end of the data");
        }

        if (std::memcmp(d.data() + p, "MTrk", 4) == 0)
        {
            const int index = static_cast<int>(file.tracks.size());
            file.tracks.push_back(ReadTrack(d, begin, begin + size, index, problems));
        }
        else if (problems != nullptr)
        {
            problems->push_back("a chunk other than MTrk at byte " + std::to_string(p));
        }

        p = begin + size;
    }

    if (p != d.size() && problems != nullptr)
    {
        problems->push_back(std::to_string(d.size() - p) + " bytes after the last track");
    }

    return file;
}

// Returns the text of meta event `e`: the bytes after its type and length.
std::string MetaText(const Event& e)
{
    const std::vector<uint8_t> data = MetaData(e);

    return std::string(data.begin(), data.end());
}

bool IsMeta(const Event& e, uint8_t type)
{
    return e.bytes.size() >= 2 && e.bytes[0] == status::kMeta && e.bytes[1] == type;
}

} // namespace

std::vector<Event> File::Find(uint8_t status) const
{
    std::vector<Event> found;
    for (const Track& t : tracks)
    {
        for (const Event& e : t.events)
        {
            if (e.bytes[0] == status)
            {
                found.push_back(e);
            }
        }
    }

    const auto earlier = [](const Event& a, const Event& b)
    {
        return a.tick < b.tick;
    };
    std::stable_sort(found.begin(), found.end(), earlier);

    return found;
}

File Read(std::span<const uint8_t> data)
{
    return Parse(data, nullptr);
}

std::vector<uint8_t> MetaData(const Event& event)
{
    std::size_t p = 2;
    while (p < event.bytes.size() && (event.bytes[p] & 0x80))
    {
        p++;
    }

    return std::vector<uint8_t>(event.bytes.begin() + static_cast<std::ptrdiff_t>(std::min(p + 1, event.bytes.size())),
                                event.bytes.end());
}

std::vector<Note> Notes(const File& file)
{
    std::vector<Note> notes;
    for (std::size_t t = 0; t < file.tracks.size(); t++)
    {
        // The notes that sound on each channel and key, earliest first: (tick, velocity) pairs.
        std::map<std::pair<int, int>, std::deque<std::pair<uint64_t, int>>> sounding;
        for (const Event& e : file.tracks[t].events)
        {
            const std::optional<NoteEvent> note = AsNoteEvent(e.bytes);
            if (!note)
            {
                continue;
            }

            std::deque<std::pair<uint64_t, int>>& on = sounding[{note->channel, note->key}];
            if (note->on)
            {
                on.push_back({e.tick, note->velocity});
            }
            else if (!on.empty())
            {
                notes.push_back(
                    {static_cast<int>(t), note->channel, note->key, on.front().second, on.front().first, e.tick});
                on.pop_front();
            }
        }
    }

    return notes;
}

std::vector<std::string> TrackNames(const File& file)
{
    std::vector<std::string> names;
    for (const Track& t : file.tracks)
    {
        std::string name;
        for (const Event& e : t.events)
        {
            if (IsMeta(e, meta::kTrackName))
            {
                name = MetaText(e);
            }
        }
        names.push_back(name);
    }

    return names;
}

std::vector<Text> Markers(const File& file)
{
    std::vector<Text> markers;
    for (const Track& t : file.tracks)
    {
        for (const Event& e : t.events)
        {
            if (IsMeta(e, meta::kMarker))
            {
                markers.push_back({e.tick, MetaText(e)});
            }
        }
    }

    return markers;
}

std::vector<Bend> Bends(const File& file)
{
    std::vector<Bend> bends;
    for (std::size_t t = 0; t < file.tracks.size(); t++)
    {
        for (const Event& e : file.tracks[t].events)
        {
            if ((e.bytes[0] & 0xF0) == status::kPitchBend)
            {
                bends.push_back({static_cast<int>(t), e.bytes[0] & 0x0F, e.tick, e.bytes[1] | e.bytes[2] << 7});
            }
        }
    }

    return bends;
}

std::vector<Tempo> Tempos(const File& file)
{
    std::vector<Tempo> tempos;
    for (std::size_t t = 0; t < file.tracks.size(); t++)
    {
        for (const Event& e : file.tracks[t].events)
        {
            if (!IsMeta(e, meta::kTempo))
            {
                continue;
            }

            const std::vector<uint8_t> data = MetaData(e);
            if (data.size() == 3)
            {
                const uint32_t micros = uint32_t{data[0]} << 16 | uint32_t{data[1]} << 8 | data[2];
                tempos.push_back({static_cast<int>(t), e.tick, micros});
            }
        }
    }

    const auto earlier = [](const Tempo& a, const Tempo& b)
    {
        return a.tick < b.tick;
    };
    std::stable_sort(tempos.begin(), tempos.end(), earlier);

    return tempos;
}

std::vector<std::string> Problems(std::span<const uint8_t> data)
{
    std::vector<std::string> problems;
    File file;
    try
    {
        file = Parse(data, &problems);
    }
    catch (const ReadError& e)
    {
        problems.push_back(e.what());
        return problems;
    }

    // A note may not start again on its channel before it ends, and every note has to end.
    for (std::size_t t = 0; t < file.tracks.size(); t++)
    {
        const std::string label = TrackLabel(static_cast<int>(t));
        std::map<std::pair<int, int>, uint64_t> sounding;
        for (const Event& e : file.tracks[t].events)
        {
            const std::optional<NoteEvent> note = AsNoteEvent(e.bytes);
            if (!note)
            {
                continue;
            }

            const std::pair<int, int> channel_and_key = {note->channel, note->key};
            const auto it = sounding.find(channel_and_key);
            if (note->on)
            {
                if (it != sounding.end())
                {
                    problems.push_back(label + ": key " + std::to_string(note->key) + " on channel " +
                                       std::to_string(note->channel) + " starts again at tick " +
                                       std::to_string(e.tick) + " before it ends");
                }
                sounding[channel_and_key] = e.tick;
            }
            else if (it != sounding.end())
            {
                sounding.erase(it);
            }
        }

        for (const auto& [channel_and_key, tick] : sounding)
        {
            const auto& [channel, key] = channel_and_key;
            problems.push_back(label + ": key " + std::to_string(key) + " on channel " + std::to_string(channel) +
                               " from tick " + std::to_string(tick) + " never ends");
        }
    }

    return problems;
}

} // namespace seqlib::midi
