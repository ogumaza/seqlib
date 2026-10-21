// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sfz/track_file.h. Every line that TrackFiles writes is output. The hook's text is made up
// here. The rest of the expected text is fixed: a change to it is a change of seqlib's output.

#include <cstdint>
#include <string>
#include <vector>

#include "seqlib/sfz/track_file.h"
#include "test.h"

namespace
{

namespace midi = seqlib::midi;
namespace sfz = seqlib::sfz;

// The curve from CC10 to pan. sfz_text_test.cpp has the same.
const char* const kPanCurve = R"(// CC10 to pan
<curve> curve_index=7
v000=-1 v001=-0.984375 v002=-0.96875 v003=-0.953125 v004=-0.9375 v005=-0.921875 v006=-0.90625 v007=-0.890625
v008=-0.875 v009=-0.859375 v010=-0.84375 v011=-0.828125 v012=-0.8125 v013=-0.796875 v014=-0.78125 v015=-0.765625
v016=-0.75 v017=-0.734375 v018=-0.71875 v019=-0.703125 v020=-0.6875 v021=-0.671875 v022=-0.65625 v023=-0.640625
v024=-0.625 v025=-0.609375 v026=-0.59375 v027=-0.578125 v028=-0.5625 v029=-0.546875 v030=-0.53125 v031=-0.515625
v032=-0.5 v033=-0.484375 v034=-0.46875 v035=-0.453125 v036=-0.4375 v037=-0.421875 v038=-0.40625 v039=-0.390625
v040=-0.375 v041=-0.359375 v042=-0.34375 v043=-0.328125 v044=-0.3125 v045=-0.296875 v046=-0.28125 v047=-0.265625
v048=-0.25 v049=-0.234375 v050=-0.21875 v051=-0.203125 v052=-0.1875 v053=-0.171875 v054=-0.15625 v055=-0.140625
v056=-0.125 v057=-0.109375 v058=-0.09375 v059=-0.078125 v060=-0.0625 v061=-0.046875 v062=-0.03125 v063=-0.015625
v064=0 v065=0.015625 v066=0.03125 v067=0.046875 v068=0.0625 v069=0.078125 v070=0.09375 v071=0.109375
v072=0.125 v073=0.140625 v074=0.15625 v075=0.171875 v076=0.1875 v077=0.203125 v078=0.21875 v079=0.234375
v080=0.25 v081=0.265625 v082=0.28125 v083=0.296875 v084=0.3125 v085=0.328125 v086=0.34375 v087=0.359375
v088=0.375 v089=0.390625 v090=0.40625 v091=0.421875 v092=0.4375 v093=0.453125 v094=0.46875 v095=0.484375
v096=0.5 v097=0.515625 v098=0.53125 v099=0.546875 v100=0.5625 v101=0.578125 v102=0.59375 v103=0.609375
v104=0.625 v105=0.640625 v106=0.65625 v107=0.671875 v108=0.6875 v109=0.703125 v110=0.71875 v111=0.734375
v112=0.75 v113=0.765625 v114=0.78125 v115=0.796875 v116=0.8125 v117=0.828125 v118=0.84375 v119=0.859375
v120=0.875 v121=0.890625 v122=0.90625 v123=0.921875 v124=0.9375 v125=0.953125 v126=0.96875 v127=0.984375

)";

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
    midi::MidiReport midi;
    midi.bend_ranges[0] = 2;
    midi.bend_ranges[1] = 12;

    return midi;
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

SEQLIB_TEST(sfz_track_file, ThreeTracksOnTwoChannels)
{
    const std::vector<sfz::File> files = sfz::TrackFiles(ThreeTracks(), TwoChannels(), Options(), Additions);

    const std::string kTrack0 = std::string(
                                    "// Track 0 of Song \"A\", converted by conv. Load for MIDI track \"Track 0\" "
                                    "and its layers.\n\n<control>\ndefault_path=../samples/\nhint_ram_based=1\n\n") +
                                kPanCurve +
                                "// Track 0's curve\n<curve> curve_index=8\nv000=0 v127=1\n\n"
                                "<global>\nbend_up=200 bend_down=-200\n"
                                "pan_oncc10=100 pan_curvecc10=7 pan_smoothcc10=0\n" +
                                kVolumes +
                                "fil_type=lpf_1p\n"
                                "\n// Strings\n<master> loprog=0 hiprog=0\n<group>\n<region> sample=w0.wav\n"
                                "\n// Lead \n<master> loprog=2 hiprog=2\n<group>\n<region> sample=w2.wav\n";
    SEQLIB_CHECK(files.size() == 2);
    SEQLIB_CHECK(files.size() == 2 && files[0].name == "Track 0.sfz" && files[1].name == "Track 1.sfz");
    SEQLIB_CHECK(!files.empty() && files[0].text == kTrack0);
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
                                kPanCurve +
                                "// Track 1's curve\n<curve> curve_index=8\nv000=0 v127=1\n\n"
                                "<global>\nbend_up=1200 bend_down=-1200\n"
                                "pan_oncc10=100 pan_curvecc10=7 pan_smoothcc10=0\n" +
                                kVolumes +
                                "fil_type=lpf_1p\n"
                                "\n// Bass\n<master> loprog=1 hiprog=1\n<group>\n<region> sample=w1.wav\n";
    SEQLIB_CHECK(files.size() == 2 && files[0].name == "Track 0.sfz" && files[1].name == "Bass_ low.sfz");
    SEQLIB_CHECK(files.size() == 2 && files[1].text == kTrack1);
}

SEQLIB_TEST(sfz_track_file, BankSelectAbove128Presets)
{
    midi::Performance perf;
    perf.tracks[0] = {Preset(0, 130)};
    midi::MidiReport midi;
    midi.bend_ranges[0] = 2;
    sfz::TrackFileOptions options = Options();
    options.preset_names.resize(131, "name");
    sfz::TrackText text;
    text.preset = [](uint32_t)
    {
        return std::string();
    };

    options.presets = 131;
    const std::vector<sfz::File> above = sfz::TrackFiles(perf, midi, options, [&](int) { return text; });
    options.presets = 128;
    const std::vector<sfz::File> at = sfz::TrackFiles(perf, midi, options, [&](int) { return text; });

    const std::string kMaster = "\n// name\n<master> loprog=2 hiprog=2";
    SEQLIB_CHECK(above.size() == 1 && above[0].text.ends_with(kMaster + " locc0=1 hicc0=1\n"));
    SEQLIB_CHECK(at.size() == 1 && at[0].text.ends_with(kMaster + "\n"));
}

} // namespace
