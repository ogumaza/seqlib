// SPDX-License-Identifier: MIT

#include "seqlib/sf2/reader.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <utility>

#include "bytes.h"
#include "seqlib/read_error.h"
#include "seqlib/sf2/soundfont.h"

namespace seqlib::sf2
{
namespace
{

using detail::LoadLe16;
using detail::LoadLe32;

// The sizes of the pdta chunks' records: a preset header (phdr), a bag (pbag and ibag), a modulator (pmod and imod), a
// generator (pgen and igen), an instrument header (inst) and a sample header (shdr).
constexpr std::size_t kPresetHeaderSize = 38;
constexpr std::size_t kBagSize = 4;
constexpr std::size_t kModulatorSize = 10;
constexpr std::size_t kGeneratorSize = 4;
constexpr std::size_t kInstrumentHeaderSize = 22;
constexpr std::size_t kSampleHeaderSize = 46;

// The pdta chunks and the size of their records.
constexpr std::pair<const char*, std::size_t> kRecordSizes[] = {
    {"phdr", kPresetHeaderSize},     {"pbag", kBagSize}, {"pmod", kModulatorSize}, {"pgen", kGeneratorSize},
    {"inst", kInstrumentHeaderSize}, {"ibag", kBagSize}, {"imod", kModulatorSize}, {"igen", kGeneratorSize},
    {"shdr", kSampleHeaderSize},
};

// Zero points that every sample has after its end.
constexpr uint32_t kZeroPoints = 46;

// The fields of a sample header (shdr) record after its name: where the sample and its loop lie among the smpl chunk's
// points, its rate, the other sample of a stereo pair, and its type.
struct SampleHeader
{
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0;
    uint32_t rate = 0;
    uint16_t link = 0;
    uint16_t type = 0;
};

// Reads the sample header record at `record`.
SampleHeader ReadSampleHeader(const uint8_t* record)
{
    SampleHeader header;
    header.start = LoadLe32(record + 20);
    header.end = LoadLe32(record + 24);
    header.loop_start = LoadLe32(record + 28);
    header.loop_end = LoadLe32(record + 32);
    header.rate = LoadLe32(record + 36);
    header.link = LoadLe16(record + 42);
    header.type = LoadLe16(record + 44);

    return header;
}

std::string Id(std::span<const uint8_t> d, std::size_t o)
{
    return std::string(reinterpret_cast<const char*>(d.data() + o), 4);
}

// The sub-chunks of each list, by the list's type and the sub-chunk's ID.
using Lists = std::map<std::string, std::map<std::string, std::span<const uint8_t>>>;

// Reads the lists of `d`. Throws ReadError for the RIFF header, for a chunk that runs past the end of its list or of
// the data, and for a list whose sub-chunks don't end where it ends. A top-level chunk that isn't a LIST adds a line to
// `problems`, or throws ReadError if `problems` is null.
Lists ReadLists(std::span<const uint8_t> d, std::vector<std::string>* problems)
{
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "sfbk", 4) != 0 ||
        LoadLe32(d.data() + 4) != d.size() - 8)
    {
        const std::string size = d.size() >= 8 ? std::to_string(LoadLe32(d.data() + 4) + 8ull) : "no";
        throw ReadError(0, "bad RIFF header (" + size + " vs " + std::to_string(d.size()) + " bytes)");
    }

