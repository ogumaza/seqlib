// SPDX-License-Identifier: MIT

// Golden tests for seqlib/wav.h. The expected files are fixed: a change to them is a change of seqlib's output. The
// reader's tests build their files from the RIFF and WAVE specifications, and give the samples that those call for.

#include "seqlib/wav.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "seqlib/read_error.h"
#include "test.h"

namespace
{

std::vector<int16_t> Ramp()
{
    std::vector<int16_t> samples;
    for (int i = 0; i < 100; i++)
    {
        samples.push_back(static_cast<int16_t>(i * 655 - 32768));
    }

    return samples;
}

SEQLIB_TEST(wav, WavFiles)
{
    struct Case
    {
        std::vector<int16_t> samples;
        uint32_t rate;
        std::vector<uint8_t> expected;
    };
    const Case kCases[] = {
        {{}, 32000, {0x52, 0x49, 0x46, 0x46, 0x24, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74,
                     0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x7d, 0x00, 0x00, 0x00, 0xfa,
                     0x00, 0x00, 0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x00, 0x00, 0x00, 0x00}},
        {{0, 1, -1, 32767, -32768},
         32000,
         {0x52, 0x49, 0x46, 0x46, 0x2e, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20, 0x10, 0x00,
          0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x7d, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00,
          0x64, 0x61, 0x74, 0x61, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x80}},
        {{1234}, 44100, {0x52, 0x49, 0x46, 0x46, 0x26, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
                         0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x44, 0xac, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00,
                         0x02, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x02, 0x00, 0x00, 0x00, 0xd2, 0x04}},
        {Ramp(),
         48000,
         {0x52, 0x49, 0x46, 0x46, 0xec, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20, 0x10, 0x00,
          0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x77, 0x01, 0x00, 0x02, 0x00, 0x10, 0x00,
          0x64, 0x61, 0x74, 0x61, 0xc8, 0x00, 0x00, 0x00, 0x00, 0x80, 0x8f, 0x82, 0x1e, 0x85, 0xad, 0x87, 0x3c, 0x8a,
          0xcb, 0x8c, 0x5a, 0x8f, 0xe9, 0x91, 0x78, 0x94, 0x07, 0x97, 0x96, 0x99, 0x25, 0x9c, 0xb4, 0x9e, 0x43, 0xa1,
          0xd2, 0xa3, 0x61, 0xa6, 0xf0, 0xa8, 0x7f, 0xab, 0x0e, 0xae, 0x9d, 0xb0, 0x2c, 0xb3, 0xbb, 0xb5, 0x4a, 0xb8,
          0xd9, 0xba, 0x68, 0xbd, 0xf7, 0xbf, 0x86, 0xc2, 0x15, 0xc5, 0xa4, 0xc7, 0x33, 0xca, 0xc2, 0xcc, 0x51, 0xcf,
          0xe0, 0xd1, 0x6f, 0xd4, 0xfe, 0xd6, 0x8d, 0xd9, 0x1c, 0xdc, 0xab, 0xde, 0x3a, 0xe1, 0xc9, 0xe3, 0x58, 0xe6,
          0xe7, 0xe8, 0x76, 0xeb, 0x05, 0xee, 0x94, 0xf0, 0x23, 0xf3, 0xb2, 0xf5, 0x41, 0xf8, 0xd0, 0xfa, 0x5f, 0xfd,
          0xee, 0xff, 0x7d, 0x02, 0x0c, 0x05, 0x9b, 0x07, 0x2a, 0x0a, 0xb9, 0x0c, 0x48, 0x0f, 0xd7, 0x11, 0x66, 0x14,
          0xf5, 0x16, 0x84, 0x19, 0x13, 0x1c, 0xa2, 0x1e, 0x31, 0x21, 0xc0, 0x23, 0x4f, 0x26, 0xde, 0x28, 0x6d, 0x2b,
          0xfc, 0x2d, 0x8b, 0x30, 0x1a, 0x33, 0xa9, 0x35, 0x38, 0x38, 0xc7, 0x3a, 0x56, 0x3d, 0xe5, 0x3f, 0x74, 0x42,
          0x03, 0x45, 0x92, 0x47, 0x21, 0x4a, 0xb0, 0x4c, 0x3f, 0x4f, 0xce, 0x51, 0x5d, 0x54, 0xec, 0x56, 0x7b, 0x59,
          0x0a, 0x5c, 0x99, 0x5e, 0x28, 0x61, 0xb7, 0x63, 0x46, 0x66, 0xd5, 0x68, 0x64, 0x6b, 0xf3, 0x6d, 0x82, 0x70,
          0x11, 0x73, 0xa0, 0x75, 0x2f, 0x78, 0xbe, 0x7a, 0x4d, 0x7d}},
        {{-2, 2}, 32728, {0x52, 0x49, 0x46, 0x46, 0x28, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45,
                          0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
                          0xd8, 0x7f, 0x00, 0x00, 0xb0, 0xff, 0x00, 0x00, 0x02, 0x00, 0x10, 0x00,
                          0x64, 0x61, 0x74, 0x61, 0x04, 0x00, 0x00, 0x00, 0xfe, 0xff, 0x02, 0x00}},
    };

    for (const Case& c : kCases)
    {
        SEQLIB_CHECK(seqlib::WavFile(c.samples, c.rate) == c.expected);
    }
}

// One channel without a loop gives the mono file's bytes.
SEQLIB_TEST(wav, OneChannel)
{
    const std::vector<std::vector<int16_t>> kSamples = {{}, {0, 1, -1, 32767, -32768}, {1234}, Ramp()};
    for (const std::vector<int16_t>& samples : kSamples)
    {
        seqlib::Pcm pcm;
        pcm.channels = {samples};

        const std::vector<uint8_t> file = seqlib::WavFile(pcm, 44100);

        SEQLIB_CHECK(file == seqlib::WavFile(samples, 44100));
    }
}

// Two channels with a loop: interleaved samples, then a smpl chunk. The loop's end in the chunk is the last sample that
// the loop plays. A sample lasts 31,250 ns at 32 kHz.
SEQLIB_TEST(wav, StereoLoop)
{
    seqlib::Pcm pcm;
    pcm.channels = {{1, -2, 3}, {256, -32768, 32767}};
    pcm.loop = true;
    pcm.loop_start = 1;
    pcm.loop_end = 3;
    const std::vector<uint8_t> kExpected = {
        0x52, 0x49, 0x46, 0x46, 0x74, 0x00, 0x00, 0x00, // RIFF, 116 bytes
        0x57, 0x41, 0x56, 0x45,                         // WAVE
        0x66, 0x6d, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, // fmt, 16 bytes
        0x01, 0x00, 0x02, 0x00, 0x00, 0x7d, 0x00, 0x00, // PCM, 2 channels, 32 kHz
        0x00, 0xf4, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, // 128,000 bytes a second, 4 bytes a block, 16 bits
        0x64, 0x61, 0x74, 0x61, 0x0c, 0x00, 0x00, 0x00, // data, 12 bytes
        0x01, 0x00, 0x00, 0x01, 0xfe, 0xff, 0x00, 0x80, // the first two frames
        0x03, 0x00, 0xff, 0x7f,                         // the third frame
        0x73, 0x6d, 0x70, 0x6c, 0x3c, 0x00, 0x00, 0x00, // smpl, 60 bytes
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // manufacturer, product
        0x12, 0x7a, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, // 31,250 ns a sample, unity note 60
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // pitch fraction, SMPTE format
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // SMPTE offset, one loop
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // no sampler data, the loop's identifier
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, // forward, from sample 1
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // to sample 2, no fraction
        0x00, 0x00, 0x00, 0x00,                         // plays without end
    };

    const std::vector<uint8_t> file = seqlib::WavFile(pcm, 32000);

    SEQLIB_CHECK(file == kExpected);
}

// Three channels take WAVE_FORMAT_EXTENSIBLE, with no speaker positions and the PCM subtype.
SEQLIB_TEST(wav, Extensible)
{
    seqlib::Pcm pcm;
    pcm.channels = {{100}, {-100}, {0}};
    const std::vector<uint8_t> kExpected = {
        0x52, 0x49, 0x46, 0x46, 0x42, 0x00, 0x00, 0x00, // RIFF, 66 bytes
        0x57, 0x41, 0x56, 0x45,                         // WAVE
        0x66, 0x6d, 0x74, 0x20, 0x28, 0x00, 0x00, 0x00, // fmt, 40 bytes
        0xfe, 0xff, 0x03, 0x00, 0x80, 0xbb, 0x00, 0x00, // extensible, 3 channels, 48 kHz
        0x00, 0x65, 0x04, 0x00, 0x06, 0x00, 0x10, 0x00, // 288,000 bytes a second, 6 bytes a block, 16 bits
        0x16, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // 22 bytes more, 16 valid bits, no speakers
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, // KSDATAFORMAT_SUBTYPE_PCM: 00000001-0000-0010
        0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71, // 8000-00AA00389B71
        0x64, 0x61, 0x74, 0x61, 0x06, 0x00, 0x00, 0x00, // data, 6 bytes
        0x64, 0x00, 0x9c, 0xff, 0x00, 0x00,             // one frame
    };

    const std::vector<uint8_t> file = seqlib::WavFile(pcm, 48000);

    SEQLIB_CHECK(file == kExpected);
}

// No channels, channels of different lengths and loops outside the samples throw.
SEQLIB_TEST(wav, Refusals)
{
    const auto refused = [](const seqlib::Pcm& pcm)
    {
        return !seqlib::testing::ErrorOf<std::invalid_argument>([&] { seqlib::WavFile(pcm, 32000); }).empty();
    };

    seqlib::Pcm none;
    seqlib::Pcm uneven;
    uneven.channels = {{1, 2}, {3}};
    seqlib::Pcm empty_loop;
    empty_loop.channels = {{1, 2, 3}};
    empty_loop.loop = true;
    empty_loop.loop_start = 2;
    empty_loop.loop_end = 2;
    seqlib::Pcm long_loop = empty_loop;
    long_loop.loop_end = 4;
    seqlib::Pcm whole_loop = empty_loop;
    whole_loop.loop_start = 0;
    whole_loop.loop_end = 3;

    SEQLIB_CHECK(refused(none));
    SEQLIB_CHECK(refused(uneven));
    SEQLIB_CHECK(refused(empty_loop));
    SEQLIB_CHECK(refused(long_loop));
    SEQLIB_CHECK(!refused(whole_loop));
}

// Appends `value` to `out` in `bytes` little-endian bytes. Bytes past the value's eight are 0.
void Append(std::vector<uint8_t>& out, uint64_t value, std::size_t bytes)
{
    for (std::size_t i = 0; i < bytes; i++)
    {
        out.push_back(i < 8 ? static_cast<uint8_t>(value >> (8 * i)) : 0);
    }
}

// A RIFF WAVE file of `chunks`, each an ID and its data, with a pad byte after a chunk of odd size, and the RIFF
// chunk's size that they give.
std::vector<uint8_t> Riff(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& chunks)
{
    std::vector<uint8_t> body = {'W', 'A', 'V', 'E'};
    for (const auto& [id, data] : chunks)
    {
        body.insert(body.end(), id.begin(), id.end());
        Append(body, data.size(), 4);
        body.insert(body.end(), data.begin(), data.end());
        if (data.size() % 2 != 0)
        {
            body.push_back(0);
        }
    }

    std::vector<uint8_t> file = {'R', 'I', 'F', 'F'};
    Append(file, body.size(), 4);
    file.insert(file.end(), body.begin(), body.end());

    return file;
}

// The data of a fmt chunk: the format tag, the channels, the rate, the block's size and the bits per sample.
std::vector<uint8_t> Format(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t block, uint16_t bits)
{
    std::vector<uint8_t> format;
    Append(format, tag, 2);
    Append(format, channels, 2);
    Append(format, rate, 4);
    Append(format, uint64_t{rate} * block, 4);
    Append(format, block, 2);
    Append(format, bits, 2);

    return format;
}

// The data of a WAVE_FORMAT_EXTENSIBLE fmt chunk whose subtype's GUID starts with `subtype`.
std::vector<uint8_t> Extensible(uint16_t subtype, uint16_t channels, uint32_t rate, uint16_t block, uint16_t bits)
{
    std::vector<uint8_t> format = Format(0xFFFE, channels, rate, block, bits);
    Append(format, 22, 2);
    Append(format, bits, 2);
    Append(format, 0, 4);
    Append(format, subtype, 2);
    format.insert(format.end(), {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71});

    return format;
}

// The data of a smpl chunk with `loops`, each a type, a first sample, a last sample and a number of plays.
std::vector<uint8_t> Sampler(const std::vector<std::array<uint32_t, 4>>& loops)
{
    std::vector<uint8_t> sampler;
    Append(sampler, 0, 28);
    Append(sampler, loops.size(), 4);
    Append(sampler, 0, 4);
    for (const std::array<uint32_t, 4>& loop : loops)
    {
        Append(sampler, 0, 4);
        Append(sampler, loop[0], 4);
        Append(sampler, loop[1], 4);
        Append(sampler, loop[2], 4);
        Append(sampler, 0, 4);
        Append(sampler, loop[3], 4);
    }

    return sampler;
}

// The bytes of `values`, each a float or a double.
template <class Real>
std::vector<uint8_t> Reals(const std::vector<Real>& values)
{
    std::vector<uint8_t> bytes;
    for (const Real value : values)
    {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(Real));
        Append(bytes, bits, sizeof(Real));
    }

