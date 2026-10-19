// SPDX-License-Identifier: MIT

// Decoded 16-bit PCM: the samples that wav.h writes and reads, and that loop.h prepares for SoundFont 2 and SFZ files.

#pragma once

#include <cstdint>
#include <vector>

namespace seqlib
{

// Decoded 16-bit PCM: one or more channels of the same length, and a loop. A program's PCM type can derive from it and
// add a sample rate.
struct Pcm
{
    bool loop = false;       // only when loop_start < loop_end
    uint32_t loop_start = 0; // first sample of the loop
    uint32_t loop_end = 0;   // one past the last sample of the loop
    std::vector<std::vector<int16_t>> channels;
};

} // namespace seqlib
