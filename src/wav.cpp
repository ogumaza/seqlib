// SPDX-License-Identifier: MIT

#include "seqlib/wav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "bytes.h"
#include "seqlib/read_error.h"

namespace seqlib
{

using detail::AppendFourcc;
using detail::AppendLe16;
using detail::AppendLe32;
using detail::LoadLe16;
using detail::LoadLe32;

namespace
{

// WAVE_FORMAT_PCM, WAVE_FORMAT_IEEE_FLOAT and WAVE_FORMAT_EXTENSIBLE.
constexpr uint16_t kFormatPcm = 1;
constexpr uint16_t kFormatFloat = 3;
constexpr uint16_t kFormatExtensible = 0xFFFE;

// The sizes of the fmt chunk's two forms and of the extensible form's extension.
constexpr uint32_t kPcmFormatSize = 16;
constexpr uint16_t kExtensionSize = 22;
constexpr uint32_t kExtensibleFormatSize = kPcmFormatSize + 2 + kExtensionSize;

// KSDATAFORMAT_SUBTYPE_PCM, the GUID 00000001-0000-0010-8000-00AA00389B71, in a WAV file's byte order.
constexpr std::array<uint8_t, 16> kSubtypePcm = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                                 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

// The smpl chunk's unity note when a sample has none: middle C.
constexpr uint32_t kUnityNote = 60;

// Where the smpl chunk holds the number of loops, and where its loops start, each of kLoopSize bytes: an identifier, a
// type, the first sample, the last sample, a fraction and a number of plays.
constexpr std::size_t kLoopCount = 28;
constexpr std::size_t kLoops = 36;
constexpr std::size_t kLoopSize = 24;

// The size of a smpl chunk with one loop.
constexpr auto kSamplerSize = static_cast<uint32_t>(kLoops + kLoopSize);

// The size of the fmt chunk in a file of `channels` channels. More than two channels take the extensible form.
uint32_t FormatSize(uint16_t channels)
{
    return channels > 2 ? kExtensibleFormatSize : kPcmFormatSize;
}

// The bytes in the RIFF chunk before the samples of a file of `channels` channels: the form type, the fmt chunk and the
// data chunk's header.
uint32_t HeaderSize(uint16_t channels)
{
    return 4 + 8 + FormatSize(channels) + 8;
}

// Appends the RIFF chunk's header, the fmt chunk and the data chunk's header of a 16-bit PCM file of `channels`
// channels at `rate` samples per second. The samples take `data_size` bytes. Chunks of `after` bytes follow them.
void AppendHeaders(std::vector<uint8_t>& out, uint16_t channels, uint32_t rate, uint32_t data_size, uint32_t after)
{
    AppendFourcc(out, "RIFF");
    AppendLe32(out, HeaderSize(channels) + data_size + after);
    AppendFourcc(out, "WAVE");

    // A block holds one sample of each channel. A channel mask of 0 assigns no speaker to any channel.
    const auto block = static_cast<uint16_t>(channels * 2);
    const bool extensible = channels > 2;
    AppendFourcc(out, "fmt ");
    AppendLe32(out, FormatSize(channels));
    AppendLe16(out, extensible ? kFormatExtensible : kFormatPcm);
    AppendLe16(out, channels);
    AppendLe32(out, rate);
    AppendLe32(out, rate * block); // bytes a second
    AppendLe16(out, block);
    AppendLe16(out, 16); // bits a sample
    if (extensible)
    {
        AppendLe16(out, kExtensionSize);
        AppendLe16(out, 16); // valid bits a sample
        AppendLe32(out, 0);
        out.insert(out.end(), kSubtypePcm.begin(), kSubtypePcm.end());
    }

    AppendFourcc(out, "data");
    AppendLe32(out, data_size);
}

// Appends a smpl chunk for samples at `rate` samples per second: the sample period in nanoseconds, the unity note, and
// one loop. The loop plays forward from `loop_start` to the sample before `loop_end`, and repeats without end. A loop's
// end in the chunk is the last sample that the loop plays.
void AppendSampler(std::vector<uint8_t>& out, uint32_t rate, uint32_t loop_start, uint32_t loop_end)
{
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
    AppendLe32(out, loop_start);
    AppendLe32(out, loop_end - 1);
    AppendLe32(out, 0); // fraction
    AppendLe32(out, 0); // plays without end
}

// A chunk of a RIFF form: its ID, and where its data starts and how long it is.
struct Chunk
{
    std::string id;
    std::size_t start = 0;
    std::size_t size = 0;
};

// The chunks of the RIFF form in `bytes`, after its form type, up to `end`. The last chunk can lack its pad byte.
// Throws ReadError for a chunk that runs past `end`.
std::vector<Chunk> ChunksOf(std::span<const uint8_t> bytes, std::size_t end)
{
    std::vector<Chunk> chunks;
    std::size_t at = 12;
    while (at + 8 <= end)
    {
        const Chunk chunk{std::string(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(at + 4)),
                          at + 8, LoadLe32(bytes.data() + at + 4)};
        if (chunk.size > end - chunk.start)
        {
            throw ReadError(at, "chunk " + chunk.id + " runs past the end of the file");
        }

        chunks.push_back(chunk);
        at = chunk.start + chunk.size + chunk.size % 2;
    }

    return chunks;
}

// The first chunk in `chunks` with the ID `id`, if any.
std::optional<Chunk> FirstChunk(const std::vector<Chunk>& chunks, std::string_view id)
{
    for (const Chunk& chunk : chunks)
    {
        if (chunk.id == id)
        {
            return chunk;
        }
    }

    return std::nullopt;
}

// What a fmt chunk gives.
struct Format
{
    uint16_t channels = 0;
    uint32_t rate = 0;     // samples per second
    uint16_t block = 0;    // bytes per frame: one sample of each channel
    uint16_t bits = 0;     // bits per sample
    bool floating = false; // floating-point samples
    std::size_t width = 0; // bytes per sample
};

// Whether `format`'s samples fit their width. A floating-point sample has 32 or 64 bits, in 4 or 8 bytes. A PCM sample
// has 1 to 32 bits, in the fewest bytes that hold them.
bool SamplesFit(const Format& format)
{
    if (format.floating)
    {
        return (format.width == 4 || format.width == 8) && format.bits == format.width * 8;
    }

    return format.width >= 1 && format.width <= 4 && format.bits >= 1 && (format.bits + 7u) / 8u == format.width;
}

// Reads the fmt chunk `fmt` in `bytes`. The chunk holds 16 bytes or more. The extensible form's subtype gives the
// format. Throws ReadError for a format other than PCM and IEEE float, no channels, a rate of 0, and a block that
// doesn't fit the format.
Format ReadFormat(std::span<const uint8_t> bytes, const Chunk& fmt)
{
    const uint8_t* f = bytes.data() + fmt.start;
    uint16_t tag = LoadLe16(f);
    Format format;
    format.channels = LoadLe16(f + 2);
    format.rate = LoadLe32(f + 4);
    format.block = LoadLe16(f + 12);
    format.bits = LoadLe16(f + 14);
    if (tag == kFormatExtensible)
    {
        if (fmt.size < kExtensibleFormatSize ||
            std::memcmp(f + 26, kSubtypePcm.data() + 2, kSubtypePcm.size() - 2) != 0)
        {
            throw ReadError(fmt.start, "an extensible format without a subtype of PCM or IEEE float");
        }
        tag = LoadLe16(f + 24);
    }
    format.floating = tag == kFormatFloat;
    if (tag != kFormatPcm && !format.floating)
    {
        throw ReadError(fmt.start, "format " + std::to_string(tag) + " isn't PCM or IEEE float");
    }
    if (format.channels == 0 || format.rate == 0)
    {
        throw ReadError(fmt.start, "no channels, or a rate of 0");
    }

    format.width = format.block % format.channels == 0 ? format.block / format.channels : 0;
    if (!SamplesFit(format))
    {
        throw ReadError(fmt.start, "a block of " + std::to_string(format.block) + " bytes for " +
                                       std::to_string(format.channels) + " channels of " + std::to_string(format.bits) +
                                       " bits");
    }

    return format;
}

// The PCM sample of `width` bytes at `p`, 1 to 4, as 16 bits. An 8-bit sample is unsigned. A deeper one is signed, and
// rounds to 16 bits half up.
int16_t PcmSample(const uint8_t* p, std::size_t width)
{
    if (width == 1)
    {
        return static_cast<int16_t>((p[0] - 128) * 256);
    }

    int64_t value = static_cast<int8_t>(p[width - 1]);
    for (std::size_t i = width - 1; i > 0; i--)
    {
        value = value * 256 + p[i - 1];
    }
    if (width > 2)
    {
        const int shift = static_cast<int>(width - 2) * 8;
        value = (value + (int64_t{1} << (shift - 1))) >> shift;
    }

    return static_cast<int16_t>(std::min<int64_t>(value, 32767));
}

// The floating-point sample of `width` bytes at `p`, 4 or 8, as 16 bits. It scales by 32,768, rounds half away from
// zero and clamps. NaN gives 0.
int16_t FloatSample(const uint8_t* p, std::size_t width)
{
    double value = 0.0;
    if (width == 4)
    {
        const uint32_t bits = LoadLe32(p);
        float single = 0.0f;
        std::memcpy(&single, &bits, 4);
        value = single;
    }
    else
    {
        const uint64_t bits = LoadLe32(p) | uint64_t{LoadLe32(p + 4)} << 32;
        std::memcpy(&value, &bits, 8);
    }
    if (std::isnan(value))
    {
        return 0;
    }

    return static_cast<int16_t>(std::lround(std::clamp(value * 32768.0, -32768.0, 32767.0)));
}

// Reads the smpl chunk `smpl` in `bytes` into `contents`: the number of loops, and the first loop's type and number of
// plays. That loop becomes `contents.pcm`'s loop when it lies within the `frames` frames. The chunk gives a loop's last
// sample. `loop_end` is the sample after it. A chunk too short to hold the number of loops gives nothing. One too short
// to hold the first loop gives only the number of loops.
void ReadLoop(std::span<const uint8_t> bytes, const Chunk& smpl, std::size_t frames, WavContents& contents)
{
    if (smpl.size < kLoops)
    {
        return;
    }
    const uint8_t* s = bytes.data() + smpl.start;
    contents.loops = LoadLe32(s + kLoopCount);
    if (contents.loops == 0 || smpl.size < kLoops + kLoopSize)
    {
        return;
    }

    const uint8_t* loop = s + kLoops;
    contents.loop_forward = LoadLe32(loop + 4) == 0;
    contents.loop_endless = LoadLe32(loop + 20) == 0;
    const uint32_t first = LoadLe32(loop + 8);
    const uint32_t last = LoadLe32(loop + 12);
    if (first <= last && last < frames)
    {
        contents.pcm.loop = true;
        contents.pcm.loop_start = first;
        contents.pcm.loop_end = last + 1;
    }
}

} // namespace

std::vector<uint8_t> WavFile(std::span<const int16_t> samples, uint32_t rate)
{
    const uint32_t data_size = static_cast<uint32_t>(samples.size() * 2);
    std::vector<uint8_t> out;
    out.reserve(8 + HeaderSize(1) + data_size);
    AppendHeaders(out, 1, rate, data_size, 0);
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

    // The chunks' sizes. A block holds one sample of each channel. `headers` counts the bytes in the RIFF chunk other
    // than the samples.
    const auto channels = static_cast<uint16_t>(pcm.channels.size());
    const auto block = static_cast<uint16_t>(channels * 2);
    const uint32_t sampler_size = pcm.loop ? 8 + kSamplerSize : 0;
    const uint32_t headers = HeaderSize(channels) + sampler_size;
    if (frames > (std::numeric_limits<uint32_t>::max() - headers) / block)
    {
        throw std::invalid_argument("the samples don't fit in a RIFF file");
    }
    const auto data_size = static_cast<uint32_t>(frames * block);

    std::vector<uint8_t> out;
    out.reserve(8 + headers + data_size);
    AppendHeaders(out, channels, rate, data_size, sampler_size);
    for (std::size_t frame = 0; frame < frames; frame++)
    {
        for (const std::vector<int16_t>& channel : pcm.channels)
        {
            AppendLe16(out, static_cast<uint16_t>(channel[frame]));
        }
    }

    if (pcm.loop)
    {
        AppendSampler(out, rate, pcm.loop_start, pcm.loop_end);
    }

    return out;
}

WavContents ReadWav(std::span<const uint8_t> bytes)
{
    // The RIFF form, and the first chunk of each kind. The form's size can be wrong. A size too small for the form type
    // counts as unknown.
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
    {
        throw ReadError(0, "not a RIFF WAVE file");
    }
    const uint32_t form = LoadLe32(bytes.data() + 4);
    const std::size_t end = form < 4 ? bytes.size() : std::min<std::size_t>(bytes.size(), std::size_t{form} + 8);
    const std::vector<Chunk> chunks = ChunksOf(bytes, end);
    const std::optional<Chunk> fmt = FirstChunk(chunks, "fmt ");
    const std::optional<Chunk> data = FirstChunk(chunks, "data");
    const std::optional<Chunk> smpl = FirstChunk(chunks, "smpl");

    if (!fmt || fmt->size < kPcmFormatSize)
    {
        throw ReadError(fmt ? fmt->start : 12, "no fmt chunk of 16 bytes or more");
    }
    const Format format = ReadFormat(bytes, *fmt);
    WavContents contents;
    contents.rate = format.rate;
    contents.bits = format.bits;
    contents.floating = format.floating;

    // The samples. Bytes that hold no whole frame stay out.
    if (!data)
    {
        throw ReadError(12, "no data chunk");
    }
    const std::size_t frames = data->size / format.block;
    contents.dropped = data->size % format.block;
    contents.pcm.channels.assign(format.channels, std::vector<int16_t>(frames));
    const auto sample = format.floating ? FloatSample : PcmSample;
    for (std::size_t frame = 0; frame < frames; frame++)
    {
        for (std::size_t channel = 0; channel < format.channels; channel++)
        {
            const uint8_t* p = bytes.data() + data->start + frame * format.block + channel * format.width;
            contents.pcm.channels[channel][frame] = sample(p, format.width);
        }
    }

    if (smpl)
    {
        ReadLoop(bytes, *smpl, frames, contents);
    }

    return contents;
}

} // namespace seqlib