    return bytes;
}

// The one channel that `file` holds, or nothing when it holds several.
std::vector<int16_t> Mono(const std::vector<uint8_t>& file)
{
    const seqlib::WavContents contents = seqlib::ReadWav(file);

    return contents.pcm.channels.size() == 1 ? contents.pcm.channels[0] : std::vector<int16_t>{};
}

// The files that WavFile writes read back with their channels, their rate and their loop.
SEQLIB_TEST(wav, ReadWrittenFiles)
{
    seqlib::Pcm mono;
    mono.channels = {Ramp()};
    seqlib::Pcm stereo;
    stereo.channels = {{1, 2, 3, 4}, {-1, -2, -3, -4}};
    stereo.loop = true;
    stereo.loop_start = 1;
    stereo.loop_end = 4;
    seqlib::Pcm three = stereo;
    three.channels.push_back({7, 8, 9, 10});

    for (const seqlib::Pcm& pcm : {mono, stereo, three})
    {
        const seqlib::WavContents contents = seqlib::ReadWav(seqlib::WavFile(pcm, 44100));

        SEQLIB_CHECK(contents.pcm.channels == pcm.channels && contents.rate == 44100);
        SEQLIB_CHECK(contents.pcm.loop == pcm.loop && contents.pcm.loop_start == pcm.loop_start);
        SEQLIB_CHECK(contents.pcm.loop_end == pcm.loop_end && contents.loops == (pcm.loop ? 1u : 0u));
        SEQLIB_CHECK(contents.bits == 16 && !contents.floating && contents.loop_forward && contents.loop_endless);
        SEQLIB_CHECK(contents.dropped == 0);
    }
}

