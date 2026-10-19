// SPDX-License-Identifier: MIT

// SoundFont 2 writer (see writer.h).

#include "seqlib/sf2/writer.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "bytes.h"

namespace seqlib::sf2
{
namespace
{

// The zero points that the format requires after each sample.
constexpr uint32_t kZeroPoints = 46;

// RIFF chunk buffer with little-endian numeric writes.
struct Buffer
{
    void U8(uint8_t v)
    {
        data.push_back(v);
    }

    void U16(uint16_t v)
    {
        detail::AppendLe16(data, v);
    }

    void U32(uint32_t v)
    {
        detail::AppendLe32(data, v);
    }

    void Fourcc(const char* id)
    {
        detail::AppendFourcc(data, id);
    }

    // pdta names occupy 20 bytes: zero-padded, with a terminating zero. A longer name keeps its first 19 bytes, less
    // any part of a UTF-8 character that the limit cuts.
    void Name(const std::string& s)
    {
        constexpr std::size_t kSize = 20;
        std::size_t length = std::min(s.size(), kSize - 1);
        while (length > 0 && length < s.size() && (static_cast<unsigned char>(s[length]) & 0xc0) == 0x80)
        {
            length--;
        }

        for (std::size_t i = 0; i < kSize; i++)
        {
            U8(i < length ? static_cast<uint8_t>(s[i]) : 0);
        }
    }

    void Bytes(const std::vector<uint8_t>& b)
    {
        data.insert(data.end(), b.begin(), b.end());
    }

    // Writes the chunk's ID, size and payload. An odd payload size gets a zero pad byte.
    void Chunk(const char* id, const std::vector<uint8_t>& payload)
    {
        Fourcc(id);
        U32(static_cast<uint32_t>(payload.size()));
        Bytes(payload);
        if (payload.size() & 1)
        {
            U8(0);
        }
    }

    std::vector<uint8_t> data;
};

// A zero-terminated string, padded to an even length.
std::vector<uint8_t> ZString(const std::string& s)
{
    std::vector<uint8_t> v(s.begin(), s.end());
    v.push_back(0);
    if (v.size() & 1)
    {
        v.push_back(0);
    }

    return v;
}

// Generator order: key range, velocity range, other generators, then `last`, the generator that ends a zone: the
// instrument in a preset zone, or the sample in an instrument zone.
int Rank(uint16_t oper, uint16_t last)
{
    if (oper == kKeyRange)
    {
        return 0;
    }
    if (oper == kVelRange)
    {
        return 1;
    }
    if (oper == last)
    {
        return 3;
    }

    return 2;
}

// Writes the bag, modulator and generator lists of a set of zones.
struct ZoneLists
{
    // Adds `zone`, whose generators end with `last` (see Rank).
    void Add(const Zone& zone, uint16_t last)
    {
        bags.U16(static_cast<uint16_t>(gen_count));
        bags.U16(static_cast<uint16_t>(mod_count));
        bag_count++;

        for (const Modulator& m : zone.modulators)
        {
            mods.U16(m.source);
            mods.U16(m.dest);
            mods.U16(static_cast<uint16_t>(m.amount));
            mods.U16(m.amount_source);
            mods.U16(m.transform);
            mod_count++;
        }

        auto by_rank = [&](const Generator& a, const Generator& b)
        {
            return Rank(a.oper, last) < Rank(b.oper, last);
        };
        std::vector<Generator> g = zone.generators;
        std::stable_sort(g.begin(), g.end(), by_rank);
        for (const Generator& x : g)
        {
            gens.U16(x.oper);
            gens.U16(x.amount);
            gen_count++;
        }

        if (gen_count > 0xffff || mod_count > 0xffff || bag_count > 0xffff)
        {
            throw std::runtime_error("SoundFont too large (more than 65535 generators, modulators or zones)");
        }
    }

