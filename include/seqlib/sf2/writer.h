// SPDX-License-Identifier: MIT

// SoundFont 2.01 writer.

#pragma once

#include <cstdint>
#include <vector>

#include "seqlib/sf2/soundfont.h"

namespace seqlib::sf2
{

struct WriteOptions
{
    // Sort the presets by bank, then by program. Presets with the same bank and program keep their order. Without this,
    // the presets go in the order given.
    bool sort_presets = false;

    // Write 46 zero points even when there are no samples. FluidSynth rejects a SoundFont with no sample data.
    bool pad_empty_sample_data = true;
};

// Serialises a SoundFont. Writes ifil (2.01), isng, an empty INAM if `sf.info` has none, and then `sf.info`. Orders
// each zone's generators as required: key range, velocity range, other generators, then sample or instrument. Appends
// 46 zero samples after each sample. Throws std::runtime_error if there are more than 65,535 zones, generators or
// modulators, and std::invalid_argument if an INFO field's ID doesn't have four characters.
std::vector<uint8_t> Write(const SoundFont& sf, const WriteOptions& options = {});

} // namespace seqlib::sf2
