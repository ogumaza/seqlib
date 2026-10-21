// SPDX-License-Identifier: MIT

// Tests of the SoundFont writer, with the default options and with sorted presets without padding. Each synthetic
// SoundFont's golden bytes are fixed: a change to them is a change of seqlib's output.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "seqlib/sf2/writer.h"
#include "test.h"
#include "test_data.h"

namespace seqlib::test
{
namespace
{

using G = sf2::Generator;

// The bytes of StereoCase().
constexpr char kStereoGolden[] =
    "52494646340400007366626b4c49535478000000494e464f6966696c040000000200010069736e6708000000454d553830303000"
    "494e414d0c00000053746572656f20746573740049434d542a000000476c6f62616c207a6f6e65732c2073746572656f206c696e"
    "6b7320616e64206d6f64756c61746f727300495346540a000000736632207465737473004c4953543e01000073647461736d706c"
    "320100000000e80318fcff7f00800500000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000018fce803ff7f0080fbff000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000010002000300000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000004c49"
    "5354620200007064746170686472720000004c656164000000000000000000000000000000005000000000000000000000000000"
    "000000005069616e6f000000000000000000000000000000000000000100000000000000000000000000454f5000000000000000"
    "00000000000000000000000000000300000000000000000000000000706261671000000000000000010000000200000004000000"
    "706d6f640a000000000000000000000000007067656e14000000290001000f0064002b00007f2900000000000000696e73744200"
    "00005069616e6f00000000000000000000000000000000004c656164000000000000000000000000000000000300454f49000000"
    "000000000000000000000000000004006962616714000000000000000200010006000100090001000f000300696d6f6428000000"
    "8100060032000000000002050800a0f60000000087003000c0038b000000000000000000000000006967656e40000000220050fb"
    "260000002b00007f2c00017f11000cfe350000002b00007f1100f40135000100360001003a004500390001000800401f09006400"
    "350002000000000073686472b80000005069616e6f204c0000000000000000000000000000000000060000000100000005000000"
    "d87f00003c00010004005069616e6f205200000000000000000000000000340000003a0000003500000039000000d87f00003c00"
    "00000200412073616d706c65206e616d65206c6f6e676500680000006b000000680000006b00000022560000450000000100454f"
    "5300000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

// The bytes of EmptyCase().
constexpr char kEmptyGolden[] =
    "524946469e0100007366626b4c49535440000000494e464f6966696c040000000200010069736e6708000000454d553830303000"
    "494e414d06000000456d70747900495346540a000000736632207465737473004c4953546800000073647461736d706c5c000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000000000000000000000000000000000000004c495354da00000070647461"
    "7068647226000000454f500000000000000000000000000000000000000000000000000000000000000000000000706261670400"
    "000000000000706d6f640a000000000000000000000000007067656e0400000000000000696e737416000000454f490000000000"
    "0000000000000000000000000000696261670400000000000000696d6f640a000000000000000000000000006967656e04000000"
    "00000000736864722e000000454f5300000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000000000";

// The bytes of SortedCase().
constexpr char kSortedGolden[] =
    "52494646ac0300007366626b4c49535462000000494e464f6966696c040000000200010069736e6708000000454d553830303000"
    "494e414d0600000042616e6b0000495346540e000000736f72746564207465737473000049434d5416000000496e737472756d65"
    "6e747320666f722042616e6b00004c495354d200000073647461736d706cc6000000401f401fc0e0c0e000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000064009cff320000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000004c4953545c0200007064746170686472be0000004c6f770000000000"
    "00000000000000000000000001000000000000000000000000000000000053717561726500000000000000000000000000000500"
    "0000010000000000000000000000000053717561726520636f707900000000000000000005000000020000000000000000000000"
    "00004869740000000000000000000000000000000000000080000300000000000000000000000000454f50000000000000000000"
    "00000000000000000000000004000000000000000000000000007062616714000000000000000100000002000000030000000400"
    "0000706d6f640a000000000000000000000000007067656e140000002900000029000000290000002900010000000000696e7374"
    "420000005371756172650000000000000000000000000000000048697400000000000000000000000000000000000200454f4900"
    "00000000000000000000000000000000030069626167100000000000000004000100080001000b000100696d6f64140000008a02"
    "1100f40100000000000000000000000000006967656e300000002b00003c220020d136000100350000002b003d7f380032003600"
    "010035000000000002002100dbe03500010000000000736864728a00000053717561726500000000000000000000000000000000"
    "00000400000000000000040000004334000048000000010048697400000000000000000000000000000000003200000035000000"
    "3200000035000000102900003c0000000100454f5300000000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000";

// The bytes of SortedEmptyCase().
constexpr char kSortedEmptyGolden[] =
    "52494646460100007366626b4c49535444000000494e464f6966696c040000000200010069736e6708000000454d553830303000"
    "494e414d06000000456d70747900495346540e000000736f7274656420746573747300004c4953540c00000073647461736d706c"
    "000000004c495354da000000706474617068647226000000454f5000000000000000000000000000000000000000000000000000"
    "00000000000000000000706261670400000000000000706d6f640a000000000000000000000000007067656e0400000000000000"
    "696e737416000000454f4900000000000000000000000000000000000000696261670400000000000000696d6f640a0000000000"
    "00000000000000006967656e0400000000000000736864722e000000454f53000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000";

// Stereo links, global zones in an instrument and a preset, modulators with and without an amount source, generators
// and presets out of order, a name longer than 19 characters, and odd and even lengths. The INFO fields are INAM, ICMT
// and ISFT.
sf2::SoundFont StereoCase()
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Stereo test"}, {"ICMT", "Global zones, stereo links and modulators"}, {"ISFT", "sf2 tests"}};

