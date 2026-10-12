// SPDX-License-Identifier: MIT

// Conversions to SoundFont 2 units.

#pragma once

namespace seqlib::sf2
{

// Converts `ms` milliseconds to timecents, rounded to the nearest and held to `lo`..`hi`. A time of 1 ms or less gives
// `lo`.
int ToTimecents(double ms, int lo, int hi);

// Converts `ms` milliseconds to timecents, rounded to the nearest, with no limits. `ms` must be positive.
int ToTimecents(double ms);

// Converts pan `p` under a square-root pan law to the pan that gives the same left/right ratio under SoundFont's
// sin/cos pan law. Both run from -1 (left) to 1 (right). A pan outside that range is held to it.
//
// The square-root law is left = sqrt((1 - p) / 2), right = sqrt((1 + p) / 2). FluidSynth and the SoundFont 2 reference
// implementation use a sin/cos law: right = sin((p' + 1) * pi / 4). Both preserve power. Matching their left/right
// ratios therefore also matches their gains:
//
// p' = 4 / pi * atan(sqrt((1 + p) / (1 - p))) - 1
double SoundFontPan(double p);

} // namespace seqlib::sf2