// Samples of 8, 12, 24 and 32 bits, and of 32-bit and 64-bit floating point, become 16 bits. An 8-bit sample is
// unsigned, and a deeper one rounds half up. Floating point scales by 32,768, rounds half away from zero and clamps.
// The extensible form's subtypes give the same samples, channel by channel.
SEQLIB_TEST(wav, ReadSampleFormats)
{
    const std::vector<uint8_t> eight = Riff({{"fmt ", Format(1, 1, 8000, 1, 8)}, {"data", {0, 128, 255, 1}}});
    const std::vector<uint8_t> twelve = Riff({{"fmt ", Format(1, 1, 8000, 2, 12)}, {"data", {0x30, 0x12}}});
    const std::vector<uint8_t> kDeepSamples = {0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00,
                                               0x7F, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF};
    const std::vector<uint8_t> deep = Riff({{"fmt ", Format(1, 1, 8000, 3, 24)}, {"data", kDeepSamples}});
    std::vector<uint8_t> words;
    for (const uint32_t word : {0x7FFFFFFFu, 0x80000000u, 0x00008000u, 0x00007FFFu, 0x12345678u})
    {
        Append(words, word, 4);
    }
    const std::vector<uint8_t> wide = Riff({{"fmt ", Format(1, 1, 8000, 4, 32)}, {"data", words}});
    const float kNan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<float> kSingles = {0.5f, -1.0f, 1.0f, 2.0f, kNan, 1.0f / 65536.0f, -1.0f / 65536.0f};
    const std::vector<uint8_t> single = Riff({{"fmt ", Format(3, 1, 8000, 4, 32)}, {"data", Reals(kSingles)}});
    const std::vector<uint8_t> twice =
        Riff({{"fmt ", Format(3, 1, 8000, 8, 64)}, {"data", Reals(std::vector<double>{0.25, -0.5})}});
    const std::vector<uint8_t> kFrames = {0x00, 0x01, 0x00, 0x80, 0xFF, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x80};
    const std::vector<uint8_t> extensible = Riff({{"fmt ", Extensible(1, 2, 8000, 6, 24)}, {"data", kFrames}});
    const std::vector<uint8_t> extensible_float =
        Riff({{"fmt ", Extensible(3, 1, 8000, 4, 32)}, {"data", Reals(std::vector<float>{-0.25f})}});

    const seqlib::WavContents stereo = seqlib::ReadWav(extensible);

    SEQLIB_CHECK((Mono(eight) == std::vector<int16_t>{-32768, 0, 32512, -32512}));
    SEQLIB_CHECK((Mono(twelve) == std::vector<int16_t>{0x1230}));
    SEQLIB_CHECK((Mono(deep) == std::vector<int16_t>{32767, -32768, 1, 0, 0, -1}));
    SEQLIB_CHECK((Mono(wide) == std::vector<int16_t>{32767, -32768, 1, 0, 0x1234}));
    SEQLIB_CHECK((Mono(single) == std::vector<int16_t>{16384, -32768, 32767, 32767, 0, 1, -1}));
    SEQLIB_CHECK((Mono(twice) == std::vector<int16_t>{8192, -16384}));
    SEQLIB_CHECK((stereo.pcm.channels == std::vector<std::vector<int16_t>>{{1, 2}, {0, -32768}}));
    SEQLIB_CHECK(stereo.bits == 24 && !stereo.floating && seqlib::ReadWav(single).floating);
    SEQLIB_CHECK((Mono(extensible_float) == std::vector<int16_t>{-8192}));
}

