// SPDX-License-Identifier: MIT

// Helpers and data that more than one test file uses: the exact text of a double, a ramp of samples, a one-zone preset,
// a search of a list of problems, and the text of the SFZ curve from CC10 to pan.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "seqlib/sf2/soundfont.h"

namespace seqlib::test
{

// Returns `value` in the hexadecimal floating-point notation of printf's %a. The text keeps every bit of a finite
// value.
inline std::string HexFloat(double value)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%a", value);

    return text;
}

// Returns `n` samples that start at `offset` and rise by `step` each.
inline std::vector<int16_t> Ramp(int n, int step, int offset)
{
    std::vector<int16_t> samples;
    for (int i = 0; i < n; i++)
    {
        samples.push_back(static_cast<int16_t>(offset + i * step));
    }

    return samples;
}

// A preset with one zone. The zone names the instrument.
inline sf2::Preset PresetOf(const std::string& name, uint16_t bank, uint16_t program, int instrument)
{
    sf2::Preset p;
    p.name = name;
    p.bank = bank;
    p.program = program;
    p.zones = {{{sf2::Generator::Value(sf2::kInstrument, instrument)}, {}}};

    return p;
}

// Returns true if a line of `problems` contains `text`.
inline bool HasProblem(const std::vector<std::string>& problems, const std::string& text)
{
    const auto contains = [&text](const std::string& problem)
    {
        return problem.find(text) != std::string::npos;
    };
    return std::any_of(problems.begin(), problems.end(), contains);
}

// The curve from CC10 to pan that sfz::Curve writes, and that sfz::ControlHeader ends with.
inline constexpr const char* kPanCurveText = R"(// CC10 to pan
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

} // namespace seqlib::test
