// SPDX-License-Identifier: MIT

// A SoundFont's presets: their instruments' zones, the presets' names, and the copies that MIDI channel 10 plays.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "seqlib/sf2/soundfont.h"

namespace seqlib::sf2
{

// The SoundFont bank that MIDI channel 10 plays (General MIDI's drums). Synths use this bank on channel 10 whatever
// bank the channel selects.
inline constexpr uint16_t kChannel10Bank = 128;

// Copies preset `index` of `sf` to bank kChannel10Bank with the same program, for MIDI channel 10. If a preset already
// has that bank and program, adds a warning to `warnings` and leaves `sf` as it is.
void AddChannel10Copy(SoundFont& sf, uint32_t index, std::vector<std::string>& warnings);

// Names presets by bank and program ("b1 p5"), or by program alone ("p5") without `bank_prefix`. `banks_and_programs`
// gives each preset's bank and program. They're 32-bit numbers, wider than Preset's fields. A caller's preset keys then
// convert to them without a cast. Presets that share a bank and program get a number after the name, in the order
// given: "p5 #1", "p5 #2".
std::vector<std::string> PresetNames(const std::vector<std::pair<uint32_t, uint32_t>>& banks_and_programs,
                                     bool bank_prefix);

// The generators of a zone that plays a sample over a range of keys and velocities, before RegionZone writes them.
struct ZoneValues
{
    std::pair<int, int> keys{0, 127};
    std::pair<int, int> velocities{0, 127};
    double attenuation = 0.0; // centibels, 0 to 1440, written in the form that ToZoneAttenuation (units.h) gives
    int pan = 0;              // -500 (left) to 500 (right). 0 is left out
    std::optional<int> delay; // the volume envelope's delay in timecents, if the zone has one

    // The volume envelope: times in timecents, and the sustain in centibels of attenuation.
    int attack = -12000;
    int hold = -12000;
    int decay = -12000;
    int sustain = 0;
    int release = -12000;

    int root_key = 60;
    int coarse_tune = 0;     // semitones. 0 is left out
    int fine_tune = 0;       // cents. 0 is left out
    bool loop = false;       // the sample loops (sampleModes 1)
    int exclusive_class = 0; // 0 is left out
    uint16_t sample = 0;
};

// The zone with `values`' generators, in this order: the key and velocity ranges, the attenuation, the pan, the volume
// envelope, the root key, the tuning, the sample mode, the exclusive class and the sample. An attenuation that the
// generator can't hold also gives the zone a modulator (ToZoneAttenuation).
Zone RegionZone(const ZoneValues& values);

// Adds `instrument` to `sf`, and a preset named `name` that plays it, at bank index / 128 and program index % 128.
void AddInstrumentPreset(SoundFont& sf, uint32_t index, const std::string& name, Instrument instrument);

} // namespace seqlib::sf2
