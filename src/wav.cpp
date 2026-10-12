// SPDX-License-Identifier: MIT

#include "seqlib/wav.h"

#include "bytes.h"

namespace seqlib
{

using detail::AppendFourcc;
using detail::AppendLe16;
using detail::AppendLe32;

std::vector<uint8_t> WavFile(std::span<const int16_t> samples, uint32_t rate)
{
    const uint32_t data_size = static_cast<uint32_t>(samples.size() * 2);
    std::vector<uint8_t> out;
    out.reserve(44 + data_size);

    AppendFourcc(out, "RIFF");
    AppendLe32(out, 36 + data_size);
    AppendFourcc(out, "WAVE");

    AppendFourcc(out, "fmt ");
    AppendLe32(out, 16);
    AppendLe16(out, 1); // PCM
    AppendLe16(out, 1); // mono
    AppendLe32(out, rate);
    AppendLe32(out, rate * 2); // bytes a second
    AppendLe16(out, 2);        // bytes a sample
    AppendLe16(out, 16);       // bits a sample

    AppendFourcc(out, "data");
    AppendLe32(out, data_size);
    for (int16_t v : samples)
    {
        AppendLe16(out, static_cast<uint16_t>(v));
    }

    return out;
}

} // namespace seqlib
