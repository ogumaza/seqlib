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

namespace seqlib::sf2
{
namespace
{

using detail::LoadLe16;
using detail::LoadLe32;

// The pdta chunks and the size of their records.
constexpr std::pair<const char*, std::size_t> kRecordSizes[] = {
    {"phdr", 38}, {"pbag", 4},  {"pmod", 10}, {"pgen", 4},  {"inst", 22},
    {"ibag", 4},  {"imod", 10}, {"igen", 4},  {"shdr", 46},
};

// Generator operators that the check looks for.
constexpr uint16_t kInstrument = 41, kKeyRange = 43, kVelRange = 44, kSampleId = 53, kSampleModes = 54;

// Zero points that every sample has after its end.
constexpr uint32_t kZeroPoints = 46;

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

// The checks of Check(), on lists that ReadLists has read.
class Checker
{
public:
    Checker(const Lists& lists, std::vector<std::string>& errors) : lists_(lists), errors_(errors)
    {
    }

    void Run()
    {
        for (const char* name : {"INFO", "sdta", "pdta"})
        {
            if (!lists_.contains(name))
            {
                errors_.push_back(std::string("missing LIST ") + name);
                return;
            }
        }

        const auto& info = lists_.at("INFO");
        if (!info.contains("ifil") || !info.contains("isng") || !info.contains("INAM"))
        {
            errors_.push_back("INFO lacks ifil, isng or INAM");
        }
        if (!lists_.at("sdta").contains("smpl"))
        {
            errors_.push_back("missing smpl");
            return;
        }

        smpl_ = lists_.at("sdta").at("smpl");
        if (!ReadRecords())
        {
            return;
        }

        CheckTerminals();
        Zones("phdr", 24, "pbag", "pgen", kInstrument, Count("inst") - 1, "preset");
        Zones("inst", 20, "ibag", "igen", kSampleId, Count("shdr") - 1, "instrument");
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
                errors_.push_back(std::string("missing ") + name);
                return false;
            }

            const std::span<const uint8_t> chunk = pdta.at(name);
            if (chunk.size() % size != 0)
            {
                errors_.push_back(std::string(name) + " size " + std::to_string(chunk.size()) +
                                  " isn't a multiple of " + std::to_string(size));
            }
            records_[name] = {chunk, size};
        }

        for (const char* name : {"phdr", "pbag", "pgen", "inst", "ibag", "igen", "shdr"})
        {
            if (Count(name) == 0)
            {
                errors_.push_back(std::string(name) + " has no records");
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
            errors_.push_back("missing terminal records");
        }

        const uint8_t* pbag = At("pbag", Count("pbag") - 1);
        const uint8_t* ibag = At("ibag", Count("ibag") - 1);
        if (LoadLe16(pbag) != Count("pgen") - 1 || LoadLe16(ibag) != Count("igen") - 1)
        {
            errors_.push_back("terminal bag doesn't point at the terminal generator");
        }
        if (LoadLe16(pbag + 2) + std::size_t{1} != Count("pmod") ||
            LoadLe16(ibag + 2) + std::size_t{1} != Count("imod"))
        {
            errors_.push_back("terminal bag doesn't point at the terminal modulator");
        }
    }

    // Checks the zones of the presets or the instruments: the order of their generators, and the index that each zone
    // but a global one ends with.
    void Zones(const std::string& headers, std::size_t bag_field, const std::string& bags, const std::string& gens,
               uint16_t terminal, std::size_t limit, const std::string& what)
    {
        for (std::size_t i = 0; i + 1 < Count(headers); i++)
        {
            const uint16_t b0 = LoadLe16(At(headers, i) + bag_field), b1 = LoadLe16(At(headers, i + 1) + bag_field);
            if (b1 < b0)
            {
                errors_.push_back(what + " " + std::to_string(i) + ": bag indexes go backwards");
                continue;
            }

            for (uint16_t b = b0; b < b1; b++)
            {
                const std::string zone = what + " " + std::to_string(i) + " zone " + std::to_string(b - b0);
                if (b + 1u >= Count(bags))
                {
                    errors_.push_back(zone + ": bag index out of range");
                    break;
                }

                const uint16_t g0 = LoadLe16(At(bags, b)), g1 = LoadLe16(At(bags, b + 1u));
                if (g1 > Count(gens))
                {
                    errors_.push_back(zone + ": generator index out of range");
                    continue;
                }

                std::vector<uint16_t> opers;
                for (uint16_t g = g0; g < g1; g++)
                {
                    opers.push_back(LoadLe16(At(gens, g)));
                }
                CheckZone(opers, g0, gens, terminal, limit, b == b0, zone);
            }
        }
    }

