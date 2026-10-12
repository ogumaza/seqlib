// SPDX-License-Identifier: MIT

#include "seqlib/sf2/units.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace seqlib::sf2
{

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

double SoundFontPan(double p)
{
    p = std::clamp(p, -1.0, 1.0);
    if (p >= 1.0)
    {
        return 1.0;
    }

    return 4.0 / std::numbers::pi * std::atan(std::sqrt((1.0 + p) / (1.0 - p))) - 1.0;
}

} // namespace seqlib::sf2
