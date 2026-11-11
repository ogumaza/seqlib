// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sf2/presets.h. The names and the warning are output: programs write them into their
// SoundFonts and messages.

#include "seqlib/sf2/presets.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "helpers.h"
#include "test.h"

namespace
{

namespace sf2 = seqlib::sf2;

using seqlib::test::PresetOf;
using Names = std::vector<std::string>;

SEQLIB_TEST(presets, NamesWithoutBank)
{
    SEQLIB_CHECK(sf2::PresetNames({{0, 5}}, false) == Names{"p5"});
}

SEQLIB_TEST(presets, NamesWithBank)
{
    SEQLIB_CHECK(sf2::PresetNames({{1, 5}}, true) == Names{"b1 p5"});
}

SEQLIB_TEST(presets, NamesNumberRepeats)
{
    const Names with_bank = sf2::PresetNames({{1, 5}, {0, 5}, {1, 5}, {1, 6}}, true);
    const Names without_bank = sf2::PresetNames({{0, 5}, {0, 7}, {0, 5}}, false);

    SEQLIB_CHECK((with_bank == Names{"b1 p5 #1", "b0 p5", "b1 p5 #2", "b1 p6"}));
    SEQLIB_CHECK((without_bank == Names{"p5 #1", "p7", "p5 #2"}));
}

SEQLIB_TEST(presets, Channel10Copy)
{
    sf2::SoundFont sf;
    sf.presets = {PresetOf("b0 p5", 0, 5, 0), PresetOf("b0 p6", 0, 6, 0)};
    std::vector<std::string> warnings;

    sf2::AddChannel10Copy(sf, 1, warnings);

    SEQLIB_REQUIRE(warnings.empty() && sf.presets.size() == 3);
    const sf2::Preset& copy = sf.presets[2];
    SEQLIB_CHECK(copy.name == "b0 p6" && copy.bank == sf2::kChannel10Bank && copy.program == 6);
    SEQLIB_CHECK(copy.zones.size() == 1 && copy.zones[0].generators.size() == 1);
}

SEQLIB_TEST(presets, Channel10CopyWarnsOfATakenProgram)
{
    sf2::SoundFont sf;
    sf.presets = {PresetOf("b0 p5", 0, 5, 0), PresetOf("drums", sf2::kChannel10Bank, 5, 0)};
    std::vector<std::string> warnings;

    sf2::AddChannel10Copy(sf, 0, warnings);

    SEQLIB_CHECK(sf.presets.size() == 2);
    SEQLIB_CHECK(warnings == std::vector<std::string>{"b0 p5: MIDI channel 10 plays drums instead"});
}

// A zone's generators as (number, stored value) pairs, the numbers as SoundFont 2.01 gives them.
std::vector<std::pair<uint16_t, uint16_t>> Generators(const sf2::Zone& z)
{
    std::vector<std::pair<uint16_t, uint16_t>> out;
    for (const sf2::Generator& g : z.generators)
    {
        out.emplace_back(g.oper, g.amount);
    }

    return out;
}

SEQLIB_TEST(presets, RegionZoneWritesEveryGenerator)
{
    sf2::ZoneValues values;
    values.keys = {36, 72};
    values.velocities = {1, 100};
    values.attenuation = 123.4;
    values.pan = -250;
    values.delay = -7973;
    values.attack = -11000;
    values.hold = -5000;
    values.decay = 1200;
    values.sustain = 960;
    values.release = 2400;
    values.root_key = 62;
    values.coarse_tune = -2;
    values.fine_tune = 37;
    values.loop = true;
    values.exclusive_class = 5;
    values.sample = 9;

    const sf2::Zone zone = sf2::RegionZone(values);

    // The attenuation is 2.5 times 123.4 cB, rounded (units_test.cpp has the rule).
    const std::vector<std::pair<uint16_t, uint16_t>> kExpected = {
        {43, 18468}, {44, 25601}, {48, 309}, {17, 65286}, {33, 57563}, {34, 54536}, {35, 60536}, {36, 1200},
        {37, 960},   {38, 2400},  {58, 62},  {51, 65534}, {52, 37},    {54, 1},     {57, 5},     {53, 9},
    };
    SEQLIB_CHECK(Generators(zone) == kExpected);
    SEQLIB_CHECK(zone.modulators.empty());
}

// Below 0.2 cB, 2.5 times the attenuation is below 0.5 cB. It rounds to no attenuation.
SEQLIB_TEST(presets, RegionZoneLeavesOutZeros)
{
    sf2::ZoneValues values;
    values.attenuation = 0.1;
    values.sample = 3;

    const std::vector<std::pair<uint16_t, uint16_t>> kExpected = {
        {43, 32512}, {44, 32512}, {34, 53536}, {35, 53536}, {36, 53536}, {37, 0}, {38, 53536}, {58, 60}, {53, 3},
    };
    SEQLIB_CHECK(Generators(sf2::RegionZone(values)) == kExpected);
}

// The zone's attenuation as ToZoneAttenuation gives it: the generator, 48, holds 2.5 times the attenuation up to its
// limit of 1440 cB. A modulator from No Controller, 0, holds the attenuation beyond 576 cB (units_test.cpp).
SEQLIB_TEST(presets, RegionZoneScalesTheAttenuation)
{
    auto zone = [](double centibels)
    {
        sf2::ZoneValues values;
        values.attenuation = centibels;
        return sf2::RegionZone(values);
    };
    // The value of the zone's initialAttenuation generator, or none if the zone has none.
    auto generator = [](const sf2::Zone& z) -> std::optional<uint16_t>
    {
        for (const auto& [oper, amount] : Generators(z))
        {
            if (oper == sf2::kInitialAttenuation)
            {
                return amount;
            }
        }

        return std::nullopt;
    };
    auto modulator = [](const sf2::Zone& z)
    {
        if (z.modulators.size() != 1)
        {
            return 0;
        }

        const sf2::Modulator& m = z.modulators[0];
        const bool attenuates = m.source == sf2::kNoController && m.dest == sf2::kInitialAttenuation &&
                                m.amount_source == sf2::kNoController && m.transform == sf2::kLinear;
        return attenuates ? static_cast<int>(m.amount) : 0;
    };

    const sf2::Zone quiet = zone(200.0);
    const sf2::Zone quieter = zone(700.0);
    const sf2::Zone silent = zone(1500.0);
    const sf2::Zone rounded = zone(2.5);
    const sf2::Zone none = zone(-3.0);

    SEQLIB_CHECK(generator(quiet) == 500 && quiet.modulators.empty());
    SEQLIB_CHECK(generator(quieter) == 1440 && modulator(quieter) == 124);
    SEQLIB_CHECK(generator(silent) == 1440 && modulator(silent) == 864);
    SEQLIB_CHECK(generator(rounded) == 6 && rounded.modulators.empty());
    SEQLIB_CHECK(!generator(none) && none.modulators.empty());
}

// A generator added to a zone goes before the sample's, which stays last. Several go in their order.
SEQLIB_TEST(presets, AddGeneratorKeepsTheSampleLast)
{
    sf2::ZoneValues values;
    values.sample = 4;
    sf2::Zone zone = sf2::RegionZone(values);
    const std::vector<sf2::Generator> kTuning = {sf2::Generator::Value(sf2::kFineTune, -7),
                                                 sf2::Generator::Value(sf2::kScaleTuning, 0)};

    sf2::AddGenerator(zone, sf2::kInitialFilterFc, 9000);
    sf2::AddGenerators(zone, kTuning);

    const std::vector<std::pair<uint16_t, uint16_t>> kExpected = {
        {43, 32512}, {44, 32512}, {34, 53536}, {35, 53536}, {36, 53536}, {37, 0},
        {38, 53536}, {58, 60},    {8, 9000},   {52, 65529}, {56, 0},     {53, 4},
    };
    SEQLIB_CHECK(Generators(zone) == kExpected);
}

// A zone without generators gets the generator at its end.
SEQLIB_TEST(presets, AddGeneratorToAnEmptyZone)
{
    sf2::Zone zone;

    sf2::AddGenerator(zone, sf2::kPan, -500);

    SEQLIB_CHECK(Generators(zone) == (std::vector<std::pair<uint16_t, uint16_t>>{{sf2::kPan, 65036}}));
}

SEQLIB_TEST(presets, AddInstrumentPreset)
{
    sf2::SoundFont sf;
    sf.instruments.resize(1);
    sf2::Instrument instrument;
    instrument.name = "b1 p2";

    sf2::AddInstrumentPreset(sf, 130, "b1 p2", instrument);

    SEQLIB_REQUIRE_EQ(sf.instruments.size(), 2);
    SEQLIB_REQUIRE_EQ(sf.presets.size(), 1);
    SEQLIB_CHECK(sf.instruments[1].name == "b1 p2");
    SEQLIB_CHECK(sf.presets[0].name == "b1 p2");
    SEQLIB_CHECK(sf.presets[0].bank == 1 && sf.presets[0].program == 2);
    SEQLIB_REQUIRE_EQ(sf.presets[0].zones.size(), 1);
    SEQLIB_CHECK(Generators(sf.presets[0].zones[0]) ==
                 (std::vector<std::pair<uint16_t, uint16_t>>{{sf2::kInstrument, 1}}));
}

} // namespace
