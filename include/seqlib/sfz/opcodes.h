// SPDX-License-Identifier: MIT

// The opcodes of an SFZ region that come from numbers alone: its tuning, its amplitude envelope and its loop. Each
// function returns its opcodes with a space before each one. They're appended to a region's line.

#pragma once

#include <cstdint>
#include <string>

namespace seqlib::sfz
{

// The opcodes that tune a region by `cents`: transpose for the whole semitones and tune for the cents left over. Each
// goes out only if it isn't 0.
std::string TuneOpcodes(int cents);

// An amplitude envelope in SFZ's units: its times in seconds, and its sustain level in percent.
struct AmpEnvelope
{
    double delay = 0.0;
    double attack = 0.0;
    double hold = 0.0;
    double decay = 0.0;
    double sustain = 100.0;
    double release = 0.0;
};

// The ampeg opcodes of `e`, with at most 6 fractional digits. ampeg_delay, ampeg_attack and ampeg_hold go out above 0.
// ampeg_decay and ampeg_sustain go out together below a sustain of 100. ampeg_release always goes out.
std::string AmpEnvelopeOpcodes(const AmpEnvelope& e);

// The loop opcodes: a continuous loop from sample `start` to sample `end`, the last sample in the loop, or no loop.
std::string LoopOpcodes(bool loop, uint32_t start, uint32_t end);

} // namespace seqlib::sfz
