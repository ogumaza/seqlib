// SPDX-License-Identifier: MIT

// Tests of seqlib/midi/reader.h, on files built here.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "seqlib/midi/reader.h"
#include "seqlib/read_error.h"
#include "test.h"

namespace
{

using seqlib::ReadError;
namespace midi = seqlib::midi;

// Returns a file of format 1 whose track chunks hold `tracks`, timed in 1/480 of a quarter note.
std::vector<uint8_t> MidiFile(const std::vector<std::vector<uint8_t>>& tracks)
{
    const auto count = static_cast<uint16_t>(tracks.size());
    std::vector<uint8_t> d = {
        'M',  'T', 'h', 'd', 0, 0, 0, 6, 0, 1, static_cast<uint8_t>(count >> 8), static_cast<uint8_t>(count),
        0x01, 0xe0};
    for (const std::vector<uint8_t>& t : tracks)
    {
        const auto size = static_cast<uint32_t>(t.size());
        d.insert(d.end(), {'M', 'T', 'r', 'k', static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16),
                           static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)});
        d.insert(d.end(), t.begin(), t.end());
    }

    return d;
}

// A track named "Lead" with a note whose note-off is a note-on of velocity 0 in running status, a bend, a marker and
// the end of the track.
std::vector<uint8_t> LeadTrack()
{
    return {0x00, 0xff, 0x03, 0x04, 'L',  'e',  'a',  'd',  0x00, 0x90, 0x3c, 0x64, 0x60,
            0x3c, 0x00, 0x81, 0x00, 0xe1, 0x00, 0x40, 0x00, 0xff, 0x06, 0x09, 'l',  'o',
            'o',  'p',  'S',  't',  'a',  'r',  't',  0x00, 0xff, 0x2f, 0x00};
}

// A written track: a note, its note-off, and the end.
std::vector<uint8_t> WrittenTrack()
{
    return {0x00, 0xff, 0x03, 0x01, 'T', 0x00, 0x90, 0x3c, 0x64, 0x60, 0x80, 0x3c, 0x40, 0x00, 0xff, 0x2f, 0x00};
}

// Returns the error that reading `data` throws, or none.
std::optional<ReadError> ReadErrorOf(const std::vector<uint8_t>& data)
{
    try
    {
        midi::Read(data);
    }
    catch (const ReadError& e)
    {
        return e;
    }

    return std::nullopt;
}

bool HasProblem(const std::vector<std::string>& problems, const std::string& part)
{
    const auto has = [&part](const std::string& p)
    {
        return p.find(part) != std::string::npos;
    };
    return std::any_of(problems.begin(), problems.end(), has);
}

SEQLIB_TEST(midi_reader, ReadsEventsWithTheirTicks)
{
    const std::vector<uint8_t> data = MidiFile({LeadTrack()});

    const midi::File file = midi::Read(data);

    SEQLIB_CHECK(file.format == 1);
    SEQLIB_CHECK(file.division == 480);
    SEQLIB_CHECK(file.tracks.size() == 1);
    const std::vector<midi::Event>& events = file.tracks[0].events;
    SEQLIB_CHECK(events.size() == 6);
    SEQLIB_CHECK(events[1].tick == 0 && events[1].bytes == std::vector<uint8_t>({0x90, 0x3c, 0x64}));
    SEQLIB_CHECK(events[2].tick == 96 && events[2].bytes == std::vector<uint8_t>({0x90, 0x3c, 0x00}));
    SEQLIB_CHECK(events[3].tick == 224 && events[3].bytes == std::vector<uint8_t>({0xe1, 0x00, 0x40}));
    SEQLIB_CHECK(events[5].bytes == std::vector<uint8_t>({0xff, 0x2f, 0x00}));
    SEQLIB_CHECK(file.tracks[0].end_tick == 224);
}

SEQLIB_TEST(midi_reader, PairsNotesEarliestFirst)
{
    const std::vector<uint8_t> kOverlapping = {0x00, 0x91, 0x40, 0x10, 0x0a, 0x91, 0x40, 0x20, 0x0a, 0x81,
                                               0x40, 0x00, 0x0a, 0x81, 0x40, 0x00, 0x00, 0xff, 0x2f, 0x00};
    const midi::File file = midi::Read(MidiFile({LeadTrack(), kOverlapping}));

    const std::vector<midi::Note> notes = midi::Notes(file);

    SEQLIB_CHECK(notes.size() == 3);
    SEQLIB_CHECK(notes[0].track == 0 && notes[0].channel == 0 && notes[0].key == 60 && notes[0].velocity == 100);
    SEQLIB_CHECK(notes[0].on == 0 && notes[0].off == 96);
    SEQLIB_CHECK(notes[1].track == 1 && notes[1].velocity == 0x10 && notes[1].on == 0 && notes[1].off == 20);
    SEQLIB_CHECK(notes[2].velocity == 0x20 && notes[2].on == 10 && notes[2].off == 30);
}

SEQLIB_TEST(midi_reader, FindsNamesMarkersAndBends)
{
    const midi::File file = midi::Read(MidiFile({LeadTrack(), WrittenTrack()}));

    const std::vector<std::string> names = midi::TrackNames(file);
    const std::vector<midi::Text> markers = midi::Markers(file);
    const std::vector<midi::Bend> bends = midi::Bends(file);

    SEQLIB_CHECK(names == std::vector<std::string>({"Lead", "T"}));
    SEQLIB_CHECK(markers.size() == 1 && markers[0].tick == 224 && markers[0].text == "loopStart");
    SEQLIB_CHECK(bends.size() == 1 && bends[0].track == 0 && bends[0].channel == 1);
    SEQLIB_CHECK(bends[0].tick == 224 && bends[0].value == 8192);
}

