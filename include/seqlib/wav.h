// SPDX-License-Identifier: MIT

// WAV files: a writer of 16-bit files, and a reader of PCM and floating-point files into 16-bit channels.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "seqlib/pcm.h"

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

// A WAV file's samples as 16-bit channels, with its rate and its loop, and what the file held.
struct WavContents
{
    Pcm pcm;
    uint32_t rate = 0;        // samples per second
    uint16_t bits = 16;       // bits per sample in the file
    bool floating = false;    // floating-point samples
    uint32_t loops = 0;       // the loops in the smpl chunk
    bool loop_forward = true; // the first loop plays forward
    bool loop_endless = true; // the first loop repeats without end
    std::size_t dropped = 0;  // the bytes at the data chunk's end that hold no whole frame
};

// Reads a WAV file: WAVE_FORMAT_PCM with 1 to 32 bits per sample, WAVE_FORMAT_IEEE_FLOAT with 32 or 64, or
// WAVE_FORMAT_EXTENSIBLE with either subtype. Each sample becomes 16 bits. An 8-bit sample is unsigned, and becomes the
// high byte. A deeper sample rounds to the nearest 16-bit value, half up. A floating-point sample scales by 32,768 and
// rounds half away from zero. Both clamp to the 16-bit range. NaN becomes 0. The first loop of a smpl chunk becomes
// `pcm`'s loop, from its start to the sample after its last, when it lies within the samples. The RIFF chunk's size can
// be wrong: the chunks end at the RIFF chunk's end or at the end of `bytes`, whichever comes first. A size too small
// for the form type counts as unknown. The last chunk can lack its pad byte. Throws ReadError for a file that isn't a
// RIFF WAVE file, a chunk that runs past the end, a missing fmt or data chunk, another format, no channels, a rate of
// 0, and a block size that doesn't fit the format.
WavContents ReadWav(std::span<const uint8_t> bytes);

} // namespace seqlib
