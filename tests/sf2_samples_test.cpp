// SPDX-License-Identifier: MIT

// Tests of seqlib/sf2/samples.h: the samples that a prepared wave becomes, their names, loops and links, and the limit
// on a SoundFont's samples.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "seqlib/sf2/samples.h"
#include "test.h"

namespace
{

namespace sf2 = seqlib::sf2;

seqlib::PreparedWave Wave(std::size_t channels, std::size_t length, bool loop)
{
    seqlib::PreparedWave w;
    for (std::size_t c = 0; c < channels; c++)
    {
        w.channels.emplace_back(length, static_cast<int16_t>(c + 1));
    }
    w.loop = loop;
    if (loop)
    {
        w.loop_start = 8;
        w.loop_end = 40;
    }

    return w;
}

SEQLIB_TEST(sf2_samples, MonoWave)
{
    sf2::SoundFont sf;

    const std::vector<uint16_t> ids = sf2::AddSamples(sf, Wave(1, 3, false), "tone", 32000, 60);

    SEQLIB_REQUIRE(ids == std::vector<uint16_t>{0} && sf.samples.size() == 1);
    const sf2::Sample& s = sf.samples[0];
    SEQLIB_CHECK(s.name == "tone" && s.type == sf2::kMono && s.link == 0);
    SEQLIB_CHECK(s.sample_rate == 32000 && s.original_key == 60 && s.data == std::vector<int16_t>(3, 1));
    // A wave that doesn't loop: loop points around all its data.
    SEQLIB_CHECK(s.loop_start == 0 && s.loop_end == 3);
}

SEQLIB_TEST(sf2_samples, OneSampleKeepsNoLoopPoints)
{
    sf2::SoundFont sf;

    sf2::AddSamples(sf, Wave(1, 1, false), "short", 22050, 72);

    SEQLIB_REQUIRE(sf.samples.size() == 1);
    SEQLIB_CHECK(sf.samples[0].loop_start == 0 && sf.samples[0].loop_end == 0);
}

SEQLIB_TEST(sf2_samples, StereoWaveLinksItsPair)
{
    sf2::SoundFont sf;
    sf.samples.resize(2);

    const std::vector<uint16_t> ids = sf2::AddSamples(sf, Wave(3, 64, true), "pair", 44100, 64);

    SEQLIB_REQUIRE(ids == (std::vector<uint16_t>{2, 3}) && sf.samples.size() == 4);
    const sf2::Sample& left = sf.samples[2];
    const sf2::Sample& right = sf.samples[3];
    SEQLIB_CHECK(left.name == "pairL" && left.type == sf2::kLeft && left.link == 3 && left.data[0] == 1);
    SEQLIB_CHECK(right.name == "pairR" && right.type == sf2::kRight && right.link == 2 && right.data[0] == 2);
    SEQLIB_CHECK(left.loop_start == 8 && left.loop_end == 40 && right.loop_start == 8 && right.loop_end == 40);
    SEQLIB_CHECK(left.sample_rate == 44100 && right.original_key == 64);
}

SEQLIB_TEST(sf2_samples, NoChannelsAddNothing)
{
    sf2::SoundFont sf;

    SEQLIB_CHECK(sf2::AddSamples(sf, Wave(0, 0, false), "empty", 32000, 60).empty() && sf.samples.empty());
}

SEQLIB_TEST(sf2_samples, MoreThan65535SamplesThrow)
{
    sf2::SoundFont sf;
    sf.samples.resize(0xffff);
    std::string error;

    try
    {
        sf2::AddSamples(sf, Wave(1, 4, false), "one too many", 32000, 60);
    }
    catch (const std::runtime_error& e)
    {
        error = e.what();
    }

    SEQLIB_CHECK(error == "more than 65535 samples");
}

} // namespace