// The chunks end at the RIFF chunk's end or at the file's end. A size too small for the form type counts as unknown.
// Other chunks are skipped, with their pad bytes, and only the first chunk of each kind counts. The last chunk can lack
// its pad byte. A data chunk can be empty. Bytes that hold no whole frame stay out.
SEQLIB_TEST(wav, ReadChunks)
{
    const std::vector<uint8_t> file =
        Riff({{"LIST", {1, 2, 3}}, {"fmt ", Format(1, 1, 8000, 2, 16)}, {"data", {1, 0, 2, 0}}});
    std::vector<uint8_t> large = file;
    large[4] = 0xFF;
    large[5] = 0xFF;
    std::vector<uint8_t> unknown = file;
    unknown[4] = 0;
    unknown[5] = 0;
    std::vector<uint8_t> trailing = file;
    trailing.insert(trailing.end(), {'J', 'U', 'N', 'K', 0xFF, 0x00, 0x00, 0x00});
    const std::vector<uint8_t> twice = Riff({{"fmt ", Format(1, 1, 8000, 2, 16)},
                                             {"data", {1, 0, 2, 0}},
                                             {"fmt ", Format(1, 2, 8000, 4, 16)},
                                             {"data", {9, 0}}});
    std::vector<uint8_t> unpadded = Riff({{"fmt ", Format(1, 1, 8000, 1, 8)}, {"data", {128, 129, 130}}});
    unpadded.pop_back();
    const std::vector<uint8_t> empty = Riff({{"fmt ", Format(1, 1, 8000, 2, 16)}, {"data", {}}});
    const std::vector<uint8_t> partial = Riff({{"fmt ", Format(1, 2, 8000, 4, 16)}, {"data", {1, 0, 2, 0, 3, 0}}});

    const seqlib::WavContents silent = seqlib::ReadWav(empty);
    const seqlib::WavContents uneven = seqlib::ReadWav(partial);

    for (const std::vector<uint8_t>& bytes : {file, large, unknown, trailing, twice})
    {
        SEQLIB_CHECK((Mono(bytes) == std::vector<int16_t>{1, 2}));
    }
    SEQLIB_CHECK((Mono(unpadded) == std::vector<int16_t>{0, 256, 512}));
    SEQLIB_CHECK(silent.pcm.channels.size() == 1 && silent.pcm.channels[0].empty() && silent.dropped == 0);
    SEQLIB_CHECK((uneven.pcm.channels == std::vector<std::vector<int16_t>>{{1}, {2}}) && uneven.dropped == 2);
}