    Lists lists;
    std::size_t off = 12;
    while (off < d.size())
    {
        if (d.size() - off < 8)
        {
            throw ReadError(off, "chunk header runs past the end of the data");
        }

        const std::string id = Id(d, off);
        const uint32_t size = LoadLe32(d.data() + off + 4);
        if (size > d.size() - off - 8)
        {
            throw ReadError(off, "chunk " + id + " runs past the end of the data");
        }

        const std::size_t end = off + 8 + size;
        if (id != "LIST" || size < 4)
        {
            if (problems == nullptr)
            {
                throw ReadError(off, "top-level chunk " + id + " isn't a LIST");
            }

            problems->push_back("unexpected top-level chunk " + id);
            off = end + (size & 1);
            continue;
        }

        // Read the list's sub-chunks. Each one of odd size has a byte of padding after it.
        std::map<std::string, std::span<const uint8_t>>& list = lists[Id(d, off + 8)];
        std::size_t sub = off + 12;
        while (sub < end)
        {
            if (end - sub < 8 || LoadLe32(d.data() + sub + 4) > end - sub - 8)
            {
                throw ReadError(sub, "chunk runs past the end of its list");
            }

            const uint32_t sub_size = LoadLe32(d.data() + sub + 4);
            list[Id(d, sub)] = d.subspan(sub + 8, sub_size);
            sub += 8 + sub_size + (sub_size & 1);
        }
        if (sub != end)
        {
            throw ReadError(off, "the chunks of LIST " + Id(d, off + 8) + " end past it");
        }

        off = end + (size & 1);
    }

    return lists;
}

// The records of the presets' zones or of the instruments' zones: the headers, and where in a header the index of its
// first bag is, the bags, the generators, the generator that ends each zone but a global one, the chunk whose records
// that generator indexes, and what the messages call a header.
struct ZoneRecords
{
    const char* headers;
    std::size_t first_bag;
    const char* bags;
    const char* generators;
    uint16_t last;
    const char* targets;
    const char* what;
};

constexpr ZoneRecords kPresetZones = {"phdr", 24, "pbag", "pgen", kInstrument, "inst", "preset"};
constexpr ZoneRecords kInstrumentZones = {"inst", 20, "ibag", "igen", kSampleId, "shdr", "instrument"};

// The checks of Check(), on lists that ReadLists has read.
class Checker
{
public:
    Checker(const Lists& lists, std::vector<std::string>& problems) : lists_(lists), problems_(problems)
    {
    }

    void Run()
    {
        for (const char* name : {"INFO", "sdta", "pdta"})
        {
            if (!lists_.contains(name))
            {
                problems_.push_back(std::string("missing LIST ") + name);
                return;
            }
        }

        const auto& info = lists_.at("INFO");
        if (!info.contains("ifil") || !info.contains("isng") || !info.contains("INAM"))
        {
            problems_.push_back("INFO lacks ifil, isng or INAM");
        }
        if (!lists_.at("sdta").contains("smpl"))
        {
            problems_.push_back("missing smpl");
            return;
        }

        smpl_ = lists_.at("sdta").at("smpl");
        if (!ReadRecords())
        {
            return;
        }

        CheckTerminals();
        CheckZones(kPresetZones);
        CheckZones(kInstrumentZones);
        CheckPresets();
        CheckSamples();
        CheckLoopPadding();
    }

private:
    // Finds the pdta chunks and checks their sizes. Returns false if the other checks can't go on.
    bool ReadRecords()
    {
        const auto& pdta = lists_.at("pdta");
        for (const auto& [name, size] : kRecordSizes)
        {
            if (!pdta.contains(name))
            {
                problems_.push_back(std::string("missing ") + name);
                return false;
            }

            const std::span<const uint8_t> chunk = pdta.at(name);
            if (chunk.size() % size != 0)
            {
                problems_.push_back(std::string(name) + " size " + std::to_string(chunk.size()) +
                                    " isn't a multiple of " + std::to_string(size));
            }
            records_[name] = {chunk, size};
        }

        for (const char* name : {"phdr", "pbag", "pgen", "inst", "ibag", "igen", "shdr"})
        {
            if (Count(name) == 0)
            {
                problems_.push_back(std::string(name) + " has no records");
                return false;
            }
        }

        return true;
    }

    std::size_t Count(const std::string& name) const
    {
        const auto& [chunk, size] = records_.at(name);
        return chunk.size() / size;
    }

