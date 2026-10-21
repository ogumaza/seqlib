// SPDX-License-Identifier: MIT

// Tests of the MIDI file builder. The golden bytes and values are fixed: a change to them is a change of seqlib's
// output.

#include "seqlib/midi/builder.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "test.h"
#include "test_data.h"

namespace
{

using seqlib::midi::FileBuilder;
using seqlib::midi::TrackBuilder;
using seqlib::test::FromHex;
namespace cc = seqlib::midi::cc;

// The bytes of OrderCase()'s file.
constexpr char kOrderGolden[] =
    "4d546864000000060001000200604d54726b0000004100ff0304536f6e6700ff510307a12000ff58040302180830ff5103061a80"
    "30ff06096c6f6f70537461727400ff5103061a8060ff06076c6f6f70456e6408ff2f004d54726b0000007300ff03044c65616400"
    "803b000090405a00b0000100b0200000c00500b0076400e0284600903c6400903e0130c0080090415000b0000200b0200000c007"
    "009043500cb90a7f00997f7f0ce0000001e07f7f17803c0001803e0000804000018041000080430001897f00868c3db00b4000ff"
    "2f00";

// The bytes of RetimeCase()'s file.
constexpr char kRetimeGolden[] = "4d546864000000060001000100f04d54726b0000000c0a913e640a813e001eff2f00";

// Events of every kind added out of order, several at one tick, with steps, values out of range, a tempo repeated after
// a loopStart marker and a delta of three bytes.
FileBuilder OrderCase()
{
    FileBuilder file(96);

    TrackBuilder& conductor = file.AddTrack();
    conductor.Name("Song");
    conductor.Meta(96, 0x06, "loopStart");
    conductor.Tempo(0, 500000);
    conductor.TimeSignature(0, 3, 2);
    conductor.Tempo(48, 400000);
    conductor.RepeatTempo(96);
    conductor.Meta(192, 0x06, "loopEnd");
    conductor.SetEnd(200);

    TrackBuilder& track = file.AddTrack();
    track.NoteOn(0, 0, 60, 100);
    track.Program(0, 0, 5);
    track.Bank(0, 0, 1);
    track.Control(0, 0, cc::kVolume, 100);
    track.PitchBend(0, 0, 9000);
    track.NoteOff(0, 0, 59);
    track.NoteOn(0, 0, 62, 0);
    track.NoteOn(0, 0, 64, 90, true);
    track.Name("Lead");
    track.Program(48, 0, 7, 1);
    track.NoteOn(48, 0, 65, 80);
    track.Program(48, 0, 8);
    track.NoteOn(48, 0, 67, 80, false, 1);
    track.Bank(48, 0, 130, 1);
    track.Control(60, 9, cc::kPan, 200);
    track.NoteOn(60, 9, 130, 300);
    track.PitchBend(72, 0, -5);
    track.PitchBend(73, 0, 20000);
    track.NoteOff(96, 0, 60);
    track.NoteOff(97, 0, 62);
    track.NoteOff(97, 0, 64);
    track.NoteOff(98, 0, 65);
    track.NoteOff(98, 0, 67);
    track.NoteOff(99, 9, 130);
    track.Control(100000, 0, cc::kExpression, 64);

    return file;
}

// Every tick halved, then the division halved. One note's on and off fall on the same tick. The note is left out.
FileBuilder RetimeCase()
{
    FileBuilder file(480);

    TrackBuilder& track = file.AddTrack();
    track.NoteOn(10, 1, 60, 100);
    track.NoteOff(11, 1, 60);
    track.NoteOn(20, 1, 62, 100);
    track.NoteOff(40, 1, 62);
    track.SetEnd(101);

    file.Retime([](uint32_t tick) { return tick / 2; });
    file.SetDivision(240);

    return file;
}

SEQLIB_TEST(builder, Bytes)
{
    SEQLIB_CHECK(OrderCase().Encode() == FromHex(kOrderGolden));
    SEQLIB_CHECK(RetimeCase().Encode() == FromHex(kRetimeGolden));
}

SEQLIB_TEST(builder, LevelsToControllers)
{
    struct Case
    {
        double left;
        double right;
        int cc10;
        int cc11;
    };
    const Case kCases[] = {
        {128, 128, 64, 127}, {128, 0, 0, 107}, {0, 128, 127, 107}, {100, 50, 37, 100}, {30, 90, 101, 92}, {0, 0, -1, 0},
    };

    for (const Case& c : kCases)
    {
        int cc10 = -1;
        int cc11 = -1;
        seqlib::midi::LevelsToControllers(c.left, c.right, cc10, cc11);
        SEQLIB_CHECK(cc10 == c.cc10 && cc11 == c.cc11);
    }
}

// A key pressure goes after the note-offs at its tick and before the note-ons. That includes a note-on that goes before
// program changes. Pressures at one tick keep the order they were added in. Values out of range are clamped.
SEQLIB_TEST(builder, KeyPressureSortsBeforeNoteOns)
{
    TrackBuilder track;
    track.NoteOn(0, 1, 60, 100);
    track.KeyPressure(0, 1, 60, 127);
    track.Program(0, 1, 5);
    track.NoteOff(0, 1, 62);
    track.NoteOn(0, 1, 64, 90, true);
    track.KeyPressure(5, 1, 60, 200);
    track.KeyPressure(5, 1, 61, -3);

    const std::vector<uint8_t> kExpected = {
        0x00, 0x81, 0x3E, 0x00, // note off
        0x00, 0xA1, 0x3C, 0x7F, // key pressure
        0x00, 0x91, 0x40, 0x5A, // note on before programs
        0x00, 0xC1, 0x05,       // program change
        0x00, 0x91, 0x3C, 0x64, // note on
        0x05, 0xA1, 0x3C, 0x7F, // key pressure, clamped to 127
        0x00, 0xA1, 0x3D, 0x00, // key pressure, clamped to 0
        0x00, 0xFF, 0x2F, 0x00, // end of track
    };
    SEQLIB_CHECK(track.Encode() == kExpected);
}

// A system exclusive message goes after the meta events at its tick and before the note-offs and key pressures. Its
// event holds F0, the number of bytes after F0 as a variable-length quantity, then those bytes. The Standard MIDI File
// specification stores the message F0 43 12 00 07 F7 as F0 05 43 12 00 07 F7. A message of 130 bytes has 129 bytes
// after its F0. That length takes two bytes.
SEQLIB_TEST(builder, SysExSortsAfterMetaEvents)
{
    std::vector<uint8_t> long_message(130, 0x55);
    long_message.front() = 0xF0;
    long_message.back() = 0xF7;
    TrackBuilder track;
    track.KeyPressure(0, 1, 60, 127);
    track.NoteOff(0, 1, 62);
    track.SysEx(0, {0xF0, 0x43, 0x12, 0x00, 0x07, 0xF7});
    track.Meta(0, 0x06, "A");
    track.NoteOn(0, 1, 60, 100);
    track.SysEx(0, {0xF0, 0xF7});
    track.SysEx(1, long_message);

    const std::vector<uint8_t> actual = track.Encode();

    std::vector<uint8_t> expected = {
        0x00, 0xFF, 0x06, 0x01, 0x41,                   // marker "A"
        0x00, 0xF0, 0x05, 0x43, 0x12, 0x00, 0x07, 0xF7, // the specification's example
        0x00, 0xF0, 0x01, 0xF7,                         // a message without data bytes
        0x00, 0x81, 0x3E, 0x00,                         // note off
        0x00, 0xA1, 0x3C, 0x7F,                         // key pressure
        0x00, 0x91, 0x3C, 0x64,                         // note on
        0x01, 0xF0, 0x81, 0x01,                         // the long message's F0 and length
    };
    expected.insert(expected.end(), 128, 0x55);
    expected.insert(expected.end(), {0xF7, 0x00, 0xFF, 0x2F, 0x00}); // its F7, then the end of the track
    SEQLIB_CHECK(actual == expected);
}

// SysEx refuses a message that doesn't start with F0 and end with F7, or that has a byte above 127 between them. The
// track gets no event from it.
SEQLIB_TEST(builder, SysExRefusesMalformedMessages)
{
    const std::vector<std::vector<uint8_t>> kRefused = {
        {},
        {0xF0},
        {0xF7},
        {0x43, 0xF7},
        {0xF0, 0x43},
        {0xF7, 0x43, 0xF0},
        {0xF0, 0x80, 0xF7},
        {0xF0, 0xF7, 0x43, 0xF7},
    };
    TrackBuilder track;

    for (std::size_t i = 0; i < kRefused.size(); i++)
    {
        seqlib::test::SetContext("message " + std::to_string(i));
        const std::string error = seqlib::testing::ErrorOf<std::invalid_argument>([&] { track.SysEx(0, kRefused[i]); });
        SEQLIB_CHECK(!error.empty());
    }
    seqlib::test::SetContext("");

    SEQLIB_CHECK(track.Encode() == std::vector<uint8_t>({0x00, 0xFF, 0x2F, 0x00}));
}

// BendRange writes the events of six Control() calls. They keep their place among the tick's other events.
SEQLIB_TEST(builder, BendRangeMatchesItsControllers)
{
    TrackBuilder by_hand;
    by_hand.NoteOn(0, 2, 60, 100);
    by_hand.Control(0, 2, cc::kRpnMsb, 0);
    by_hand.Control(0, 2, cc::kRpnLsb, 0);
    by_hand.Control(0, 2, cc::kDataEntry, 12);
    by_hand.Control(0, 2, cc::kDataEntryLsb, 0);
    by_hand.Control(0, 2, cc::kRpnMsb, 127);
    by_hand.Control(0, 2, cc::kRpnLsb, 127);
    by_hand.PitchBend(0, 2, 8192);
    TrackBuilder with_range;
    with_range.NoteOn(0, 2, 60, 100);

    with_range.BendRange(0, 2, 12);
    with_range.PitchBend(0, 2, 8192);

    // Each event takes four bytes: the note on, the six controllers, the bend and the end of the track.
    SEQLIB_CHECK(with_range.Encode() == by_hand.Encode());
    SEQLIB_CHECK(by_hand.Encode().size() == 9 * 4);
}

} // namespace
