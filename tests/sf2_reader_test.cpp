// SPDX-License-Identifier: MIT

// Tests of the SoundFont reader and checker, on SoundFonts that seqlib's writer makes. Some of them are damaged on
// purpose.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "seqlib/read_error.h"
#include "seqlib/sf2/reader.h"
#include "seqlib/sf2/soundfont.h"
#include "seqlib/sf2/writer.h"
#include "test.h"

namespace seqlib::test
{
namespace
{

// A mono sample of 64 points with a loop that SoundFont players accept: 8 points before it, 40 in it and 16 after it.
// The point at 20 holds `peak`.
sf2::Sample LoopedSample(const std::string& name, int16_t peak)
{
    sf2::Sample s;
    s.name = name;
    s.sample_rate = 32000;
    for (int i = 0; i < 64; i++)
    {
        s.data.push_back(static_cast<int16_t>(i == 20 ? peak : i));
    }
    s.loop_start = 8;
    s.loop_end = 48;

    return s;
}

// A SoundFont with a looping mono sample, a stereo pair, and a preset for each of its two instruments.
sf2::SoundFont ValidSoundFont()
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Reader test"}, {"ISFT", "seqlib"}};
    sf.samples = {LoopedSample("Mono", 1000), LoopedSample("Left", -2000), LoopedSample("Right", 300)};
    sf.samples[1].type = sf2::kLeft;
    sf.samples[1].link = 2;
    sf.samples[2].type = sf2::kRight;
    sf.samples[2].link = 1;

    sf2::Instrument mono;
    mono.name = "Mono";
    mono.zones = {{{sf2::Generator::Range(sf2::kKeyRange, 0, 127), sf2::Generator::Value(sf2::kSampleModes, 1),
                    sf2::Generator::Value(sf2::kSampleId, 0)},
                   {}}};
    sf2::Instrument stereo;
    stereo.name = "Stereo";
    stereo.zones = {{{sf2::Generator::Value(sf2::kPan, -500), sf2::Generator::Value(sf2::kSampleId, 1)}, {}},
                    {{sf2::Generator::Value(sf2::kPan, 500), sf2::Generator::Value(sf2::kSampleId, 2)}, {}}};
    sf.instruments = {mono, stereo};

    sf2::Preset first;
    first.name = "First";
    first.zones = {{{sf2::Generator::Value(sf2::kInstrument, 0)}, {}}};
    sf2::Preset second;
    second.name = "Second";
    second.program = 1;
    second.zones = {{{sf2::Generator::Value(sf2::kInstrument, 1)}, {}}};
    sf.presets = {first, second};