    sf2::Sample left;
    left.name = "Piano L";
    left.data = {0, 1000, -1000, 32767, -32768, 5};
    left.sample_rate = 32728;
    left.loop_start = 1;
    left.loop_end = 5;
    left.link = 1;
    left.type = sf2::kLeft;
    sf2::Sample right = left;
    right.name = "Piano R";
    right.data = {0, -1000, 1000, 32767, -32768, -5};
    right.link = 0;
    right.type = sf2::kRight;
    sf2::Sample lead;
    lead.name = "A sample name longer than nineteen";
    lead.data = {1, 2, 3};
    lead.sample_rate = 22050;
    lead.original_key = 69;
    lead.loop_end = 3;
    sf.samples = {left, right, lead};

    sf2::Instrument piano;
    piano.name = "Piano";
    sf2::Zone global;
    global.generators = {G::Value(sf2::kAttackVolEnv, -1200), G::Value(sf2::kReleaseVolEnv, 0)};
    global.modulators = {{sf2::kController | 1, sf2::kVibLfoToPitch, 50, 0}};
    sf2::Zone l;
    l.generators = {G::Value(sf2::kSampleId, 0), G::Value(sf2::kPan, -500), G::Range(sf2::kKeyRange, 0, 127),
                    G::Range(sf2::kVelRange, 1, 127)};
    sf2::Zone r;
    r.generators = {G::Range(sf2::kKeyRange, 0, 127), G::Value(sf2::kSampleId, 1), G::Value(sf2::kPan, 500)};
    piano.zones = {global, l, r};

    sf2::Instrument lead_inst;
    lead_inst.name = "Lead";
    sf2::Zone z;
    z.generators = {
        G::Value(sf2::kSampleId, 2),       G::Value(sf2::kSampleModes, 1),        G::Value(sf2::kOverridingRootKey, 69),
        G::Value(sf2::kExclusiveClass, 1), G::Value(sf2::kInitialFilterFc, 8000), G::Value(sf2::kInitialFilterQ, 100),
    };
    z.modulators = {{sf2::kNoteOnVelocity | sf2::kNegative | sf2::kConcave, sf2::kInitialFilterFc, -2400, 0},
                    {sf2::kController | 7, sf2::kInitialAttenuation, 960, sf2::kController | 11}};
    lead_inst.zones = {z};
    sf.instruments = {piano, lead_inst};

    sf2::Preset piano_preset;
    piano_preset.name = "Piano";
    sf2::Zone preset_global;
    preset_global.generators = {G::Value(sf2::kChorusEffectsSend, 100)};
    sf2::Zone preset_zone;
    preset_zone.generators = {G::Value(sf2::kInstrument, 0), G::Range(sf2::kKeyRange, 0, 127)};
    piano_preset.zones = {preset_global, preset_zone};
    sf2::Preset lead_preset;
    lead_preset.name = "Lead";
    lead_preset.program = 80;
    lead_preset.zones = {{{G::Value(sf2::kInstrument, 1)}, {}}};
    sf.presets = {lead_preset, piano_preset};

