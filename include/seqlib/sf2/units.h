// SPDX-License-Identifier: MIT

// Conversions to SoundFont 2 units.

#pragma once

#include <optional>

#include "seqlib/sf2/soundfont.h"

namespace seqlib::sf2
{

// Converts `ms` milliseconds to timecents, rounded to the nearest and held to `lo`..`hi`. A time of 1 ms or less gives
// `lo`.
int ToTimecents(double ms, int lo, int hi);

// Converts `ms` milliseconds to timecents, rounded to the nearest, with no limits. `ms` must be positive.
int ToTimecents(double ms);

// Converts an LFO's frequency of `hz` hertz to the absolute cents of freqModLFO and freqVibLFO, 1200 log2(hz / 8.176),
// rounded to the nearest and held to the generators' -16000..4500 (SoundFont 2.01, sections 8.1.2 and 8.1.3). `hz` must
// be positive and finite.
int LfoFrequency(double hz);

// Converts pan `p` under a square-root pan law to the pan that gives the same left/right ratio under SoundFont's
// sin/cos pan law. Both run from -1 (left) to 1 (right). A pan outside that range is held to it.
//
// The square-root law is left = sqrt((1 - p) / 2), right = sqrt((1 + p) / 2). FluidSynth and the SoundFont 2 reference
// implementation use a sin/cos law: right = sin((p' + 1) * pi / 4). Both preserve power. Matching their left/right
// ratios therefore also matches their gains:
//
// p' = 4 / pi * atan(sqrt((1 + p) / (1 - p))) - 1
double SoundFontPan(double p);

// An attenuation in the form that a zone holds it (ToZoneAttenuation).
struct ZoneAttenuation
{
    int generator = 0;                  // initialAttenuation's value in centibels, 0 to 1440. A zone leaves 0 out
    std::optional<Modulator> modulator; // the attenuation that the generator can't hold, if there is any
};

// Converts an attenuation of `centibels` into the value of the initialAttenuation generator and a modulator for the
// rest. The SoundFont 2.04 specification attenuates a note by the generator's value (section 8.1.2, generator 48). The
// generator's range is 0 to 1440 cB (section 8.1.3). The E-mu chips of Creative's Sound Blaster cards, FluidSynth,
// BASSMIDI and most other SoundFont synthesizers apply 0.4 of the value. The generator therefore holds 2.5 times the
// attenuation, rounded to the nearest centibel. Those synthesizers then play the attenuation to within 0.2 cB. Rounding
// the attenuation before the product would add up to 0.5 cB more.
//
// Those synthesizers play the generator's limit of 1440 cB as 576 cB. A modulator holds any attenuation beyond that,
// rounded to the nearest centibel. Its source and its amount source are No Controller. Section 8.2.1 counts No
// Controller as 1. The modulator's transform is linear. Its amount adds to the generator's value (sections 7.8 and
// 9.5.1). From version 2.4.5, FluidSynth applies all of the modulator's amount. BASSMIDI and earlier versions of
// FluidSynth ignore the modulator. They play the zone at 576 cB.
//
// A synthesizer that follows the specification plays each attenuated zone more quietly than these synthesizers do: at
// 2.5 times the attenuation, and at 144 dB or more from 576 cB up. `centibels` is held to 0 to 1440. NaN gives no
// attenuation.
ZoneAttenuation ToZoneAttenuation(double centibels);

} // namespace seqlib::sf2
