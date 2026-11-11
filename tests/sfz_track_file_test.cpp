// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sfz/track_file.h. Every line that TrackFiles writes is output. The hook's text is made up
// here. The rest of the expected text is fixed: a change to it is a change of seqlib's output.

#include <cstdint>
#include <string>
#include <vector>

#include "helpers.h"
#include "seqlib/sfz/track_file.h"
#include "test.h"

namespace
{

namespace midi = seqlib::midi;
namespace sfz = seqlib::sfz;

using seqlib::test::kPanCurveText;

// The <global> header's volume controllers.
const char* const kVolumes =
    "amplitude_oncc7=100 amplitude_curvecc7=4 amplitude_smoothcc7=0 amplitude_oncc11=100 "
    "amplitude_curvecc11=4 amplitude_smoothcc11=0\n";

midi::TrackEvent Preset(uint32_t tick, uint32_t preset)
{
    midi::TrackEvent e;
    e.tick = tick;
    e.kind = midi::EventKind::kPreset;
    e.preset = preset;

    return e;
}

// Track 0 plays presets 2 and 0, and track 1, named "Bass: low", plays preset 1. Track 2 plays preset 0 without a MIDI
// channel.
midi::Performance ThreeTracks()
{
    midi::Performance perf;
    perf.tracks[0] = {Preset(0, 2), Preset(96, 0), Preset(192, 2)};
    perf.tracks[1] = {Preset(0, 1)};
    perf.tracks[2] = {Preset(0, 0)};
    perf.track_names[1] = "Bass: low";

    return perf;
}

// Tracks 0 and 1 have MIDI channels, with bend ranges of 2 and 12 semitones.
midi::MidiReport TwoChannels()
{
    midi::MidiReport report;
    report.bend_ranges[0] = 2;
    report.bend_ranges[1] = 12;

    return report;
}

// What each track adds: a curve, an opcode in <global>, and a group with one region for each preset.
sfz::TrackText Additions(int track)
{
    sfz::TrackText text;
    text.curves = "// Track " + std::to_string(track) + "'s curve\n<curve> curve_index=8\nv000=0 v127=1\n\n";
    text.global = "fil_type=lpf_1p\n";
    text.preset = [](uint32_t preset)
    {
        return "<group>\n<region> sample=w" + std::to_string(preset) + ".wav\n";
    };

    return text;
}

sfz::TrackFileOptions Options()
{
    sfz::TrackFileOptions options;
    options.title = "Song\t\"A\"";
    options.program = "conv";
    options.preset_names = {"Strings", "Bass", "Lead\n"};
    options.presets = 3;

    return options;
}

SEQLIB_TEST(sfz_track_file, ControlHeaderEndsWithThePanCurve)
{
    const std::string expected =
        std::string("<control>\ndefault_path=../samples/\nhint_ram_based=1\n\n") + kPanCurveText;

    SEQLIB_CHECK_EQ(sfz::kPanCurve, 7);
    SEQLIB_CHECK(sfz::ControlHeader() == expected);
}

SEQLIB_TEST(sfz_track_file, GlobalHeaderGivesTheBendRangeInCents)
{
    const std::string expected = std::string(
                                     "<global>\nbend_up=1200 bend_down=-1200\n"
                                     "pan_oncc10=100 pan_curvecc10=7 pan_smoothcc10=0\n") +
                                 kVolumes;

    SEQLIB_CHECK(sfz::GlobalHeader(12) == expected);
}

SEQLIB_TEST(sfz_track_file, ThreeTracksOnTwoChannels)
{
    const std::vector<sfz::File> files = sfz::TrackFiles(ThreeTracks(), TwoChannels(), Options(), Additions);

    const std::string kTrack0 = std::string(
                                    "// Track 0 of Song \"A\", converted by conv. Load for MIDI track \"Track 0\" "
                                    "and its layers.\n\n<control>\ndefault_path=../samples/\nhint_ram_based=1\n\n") +
                                kPanCurveText +
                                "// Track 0's curve\n<curve> curve_index=8\nv000=0 v127=1\n\n"
                                "<global>\nbend_up=200 bend_down=-200\n"
                                "pan_oncc10=100 pan_curvecc10=7 pan_smoothcc10=0\n" +
                                kVolumes +
                                "fil_type=lpf_1p\n"
                                "\n// Strings\n<master> loprog=0 hiprog=0\n<group>\n<region> sample=w0.wav\n"
                                "\n// Lead \n<master> loprog=2 hiprog=2\n<group>\n<region> sample=w2.wav\n";
    SEQLIB_REQUIRE_EQ(files.size(), 2);
    SEQLIB_CHECK(files[0].name == "Track 0.sfz" && files[1].name == "Track 1.sfz");
    SEQLIB_CHECK(files[0].text == kTrack0);
}

SEQLIB_TEST(sfz_track_file, TrackNamesAndLevelCut)
{
    sfz::TrackFileOptions options = Options();
    options.track_names = true;
    options.level_cut_db = 1.5;

    const std::vector<sfz::File> files = sfz::TrackFiles(ThreeTracks(), TwoChannels(), options, Additions);

    const std::string kTrack1 = std::string(
                                    "// Bass: low of Song \"A\", converted by conv. Load for MIDI track \"Bass: "
                                    "low\" and its layers.\n// MIDI gain was reduced by 1.5 dB. Region volumes "
                                    "restore that gain.\n\n<control>\ndefault_path=../samples/\n"
                                    "hint_ram_based=1\n\n") +
                                kPanCurveText +
                                "// Track 1's curve\n<curve> curve_index=8\nv000=0 v127=1\n\n"
                                "<global>\nbend_up=1200 bend_down=-1200\n"
                                "pan_oncc10=100 pan_curvecc10=7 pan_smoothcc10=0\n" +
                                kVolumes +
                                "fil_type=lpf_1p\n"
                                "\n// Bass\n<master> loprog=1 hiprog=1\n<group>\n<region> sample=w1.wav\n";
    SEQLIB_REQUIRE_EQ(files.size(), 2);
    SEQLIB_CHECK(files[0].name == "Track 0.sfz" && files[1].name == "Bass_ low.sfz");
    SEQLIB_CHECK(files[1].text == kTrack1);
}

SEQLIB_TEST(sfz_track_file, BankSelectAbove128Presets)
{
    midi::Performance perf;
    perf.tracks[0] = {Preset(0, 130)};
    midi::MidiReport report;
    report.bend_ranges[0] = 2;
    sfz::TrackFileOptions options = Options();
    options.preset_names.resize(131, "name");
    sfz::TrackText text;
    text.preset = [](uint32_t)
    {
        return std::string();
    };

    options.presets = 131;
    const std::vector<sfz::File> above = sfz::TrackFiles(perf, report, options, [&](int) { return text; });
    options.presets = 128;
    const std::vector<sfz::File> at = sfz::TrackFiles(perf, report, options, [&](int) { return text; });

    const std::string kMaster = "\n// name\n<master> loprog=2 hiprog=2";
    SEQLIB_CHECK(above.size() == 1 && above[0].text.ends_with(kMaster + " locc0=1 hicc0=1\n"));
    SEQLIB_CHECK(at.size() == 1 && at[0].text.ends_with(kMaster + "\n"));
}

} // namespace