    void CheckZone(const std::vector<uint16_t>& opers, uint16_t g0, const std::string& gens, uint16_t terminal,
                   std::size_t limit, bool first, const std::string& zone)
    {
        const auto index = [&opers](uint16_t oper)
        {
            return static_cast<std::size_t>(std::find(opers.begin(), opers.end(), oper) - opers.begin());
        };
        const std::size_t key_range = index(kKeyRange), vel_range = index(kVelRange), last = index(terminal);
        if (key_range < opers.size() && key_range != 0)
        {
            errors_.push_back(zone + ": keyRange isn't first");
        }
        if (vel_range < opers.size() && vel_range > (key_range < opers.size() ? 1u : 0u))
        {
            errors_.push_back(zone + ": velRange out of place");
        }

        if (last < opers.size())
        {
            if (last != opers.size() - 1)
            {
                errors_.push_back(zone + ": generator " + std::to_string(terminal) + " isn't last");
            }

            const uint16_t target = LoadLe16(At(gens, g0 + last) + 2);
            if (target >= limit)
            {
                errors_.push_back(zone + ": index " + std::to_string(target) + " out of range");
            }
        }
        else if (!first)
        {
            errors_.push_back(zone + ": missing " + std::to_string(terminal) + " in a non-global zone");
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
                errors_.push_back("duplicate preset bank " + std::to_string(bank) + " program " +
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
            const uint8_t* s = At("shdr", i);
            const uint32_t start = LoadLe32(s + 20), end = LoadLe32(s + 24), loop_start = LoadLe32(s + 28),
                           loop_end = LoadLe32(s + 32);
            const uint32_t rate = LoadLe32(s + 36);
            const uint16_t link = LoadLe16(s + 42), type = LoadLe16(s + 44);
            const std::string sample = "sample " + std::to_string(i);
            if (!(start < end && end <= points))
            {
                errors_.push_back(sample + ": bounds " + std::to_string(start) + "-" + std::to_string(end) +
                                  " outside " + std::to_string(points));
            }
            if (!ZerosAfter(end))
            {
                errors_.push_back(sample + ": not followed by 46 zero samples");
            }
            if (loop_end > loop_start && !(start <= loop_start && loop_end <= end))
            {
                errors_.push_back(sample + ": loop " + std::to_string(loop_start) + "-" + std::to_string(loop_end) +
                                  " outside " + std::to_string(start) + "-" + std::to_string(end));
            }

            if (type == 2 || type == 4)
            {
                if (link >= samples || LoadLe16(At("shdr", link) + 44) != 6 - type ||
                    LoadLe16(At("shdr", link) + 42) != i)
                {
                    errors_.push_back(sample + ": stereo link broken");
                }
            }
            else if (type != 1)
            {
                errors_.push_back(sample + ": type " + std::to_string(type));
            }
            if (rate < 400 || rate > 200000)
            {
                errors_.push_back(sample + ": rate " + std::to_string(rate));
            }
        }
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

                const uint8_t* s = At("shdr", gens[kSampleId]);
                const int64_t start = LoadLe32(s + 20), end = LoadLe32(s + 24), loop_start = LoadLe32(s + 28),
                              loop_end = LoadLe32(s + 32);
                if (loop_start - start < 8 || end - loop_end < 8 || loop_end - loop_start < 32)
                {
                    errors_.push_back("instrument " + std::to_string(i) + ": looping sample " +
                                      std::to_string(gens[kSampleId]) + " breaks the 8/32/8 rule");
                }
            }
        }
    }

    const Lists& lists_;
    std::vector<std::string>& errors_;
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
    const uint8_t* header = Record("shdr", 46, sample);
    const std::vector<uint8_t>& points = chunks.at("smpl");
    const std::size_t end = std::min<std::size_t>(LoadLe32(header + 24), points.size() / 2);
    int peak = 0;
    for (std::size_t i = LoadLe32(header + 20); i < end; i++)
    {
        peak = std::max(peak, std::abs(static_cast<int>(static_cast<int16_t>(LoadLe16(&points[2 * i])))));
    }

    return peak;
}

std::map<uint16_t, uint16_t> Records::ZoneGens(std::size_t zone) const
{
    std::map<uint16_t, uint16_t> gens;
    const uint16_t first = LoadLe16(Record("ibag", 4, zone)), last = LoadLe16(Record("ibag", 4, zone + 1));
    for (uint16_t g = first; g < last; g++)
    {
        gens[LoadLe16(Record("igen", 4, g))] = LoadLe16(Record("igen", 4, g) + 2);
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
    std::vector<std::string> errors;
    Lists lists;
    try
    {
        lists = ReadLists(data, &errors);
    }
    catch (const ReadError& e)
    {
        errors.push_back(e.what());
        return errors;
    }

    Checker(lists, errors).Run();

    return errors;
}

} // namespace seqlib::sf2
