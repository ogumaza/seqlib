// SPDX-License-Identifier: MIT

// Tests of the MIDI encoder and the performance writer.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "midi_writer_cases.h"
#include "seqlib/midi/encoder.h"
#include "seqlib/midi/performance_writer.h"
#include "seqlib/midi/reader.h"
#include "test.h"
#include "test_data.h"

namespace seqlib::test
{
namespace
{

using midi::EventKind;

std::vector<uint8_t> Vlq(uint32_t v)
{
    std::vector<uint8_t> out;
    midi::AppendVlq(out, v);

    return out;
}

SEQLIB_TEST(midi_writer, VlqUsesSevenBitsAByte)
{
    SEQLIB_CHECK(Vlq(0) == std::vector<uint8_t>({0x00}));
    SEQLIB_CHECK(Vlq(0x7f) == std::vector<uint8_t>({0x7f}));
    SEQLIB_CHECK(Vlq(0x80) == std::vector<uint8_t>({0x81, 0x00}));
    SEQLIB_CHECK(Vlq(0x3fff) == std::vector<uint8_t>({0xff, 0x7f}));
    SEQLIB_CHECK(Vlq(0x0fffffff) == std::vector<uint8_t>({0xff, 0xff, 0xff, 0x7f}));
}

SEQLIB_TEST(midi_writer, HeaderAndTrackChunks)
{
    std::vector<uint8_t> out;

    midi::AppendHeader(out, 1, 3, 480);
    const std::vector<uint8_t> kBody = {0x00, 0xff, 0x2f, 0x00};
    midi::AppendTrack(out, kBody);

    const std::vector<uint8_t> kExpected = {'M',  'T', 'h', 'd', 0,   0, 0, 6, 0, 1,    0,    3,    0x01,
                                            0xe0, 'M', 'T', 'r', 'k', 0, 0, 0, 4, 0x00, 0xff, 0x2f, 0x00};
    SEQLIB_CHECK(out == kExpected);
}

SEQLIB_TEST(midi_writer, EncoderClampsTimeBeforeTheLastEvent)
{
    midi::TrackEncoder track;

    track.Event(100, {0x90, 60, 100});
    track.Event(40, {0x80, 60, 64});
    track.End(0);

    const std::vector<uint8_t> kExpected = {0x64, 0x90, 60, 100, 0x00, 0x80, 60, 64, 0x00, 0xff, 0x2f, 0x00};
    SEQLIB_CHECK(track.Data() == kExpected);
}

SEQLIB_TEST(midi_writer, EncoderSplitsLongDeltasWithEmptyTextEvents)
{
    midi::TrackEncoder track;

    track.End(0x0fffffffull + 5);

    const std::vector<uint8_t> kExpected = {0xff, 0xff, 0xff, 0x7f, 0xff, 0x01, 0x00, 0x05, 0xff, 0x2f, 0x00};
    SEQLIB_CHECK(track.Data() == kExpected);
}

SEQLIB_TEST(midi_writer, EncoderWritesStoredEventsAndMetaEvents)
{
    midi::TrackEncoder track;
    const std::vector<uint8_t> kTempo = {0xff, 0x51, 0x03, 0x07, 0xa1, 0x20};

    track.Meta(0, 0x03, "Lead");
    track.Event(10, kTempo);
    track.Tempo(20, 120.0);

    const std::vector<uint8_t> kExpected = {0x00, 0xff, 0x03, 0x04, 'L',  'e',  'a',  'd',  0x0a, 0xff, 0x51,
                                            0x03, 0x07, 0xa1, 0x20, 0x0a, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20};
    SEQLIB_CHECK(track.Data() == kExpected);
}

SEQLIB_TEST(midi_writer, MetaAndTempoEventBytes)
{
    const std::vector<uint8_t> meta = midi::MetaEvent(0x03, "Lead");
    const std::vector<uint8_t> tempo = midi::TempoEvent(500000);
    const std::vector<uint8_t> wide = midi::TempoEvent(0x01234567);

    SEQLIB_CHECK(meta == std::vector<uint8_t>({0xff, 0x03, 0x04, 'L', 'e', 'a', 'd'}));
    SEQLIB_CHECK(tempo == std::vector<uint8_t>({0xff, 0x51, 0x03, 0x07, 0xa1, 0x20}));
    SEQLIB_CHECK(wide == std::vector<uint8_t>({0xff, 0x51, 0x03, 0x23, 0x45, 0x67}));
}

SEQLIB_TEST(midi_writer, ControlKindsSortAfterTheNamedKinds)
{
    SEQLIB_CHECK(midi::ControlKind(102) > EventKind::kSoundOff);
    SEQLIB_CHECK(midi::ControlKind(102) < midi::ControlKind(103));
    SEQLIB_CHECK(midi::IsControlKind(midi::ControlKind(0)));
    SEQLIB_CHECK(!midi::IsControlKind(EventKind::kSoundOff));
    SEQLIB_CHECK(midi::ControllerOf(midi::ControlKind(104)) == 104);
}

// Writes each case with `timing` and compares the file and the report with the ones in `folder`. A case with no file
// there is skipped.
void CheckCasesAgainstFiles(midi::FrameTiming timing, const std::string& folder)
{
    int compared = 0;
    for (const MidiCase& c : MidiWriterCases())
    {
        const std::vector<uint8_t> expected = ReadHexTestData("midi/" + folder + "/" + c.name + ".hex");
        if (expected.empty())
        {
            continue;
        }

        const std::vector<uint8_t> expected_report = ReadTestData("midi/" + folder + "/" + c.name + ".txt");
        midi::MidiReport report;
        const midi::MidiOptions options{timing};

        const std::vector<uint8_t> written = midi::WriteMidi(c.perf, "Test " + c.name, report, options);

        SetContext("case " + c.name + " in " + folder);
        SEQLIB_CHECK(written == expected);
        const std::string text = ReportText(report);
        SEQLIB_CHECK(std::vector<uint8_t>(text.begin(), text.end()) == expected_report);
        compared++;
    }

    SetContext("");

    SEQLIB_CHECK(compared > 0);
}

SEQLIB_TEST(midi_writer, WriterMatchesFilesWithFramesNotAfterTicks)
{
    CheckCasesAgainstFiles(midi::FrameTiming::kNotAfterTick, "not_after_tick");
}

SEQLIB_TEST(midi_writer, WriterMatchesFilesWithFramesAtTheirStart)
{
    CheckCasesAgainstFiles(midi::FrameTiming::kFrameStart, "frame_start");
}

SEQLIB_TEST(midi_writer, NoteCountCountsNoteOns)
{
    const midi::Performance perf = LayersCase().perf;

    const uint32_t notes = midi::NoteCount(perf);

    SEQLIB_CHECK(notes == 17);
}

// Returns a performance of `tracks` tracks. Each plays one note on its first part, after a preset with its index.
midi::Performance OneNotePerTrack(int tracks)
{
    midi::Performance perf;
    perf.timebase = 96;
    perf.tempo.push_back({0, 120.0});
    for (int t = 0; t < tracks; t++)
    {
        const uint8_t key = static_cast<uint8_t>(40 + t);
        const uint32_t note = static_cast<uint32_t>(t + 1);
        std::vector<midi::TrackEvent>& events = perf.tracks[static_cast<std::size_t>(t)];
        events.push_back({.tick = 0, .kind = EventKind::kPreset, .preset = static_cast<uint32_t>(t)});
        events.push_back({.tick = 0, .kind = EventKind::kNoteOn, .key = key, .value = 100, .note = note});
        events.push_back({.tick = 96, .kind = EventKind::kNoteOff, .key = key, .note = note});
    }
    perf.end_tick = 192;

    return perf;
}

SEQLIB_TEST(midi_writer, PartWithoutNotesSendsNothing)
{
    // Track 0's second part plays no note. On the first part's channel, the second part's settings would change the
    // note that sounds there, and its sound off would cut that note.
    midi::Performance perf = OneNotePerTrack(1);
    midi::MidiReport expected_report;
    const std::vector<uint8_t> expected = midi::WriteMidi(perf, "Test", expected_report);

    const std::vector<midi::TrackEvent> kSecondPart = {
        {.tick = 48, .kind = EventKind::kVolume, .value = 40, .part = 1},
        {.tick = 48, .kind = EventKind::kPitch, .semitones = 2.0f, .part = 1},
        {.tick = 48, .kind = EventKind::kPreset, .preset = 9, .part = 1},
        {.tick = 60, .kind = EventKind::kSoundOff, .part = 1},
    };
    perf.tracks[0].insert(perf.tracks[0].begin() + 2, kSecondPart.begin(), kSecondPart.end());
    midi::MidiReport report;

    const std::vector<uint8_t> written = midi::WriteMidi(perf, "Test", report);

    SEQLIB_CHECK(written == expected);
}

SEQLIB_TEST(midi_writer, PartWithoutAChannelPlaysOnTheFirstPart)
{
    // Sixteen tracks take every channel, and the last track gets the drum channel. Track 0's second part plays a note
    // but gets no channel. The note and the part's volume go to track 0's channel. Track 15's second part plays no
    // note. That part's preset never reaches the drum channel.
    midi::Performance perf = OneNotePerTrack(16);
    const std::vector<midi::TrackEvent> kSecondPart = {
        {.tick = 48, .kind = EventKind::kVolume, .value = 40, .part = 1},
        {.tick = 48, .kind = EventKind::kNoteOn, .key = 70, .value = 90, .note = 100, .part = 1},
        {.tick = 72, .kind = EventKind::kNoteOff, .key = 70, .note = 100, .part = 1},
    };
    perf.tracks[0].insert(perf.tracks[0].begin() + 2, kSecondPart.begin(), kSecondPart.end());
    perf.tracks[15].insert(perf.tracks[15].begin() + 2,
                           {.tick = 48, .kind = EventKind::kPreset, .preset = 77, .part = 1});
    midi::MidiReport report;

    const midi::File file = midi::Read(midi::WriteMidi(perf, "Test", report));

    const uint64_t scale = file.division / perf.timebase;
    const std::vector<midi::Event> controllers = file.Find(0xb0);
    const midi::Event volume = {48 * scale, {0xb0, 7, 40}};
    SEQLIB_CHECK(std::find(controllers.begin(), controllers.end(), volume) != controllers.end());

    auto second_part = [&](const midi::Note& n)
    {
        return n.channel == 0 && n.key == 70 && n.velocity == 90 && n.on == 48 * scale && n.off == 72 * scale;
    };
    SEQLIB_CHECK(std::ranges::any_of(midi::Notes(file), second_part));

    SEQLIB_CHECK(report.channels == 16);
    SEQLIB_CHECK(report.channel_10_presets == std::set<uint32_t>{15});
}

SEQLIB_TEST(midi_writer, TempoOnTheLoopsFirstTickFollowsItsMarkers)
{
    // A player that loops goes back to the event after the loopStart marker, or after CC111. A tempo change on the
    // loop's first tick comes after both. Otherwise the player would skip it on the next pass.
    for (const uint32_t start : {0u, 96u})
    {
        SetContext("a loop from tick " + std::to_string(start));
        midi::Performance perf = OneNotePerTrack(1);
        perf.tempo = {{0, 120.0}, {96, 140.0}, {150, 90.0}};
        perf.loop = midi::Performance::Loop{start, 192};
        midi::MidiReport report;

        const midi::File file = midi::Read(midi::WriteMidi(perf, "Test", report));

        const uint64_t scale = file.division / perf.timebase;
        std::vector<std::string> order;
        for (const midi::Event& e : file.tracks[0].events)
        {
            if (e.tick != start * scale || e.bytes.size() < 2)
            {
                continue;
            }

            if (e.bytes[0] == 0xff && e.bytes[1] == 0x06)
            {
                order.push_back("marker");
            }
            else if (e.bytes[0] == 0xff && e.bytes[1] == 0x51)
            {
                order.push_back("tempo");
            }
            else if ((e.bytes[0] & 0xf0) == 0xb0 && e.bytes[1] == 111)
            {
                order.push_back("CC111");
            }
        }

        const std::vector<std::string> kExpected = {"marker", "CC111", "tempo"};
        SEQLIB_CHECK(order == kExpected);
    }
}

SEQLIB_TEST(midi_writer, EventsBeforeAMarkerStayBeforeIt)
{
    // Two notes end on the tick before a loop marker, in a frame that starts on the marker's MIDI tick, or just before
    // it and rounds there. A player that loops at the marker could skip a note-off on the marker's tick, and the note
    // would hang. Each note-off goes one MIDI tick before the marker, in either timing. The notes that start on the
    // markers' ticks stay there.
    for (const midi::FrameTiming timing : {midi::FrameTiming::kNotAfterTick, midi::FrameTiming::kFrameStart})
    {
        SetContext(timing == midi::FrameTiming::kNotAfterTick ? "frames not after their ticks"
                                                              : "frames at their start");
        midi::Performance perf;
        perf.timebase = 48;
        perf.tempo.push_back({0, 120.0});
        perf.tracks[0] = {
            {.tick = 0, .kind = EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
            {.tick = 23, .kind = EventKind::kNoteOff, .key = 60, .note = 1, .after = 1.0},
            {.tick = 24, .kind = EventKind::kNoteOn, .key = 62, .value = 100, .note = 2},
            {.tick = 71, .kind = EventKind::kNoteOff, .key = 62, .note = 2, .after = 0.999},
            {.tick = 72, .kind = EventKind::kNoteOn, .key = 64, .value = 100, .note = 3},
            {.tick = 84, .kind = EventKind::kNoteOff, .key = 64, .note = 3},
        };
        perf.end_tick = 96;
        perf.loop = midi::Performance::Loop{24, 72};
        midi::MidiReport report;

        const midi::File file = midi::Read(midi::WriteMidi(perf, "Test", report, {timing}));

        const uint64_t scale = file.division / perf.timebase;
        const std::vector<midi::Note> notes = midi::Notes(file);
        SEQLIB_REQUIRE_EQ(notes.size(), 3);
        SEQLIB_CHECK_EQ(notes[0].off, 24 * scale - 1);
        SEQLIB_CHECK_EQ(notes[1].on, 24 * scale);
        SEQLIB_CHECK_EQ(notes[1].off, 72 * scale - 1);
        SEQLIB_CHECK_EQ(notes[2].on, 72 * scale);
    }
}

} // namespace
} // namespace seqlib::test
