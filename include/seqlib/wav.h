// SPDX-License-Identifier: MIT

// WAV files.

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "seqlib/loop.h"

namespace seqlib
{

// Returns a mono 16-bit PCM WAV file that holds `samples` at `rate` samples per second.
std::vector<uint8_t> WavFile(std::span<const int16_t> samples, uint32_t rate);

// Returns a 16-bit PCM WAV file that holds `pcm`'s channels at `rate` samples per second. One or two channels take the
// format of the mono file. More channels take WAVE_FORMAT_EXTENSIBLE, without speaker positions. A loop goes in a smpl
// chunk after the samples. The chunk holds a forward loop that repeats without end, from loop_start to the sample
// before loop_end. Its unity note is 60. One channel without a loop gives the bytes of the mono file. Throws
// std::invalid_argument for no channels, more than 32,767 channels or channels of different lengths, for a loop outside
// the samples, and for samples that don't fit in a RIFF file.
std::vector<uint8_t> WavFile(const Pcm& pcm, uint32_t rate);

} // namespace seqlib