// A file that isn't a WAV file, a chunk past the end, a missing or short fmt chunk, a missing data chunk, another
// format, no channels, a rate of 0, a block that doesn't fit the format, and an extensible form that is short or has
// another subtype are refused. A short fmt chunk is refused even when the bytes after it would complete it.
SEQLIB_TEST(wav, ReadRefusals)
{
    const auto refused = [](const std::vector<uint8_t>& bytes)
    {
        return !seqlib::testing::ErrorOf<seqlib::ReadError>([&] { seqlib::ReadWav(bytes); }).empty();
    };

    const std::vector<uint8_t> kData = {0, 0, 0, 0};
    std::vector<uint8_t> avi = Riff({{"fmt ", Format(1, 1, 8000, 2, 16)}, {"data", kData}});
    std::memcpy(avi.data() + 8, "AVI ", 4);
    std::vector<uint8_t> past = Riff({{"fmt ", Format(1, 1, 8000, 2, 16)}, {"data", kData}});
    past.resize(past.size() - 1);
    std::vector<uint8_t> short_format = Format(1, 1, 8000, 2, 16);
    short_format.resize(14);
    const std::string kCompleting = {'\x10', '\x00', 'x', 'x'};
    std::vector<uint8_t> other_guid = Extensible(1, 1, 8000, 2, 16);
    other_guid[26] = 0x21;
    other_guid[27] = 0x07;

    SEQLIB_CHECK(refused({}) && refused(avi) && refused(past));
    SEQLIB_CHECK(refused(Riff({{"data", kData}})) && refused(Riff({{"fmt ", short_format}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", short_format}, {kCompleting, {}}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(1, 1, 8000, 2, 16)}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(2, 1, 8000, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(1, 0, 8000, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(1, 1, 0, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(1, 2, 8000, 3, 8)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(1, 1, 8000, 2, 24)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(3, 1, 8000, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(3, 1, 8000, 4, 24)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Format(0xFFFE, 1, 8000, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", Extensible(2, 1, 8000, 2, 16)}, {"data", kData}})));
    SEQLIB_CHECK(refused(Riff({{"fmt ", other_guid}, {"data", kData}})));
    SEQLIB_CHECK(!refused(Riff({{"fmt ", Format(1, 1, 8000, 2, 16)}, {kCompleting, {}}, {"data", kData}})));
}

