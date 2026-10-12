// SPDX-License-Identifier: MIT

// WAV files.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace seqlib
{

// Returns a mono 16-bit PCM WAV file that holds `samples` at `rate` samples per second.
std::vector<uint8_t> WavFile(std::span<const int16_t> samples, uint32_t rate);

} // namespace seqlib