    const uint8_t* At(const std::string& name, std::size_t index) const
    {
        const auto& [chunk, size] = records_.at(name);
        return chunk.data() + index * size;
    }

    // Returns the 20-byte name at the start of record `index`, up to its first zero byte.
    std::string Name(const std::string& chunk, std::size_t index) const
    {
        const char* p = reinterpret_cast<const char*>(At(chunk, index));
        return std::string(p, std::find(p, p + 20, '\0'));
    }

    void CheckTerminals()
    {
        if (Name("phdr", Count("phdr") - 1) != "EOP" || Name("inst", Count("inst") - 1) != "EOI" ||
            Name("shdr", Count("shdr") - 1) != "EOS")
        {
            problems_.push_back("missing terminal records");
        }

        const uint8_t* pbag = At("pbag", Count("pbag") - 1);
        const uint8_t* ibag = At("ibag", Count("ibag") - 1);
        if (LoadLe16(pbag) != Count("pgen") - 1 || LoadLe16(ibag) != Count("igen") - 1)
        {
            problems_.push_back("terminal bag doesn't point at the terminal generator");
        }
        if (LoadLe16(pbag + 2) + std::size_t{1} != Count("pmod") ||
            LoadLe16(ibag + 2) + std::size_t{1} != Count("imod"))
        {
            problems_.push_back("terminal bag doesn't point at the terminal modulator");
        }
    }

    // Checks the zones of the presets or the instruments: the order of their generators, and the index that each zone
    // but a global one ends with.
    void CheckZones(const ZoneRecords& records)
    {
        for (std::size_t i = 0; i + 1 < Count(records.headers); i++)
        {
            const uint16_t b0 = LoadLe16(At(records.headers, i) + records.first_bag);
            const uint16_t b1 = LoadLe16(At(records.headers, i + 1) + records.first_bag);
            const std::string header = std::string(records.what) + " " + std::to_string(i);
            if (b1 < b0)
            {
                problems_.push_back(header + ": bag indexes go backwards");
                continue;
            }

            for (uint16_t b = b0; b < b1; b++)
            {
                const std::string zone = header + " zone " + std::to_string(b - b0);
                if (b + 1u >= Count(records.bags))
                {
                    problems_.push_back(zone + ": bag index out of range");
                    break;
                }

                const uint16_t g0 = LoadLe16(At(records.bags, b)), g1 = LoadLe16(At(records.bags, b + 1u));
                if (g1 > Count(records.generators))
                {
                    problems_.push_back(zone + ": generator index out of range");
                    continue;
                }

                std::vector<std::pair<uint16_t, uint16_t>> generators;
                for (uint16_t g = g0; g < g1; g++)
                {
                    const uint8_t* generator = At(records.generators, g);
                    generators.emplace_back(LoadLe16(generator), LoadLe16(generator + 2));
                }
                CheckZone(records, generators, b == b0, zone);
            }
        }
    }

    // Checks the order of a zone's generators, given as operators and amounts, and the index that the zone ends with.
    // Only a header's first zone can be global, without that index.
    void CheckZone(const ZoneRecords& records, const std::vector<std::pair<uint16_t, uint16_t>>& generators, bool first,
                   const std::string& zone)
    {
        const auto index = [&generators](uint16_t oper)
        {
            const auto is_oper = [oper](const std::pair<uint16_t, uint16_t>& generator)
            {
                return generator.first == oper;
            };
            return static_cast<std::size_t>(std::find_if(generators.begin(), generators.end(), is_oper) -
                                            generators.begin());
        };
        const std::size_t key_range = index(kKeyRange), vel_range = index(kVelRange), last = index(records.last);
        if (key_range < generators.size() && key_range != 0)
        {
            problems_.push_back(zone + ": keyRange isn't first");
        }
        if (vel_range < generators.size() && vel_range > (key_range < generators.size() ? 1u : 0u))
        {
            problems_.push_back(zone + ": velRange out of place");
        }

        if (last < generators.size())
        {
            if (last != generators.size() - 1)
            {
                problems_.push_back(zone + ": generator " + std::to_string(records.last) + " isn't last");
            }

            // The last record of the targets' chunk is the terminal record. No zone may name it.
            const uint16_t target = generators[last].second;
            if (target >= Count(records.targets) - 1)
            {
                problems_.push_back(zone + ": index " + std::to_string(target) + " out of range");
            }
        }
        else if (!first)
        {
            problems_.push_back(zone + ": missing " + std::to_string(records.last) + " in a non-global zone");
        }
    }

