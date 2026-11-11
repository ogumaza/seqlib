// SPDX-License-Identifier: MIT

// The limits of the values in MIDI data bytes, for the builder, the reader, the recorder and the performance writer.
// The header is private and isn't installed.

#pragma once

namespace seqlib::detail
{

// The largest data byte. A byte above it is a status byte.
constexpr int kMaxDataByte = 127;

// The pitch wheel: the value that bends nothing, and the largest value. The wheel has 8192 steps below the centre but
// only 8191 above it.
constexpr int kPitchWheelCentre = 8192;
constexpr int kPitchWheelMax = 16383;

} // namespace seqlib::detail
