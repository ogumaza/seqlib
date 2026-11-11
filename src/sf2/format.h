// SPDX-License-Identifier: MIT

// The rules of the SoundFont 2 format on sample data that the writer, the reader and the loop preparation follow: the
// zero points after each sample, and the 8/32/8 rule on loops. The header is private and isn't installed.

#pragma once

#include <cstdint>

namespace seqlib::detail
{

// The zero points that the SoundFont format requires after each sample.
constexpr uint32_t kZeroPoints = 46;

// The 8/32/8 rule: the fewest points that a looping sample may have before and after its loop, and in the loop.
constexpr uint32_t kLoopPadding = 8;
constexpr uint32_t kMinLoop = 32;

} // namespace seqlib::detail
