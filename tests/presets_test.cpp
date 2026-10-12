// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sf2/presets.h. The names and the warning are output: programs write them into their
// SoundFonts and messages.

#include "seqlib/sf2/presets.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "test.h"

namespace
{

namespace sf2 = seqlib::sf2;

using Names = std::vector<std::string>;

sf2::Preset NewPreset(const std::string& name, uint16_t bank, uint16_t program)
{
    sf2::Preset p;
    p.name = name;
    p.bank = bank;
    p.program = program;
    p.zones.push_back(sf2::Zone{{sf2::Generator::Value(sf2::kInstrument, 0)}, {}});

    return p;
}

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
    sf.presets = {NewPreset("b0 p5", 0, 5), NewPreset("b0 p6", 0, 6)};
    std::vector<std::string> warnings;

    sf2::AddChannel10Copy(sf, 1, warnings);

    SEQLIB_CHECK(warnings.empty() && sf.presets.size() == 3);
    const sf2::Preset& copy = sf.presets[2];
    SEQLIB_CHECK(copy.name == "b0 p6" && copy.bank == sf2::kChannel10Bank && copy.program == 6);
    SEQLIB_CHECK(copy.zones.size() == 1 && copy.zones[0].generators.size() == 1);
}

SEQLIB_TEST(presets, Channel10CopyWarnsOfATakenProgram)
{
    sf2::SoundFont sf;
    sf.presets = {NewPreset("b0 p5", 0, 5), NewPreset("drums", sf2::kChannel10Bank, 5)};
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

    const std::vector<std::pair<uint16_t, uint16_t>> kExpected = {
        {43, 18468}, {44, 25601}, {48, 123}, {17, 65286}, {33, 57563}, {34, 54536}, {35, 60536}, {36, 1200},
        {37, 960},   {38, 2400},  {58, 62},  {51, 65534}, {52, 37},    {54, 1},     {57, 5},     {53, 9},
    };
    SEQLIB_CHECK(Generators(sf2::RegionZone(values)) == kExpected);
}

SEQLIB_TEST(presets, RegionZoneLeavesOutZeros)
{
    sf2::ZoneValues values;
    values.attenuation = 0.4;
    values.sample = 3;

    const std::vector<std::pair<uint16_t, uint16_t>> kExpected = {
        {43, 32512}, {44, 32512}, {34, 53536}, {35, 53536}, {36, 53536}, {37, 0}, {38, 53536}, {58, 60}, {53, 3},
    };
    SEQLIB_CHECK(Generators(sf2::RegionZone(values)) == kExpected);
}

SEQLIB_TEST(presets, RegionZoneRoundsAndLimitsTheAttenuation)
{
    auto attenuation = [](double centibels)
    {
        sf2::ZoneValues values;
        values.attenuation = centibels;
        const std::vector<std::pair<uint16_t, uint16_t>> generators = Generators(sf2::RegionZone(values));
        return generators.size() > 2 && generators[2].first == 48 ? generators[2].second : 0;
    };

    SEQLIB_CHECK(attenuation(1500.0) == 1440);
    SEQLIB_CHECK(attenuation(2.5) == 3);
    SEQLIB_CHECK(attenuation(-3.0) == 0);
}

SEQLIB_TEST(presets, AddInstrumentPreset)
{
    sf2::SoundFont sf;
    sf.instruments.resize(1);
    sf2::Instrument instrument;
    instrument.name = "b1 p2";

    sf2::AddInstrumentPreset(sf, 130, "b1 p2", instrument);

    SEQLIB_CHECK(sf.instruments.size() == 2 && sf.instruments[1].name == "b1 p2");
    SEQLIB_CHECK(sf.presets.size() == 1 && sf.presets[0].name == "b1 p2");
    SEQLIB_CHECK(sf.presets.size() == 1 && sf.presets[0].bank == 1 && sf.presets[0].program == 2);
    SEQLIB_CHECK(sf.presets.size() == 1 && sf.presets[0].zones.size() == 1 &&
                 Generators(sf.presets[0].zones[0]) == (std::vector<std::pair<uint16_t, uint16_t>>{{41, 1}}));
}

} // namespace
