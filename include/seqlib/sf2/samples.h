// SPDX-License-Identifier: MIT

// SoundFont samples from a wave that seqlib/loop.h has prepared.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "seqlib/loop.h"
#include "seqlib/sf2/soundfont.h"

namespace seqlib::sf2
{

// Adds the channels of `wave` to `sf` as samples that play at `sample_rate` Hz, with `root_key` as their root key. One
// channel becomes a mono sample named `name`. Two or more channels become a linked left and right pair from the first
// two, named `name` with "L" and "R" after it. A wave of two or more points that doesn't loop gets loop points around
// all its data. Some synths check unused loops too. Returns the samples' indexes in `sf.samples`. Throws
// std::runtime_error if `sf` then holds more than 65,535 samples.
std::vector<uint16_t> AddSamples(SoundFont& sf, PreparedWave wave, const std::string& name, uint32_t sample_rate,
                                 uint8_t root_key);

} // namespace seqlib::sf2
