// SPDX-License-Identifier: MIT

#include "seqlib/sfz/track_file.h"

#include <set>

#include "seqlib/file_name.h"
#include "seqlib/sfz/text.h"

namespace seqlib::sfz
{
namespace
{

// Custom curve IDs; sfizz reserves 0-6. The hook's curves come after this one.
constexpr int kPanCurve = 7; // CC10 to pan

// sfizz divides pitch wheel values by 8191; the MIDI writer uses 8192 steps per full range. Scale the SFZ bend range by
// 8191/8192.
constexpr double kBendScale = 8191.0 / 8192.0;

// The name of track `t`: the MIDI track's name with `track_names`, where it has one, and otherwise "Track <t>".
std::string TrackName(const midi::Performance& perf, int t, bool track_names)
{
    const std::string& name = perf.track_names[static_cast<std::size_t>(t)];
    return track_names && !name.empty() ? name : "Track " + std::to_string(t);
}

// Writes track `t`'s SFZ file.
std::string TrackFile(const midi::Performance& perf, int t, int bend_range, const TrackFileOptions& options,
                      const TrackText& text)
{
    std::set<uint32_t> presets;
    for (const midi::TrackEvent& e : perf.tracks[static_cast<std::size_t>(t)])
    {
        if (e.kind == midi::EventKind::kPreset)
        {
            presets.insert(e.preset);
        }
    }

    const std::string name = CommentText(TrackName(perf, t, options.track_names));
    std::string s = "// " + name + " of " + CommentText(options.title) + ", converted by " + options.program +
                    ". Load for MIDI track \"" + name + "\" and its layers.\n";
    if (options.level_cut_db > 0.0)
    {
        s += "// MIDI gain was reduced by " + Number(options.level_cut_db, 2) +
             " dB. Region volumes restore that gain.\n";
    }
    s += "\n";
    s += "<control>\ndefault_path=../" + kSamplesFolder + "/\nhint_ram_based=1\n\n";

    // Map CC10 to (value - 64) / 64. That's the SoundFont pan convention that the MIDI writer uses.
    s += Curve(kPanCurve, "CC10 to pan", [](int v) { return (v - 64) / 64.0; });
    s += text.curves;

    const std::string bend = Number(bend_range * 100.0 * kBendScale, 6);
    s += "<global>\nbend_up=" + bend + " bend_down=-" + bend + "\n";

    // CC7, CC10 and CC11 take effect at once. sfizz would otherwise smooth them over 10 ms, from CC7 100 at first. A
    // note whose track sets its volume or pan in the tick the note starts would then begin at the old value. Curve 4
    // squares CC7 and CC11: General MIDI's volume curve, 40 log10(value / 127) dB.
    s += "pan_oncc10=100 pan_curvecc10=" + std::to_string(kPanCurve) + " pan_smoothcc10=0\n";
    s += "amplitude_oncc7=100 amplitude_curvecc7=4 amplitude_smoothcc7=0 amplitude_oncc11=100 amplitude_curvecc11=4 "
         "amplitude_smoothcc11=0\n";
    s += text.global;

    const bool banks_selected = options.presets > 128;
    for (uint32_t p : presets)
    {
        s += "\n// " + CommentText(options.preset_names[p]) + "\n<master> loprog=" + std::to_string(p % 128) +
             " hiprog=" + std::to_string(p % 128);
        if (banks_selected)
        {
            s += " locc0=" + std::to_string(p / 128) + " hicc0=" + std::to_string(p / 128);
        }
        s += "\n" + text.preset(p);
    }

    return s;
}

} // namespace

std::vector<File> TrackFiles(const midi::Performance& perf, const midi::MidiReport& midi,
                             const TrackFileOptions& options, const TrackTextFunction& text)
{
    std::vector<File> files;
    for (int t = 0; t < 16; t++)
    {
        const int bend_range = midi.bend_ranges[static_cast<std::size_t>(t)];
        if (bend_range == 0)
        {
            continue; // no MIDI channel assigned to this track
        }

        const std::string name = TrackName(perf, t, options.track_names);
        files.push_back({(options.track_names ? SafeFileName(name) : name) + ".sfz",
                         TrackFile(perf, t, bend_range, options, text(t))});
    }

    return files;
}

} // namespace seqlib::sfz