// The smpl chunk's first loop becomes the loop, with its type and plays. A loop past the samples stays out. A chunk
// without loops gives none, and so does a chunk too short for its header or for its first loop.
SEQLIB_TEST(wav, ReadLoops)
{
    const auto read = [](const std::vector<uint8_t>& sampler)
    {
        return seqlib::ReadWav(
            Riff({{"fmt ", Format(1, 1, 8000, 1, 8)}, {"smpl", sampler}, {"data", std::vector<uint8_t>(16, 128)}}));
    };

    std::vector<uint8_t> short_header(32, 0);
    short_header[28] = 1;
    std::vector<uint8_t> short_loop = Sampler({});
    short_loop[28] = 1;

    const seqlib::WavContents forward = read(Sampler({{0, 2, 5, 0}}));
    const seqlib::WavContents bouncing = read(Sampler({{1, 0, 15, 3}, {0, 1, 2, 0}}));
    const seqlib::WavContents past = read(Sampler({{0, 2, 16, 0}}));
    const seqlib::WavContents none = read(Sampler({}));
    const seqlib::WavContents headless = read(short_header);
    const seqlib::WavContents loopless = read(short_loop);

    SEQLIB_CHECK(forward.pcm.loop && forward.pcm.loop_start == 2 && forward.pcm.loop_end == 6);
    SEQLIB_CHECK(forward.loops == 1 && forward.loop_forward && forward.loop_endless);
    SEQLIB_CHECK(bouncing.pcm.loop && bouncing.pcm.loop_start == 0 && bouncing.pcm.loop_end == 16);
    SEQLIB_CHECK(bouncing.loops == 2 && !bouncing.loop_forward && !bouncing.loop_endless);
    SEQLIB_CHECK(!past.pcm.loop && past.loops == 1);
    SEQLIB_CHECK(!none.pcm.loop && none.loops == 0 && !headless.pcm.loop && headless.loops == 0);
    SEQLIB_CHECK(!loopless.pcm.loop && loopless.loops == 1 && loopless.loop_forward && loopless.loop_endless);
}

} // namespace