SEQLIB_TEST(midi_reader, FindsEventsInOrderOfTick)
{
    const std::vector<uint8_t> kFirst = {0x0a, 0xb0, 0x07, 0x64, 0x00, 0xff, 0x2f, 0x00};
    const std::vector<uint8_t> kSecond = {0x05, 0xb0, 0x07, 0x32, 0x05, 0xb0, 0x0a, 0x40, 0x00, 0xff, 0x2f, 0x00};
    const midi::File file = midi::Read(MidiFile({kFirst, kSecond}));

    const std::vector<midi::Event> found = file.Find(0xb0);

    SEQLIB_CHECK(found.size() == 3);
    SEQLIB_CHECK(found[0].tick == 5 && found[0].bytes[2] == 0x32);
    SEQLIB_CHECK(found[1].tick == 10 && found[1].bytes[2] == 0x64);
    SEQLIB_CHECK(found[2].tick == 10 && found[2].bytes[2] == 0x40);
}

SEQLIB_TEST(midi_reader, ThrowsOnMalformedData)
{
    std::vector<uint8_t> overrun = MidiFile({WrittenTrack()});
    overrun[21] += 1;
    const std::vector<uint8_t> kLongDelta = {0x81, 0x81, 0x81, 0x81, 0x00, 0xff, 0x2f, 0x00};
    const std::vector<uint8_t> kNoStatus = {0x00, 0x3c, 0x64, 0x00, 0xff, 0x2f, 0x00};
    const std::vector<uint8_t> kCutShort = {0x00, 0x90, 0x3c};
    std::vector<uint8_t> two_tracks = MidiFile({WrittenTrack()});
    two_tracks[11] = 2;

    const std::optional<ReadError> header = ReadErrorOf({'M', 'T', 'h', 'd'});
    const std::optional<ReadError> chunk = ReadErrorOf(overrun);
    const std::optional<ReadError> delta = ReadErrorOf(MidiFile({kLongDelta}));
    const std::optional<ReadError> running = ReadErrorOf(MidiFile({kNoStatus}));
    const std::optional<ReadError> event = ReadErrorOf(MidiFile({kCutShort}));
    const std::optional<ReadError> tracks = ReadErrorOf(two_tracks);

    SEQLIB_CHECK(header && header->Offset() == 0);
    SEQLIB_CHECK(chunk && chunk->Offset() == 14 && chunk->Reason().find("past the end") != std::string::npos);
    SEQLIB_CHECK(delta && delta->Offset() == 22 && delta->Reason().find("four bytes") != std::string::npos);
    SEQLIB_CHECK(running && running->Offset() == 23 && running->Reason().find("running status") != std::string::npos);
    SEQLIB_CHECK(event && event->Reason().find("past the end of its track") != std::string::npos);
    SEQLIB_CHECK(tracks && tracks->Reason().find("2 tracks") != std::string::npos);
}

SEQLIB_TEST(midi_reader, WrittenFileHasNoProblems)
{
    const std::vector<uint8_t> data = MidiFile({WrittenTrack(), WrittenTrack()});

    const std::vector<std::string> problems = midi::Problems(data);

    SEQLIB_CHECK(problems.empty());
}

SEQLIB_TEST(midi_reader, ListsProblems)
{
    const std::vector<uint8_t> kAgain = {0x00, 0x90, 0x3c, 0x64, 0x01, 0x90, 0x3c, 0x64, 0x01, 0x80,
                                         0x3c, 0x40, 0x01, 0x80, 0x3c, 0x40, 0x00, 0xff, 0x2f, 0x00};
    const std::vector<uint8_t> kUnended = {0x00, 0x90, 0x3c, 0x64, 0x00, 0xff, 0x2f, 0x00};
    const std::vector<uint8_t> kNoEnd = {0x00, 0x90, 0x3c, 0x64, 0x01, 0x80, 0x3c, 0x40};
    const std::vector<uint8_t> kLongDelta = {0x81, 0x81, 0x81, 0x81, 0x00, 0xff, 0x2f, 0x00};
    std::vector<uint8_t> trailing = MidiFile({WrittenTrack()});
    trailing.push_back(0);
    std::vector<uint8_t> long_header = MidiFile({WrittenTrack()});
    long_header[7] = 8;
    long_header.insert(long_header.begin() + 14, {0, 0});

    const std::vector<std::string> running = midi::Problems(MidiFile({LeadTrack()}));
    const std::vector<std::string> overlapping = midi::Problems(MidiFile({kAgain}));
    const std::vector<std::string> never_ends = midi::Problems(MidiFile({kUnended}));
    const std::vector<std::string> missing_end = midi::Problems(MidiFile({kNoEnd}));
    const std::vector<std::string> delta = midi::Problems(MidiFile({kLongDelta}));
    const std::vector<std::string> after = midi::Problems(trailing);
    const std::vector<std::string> header = midi::Problems(long_header);

    SEQLIB_CHECK(running.size() == 1 && HasProblem(running, "track 0: running status in 1 events"));
    SEQLIB_CHECK(overlapping.size() == 1 && HasProblem(overlapping, "key 60 on channel 0 starts again at tick 1"));
    SEQLIB_CHECK(never_ends.size() == 1 && HasProblem(never_ends, "key 60 on channel 0 from tick 0 never ends"));
    SEQLIB_CHECK(missing_end.size() == 1 && HasProblem(missing_end, "no end-of-track event"));
    SEQLIB_CHECK(delta.size() == 1 && HasProblem(delta, "longer than four bytes"));
    SEQLIB_CHECK(after.size() == 1 && HasProblem(after, "1 bytes after the last track"));
    SEQLIB_CHECK(header.size() == 1 && HasProblem(header, "header of 8 bytes, not 6"));
}

} // namespace
