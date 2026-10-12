// SPDX-License-Identifier: MIT

// Loop preparation for SoundFont 2 and SFZ samples.

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

// Samples and loop bounds prepared for SoundFont or SFZ output.
struct PreparedWave
{
    std::vector<std::vector<int16_t>> channels;
    bool loop = false;
    uint32_t loop_start = 0, loop_end = 0;
};

// Unrolls `pcm`'s loop so it starts at or after `min_start`, has at least `min_before` repeated samples before it, and
// is at least `min_length` samples long. Appends `after` samples from the loop start for interpolation. An empty
// channel becomes one silent sample. Playback is unchanged. A loop requires loop_start < loop_end <= each channel's
// length.
PreparedWave UnrollLoop(const Pcm& pcm, uint32_t min_start, uint32_t min_before, uint32_t min_length, uint32_t after);

// Repeats loop data to meet SoundFont requirements: 8 samples before the loop, at least 32 within it and 32 after it.
// Playback is unchanged.
PreparedWave PrepareForSoundFont(const Pcm& pcm);

// Prepares samples for sfizz. The loop crossfade in sfizz spans at least 1 ms and blends the loop end with data before
// the loop start. Prepends repeated loop data: `crossfade` samples, and at least 64. The crossfade then blends
// identical samples. Extends the loop to at least 64 samples and appends 64 samples from its start for interpolation.
// Playback is unchanged. For a crossfade of 4 ms, pass (rate * 4 + 999) / 1000.
PreparedWave PrepareForSfz(const Pcm& pcm, uint32_t crossfade);

} // namespace seqlib
