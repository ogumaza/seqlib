// SPDX-License-Identifier: MIT

#include "seqlib/sf2/presets.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace seqlib::sf2
{

void AddChannel10Copy(SoundFont& sf, uint32_t index, std::vector<std::string>& warnings)
{
    Preset copy = sf.presets[index];
    for (const Preset& p : sf.presets)
    {
        if (p.bank == kChannel10Bank && p.program == copy.program)
        {
            warnings.push_back(copy.name + ": MIDI channel 10 plays " + p.name + " instead");
            return;
        }
    }

    copy.bank = kChannel10Bank;
    sf.presets.push_back(std::move(copy));
}

std::vector<std::string> PresetNames(const std::vector<std::pair<uint32_t, uint32_t>>& banks_and_programs,
                                     bool bank_prefix)
{
    std::map<std::pair<uint32_t, uint32_t>, int> count, seen;
    for (const std::pair<uint32_t, uint32_t>& key : banks_and_programs)
    {
        count[key]++;
    }

    std::vector<std::string> names;
    for (const std::pair<uint32_t, uint32_t>& key : banks_and_programs)
    {
        std::string n = (bank_prefix ? "b" + std::to_string(key.first) + " " : "") + "p" + std::to_string(key.second);
        if (count[key] > 1)
        {
            n += " #" + std::to_string(++seen[key]);
        }

        names.push_back(n);
    }

    return names;
}

Zone RegionZone(const ZoneValues& values)
{
    Zone z;
    auto gen = [&](uint16_t oper, int v)
    {
        z.generators.push_back(Generator::Value(oper, v));
    };

    z.generators.push_back(Generator::Range(kKeyRange, values.keys.first, values.keys.second));
    z.generators.push_back(Generator::Range(kVelRange, values.velocities.first, values.velocities.second));
    const int attenuation = std::clamp(static_cast<int>(std::lround(values.attenuation)), 0, 1440);
    if (attenuation != 0)
    {
        gen(kInitialAttenuation, attenuation);
    }
    if (values.pan != 0)
    {
        gen(kPan, values.pan);
    }

    if (values.delay)
    {
        gen(kDelayVolEnv, *values.delay);
    }
    gen(kAttackVolEnv, values.attack);
    gen(kHoldVolEnv, values.hold);
    gen(kDecayVolEnv, values.decay);
    gen(kSustainVolEnv, values.sustain);
    gen(kReleaseVolEnv, values.release);

    gen(kOverridingRootKey, values.root_key);
    if (values.coarse_tune != 0)
    {
        gen(kCoarseTune, values.coarse_tune);
    }
    if (values.fine_tune != 0)
    {
        gen(kFineTune, values.fine_tune);
    }

    if (values.loop)
    {
        gen(kSampleModes, 1);
    }
    if (values.exclusive_class != 0)
    {
        gen(kExclusiveClass, values.exclusive_class);
    }
    gen(kSampleId, values.sample);

    return z;
}

void AddInstrumentPreset(SoundFont& sf, uint32_t index, const std::string& name, Instrument instrument)
{
    const auto instrument_index = static_cast<uint16_t>(sf.instruments.size());
    sf.instruments.push_back(std::move(instrument));

    Preset preset;
    preset.name = name;
    preset.bank = static_cast<uint16_t>(index / 128);
    preset.program = static_cast<uint16_t>(index % 128);
    Zone z;
    z.generators.push_back(Generator::Value(kInstrument, instrument_index));
    preset.zones.push_back(std::move(z));
    sf.presets.push_back(std::move(preset));
}

} // namespace seqlib::sf2
