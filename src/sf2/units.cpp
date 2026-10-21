// SPDX-License-Identifier: MIT

// Conversions to SoundFont 2 units (see units.h).

#include "seqlib/sf2/units.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace seqlib::sf2
{
namespace
{

// Synthesizers that apply 0.4 of the initialAttenuation generator play its value divided by this (see
// ToZoneAttenuation).
constexpr double kGeneratorScale = 2.5;

// The greatest value of the initialAttenuation generator, in centibels (SoundFont 2.04, section 8.1.3).
// ToZoneAttenuation writes no greater attenuation.
constexpr double kMaxCentibels = 1440.0;

} // namespace

int ToTimecents(double ms, int lo, int hi)
{
    if (ms <= 1.0)
    {
        return lo;
    }

    return std::clamp(ToTimecents(ms), lo, hi);
}

int ToTimecents(double ms)
{
    return static_cast<int>(std::lround(1200.0 * std::log2(ms / 1000.0)));
}

int LfoFrequency(double hz)
{
    return std::clamp(static_cast<int>(std::lround(1200.0 * std::log2(hz / 8.176))), -16000, 4500);
}

double SoundFontPan(double p)
{
    p = std::clamp(p, -1.0, 1.0);
    if (p >= 1.0)
    {
        return 1.0;
    }

    return 4.0 / std::numbers::pi * std::atan(std::sqrt((1.0 + p) / (1.0 - p))) - 1.0;
}

ZoneAttenuation ToZoneAttenuation(double centibels)
{
    // NaN, and an attenuation of 0 or less, give none.
    if (!(centibels > 0.0))
    {
        return {};
    }

    const double held = std::min(centibels, kMaxCentibels);
    ZoneAttenuation result;
    result.generator = static_cast<int>(std::lround(std::min(held * kGeneratorScale, kMaxCentibels)));

    // The modulator holds what the generator leaves. Up to 576 cB, that's at most 0.2 cB. It rounds to none.
    const auto rest = static_cast<int16_t>(std::lround(held - result.generator / kGeneratorScale));
    if (rest > 0)
    {
        result.modulator = Modulator{kNoController, kInitialAttenuation, rest, kNoController, kLinear};
    }

    return result;
}

} // namespace seqlib::sf2
