// SPDX-License-Identifier: MIT

// The controllers that set a channel's pitch bend range, for TrackBuilder::BendRange and the performance writer. The
// header is private and isn't installed.

#pragma once

#include <array>
#include <cstdint>
#include <utility>

#include "seqlib/midi/controllers.h"

namespace seqlib::detail
{

// The six controllers and their values that set the pitch bend range (RPN 0) to `semitones`, then select the null RPN.
// With the null RPN selected, later data entry changes nothing.
inline std::array<std::pair<uint8_t, uint8_t>, 6> BendRangeControllers(uint8_t semitones)
{
    namespace cc = midi::cc;

    return {{
        {cc::kRpnMsb, 0},
        {cc::kRpnLsb, 0},
        {cc::kDataEntry, semitones},
        {cc::kDataEntryLsb, 0},
        {cc::kRpnMsb, cc::kNullRpn},
        {cc::kRpnLsb, cc::kNullRpn},
    }};
}

} // namespace seqlib::detail
