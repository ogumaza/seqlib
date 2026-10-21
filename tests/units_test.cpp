// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sf2/units.h. The expected values are fixed: a change to them is a change of seqlib's output.
// They hold for code built with -ffp-contract=off.

#include "seqlib/sf2/units.h"

#include <cmath>
#include <limits>
#include <string>

#include "test.h"

namespace
{

struct TimecentsCase
{
    double ms;
    int lo, hi;
    int expected;
};

constexpr TimecentsCase kTimecents[] = {
    {-0x1p+0, -12000, 8000, -12000},
    {-0x1p+0, -12000, 5000, -12000},
    {-0x1p+0, -32768, 32767, -32768},
    {0x0p+0, -12000, 8000, -12000},
    {0x0p+0, -12000, 5000, -12000},
    {0x0p+0, -32768, 32767, -32768},
    {0x1p-1, -12000, 8000, -12000},
    {0x1p-1, -12000, 5000, -12000},
    {0x1p-1, -32768, 32767, -32768},
    {0x1p+0, -12000, 8000, -12000},
    {0x1p+0, -12000, 5000, -12000},
    {0x1p+0, -32768, 32767, -32768},
    {0x1.0000000000001p+0, -12000, 8000, -11959},
    {0x1.0000000000001p+0, -12000, 5000, -11959},
    {0x1.0000000000001p+0, -32768, 32767, -11959},
    {0x1.004189374bc6ap+0, -12000, 8000, -11957},
    {0x1.004189374bc6ap+0, -12000, 5000, -11957},
    {0x1.004189374bc6ap+0, -32768, 32767, -11957},
    {0x1.8p+0, -12000, 8000, -11257},
    {0x1.8p+0, -12000, 5000, -11257},
    {0x1.8p+0, -32768, 32767, -11257},
    {0x1p+1, -12000, 8000, -10759},
    {0x1p+1, -12000, 5000, -10759},
    {0x1p+1, -32768, 32767, -10759},
    {0x1.d99999999999ap+1, -12000, 8000, -9694},
    {0x1.d99999999999ap+1, -12000, 5000, -9694},
    {0x1.d99999999999ap+1, -32768, 32767, -9694},
    {0x1.4p+3, -12000, 8000, -7973},
    {0x1.4p+3, -12000, 5000, -7973},
    {0x1.4p+3, -32768, 32767, -7973},
    {0x1.7ap+5, -12000, 8000, -5284},
    {0x1.7ap+5, -12000, 5000, -5284},
    {0x1.7ap+5, -32768, 32767, -5284},
    {0x1.9p+6, -12000, 8000, -3986},
    {0x1.9p+6, -12000, 5000, -3986},
    {0x1.9p+6, -32768, 32767, -3986},
    {0x1.edd2f1a9fbe77p+6, -12000, 8000, -3622},
    {0x1.edd2f1a9fbe77p+6, -12000, 5000, -3622},
    {0x1.edd2f1a9fbe77p+6, -32768, 32767, -3622},
    {0x1.f3ffdf3b645a2p+9, -12000, 8000, 0},
    {0x1.f3ffdf3b645a2p+9, -12000, 5000, 0},
    {0x1.f3ffdf3b645a2p+9, -32768, 32767, 0},
    {0x1.f4p+9, -12000, 8000, 0},
    {0x1.f4p+9, -12000, 5000, 0},
    {0x1.f4p+9, -32768, 32767, 0},
    {0x1.f44p+9, -12000, 8000, 1},
    {0x1.f44p+9, -12000, 5000, 1},
    {0x1.f44p+9, -32768, 32767, 1},
    {0x1.f4p+10, -12000, 8000, 1200},
    {0x1.f4p+10, -12000, 5000, 1200},
    {0x1.f4p+10, -32768, 32767, 1200},
    {0x1.388p+12, -12000, 8000, 2786},
    {0x1.388p+12, -12000, 5000, 2786},
    {0x1.388p+12, -32768, 32767, 2786},
    {0x1.81cd6c8b43958p+13, -12000, 8000, 4351},
    {0x1.81cd6c8b43958p+13, -12000, 5000, 4351},
    {0x1.81cd6c8b43958p+13, -32768, 32767, 4351},
    {0x1.fffep+15, -12000, 8000, 7241},
    {0x1.fffep+15, -12000, 5000, 5000},
    {0x1.fffep+15, -32768, 32767, 7241},
    {0x1.8cd9p+16, -12000, 8000, 8000},
    {0x1.8cd9p+16, -12000, 5000, 5000},
    {0x1.8cd9p+16, -32768, 32767, 8000},
    {0x1.e848p+19, -12000, 8000, 8000},
    {0x1.e848p+19, -12000, 5000, 5000},
    {0x1.e848p+19, -32768, 32767, 11959},
    {0x1.7e43c8800759cp+996, -12000, 8000, 8000},
    {0x1.7e43c8800759cp+996, -12000, 5000, 5000},
    {0x1.7e43c8800759cp+996, -32768, 32767, 32767},
};

struct PanCase
{
    double pan;
    double expected;
};

constexpr PanCase kPans[] = {
    {-0x1p+1, -0x1p+0},
    {-0x1p+0, -0x1p+0},
    {-0x1.ffffde7210be9p-1, -0x1.ff89fe6516678p-1},
    {-0x1.8p-1, -0x1.146cdde5cc972p-1},
    {-0x1p-1, -0x1.5555555555556p-2},
    {-0x1p-2, -0x1.49719f07a5374p-3},
    {-0x1.56e1fc2f8f359p-997, 0x0p+0},
    {-0x0p+0, 0x0p+0},
    {0x0p+0, 0x0p+0},
    {0x1.56e1fc2f8f359p-997, 0x0p+0},
    {0x1.999999999999ap-4, 0x1.05322eb667bfp-4},
    {0x1p-2, 0x1.49719f07a5378p-3},
    {0x1p-1, 0x1.5555555555558p-2},
    {0x1.8p-1, 0x1.146cdde5cc972p-1},
    {0x1.ffffde7210be9p-1, 0x1.ff89fe6516678p-1},
    {0x1p+0, 0x1p+0},
    {0x1.8p+0, 0x1p+0},
};

// The values that a file gets from SoundFontPan((k - 64) / 63.0) for k = 0..127: SoundFont's pan generator (500 times
// the pan, rounded), SFZ's pan in tenths of a percent (1000 times) and a MIDI pan (64 + 64 times, held to 0..127).
struct RoundedPan
{
    int generator, tenths, midi;
};

constexpr RoundedPan kRoundedPans[128] = {
    {-500, -1000, 0}, {-500, -1000, 0}, {-443, -886, 7},  {-420, -839, 10}, {-401, -803, 13}, {-386, -772, 15},
    {-372, -745, 16}, {-360, -720, 18}, {-349, -697, 19}, {-338, -676, 21}, {-328, -656, 22}, {-318, -636, 23},
    {-309, -618, 24}, {-300, -601, 26}, {-292, -584, 27}, {-284, -567, 28}, {-276, -551, 29}, {-268, -536, 30},
    {-261, -521, 31}, {-253, -506, 32}, {-246, -492, 32}, {-239, -478, 33}, {-232, -465, 34}, {-226, -451, 35},
    {-219, -438, 36}, {-212, -425, 37}, {-206, -412, 38}, {-200, -400, 38}, {-194, -387, 39}, {-187, -375, 40},
    {-181, -363, 41}, {-175, -351, 42}, {-170, -339, 42}, {-164, -328, 43}, {-158, -316, 44}, {-152, -305, 45},
    {-147, -293, 45}, {-141, -282, 46}, {-135, -271, 47}, {-130, -260, 47}, {-124, -249, 48}, {-119, -238, 49},
    {-114, -227, 49}, {-108, -216, 50}, {-103, -206, 51}, {-98, -195, 52},  {-92, -184, 52},  {-87, -174, 53},
    {-82, -163, 54},  {-77, -153, 54},  {-71, -143, 55},  {-66, -132, 56},  {-61, -122, 56},  {-56, -112, 57},
    {-51, -101, 58},  {-46, -91, 58},   {-41, -81, 59},   {-35, -71, 59},   {-30, -61, 60},   {-25, -51, 61},
    {-20, -40, 61},   {-15, -30, 62},   {-10, -20, 63},   {-5, -10, 63},    {0, 0, 64},       {5, 10, 65},
    {10, 20, 65},     {15, 30, 66},     {20, 40, 67},     {25, 51, 67},     {30, 61, 68},     {35, 71, 69},
    {41, 81, 69},     {46, 91, 70},     {51, 101, 70},    {56, 112, 71},    {61, 122, 72},    {66, 132, 72},
    {71, 143, 73},    {77, 153, 74},    {82, 163, 74},    {87, 174, 75},    {92, 184, 76},    {98, 195, 76},
    {103, 206, 77},   {108, 216, 78},   {114, 227, 79},   {119, 238, 79},   {124, 249, 80},   {130, 260, 81},
    {135, 271, 81},   {141, 282, 82},   {147, 293, 83},   {152, 305, 83},   {158, 316, 84},   {164, 328, 85},
    {170, 339, 86},   {175, 351, 86},   {181, 363, 87},   {187, 375, 88},   {194, 387, 89},   {200, 400, 90},
    {206, 412, 90},   {212, 425, 91},   {219, 438, 92},   {226, 451, 93},   {232, 465, 94},   {239, 478, 95},
    {246, 492, 96},   {253, 506, 96},   {261, 521, 97},   {268, 536, 98},   {276, 551, 99},   {284, 567, 100},
    {292, 584, 101},  {300, 601, 102},  {309, 618, 104},  {318, 636, 105},  {328, 656, 106},  {338, 676, 107},
    {349, 697, 109},  {360, 720, 110},  {372, 745, 112},  {386, 772, 113},  {401, 803, 115},  {420, 839, 118},
    {443, 886, 121},  {500, 1000, 127},
};

// Times of whole frames of 280,896 cycles at 16,777,216 cycles a second. ToTimecents takes them with no limits.
struct UnlimitedTimecentsCase
{
    double ms;
    int expected;
};

constexpr UnlimitedTimecentsCase kUnlimitedTimecents[] = {
    {16.742706298828125, -7080}, {33.48541259765625, -5880}, {50.228118896484375, -5178}, {117.19894409179688, -3712},
    {502.28118896484375, -1192}, {1004.5623779296875, 8},    {4269.3901062011719, 2513},  {4286.1328125, 2520},
    {16742.706298828125, 4879},  {68561.382293701172, 7319}, {1097233.2572937012, 12120}, {16742706.298828125, 16837},
};

// LFO frequencies by SoundFont 2.01, sections 8.1.2 and 8.1.3: 8.176 Hz is 0 cents, an octave is 1200 cents, 10 mHz is
// -11610 cents, and the generators run from -16000 to 4500 cents.
struct LfoCase
{
    double hz;
    int expected;
};

constexpr LfoCase kLfoFrequencies[] = {
    {8.176, 0}, {16.352, 1200}, {4.088, -1200}, {0.01, -11610}, {1000.0, 4500}, {0.0001, -16000},
};

// Attenuations in the form that zones hold them. SoundFont 2.04 gives initialAttenuation, generator 48, a range of 0 to
// 1440 cB (section 8.1.3). The generator holds 2.5 times the attenuation, rounded to the nearest centibel, up to that
// limit. Synthesizers that apply 0.4 of the generator play its limit as 576 cB. Beyond that, a modulator holds the
// rest, rounded to the nearest centibel. Its source and amount source are 0, No Controller. Section 8.2.1 counts No
// Controller as 1. The modulator's destination is 48. Its transform is 0, linear. Its amount adds to the generator's
// value (sections 7.8 and 9.5.1).
struct ZoneAttenuationCase
{
    double centibels;
    int generator;
    int modulator; // the modulator's amount, or 0 for no modulator
};

constexpr ZoneAttenuationCase kZoneAttenuations[] = {
    {-std::numeric_limits<double>::infinity(), 0, 0},
    {-3.0, 0, 0},
    {0.0, 0, 0},
    {0.1, 0, 0},
    {0.4, 1, 0},
    {2.5, 6, 0},      // 6.25 rounds down
    {60.0, 150, 0},   // section 8.1.2's example, 6 dB below full scale
    {123.4, 309, 0},  // 308.5 rounds away from zero
    {200.0, 500, 0},  // 20 dB
    {575.9, 1440, 0}, // 1439.75 rounds up to the limit
    {576.0, 1440, 0}, // the limit, played as 576 cB
    {576.4, 1440, 0}, // 0.4 cB more rounds to no modulator
    {576.5, 1440, 1}, // 0.5 cB more rounds away from zero
    {700.0, 1440, 124},
    {1440.0, 1440, 864},
    {1500.0, 1440, 864}, // held to 1440 cB
    {std::numeric_limits<double>::infinity(), 1440, 864},
    {std::numeric_limits<double>::quiet_NaN(), 0, 0},
};

// Math libraries can round atan differently in its last bits. Near a pan of 0.5, SoundFontPan's subtraction of 1
// magnifies that to a few ulps of its result. The rounded values that reach a file still have to match exactly (see
// RoundedPans).
constexpr double kPanTolerance = 1e-15;

SEQLIB_TEST(units, ToTimecentsWithLimits)
{
    for (const TimecentsCase& c : kTimecents)
    {
        seqlib::test::SetContext("ms " + std::to_string(c.ms));
        SEQLIB_CHECK(seqlib::sf2::ToTimecents(c.ms, c.lo, c.hi) == c.expected);
    }
}

SEQLIB_TEST(units, ToTimecentsWithNoLimits)
{
    for (const UnlimitedTimecentsCase& c : kUnlimitedTimecents)
    {
        seqlib::test::SetContext("ms " + std::to_string(c.ms));
        SEQLIB_CHECK(seqlib::sf2::ToTimecents(c.ms) == c.expected);
    }
}

SEQLIB_TEST(units, LfoFrequencies)
{
    for (const LfoCase& c : kLfoFrequencies)
    {
        seqlib::test::SetContext("hz " + std::to_string(c.hz));
        SEQLIB_CHECK(seqlib::sf2::LfoFrequency(c.hz) == c.expected);
    }
}

SEQLIB_TEST(units, SoundFontPans)
{
    for (const PanCase& c : kPans)
    {
        seqlib::test::SetContext("pan " + std::to_string(c.pan));
        SEQLIB_CHECK(std::abs(seqlib::sf2::SoundFontPan(c.pan) - c.expected) <= kPanTolerance);
    }
}

SEQLIB_TEST(units, RoundedPans)
{
    for (int k = 0; k < 128; k++)
    {
        const double pan = seqlib::sf2::SoundFontPan((k - 64) / 63.0);
        const RoundedPan& expected = kRoundedPans[k];
        seqlib::test::SetContext("k " + std::to_string(k));
        SEQLIB_CHECK(std::lround(pan * 500.0) == expected.generator);
        SEQLIB_CHECK(std::lround(pan * 1000.0) == expected.tenths);
        SEQLIB_CHECK(std::lround(64.0 + pan * 64.0) == expected.midi || (expected.midi == 127 && pan == 1.0));
    }
}

SEQLIB_TEST(units, ZoneAttenuations)
{
    for (const ZoneAttenuationCase& c : kZoneAttenuations)
    {
        seqlib::test::SetContext("cB " + std::to_string(c.centibels));

        const seqlib::sf2::ZoneAttenuation attenuation = seqlib::sf2::ToZoneAttenuation(c.centibels);

        SEQLIB_CHECK(attenuation.generator == c.generator);
        SEQLIB_REQUIRE(attenuation.modulator.has_value() == (c.modulator != 0));
        if (attenuation.modulator)
        {
            const seqlib::sf2::Modulator& m = *attenuation.modulator;
            SEQLIB_CHECK(m.source == 0 && m.dest == 48 && m.amount == c.modulator);
            SEQLIB_CHECK(m.amount_source == 0 && m.transform == 0);
        }
    }
}

// In a synthesizer that applies 0.4 of the generator and all of the modulator, every attenuation from 0 to 1440 cB
// plays to within 0.2 cB up to 576 cB, and to within 0.5 cB above it.
SEQLIB_TEST(units, ZoneAttenuationsPlayAtTheirLevel)
{
    for (int tenths = 0; tenths <= 14400; tenths++)
    {
        const double centibels = tenths / 10.0;
        seqlib::test::SetContext("cB " + std::to_string(centibels));

        const seqlib::sf2::ZoneAttenuation attenuation = seqlib::sf2::ToZoneAttenuation(centibels);

        const double played = 0.4 * attenuation.generator + (attenuation.modulator ? attenuation.modulator->amount : 0);
        SEQLIB_CHECK(std::abs(played - centibels) <= (centibels <= 576.0 ? 0.2 : 0.5) + 1e-9);
    }
}

} // namespace