    return sf;
}

// A SoundFont with no samples, instruments or presets.
sf2::SoundFont EmptyCase()
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Empty"}, {"ISFT", "sf2 tests"}};

    return sf;
}

// A preset with one zone. The zone names the instrument.
sf2::Preset PresetOf(const std::string& name, uint16_t bank, uint16_t program, int instrument)
{
    sf2::Preset p;
    p.name = name;
    p.bank = bank;
    p.program = program;
    p.zones = {{{G::Value(sf2::kInstrument, instrument)}, {}}};

    return p;
}

// The sorted form: INAM, ISFT, then ICMT, presets out of order with two of the same bank and program, a sample with a
// loop and one without, generators out of order, and a modulator.
sf2::SoundFont SortedCase()
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Bank"}, {"ISFT", "sorted tests"}, {"ICMT", "Instruments for Bank"}};

    sf2::Sample square;
    square.name = "Square";
    square.data = {8000, 8000, -8000, -8000};
    square.sample_rate = 13379;
    square.original_key = 72;
    square.loop_start = 0;
    square.loop_end = 4;
    sf2::Sample hit;
    hit.name = "Hit";
    hit.data = {100, -100, 50};
    hit.sample_rate = 10512;
    hit.loop_start = 0;
    hit.loop_end = 3;
    sf.samples = {square, hit};

    sf2::Instrument square_inst;
    square_inst.name = "Square";
    sf2::Zone low;
    low.generators = {G::Value(sf2::kSampleId, 0), G::Range(sf2::kKeyRange, 0, 60),
                      G::Value(sf2::kAttackVolEnv, -12000), G::Value(sf2::kSampleModes, 1)};
    low.modulators = {{sf2::kController | sf2::kBipolar | 10, sf2::kPan, 500, 0, sf2::kLinear}};
    sf2::Zone high;
    high.generators = {G::Range(sf2::kKeyRange, 61, 127), G::Value(sf2::kScaleTuning, 50), G::Value(sf2::kSampleId, 0),
                       G::Value(sf2::kSampleModes, 1)};
    square_inst.zones = {low, high};
    sf2::Instrument hit_inst;
    hit_inst.name = "Hit";
    sf2::Zone hit_zone;
    hit_zone.generators = {G::Value(sf2::kStartAddrsOffset, 2), G::Value(sf2::kDelayVolEnv, -7973),
                           G::Value(sf2::kSampleId, 1)};
    hit_inst.zones = {hit_zone};
    sf.instruments = {square_inst, hit_inst};

    sf.presets = {PresetOf("Hit", 128, 0, 1), PresetOf("Square", 0, 5, 0), PresetOf("Square copy", 0, 5, 0),
                  PresetOf("Low", 0, 1, 0)};

    return sf;
}

// The sorted form with no samples, instruments or presets.
sf2::SoundFont SortedEmptyCase()
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Empty"}, {"ISFT", "sorted tests"}};

    return sf;
}

// Sorted presets, and no padding of empty sample data.
constexpr sf2::WriteOptions kSorted = {.sort_presets = true, .pad_empty_sample_data = false};

SEQLIB_TEST(sf2_writer, DefaultOptionsKeepTheGivenOrderAndPad)
{
    SEQLIB_CHECK(sf2::Write(StereoCase()) == FromHex(kStereoGolden));
    SEQLIB_CHECK(sf2::Write(EmptyCase()) == FromHex(kEmptyGolden));
}

SEQLIB_TEST(sf2_writer, SortedPresetsWithoutPadding)
{
    SEQLIB_CHECK(sf2::Write(SortedCase(), kSorted) == FromHex(kSortedGolden));
    SEQLIB_CHECK(sf2::Write(SortedEmptyCase(), kSorted) == FromHex(kSortedEmptyGolden));
}

