// SPDX-License-Identifier: MIT

// Golden tests for seqlib/loop.h. The expected results are each channel's length and its FNV-1a hash. They're fixed: a
// change to them is a change of seqlib's output.

#include "seqlib/loop.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "test.h"

namespace
{

struct LoopCase
{
    std::string name;
    std::vector<std::vector<int16_t>> channels;
    bool loop;
    uint32_t loop_start, loop_end;
};

std::vector<int16_t> Ramp(int n, int step, int offset)
{
    std::vector<int16_t> samples;
    for (int i = 0; i < n; i++)
    {
        samples.push_back(static_cast<int16_t>(offset + i * step));
    }

    return samples;
}

// Loops shorter and longer than the minimums of PrepareForSoundFont (8 before, 32 long) and PrepareForSfz (64).
std::vector<LoopCase> Cases()
{
    return {
        {"no loop", {Ramp(10, 3, 1)}, false, 0, 0},
        {"empty", {{}}, false, 0, 0},
        {"one-sample loop", {{7}}, true, 0, 1},
        {"short loop", {Ramp(8, 5, -20)}, true, 3, 8},
        {"loop of 31 from 9", {Ramp(40, 11, 100)}, true, 9, 40},
        {"loop of 32 from 8", {Ramp(40, 13, -300)}, true, 8, 40},
        {"loop of 33 from 7", {Ramp(40, 17, 5)}, true, 7, 40},
        {"loop of 40 from 10", {Ramp(50, -7, 1000)}, true, 10, 50},
        {"loop of 63 from 0", {Ramp(63, 9, -9)}, true, 0, 63},
        {"loop of 64 from 0", {Ramp(64, 9, -9)}, true, 0, 64},
        {"long loop", {Ramp(320, 101, -16000)}, true, 100, 300},
        {"stereo", {Ramp(12, 3, 0), Ramp(12, -3, 0)}, true, 4, 12},
        {"stereo, no loop", {Ramp(5, 1, 0), Ramp(5, 2, 0)}, false, 0, 0},
    };
}

struct Channel
{
    std::size_t size;
    uint64_t hash;
};

struct Expected
{
    bool loop;
    uint32_t loop_start, loop_end;
    std::vector<Channel> channels;
};

struct PrepareCase
{
    const char* name;
    bool sfz;           // PrepareForSfz, or PrepareForSoundFont
    uint32_t crossfade; // (rate * 4 + 999) / 1000 for 8000, 22050, 32000, 32728 and 48000 Hz
    Expected expected;
};

const PrepareCase kPrepared[] = {
    {"no loop", false, 0, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", true, 32, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", true, 89, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", true, 128, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", true, 131, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", true, 192, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"empty", false, 0, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", true, 32, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", true, 89, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", true, 128, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", true, 131, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", true, 192, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"one-sample loop", false, 0, {true, 8, 40, {{72, 0xcc08702e728ed185ull}}}},
    {"one-sample loop", true, 32, {true, 64, 128, {{192, 0x24460bfca3efd425ull}}}},
    {"one-sample loop", true, 89, {true, 89, 153, {{217, 0x2fed2cf8d8cb3692ull}}}},
    {"one-sample loop", true, 128, {true, 128, 192, {{256, 0x732c1aaebfcb0f25ull}}}},
    {"one-sample loop", true, 131, {true, 131, 195, {{259, 0x64eab3287d8536caull}}}},
    {"one-sample loop", true, 192, {true, 192, 256, {{320, 0xc6c1dff16d9c4a25ull}}}},
    {"short loop", false, 0, {true, 8, 43, {{75, 0xf3b18810e9830b1dull}}}},
    {"short loop", true, 32, {true, 68, 133, {{197, 0xaa8aca01e5a32632ull}}}},
    {"short loop", true, 89, {true, 93, 158, {{222, 0x870e5bde6c23c210ull}}}},
    {"short loop", true, 128, {true, 133, 198, {{262, 0x9d3d889b359cbdc0ull}}}},
    {"short loop", true, 131, {true, 138, 203, {{267, 0x37103b181180b786ull}}}},
    {"short loop", true, 192, {true, 198, 263, {{327, 0xf0d831d94fe111deull}}}},
    {"loop of 31 from 9", false, 0, {true, 9, 71, {{103, 0x197522cb24feaa3dull}}}},
    {"loop of 31 from 9", true, 32, {true, 102, 195, {{259, 0x507f6675f98f4c8cull}}}},
    {"loop of 31 from 9", true, 89, {true, 102, 195, {{259, 0x507f6675f98f4c8cull}}}},
    {"loop of 31 from 9", true, 128, {true, 164, 257, {{321, 0xbb089f4a8dd5f9d4ull}}}},
    {"loop of 31 from 9", true, 131, {true, 164, 257, {{321, 0xbb089f4a8dd5f9d4ull}}}},
    {"loop of 31 from 9", true, 192, {true, 226, 319, {{383, 0x057720f7afe2a47cull}}}},
    {"loop of 32 from 8", false, 0, {true, 8, 40, {{72, 0x1a04d955f9622ea9ull}}}},
    {"loop of 32 from 8", true, 32, {true, 72, 136, {{200, 0x146a0936660406a9ull}}}},
    {"loop of 32 from 8", true, 89, {true, 104, 168, {{232, 0x7cd3a1dc311f3e29ull}}}},
    {"loop of 32 from 8", true, 128, {true, 136, 200, {{264, 0xcafd1ad8a018f2a9ull}}}},
    {"loop of 32 from 8", true, 131, {true, 168, 232, {{296, 0x8d9f06f0c1202a29ull}}}},
    {"loop of 32 from 8", true, 192, {true, 200, 264, {{328, 0xd1e6dab67205dea9ull}}}},
    {"loop of 33 from 7", false, 0, {true, 40, 73, {{105, 0x981463c1e407d050ull}}}},
    {"loop of 33 from 7", true, 32, {true, 73, 139, {{203, 0x5760f355b2896e26ull}}}},
    {"loop of 33 from 7", true, 89, {true, 106, 172, {{236, 0x960661723515e23dull}}}},
    {"loop of 33 from 7", true, 128, {true, 139, 205, {{269, 0x22d7ac70bb03878eull}}}},
    {"loop of 33 from 7", true, 131, {true, 139, 205, {{269, 0x22d7ac70bb03878eull}}}},
    {"loop of 33 from 7", true, 192, {true, 205, 271, {{335, 0x70de367b39b67c76ull}}}},
    {"loop of 40 from 10", false, 0, {true, 10, 50, {{82, 0x0c3d0a8708684994ull}}}},
    {"loop of 40 from 10", true, 32, {true, 90, 170, {{234, 0x0dbf10a82611ac2cull}}}},
    {"loop of 40 from 10", true, 89, {true, 130, 210, {{274, 0x3d82846b5d44960cull}}}},
    {"loop of 40 from 10", true, 128, {true, 170, 250, {{314, 0xbbf03b9be379956cull}}}},
    {"loop of 40 from 10", true, 131, {true, 170, 250, {{314, 0xbbf03b9be379956cull}}}},
    {"loop of 40 from 10", true, 192, {true, 210, 290, {{354, 0xdd4a049c8293efccull}}}},
    {"loop of 63 from 0", false, 0, {true, 63, 126, {{158, 0x5d67903b28c31584ull}}}},
    {"loop of 63 from 0", true, 32, {true, 126, 252, {{316, 0xe8b09a4fe185372eull}}}},
    {"loop of 63 from 0", true, 89, {true, 126, 252, {{316, 0xe8b09a4fe185372eull}}}},
    {"loop of 63 from 0", true, 128, {true, 189, 315, {{379, 0xcba4172ad776a457ull}}}},
    {"loop of 63 from 0", true, 131, {true, 189, 315, {{379, 0xcba4172ad776a457ull}}}},
    {"loop of 63 from 0", true, 192, {true, 252, 378, {{442, 0xd879a67a54d04b92ull}}}},
    {"loop of 64 from 0", false, 0, {true, 64, 128, {{160, 0x9e4fb02b15f67c60ull}}}},
    {"loop of 64 from 0", true, 32, {true, 64, 128, {{192, 0xf9bedbff0edac158ull}}}},
    {"loop of 64 from 0", true, 89, {true, 128, 192, {{256, 0x8e7e7f1f010dc955ull}}}},
    {"loop of 64 from 0", true, 128, {true, 128, 192, {{256, 0x8e7e7f1f010dc955ull}}}},
    {"loop of 64 from 0", true, 131, {true, 192, 256, {{320, 0x9d91c7a654cfcb40ull}}}},
    {"loop of 64 from 0", true, 192, {true, 192, 256, {{320, 0x9d91c7a654cfcb40ull}}}},
    {"long loop", false, 0, {true, 100, 300, {{332, 0xbdcfc24a24eac7bfull}}}},
    {"long loop", true, 32, {true, 300, 500, {{564, 0x2a33ee1cc2e580efull}}}},
    {"long loop", true, 89, {true, 300, 500, {{564, 0x2a33ee1cc2e580efull}}}},
    {"long loop", true, 128, {true, 300, 500, {{564, 0x2a33ee1cc2e580efull}}}},
    {"long loop", true, 131, {true, 300, 500, {{564, 0x2a33ee1cc2e580efull}}}},
    {"long loop", true, 192, {true, 300, 500, {{564, 0x2a33ee1cc2e580efull}}}},
    {"stereo", false, 0, {true, 12, 44, {{76, 0x9407aa1f009065f1ull}, {76, 0x214511ff3183a62cull}}}},
    {"stereo", true, 32, {true, 68, 132, {{196, 0xcf270c236787d5f9ull}, {196, 0xe8ac34bb0295d54cull}}}},
    {"stereo", true, 89, {true, 100, 164, {{228, 0xc2c1cbe558889f39ull}, {228, 0xb57b465fb97e854cull}}}},
    {"stereo", true, 128, {true, 132, 196, {{260, 0x547804fa7842a879ull}, {260, 0xfa625a281d17354cull}}}},
    {"stereo", true, 131, {true, 140, 204, {{268, 0x9951772f8c23bd71ull}, {268, 0x8ac6bdbafccfc62cull}}}},
    {"stereo", true, 192, {true, 196, 260, {{324, 0xca927dfa28e27af9ull}, {324, 0x5eca0a96aa58954cull}}}},
    {"stereo, no loop", false, 0, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", true, 32, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", true, 89, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", true, 128, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", true, 131, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", true, 192, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
};

struct UnrollCase
{
    const char* name;
    uint32_t min_start, min_before, min_length, after;
    Expected expected;
};

const UnrollCase kUnrolled[] = {
    {"no loop", 0, 0, 1, 0, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", 5, 3, 7, 2, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", 100, 0, 1, 1, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"no loop", 0, 200, 10, 300, {false, 0, 0, {{10, 0x16776594c6135e80ull}}}},
    {"empty", 0, 0, 1, 0, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", 5, 3, 7, 2, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", 100, 0, 1, 1, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"empty", 0, 200, 10, 300, {false, 0, 0, {{1, 0x08328807b4eb6fedull}}}},
    {"one-sample loop", 0, 0, 1, 0, {true, 0, 1, {{1, 0x08285607b4e2c672ull}}}},
    {"one-sample loop", 5, 3, 7, 2, {true, 5, 12, {{14, 0x4692bc1a44da0ecdull}}}},
    {"one-sample loop", 100, 0, 1, 1, {true, 100, 101, {{102, 0x75fa80effb43f9edull}}}},
    {"one-sample loop", 0, 200, 10, 300, {true, 200, 210, {{510, 0x8f412766305cb40dull}}}},
    {"short loop", 0, 0, 1, 0, {true, 3, 8, {{8, 0x7f9a4af7c5f0f7f9ull}}}},
    {"short loop", 5, 3, 7, 2, {true, 8, 18, {{20, 0x627a67d13007249full}}}},
    {"short loop", 100, 0, 1, 1, {true, 103, 108, {{109, 0x6eaeeda5df5069f3ull}}}},
    {"short loop", 0, 200, 10, 300, {true, 203, 213, {{513, 0x29d82a1402d9bd53ull}}}},
    {"loop of 31 from 9", 0, 0, 1, 0, {true, 9, 40, {{40, 0xc40ee683c7f9d69aull}}}},
    {"loop of 31 from 9", 5, 3, 7, 2, {true, 40, 71, {{73, 0x9fff2ddfec2f3534ull}}}},
    {"loop of 31 from 9", 100, 0, 1, 1, {true, 102, 133, {{134, 0xa894d64316be88aeull}}}},
    {"loop of 31 from 9", 0, 200, 10, 300, {true, 226, 257, {{557, 0xcae528f82491edcaull}}}},
    {"loop of 32 from 8", 0, 0, 1, 0, {true, 8, 40, {{40, 0xb303beea402c7a29ull}}}},
    {"loop of 32 from 8", 5, 3, 7, 2, {true, 40, 72, {{74, 0x69a8247b22f1f32cull}}}},
    {"loop of 32 from 8", 100, 0, 1, 1, {true, 104, 136, {{137, 0x8c3c55deeca07270ull}}}},
    {"loop of 32 from 8", 0, 200, 10, 300, {true, 232, 264, {{564, 0xc82573ce1be7c6edull}}}},
    {"loop of 33 from 7", 0, 0, 1, 0, {true, 7, 40, {{40, 0x89e77dac9da50f5aull}}}},
    {"loop of 33 from 7", 5, 3, 7, 2, {true, 40, 73, {{75, 0x87bc515cebbfe808ull}}}},
    {"loop of 33 from 7", 100, 0, 1, 1, {true, 106, 139, {{140, 0xe214cc3092a0e0ddull}}}},
    {"loop of 33 from 7", 0, 200, 10, 300, {true, 238, 271, {{571, 0xefba8d7638bdd615ull}}}},
    {"loop of 40 from 10", 0, 0, 1, 0, {true, 10, 50, {{50, 0xebd06454beb50a0cull}}}},
    {"loop of 40 from 10", 5, 3, 7, 2, {true, 50, 90, {{92, 0x2262ffcbda3d7fe9ull}}}},
    {"loop of 40 from 10", 100, 0, 1, 1, {true, 130, 170, {{171, 0xb8619173f7759a8bull}}}},
    {"loop of 40 from 10", 0, 200, 10, 300, {true, 210, 250, {{550, 0x4fe074e84d128190ull}}}},
    {"loop of 63 from 0", 0, 0, 1, 0, {true, 0, 63, {{63, 0x76115499de84f688ull}}}},
    {"loop of 63 from 0", 5, 3, 7, 2, {true, 63, 126, {{128, 0xeed578e9dc760b37ull}}}},
    {"loop of 63 from 0", 100, 0, 1, 1, {true, 126, 189, {{190, 0x86348deeafbcf1daull}}}},
    {"loop of 63 from 0", 0, 200, 10, 300, {true, 252, 315, {{615, 0xd660d1a07fabc40dull}}}},
    {"loop of 64 from 0", 0, 0, 1, 0, {true, 0, 64, {{64, 0xce36dbfe774caf30ull}}}},
    {"loop of 64 from 0", 5, 3, 7, 2, {true, 64, 128, {{130, 0xf3ab175a6bae190bull}}}},
    {"loop of 64 from 0", 100, 0, 1, 1, {true, 128, 192, {{193, 0x62db82bade25a746ull}}}},
    {"loop of 64 from 0", 0, 200, 10, 300, {true, 256, 320, {{620, 0x9167ec3d7d4ea64dull}}}},
    {"long loop", 0, 0, 1, 0, {true, 100, 300, {{300, 0x35bb653409def4b2ull}}}},
    {"long loop", 5, 3, 7, 2, {true, 300, 500, {{502, 0xf4e4ebfeebd36a5aull}}}},
    {"long loop", 100, 0, 1, 1, {true, 100, 300, {{301, 0x424d79d42fd9c22eull}}}},
    {"long loop", 0, 200, 10, 300, {true, 300, 500, {{800, 0x9ffdda7da05aaa49ull}}}},
    {"stereo", 0, 0, 1, 0, {true, 4, 12, {{12, 0xd9dfcdc3f053f371ull}, {12, 0xd9e04ed1ddea462cull}}}},
    {"stereo", 5, 3, 7, 2, {true, 12, 20, {{22, 0x7f6f938f5583f822ull}, {22, 0x78443937a22d5d89ull}}}},
    {"stereo", 100, 0, 1, 1, {true, 100, 108, {{109, 0xa7f368ed9bfd77c5ull}, {109, 0x71cbc0c570d906b5ull}}}},
    {"stereo", 0, 200, 10, 300, {true, 204, 220, {{520, 0x6fa15c3a48dc1bcdull}, {520, 0x951a9a290d1bb260ull}}}},
    {"stereo, no loop", 0, 0, 1, 0, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", 5, 3, 7, 2, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", 100, 0, 1, 1, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
    {"stereo, no loop", 0, 200, 10, 300, {false, 0, 0, {{5, 0x63fcc5209ba9e8a9ull}, {5, 0x757f8d232b90fd55ull}}}},
};

uint64_t Fnv1a(std::span<const int16_t> samples)
{
    uint64_t hash = 14695981039346656037ull;
    for (int16_t v : samples)
    {
        for (int i = 0; i < 2; i++)
        {
            hash ^= static_cast<uint8_t>(static_cast<uint16_t>(v) >> (8 * i));
            hash *= 1099511628211ull;
        }
    }

    return hash;
}

seqlib::Pcm ToPcm(const LoopCase& c)
{
    seqlib::Pcm pcm;
    pcm.loop = c.loop;
    pcm.loop_start = c.loop_start;
    pcm.loop_end = c.loop_end;
    pcm.channels = c.channels;

    return pcm;
}

const LoopCase& Find(const std::vector<LoopCase>& cases, const std::string& name)
{
    for (const LoopCase& c : cases)
    {
        if (c.name == name)
        {
            return c;
        }
    }

    return cases.front();
}

bool Matches(const seqlib::PreparedWave& w, const Expected& expected)
{
    if (w.loop != expected.loop || w.loop_start != expected.loop_start || w.loop_end != expected.loop_end ||
        w.channels.size() != expected.channels.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < w.channels.size(); i++)
    {
        if (w.channels[i].size() != expected.channels[i].size || Fnv1a(w.channels[i]) != expected.channels[i].hash)
        {
            return false;
        }
    }

    return true;
}

SEQLIB_TEST(loop, PreparedChannels)
{
    const std::vector<LoopCase> cases = Cases();
    for (const PrepareCase& c : kPrepared)
    {
        const seqlib::Pcm pcm = ToPcm(Find(cases, c.name));
        seqlib::test::SetContext(std::string(c.name) + (c.sfz ? ", sfz " + std::to_string(c.crossfade) : ", sf2"));
        SEQLIB_CHECK(
            Matches(c.sfz ? seqlib::PrepareForSfz(pcm, c.crossfade) : seqlib::PrepareForSoundFont(pcm), c.expected));
    }
}

SEQLIB_TEST(loop, UnrolledLoops)
{
    const std::vector<LoopCase> cases = Cases();
    for (const UnrollCase& c : kUnrolled)
    {
        const seqlib::Pcm pcm = ToPcm(Find(cases, c.name));
        seqlib::test::SetContext(c.name);
        SEQLIB_CHECK(Matches(seqlib::UnrollLoop(pcm, c.min_start, c.min_before, c.min_length, c.after), c.expected));
    }
}

SEQLIB_TEST(loop, ShortLoopUnrollsAsExpected)
{
    seqlib::Pcm pcm;
    pcm.loop = true;
    pcm.loop_start = 1;
    pcm.loop_end = 3;
    pcm.channels = {{10, 20, 30}};

    const seqlib::PreparedWave w = seqlib::UnrollLoop(pcm, 4, 0, 5, 3);

    // Two repeats move the loop to sample 5. Three repeats reach the length of 5. Then 3 samples from the loop start.
    const std::vector<int16_t> kExpected = {10, 20, 30, 20, 30, 20, 30, 20, 30, 20, 30, 20, 30, 20};
    SEQLIB_CHECK(w.loop && w.loop_start == 5 && w.loop_end == 11);
    SEQLIB_CHECK(w.channels.size() == 1 && w.channels[0] == kExpected);
}

} // namespace