    void CheckPresets()
    {
        std::set<std::pair<uint16_t, uint16_t>> presets;
        for (std::size_t i = 0; i + 1 < Count("phdr"); i++)
        {
            const uint16_t program = LoadLe16(At("phdr", i) + 20), bank = LoadLe16(At("phdr", i) + 22);
            if (!presets.insert({bank, program}).second)
            {
                problems_.push_back("duplicate preset bank " + std::to_string(bank) + " program " +
                                    std::to_string(program));
            }
        }
    }

    void CheckSamples()
    {
        const std::size_t points = smpl_.size() / 2;
        const std::size_t samples = Count("shdr") - 1;
        for (std::size_t i = 0; i < samples; i++)
        {
            const SampleHeader s = ReadSampleHeader(At("shdr", i));
            const std::string sample = "sample " + std::to_string(i);
            if (!(s.start < s.end && s.end <= points))
            {
                problems_.push_back(sample + ": bounds " + std::to_string(s.start) + "-" + std::to_string(s.end) +
                                    " outside " + std::to_string(points));
            }
            if (!ZerosAfter(s.end))
            {
                problems_.push_back(sample + ": not followed by 46 zero samples");
            }
            if (s.loop_end > s.loop_start && !(s.start <= s.loop_start && s.loop_end <= s.end))
            {
                problems_.push_back(sample + ": loop " + std::to_string(s.loop_start) + "-" +
                                    std::to_string(s.loop_end) + " outside " + std::to_string(s.start) + "-" +
                                    std::to_string(s.end));
            }

            if (s.type == kRight || s.type == kLeft)
            {
                if (!LinksBack(i, s, samples))
                {
                    problems_.push_back(sample + ": stereo link broken");
                }
            }
            else if (s.type != kMono)
            {
                problems_.push_back(sample + ": type " + std::to_string(s.type));
            }
            if (s.rate < 400 || s.rate > 200000)
            {
                problems_.push_back(sample + ": rate " + std::to_string(s.rate));
            }
        }
    }

    // Returns true if the stereo sample `i`, whose header is `header`, links to one of the first `samples` samples, and
    // that sample links back to sample `i` with the other side's type.
    bool LinksBack(std::size_t i, const SampleHeader& header, std::size_t samples) const
    {
        if (header.link >= samples)
        {
            return false;
        }

        const SampleHeader other = ReadSampleHeader(At("shdr", header.link));
        return other.type == kLeft + kRight - header.type && other.link == i;
    }

    // Returns true if 46 zero points follow point `end` of the sample data.
    bool ZerosAfter(uint32_t end) const
    {
        const std::size_t points = smpl_.size() / 2;
        if (end > points || points - end < kZeroPoints)
        {
            return false;
        }

        const auto zero = smpl_.subspan(static_cast<std::size_t>(end) * 2, kZeroPoints * 2);
        return std::all_of(zero.begin(), zero.end(), [](uint8_t b) { return b == 0; });
    }