    return sf;
}

// Returns the offset of the data of the first chunk `id` in `data`, or none if `data` has no such chunk.
std::optional<std::size_t> ChunkData(const std::vector<uint8_t>& data, const std::string& id)
{
    const auto it = std::search(data.begin(), data.end(), id.begin(), id.end());
    if (it == data.end())
    {
        return std::nullopt;
    }

    return static_cast<std::size_t>(it - data.begin()) + 8;
}

SEQLIB_TEST(sf2_reader, WrittenSoundFontHasNoProblems)
{
    const std::vector<uint8_t> data = sf2::Write(ValidSoundFont());

    const std::vector<std::string> problems = sf2::Check(data);

    SEQLIB_CHECK(problems.empty());
}

SEQLIB_TEST(sf2_reader, EmptySoundFontHasNoProblems)
{
    const std::vector<uint8_t> data = sf2::Write(sf2::SoundFont{});

    const std::vector<std::string> problems = sf2::Check(data);
    const sf2::Records records = sf2::Read(data);

    SEQLIB_CHECK(problems.empty());
    SEQLIB_CHECK(records.chunks.contains("INAM"));
}

SEQLIB_TEST(sf2_reader, RecordsHoldTheChunks)
{
    const std::vector<uint8_t> data = sf2::Write(ValidSoundFont());

    const sf2::Records records = sf2::Read(data);

    SEQLIB_CHECK(records.Count("phdr", 38) == 3);
    SEQLIB_CHECK(records.Count("inst", 22) == 3);
    SEQLIB_CHECK(records.Count("shdr", 46) == 4);
    SEQLIB_CHECK(records.Count("none", 4) == 0);
    SEQLIB_CHECK(std::string(reinterpret_cast<const char*>(records.Record("phdr", 38, 1))) == "Second");
    SEQLIB_CHECK(records.SamplePeak(0) == 1000);
    SEQLIB_CHECK(records.SamplePeak(1) == 2000);
    const std::map<uint16_t, uint16_t> gens = records.ZoneGens(1);
    SEQLIB_CHECK(gens.size() == 2);
    SEQLIB_CHECK(gens.at(sf2::kPan) == static_cast<uint16_t>(-500));
    SEQLIB_CHECK(gens.at(sf2::kSampleId) == 1);
}

// The record sizes are those of section 7.2 of the SoundFont 2.04 specification.
SEQLIB_TEST(sf2_reader, RecordSizesFollowTheSpecification)
{
    const sf2::Records records = sf2::Read(sf2::Write(ValidSoundFont()));

    const std::size_t presets = records.Count("phdr", sf2::kPresetHeaderSize);
    const std::size_t instruments = records.Count("inst", sf2::kInstrumentHeaderSize);
    const std::size_t samples = records.Count("shdr", sf2::kSampleHeaderSize);

    SEQLIB_CHECK(sf2::kPresetHeaderSize == 38);
    SEQLIB_CHECK(sf2::kBagSize == 4);
    SEQLIB_CHECK(sf2::kModulatorSize == 10);
    SEQLIB_CHECK(sf2::kGeneratorSize == 4);
    SEQLIB_CHECK(sf2::kInstrumentHeaderSize == 22);
    SEQLIB_CHECK(sf2::kSampleHeaderSize == 46);
    SEQLIB_CHECK(presets == 3 && instruments == 3 && samples == 4);
    SEQLIB_CHECK(records.chunks.at("pbag").size() % sf2::kBagSize == 0);
    SEQLIB_CHECK(records.chunks.at("pmod").size() % sf2::kModulatorSize == 0);
    SEQLIB_CHECK(records.chunks.at("igen").size() % sf2::kGeneratorSize == 0);
}

SEQLIB_TEST(sf2_reader, RecordPastTheEndThrows)
{
    const sf2::Records records = sf2::Read(sf2::Write(ValidSoundFont()));

    const std::string error = seqlib::testing::ErrorOf<std::out_of_range>([&] { records.Record("phdr", 38, 3); });

    SEQLIB_CHECK(!error.empty());
}

SEQLIB_TEST(sf2_reader, ReadThrowsForAWrongRiffSize)
{
    std::vector<uint8_t> data = sf2::Write(ValidSoundFont());
    const std::string sizes = std::to_string(data.size()) + " vs " + std::to_string(data.size() + 1);
    data.push_back(0);

    bool thrown = false;
    try
    {
        sf2::Read(data);
    }
    catch (const ReadError& e)
    {
        thrown = e.Offset() == 0;
    }
    const std::vector<std::string> problems = sf2::Check(data);

    SEQLIB_CHECK(thrown);
    SEQLIB_CHECK(problems == (std::vector<std::string>{"bad RIFF header (" + sizes + " bytes) (at byte 0)"}));
}

SEQLIB_TEST(sf2_reader, CheckFindsALoopWithoutPadding)
{
    sf2::SoundFont sf = ValidSoundFont();
    sf.samples[0].loop_start = 2;

    const std::vector<std::string> problems = sf2::Check(sf2::Write(sf));

    SEQLIB_CHECK(problems == (std::vector<std::string>{"instrument 0: looping sample 0 breaks the 8/32/8 rule"}));
}

SEQLIB_TEST(sf2_reader, CheckFindsABrokenStereoLink)
{
    sf2::SoundFont sf = ValidSoundFont();
    sf.samples[2].link = 0;

    const std::vector<std::string> problems = sf2::Check(sf2::Write(sf));

    SEQLIB_CHECK(problems ==
                 (std::vector<std::string>{"sample 1: stereo link broken", "sample 2: stereo link broken"}));
}

SEQLIB_TEST(sf2_reader, CheckFindsDuplicatePresets)
{
    sf2::SoundFont sf = ValidSoundFont();
    sf.presets[1].program = 0;

    const std::vector<std::string> problems = sf2::Check(sf2::Write(sf));

    SEQLIB_CHECK(problems == (std::vector<std::string>{"duplicate preset bank 0 program 0"}));
}

SEQLIB_TEST(sf2_reader, CheckFindsASampleIdThatIsntLast)
{
    // Records 1 and 2 of igen, 4 bytes each, are the sampleModes and the sampleID of instrument 0's zone. The swap puts
    // the sampleID before the sampleModes.
    std::vector<uint8_t> data = sf2::Write(ValidSoundFont());
    const std::optional<std::size_t> igen = ChunkData(data, "igen");
    SEQLIB_REQUIRE(igen.has_value());
    std::swap_ranges(data.begin() + static_cast<std::ptrdiff_t>(*igen + 4),
                     data.begin() + static_cast<std::ptrdiff_t>(*igen + 8),
                     data.begin() + static_cast<std::ptrdiff_t>(*igen + 8));

    const std::vector<std::string> problems = sf2::Check(data);

    SEQLIB_CHECK(problems == (std::vector<std::string>{"instrument 0 zone 0: generator 53 isn't last"}));
}

SEQLIB_TEST(sf2_reader, CheckFindsDataInASamplesPadding)
{
    // Sample 0 has 64 points. Point 70 is in the 46 zero points after them. Each point has 2 bytes.
    std::vector<uint8_t> data = sf2::Write(ValidSoundFont());
    const std::optional<std::size_t> smpl = ChunkData(data, "smpl");
    SEQLIB_REQUIRE(smpl.has_value());
    data[*smpl + 2 * 70] = 1;

    const std::vector<std::string> problems = sf2::Check(data);

    SEQLIB_CHECK(problems == (std::vector<std::string>{"sample 0: not followed by 46 zero samples"}));
}

} // namespace
} // namespace seqlib::test