SEQLIB_TEST(sf2_writer, LongNamesKeepWholeCharacters)
{
    // A name keeps at most 19 bytes. A UTF-8 character that would end past them goes whole, and one that ends on the
    // 19th byte stays.
    const std::string kCut = "ABCDEFGHIJKLMNOPQR\xc3\xa9";
    const std::string kFits = "abcdefghijklmnopq\xc3\xa9";
    sf2::SoundFont sf = SortedCase();
    sf.presets[0].name = kCut;
    sf.presets[1].name = kFits;

    const std::vector<uint8_t> bytes = sf2::Write(sf);

    // The 20 bytes of the name record that starts with `start`.
    const std::string text(bytes.begin(), bytes.end());
    auto record = [&](const std::string& start)
    {
        const std::size_t at = text.find(start);
        return at == std::string::npos ? std::string() : text.substr(at, 20);
    };
    SEQLIB_CHECK(record(kCut.substr(0, 18)) == kCut.substr(0, 18) + std::string(2, '\0'));
    SEQLIB_CHECK(record(kFits) == kFits + std::string(1, '\0'));
}

SEQLIB_TEST(sf2_writer, TooManyGeneratorsThrow)
{
    sf2::SoundFont sf;
    sf2::Instrument inst;
    inst.zones.resize(1);
    inst.zones[0].generators.assign(65536, G::Value(sf2::kPan, 0));
    sf.instruments = {inst};

    const std::string message = seqlib::testing::ErrorOf<std::runtime_error>([&] { sf2::Write(sf); });

    SEQLIB_CHECK(message == "SoundFont too large (more than 65535 generators, modulators or zones)");
}

SEQLIB_TEST(sf2_writer, InfoIdWithoutFourCharactersThrows)
{
    sf2::SoundFont sf;
    sf.info = {{"NAME!", "five characters"}};

    const std::string error = seqlib::testing::ErrorOf<std::invalid_argument>([&] { sf2::Write(sf); });

    SEQLIB_CHECK(!error.empty());
}

// The modulators that every global zone starts with. The writer stores each in the imod chunk as its source,
// destination, amount, amount source and transform, 16 bits each. The terminal record follows them. The fourth
// modulator is kCc10ToPan: source 0x028A, destination 17, amount 500, no amount source and a linear transform.
SEQLIB_TEST(sf2_writer, GlobalZoneModulators)
{
    sf2::SoundFont sf;
    sf.info = {{"INAM", "Modulators"}};
    sf2::Instrument instrument;
    instrument.name = "Global zone";
    instrument.zones.emplace_back();
    instrument.zones[0].modulators.assign(sf2::kGlobalZoneModulators.begin(), sf2::kGlobalZoneModulators.end());
    sf.instruments.push_back(instrument);

    const std::vector<uint8_t> file = sf2::Write(sf);
    const std::string text(file.begin(), file.end());
    const std::size_t imod = text.find("imod");

    SEQLIB_REQUIRE(imod != std::string::npos && imod + 58 <= file.size());
    const std::vector<uint8_t> chunk(file.begin() + static_cast<std::ptrdiff_t>(imod),
                                     file.begin() + static_cast<std::ptrdiff_t>(imod + 58));
    SEQLIB_CHECK(chunk == FromHex("696d6f6432000000"
                                  "02010800000000000000"
                                  "020108000000020c0000"
                                  "0e023400003210000000"
                                  "8a021100f40100000000"
                                  "00000000000000000000"));
    SEQLIB_CHECK(&sf2::kNoVelocityFilterFullBend == &sf2::kGlobalZoneModulators);
}

// SoundFont 2.04's default modulator from CC10 to pan (section 8.4.6): source 0x028A (CC10, linear, bipolar and
// positive), destination 17 (pan), amount 1000, amount source 0 (No Controller) and transform 0 (linear). The section
// lists Initial Attenuation as the destination. The section's title and text give the pan. kCc10ToPan matches every
// field but the amount. Section 9.5.1 counts two modulators with the same source, destination, amount source and
// transform as identical. In a global zone, kCc10ToPan then replaces the default modulator.
SEQLIB_TEST(sf2_writer, Cc10ToPanIsTheDefaultModulatorAtHalfItsAmount)
{
    const sf2::Modulator& m = sf2::kCc10ToPan;

    SEQLIB_CHECK(m.source == 0x028A && m.dest == 17 && m.amount_source == 0 && m.transform == 0);
    SEQLIB_CHECK(m.amount == 500);
}

} // namespace
} // namespace seqlib::test
