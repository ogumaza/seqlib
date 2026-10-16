// SPDX-License-Identifier: MIT

#include "seqlib/wav.h"

#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include "bytes.h"

namespace seqlib
{

using detail::AppendFourcc;
using detail::AppendLe16;
using detail::AppendLe32;

namespace
{

// WAVE_FORMAT_PCM and WAVE_FORMAT_EXTENSIBLE.
constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatExtensible = 0xFFFE;

// The sizes of the fmt chunk's two forms and of the extensible form's extension, and the size of a smpl chunk with one
// loop.
constexpr uint32_t kPcmFormatSize = 16;
constexpr uint16_t kExtensionSize = 22;
constexpr uint32_t kExtensibleFormatSize = kPcmFormatSize + 2 + kExtensionSize;
constexpr uint32_t kSamplerSize = 60;

// KSDATAFORMAT_SUBTYPE_PCM, the GUID 00000001-0000-0010-8000-00AA00389B71, in a WAV file's byte order.
constexpr std::array<uint8_t, 16> kSubtypePcm = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                                 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

// The smpl chunk's unity note when a sample has none: middle C.
constexpr uint32_t kUnityNote = 60;

} // namespace

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

std::vector<uint8_t> WavFile(const Pcm& pcm, uint32_t rate)
{
    // The channels and the loop.
    if (pcm.channels.empty() || pcm.channels.size() > 32767)
    {
        throw std::invalid_argument("a WAV file holds 1 to 32,767 channels");
    }
    const std::size_t frames = pcm.channels[0].size();
    for (const std::vector<int16_t>& channel : pcm.channels)
    {
        if (channel.size() != frames)
        {
            throw std::invalid_argument("the channels of a WAV file have the same length");
        }
    }
    if (pcm.loop && (pcm.loop_start >= pcm.loop_end || pcm.loop_end > frames))
    {
        throw std::invalid_argument("a WAV file's loop lies within its samples");
    }

    // The chunks' sizes. A block holds one sample of each channel. The RIFF chunk's size counts its form type and every
    // chunk with its header.
    const auto channels = static_cast<uint16_t>(pcm.channels.size());
    const auto block = static_cast<uint16_t>(channels * 2);
    const uint32_t format_size = channels > 2 ? kExtensibleFormatSize : kPcmFormatSize;
    const uint32_t sampler_size = pcm.loop ? 8 + kSamplerSize : 0;
    const uint32_t headers = 4 + 8 + format_size + 8 + sampler_size;
    if (frames > (std::numeric_limits<uint32_t>::max() - headers) / block)
    {
        throw std::invalid_argument("the samples don't fit in a RIFF file");
    }
    const auto data_size = static_cast<uint32_t>(frames * block);
    std::vector<uint8_t> out;
    out.reserve(8 + headers + data_size);

    AppendFourcc(out, "RIFF");
    AppendLe32(out, headers + data_size);
    AppendFourcc(out, "WAVE");

    // More than two channels take the extensible form. A channel mask of 0 assigns no speaker to any channel.
    AppendFourcc(out, "fmt ");
    AppendLe32(out, format_size);
    AppendLe16(out, channels > 2 ? kFormatExtensible : kFormatPcm);
    AppendLe16(out, channels);
    AppendLe32(out, rate);
    AppendLe32(out, rate * block); // bytes a second
    AppendLe16(out, block);
    AppendLe16(out, 16); // bits a sample
    if (channels > 2)
    {
        AppendLe16(out, kExtensionSize);
        AppendLe16(out, 16); // valid bits a sample
        AppendLe32(out, 0);
        out.insert(out.end(), kSubtypePcm.begin(), kSubtypePcm.end());
    }

    AppendFourcc(out, "data");
    AppendLe32(out, data_size);
    for (std::size_t frame = 0; frame < frames; frame++)
    {
        for (const std::vector<int16_t>& channel : pcm.channels)
        {
            AppendLe16(out, static_cast<uint16_t>(channel[frame]));
        }
    }
    if (!pcm.loop)
    {
        return out;
    }

    // The smpl chunk: the sample period in nanoseconds, the unity note, and one loop. A loop's end in the chunk is the
    // last sample that the loop plays.
    AppendFourcc(out, "smpl");
    AppendLe32(out, kSamplerSize);
    AppendLe32(out, 0); // manufacturer
    AppendLe32(out, 0); // product
    AppendLe32(out, rate == 0 ? 0 : static_cast<uint32_t>((1000000000ull + rate / 2) / rate));
    AppendLe32(out, kUnityNote);
    AppendLe32(out, 0); // pitch fraction
    AppendLe32(out, 0); // SMPTE format
    AppendLe32(out, 0); // SMPTE offset
    AppendLe32(out, 1); // loops
    AppendLe32(out, 0); // sampler data
    AppendLe32(out, 0); // the loop's identifier
    AppendLe32(out, 0); // forward
    AppendLe32(out, pcm.loop_start);
    AppendLe32(out, pcm.loop_end - 1);
    AppendLe32(out, 0); // fraction
    AppendLe32(out, 0); // plays without end

    return out;
}

} // namespace seqlib