    // Appends the required terminal record to each list.
    void Terminate()
    {
        bags.U16(static_cast<uint16_t>(gen_count));
        bags.U16(static_cast<uint16_t>(mod_count));
        for (int i = 0; i < 5; i++)
        {
            mods.U16(0);
        }
        gens.U32(0);
    }

    Buffer bags, mods, gens;
    uint32_t bag_count = 0, mod_count = 0, gen_count = 0;
};

// INFO metadata: version, sound engine, then the caller's fields in their order.
Buffer InfoList(const SoundFont& sf)
{
    Buffer ifil;
    ifil.U16(2);
    ifil.U16(1);

    Buffer info;
    info.Fourcc("INFO");
    info.Chunk("ifil", ifil.data);
    info.Chunk("isng", ZString(sf.sound_engine));

    // The format requires a name. Without one in `sf.info`, the name is empty.
    auto is_name = [](const InfoField& field)
    {
        return field.id == "INAM";
    };
    if (std::none_of(sf.info.begin(), sf.info.end(), is_name))
    {
        info.Chunk("INAM", ZString(""));
    }

    for (const InfoField& field : sf.info)
    {
        if (field.id.size() != 4)
        {
            throw std::invalid_argument("SoundFont INFO field ID \"" + field.id + "\" doesn't have four characters");
        }

        info.Chunk(field.id.c_str(), ZString(field.text));
    }

    return info;
}

// Writes sdta with 46 zero points after each sample. Unless the options turn it off, writes the 46 points even when
// there are no samples. FluidSynth rejects SoundFonts with empty sample data.
Buffer SdtaList(const SoundFont& sf, const WriteOptions& options)
{
    Buffer smpl;
    for (const Sample& s : sf.samples)
    {
        for (int16_t v : s.data)
        {
            smpl.U16(static_cast<uint16_t>(v));
        }
        for (uint32_t i = 0; i < kZeroPoints; i++)
        {
            smpl.U16(0);
        }
    }

    if (sf.samples.empty() && options.pad_empty_sample_data)
    {
        smpl.data.resize(kZeroPoints * 2);
    }

    Buffer sdta;
    sdta.Fourcc("sdta");
    sdta.Chunk("smpl", smpl.data);

    return sdta;
}

// The presets in the order they're written: as given, or sorted by bank and then program.
std::vector<const Preset*> PresetOrder(const SoundFont& sf, const WriteOptions& options)
{
    std::vector<const Preset*> order;
    order.reserve(sf.presets.size());
    for (const Preset& p : sf.presets)
    {
        order.push_back(&p);
    }

    if (options.sort_presets)
    {
        auto by_bank_then_program = [](const Preset* a, const Preset* b)
        {
            return a->bank != b->bank ? a->bank < b->bank : a->program < b->program;
        };
        std::stable_sort(order.begin(), order.end(), by_bank_then_program);
    }

    return order;
}

// Appends a preset header: the preset's name, program and bank, the index of its first zone, and three reserved
// fields (library, genre and morphology).
void AppendPresetHeader(Buffer& phdr, const std::string& name, uint16_t program, uint16_t bank, uint32_t first_zone)
{
    phdr.Name(name);
    phdr.U16(program);
    phdr.U16(bank);
    phdr.U16(static_cast<uint16_t>(first_zone));
    phdr.U32(0);
    phdr.U32(0);
    phdr.U32(0);
}

// The preset headers (phdr), in the order that PresetOrder gives, and the terminal record EOP. The presets' zones go to
// `zones`.
Buffer PresetHeaders(const SoundFont& sf, const WriteOptions& options, ZoneLists& zones)
{
    Buffer phdr;
    for (const Preset* p : PresetOrder(sf, options))
    {
        AppendPresetHeader(phdr, p->name, p->program, p->bank, zones.bag_count);
        for (const Zone& z : p->zones)
        {
            zones.Add(z, kInstrument);
        }
    }

    AppendPresetHeader(phdr, "EOP", 0, 0, zones.bag_count);
    zones.Terminate();

    return phdr;
}

// Appends an instrument header: the instrument's name and the index of its first zone.
void AppendInstrumentHeader(Buffer& inst, const std::string& name, uint32_t first_zone)
{
    inst.Name(name);
    inst.U16(static_cast<uint16_t>(first_zone));
}

// The instrument headers (inst) and the terminal record EOI. The instruments' zones go to `zones`.
Buffer InstrumentHeaders(const SoundFont& sf, ZoneLists& zones)
{
    Buffer inst;
    for (const Instrument& i : sf.instruments)
    {
        AppendInstrumentHeader(inst, i.name, zones.bag_count);
        for (const Zone& z : i.zones)
        {
            zones.Add(z, kSampleId);
        }
    }

    AppendInstrumentHeader(inst, "EOI", zones.bag_count);
    zones.Terminate();

    return inst;
}

// The sample headers (shdr) and the terminal record EOS. Positions are sample offsets within smpl. They follow the
// layout in SdtaList.
Buffer SampleHeaders(const SoundFont& sf)
{
    Buffer shdr;
    uint32_t start = 0;
    for (const Sample& s : sf.samples)
    {
        shdr.Name(s.name);
        shdr.U32(start);
        shdr.U32(start + static_cast<uint32_t>(s.data.size()));
        shdr.U32(start + s.loop_start);
        shdr.U32(start + s.loop_end);
        shdr.U32(s.sample_rate);
        shdr.U8(s.original_key);
        shdr.U8(0); // pitch correction
        shdr.U16(s.link);
        shdr.U16(s.type);
        start += static_cast<uint32_t>(s.data.size()) + kZeroPoints;
    }

    shdr.Name("EOS");
    for (int i = 0; i < 5; i++)
    {
        shdr.U32(0);
    }
    shdr.U8(0);
    shdr.U8(0);
    shdr.U16(0);
    shdr.U16(0);

    return shdr;
}

// Writes pdta: the preset, instrument and sample headers, and the bags, modulators and generators of the presets' and
// the instruments' zones.
Buffer PdtaList(const SoundFont& sf, const WriteOptions& options)
{
    ZoneLists preset_zones;
    const Buffer phdr = PresetHeaders(sf, options, preset_zones);
    ZoneLists instrument_zones;
    const Buffer inst = InstrumentHeaders(sf, instrument_zones);
    const Buffer shdr = SampleHeaders(sf);

    Buffer pdta;
    pdta.Fourcc("pdta");
    pdta.Chunk("phdr", phdr.data);
    pdta.Chunk("pbag", preset_zones.bags.data);
    pdta.Chunk("pmod", preset_zones.mods.data);
    pdta.Chunk("pgen", preset_zones.gens.data);
    pdta.Chunk("inst", inst.data);
    pdta.Chunk("ibag", instrument_zones.bags.data);
    pdta.Chunk("imod", instrument_zones.mods.data);
    pdta.Chunk("igen", instrument_zones.gens.data);
    pdta.Chunk("shdr", shdr.data);

    return pdta;
}

} // namespace

std::vector<uint8_t> Write(const SoundFont& sf, const WriteOptions& options)
{
    const Buffer lists[] = {InfoList(sf), SdtaList(sf, options), PdtaList(sf, options)};
    std::size_t size = 4; // "sfbk"
    for (const Buffer& list : lists)
    {
        size += 8 + list.data.size();
    }

    Buffer riff;
    riff.data.reserve(8 + size);
    riff.Fourcc("RIFF");
    riff.U32(static_cast<uint32_t>(size));
    riff.Fourcc("sfbk");
    for (const Buffer& list : lists)
    {
        riff.Fourcc("LIST");
        riff.U32(static_cast<uint32_t>(list.data.size()));
        riff.Bytes(list.data);
    }

    return std::move(riff.data);
}

} // namespace seqlib::sf2
