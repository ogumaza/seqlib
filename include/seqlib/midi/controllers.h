// SPDX-License-Identifier: MIT

// MIDI controller numbers: the controllers that the MIDI file builder and the performance writer send.

#pragma once

namespace seqlib::midi::cc
{

constexpr int kBankSelect = 0;
constexpr int kModulation = 1;
constexpr int kDataEntry = 6;
constexpr int kVolume = 7;
constexpr int kPan = 10;
constexpr int kExpression = 11;
constexpr int kBankSelectLsb = 32;
constexpr int kDataEntryLsb = 38;
constexpr int kReverb = 91;
constexpr int kChorus = 93;
constexpr int kRpnLsb = 100;
constexpr int kRpnMsb = 101;

// The loop's start in RPG Maker's players. The MIDI specification leaves controller 111 undefined.
constexpr int kLoopStart = 111;

constexpr int kAllSoundOff = 120;

} // namespace seqlib::midi::cc