    // Checks the 8/32/8 rule on each looping zone's sample: 8 points before the loop, 32 in it and 8 after it.
    void CheckLoopPadding()
    {
        for (std::size_t i = 0; i + 1 < Count("inst"); i++)
        {
            const uint16_t b0 = LoadLe16(At("inst", i) + 20), b1 = LoadLe16(At("inst", i + 1) + 20);
            for (uint16_t b = b0; b < b1 && b + 1u < Count("ibag"); b++)
            {
                std::map<uint16_t, uint16_t> gens;
                const uint16_t g0 = LoadLe16(At("ibag", b)), g1 = LoadLe16(At("ibag", b + 1u));
                for (uint16_t g = g0; g < g1 && g < Count("igen"); g++)
                {
                    gens[LoadLe16(At("igen", g))] = LoadLe16(At("igen", g) + 2);
                }

                if (!(gens[kSampleModes] & 1) || !gens.contains(kSampleId) || gens[kSampleId] >= Count("shdr"))
                {
                    continue;
                }

                // The points before and after the loop, and the loop's length. A broken header can make them negative.
                const SampleHeader s = ReadSampleHeader(At("shdr", gens[kSampleId]));
                const int64_t before = int64_t{s.loop_start} - s.start;
                const int64_t after = int64_t{s.end} - s.loop_end;
                const int64_t length = int64_t{s.loop_end} - s.loop_start;
                if (before < 8 || after < 8 || length < 32)
                {
                    problems_.push_back("instrument " + std::to_string(i) + ": looping sample " +
                                        std::to_string(gens[kSampleId]) + " breaks the 8/32/8 rule");
                }
            }
        }
    }

    const Lists& lists_;
    std::vector<std::string>& problems_;
    std::span<const uint8_t> smpl_;
    std::map<std::string, std::pair<std::span<const uint8_t>, std::size_t>> records_;
};

} // namespace

const uint8_t* Records::Record(const std::string& chunk, std::size_t size, std::size_t index) const
{
    const auto it = chunks.find(chunk);
    if (it == chunks.end() || (index + 1) * size > it->second.size())
    {
        throw std::out_of_range("no record " + std::to_string(index) + " in chunk " + chunk);
    }

    return it->second.data() + index * size;
}

std::size_t Records::Count(const std::string& chunk, std::size_t size) const
{
    const auto it = chunks.find(chunk);
    return it == chunks.end() ? 0 : it->second.size() / size;
}

int Records::SamplePeak(std::size_t sample) const
{
    const SampleHeader header = ReadSampleHeader(Record("shdr", kSampleHeaderSize, sample));
    const std::vector<uint8_t>& points = chunks.at("smpl");
    const std::size_t end = std::min<std::size_t>(header.end, points.size() / 2);
    int peak = 0;
    for (std::size_t i = header.start; i < end; i++)
    {
        peak = std::max(peak, std::abs(static_cast<int>(static_cast<int16_t>(LoadLe16(&points[2 * i])))));
    }

    return peak;
}

std::map<uint16_t, uint16_t> Records::ZoneGens(std::size_t zone) const
{
    std::map<uint16_t, uint16_t> gens;
    const uint16_t first = LoadLe16(Record("ibag", kBagSize, zone));
    const uint16_t last = LoadLe16(Record("ibag", kBagSize, zone + 1));
    for (uint16_t g = first; g < last; g++)
    {
        const uint8_t* generator = Record("igen", kGeneratorSize, g);
        gens[LoadLe16(generator)] = LoadLe16(generator + 2);
    }

    return gens;
}

Records Read(std::span<const uint8_t> data)
{
    Records records;
    for (const auto& [type, list] : ReadLists(data, nullptr))
    {
        for (const auto& [id, chunk] : list)
        {
            records.chunks[id] = std::vector<uint8_t>(chunk.begin(), chunk.end());
        }
    }

    return records;
}

std::vector<std::string> Check(std::span<const uint8_t> data)
{
    std::vector<std::string> problems;
    Lists lists;
    try
    {
        lists = ReadLists(data, &problems);
    }
    catch (const ReadError& e)
    {
        problems.push_back(e.what());
        return problems;
    }

    Checker(lists, problems).Run();

    return problems;
}

} // namespace seqlib::sf2
