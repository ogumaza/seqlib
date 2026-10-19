// SPDX-License-Identifier: MIT

// SoundFont 2.01 file structure.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace seqlib::sf2
{

// Generator operators (SoundFont 2.01, section 8.1.2).
enum Gen : uint16_t
{
    kStartAddrsOffset = 0,
    kEndAddrsOffset = 1,
    kStartloopAddrsOffset = 2,
    kEndloopAddrsOffset = 3,
    kStartAddrsCoarseOffset = 4,
    kModLfoToPitch = 5,
    kVibLfoToPitch = 6,
    kModEnvToPitch = 7,
    kInitialFilterFc = 8,
    kInitialFilterQ = 9,
    kModLfoToFilterFc = 10,
    kModEnvToFilterFc = 11,
    kEndAddrsCoarseOffset = 12,
    kModLfoToVolume = 13,
    kUnused1 = 14,
    kChorusEffectsSend = 15,
    kReverbEffectsSend = 16,
    kPan = 17,
    kUnused2 = 18,
    kUnused3 = 19,
    kUnused4 = 20,
    kDelayModLfo = 21,
    kFreqModLfo = 22,
    kDelayVibLfo = 23,
    kFreqVibLfo = 24,
    kDelayModEnv = 25,
    kAttackModEnv = 26,
    kHoldModEnv = 27,
    kDecayModEnv = 28,
    kSustainModEnv = 29,
    kReleaseModEnv = 30,
    kKeynumToModEnvHold = 31,
    kKeynumToModEnvDecay = 32,
    kDelayVolEnv = 33,
    kAttackVolEnv = 34,
    kHoldVolEnv = 35,
    kDecayVolEnv = 36,
    kSustainVolEnv = 37,
    kReleaseVolEnv = 38,
    kKeynumToVolEnvHold = 39,
    kKeynumToVolEnvDecay = 40,
    kInstrument = 41,
    kReserved1 = 42,
    kKeyRange = 43,
    kVelRange = 44,
    kStartloopAddrsCoarseOffset = 45,
    kKeynum = 46,
    kVelocity = 47,
    kInitialAttenuation = 48,
    kReserved2 = 49,
    kEndloopAddrsCoarseOffset = 50,
    kCoarseTune = 51,
    kFineTune = 52,
    kSampleId = 53,
    kSampleModes = 54,
    kReserved3 = 55,
    kScaleTuning = 56,
    kExclusiveClass = 57,
    kOverridingRootKey = 58,
    kUnused5 = 59,
    kEndOper = 60,
};

// Modulator sources (section 8.2.1): a controller in the low 7 bits, with flags for the controller palette, direction,
// polarity and curve. Without kController, the low 7 bits pick a source from the general controller palette.
enum Source : uint16_t
{
    kNoController = 0,
    kNoteOnVelocity = 2,
    kNoteOnKeyNumber = 3,
    kPolyPressure = 10,
    kChannelPressure = 13,
    kPitchWheel = 14,
    kPitchWheelSensitivity = 16,
    kLink = 127,
    kController = 0x80, // a MIDI controller, whose number is in the low 7 bits
    kNegative = 0x100,  // from the maximum down to the minimum
    kBipolar = 0x200,   // from -1 to 1 rather than from 0 to 1
    kConcave = 0x400,
    kConvex = 0x800,
    kSwitch = 0xC00,
};

// Modulator transforms (section 8.3).
enum Transform : uint16_t
{
    kLinear = 0,
    kAbsoluteValue = 2,
};

// A generator: an operator and its amount.
struct Generator
{
    // A generator with a signed amount.
    static Generator Value(uint16_t oper, int v)
    {
        return {oper, static_cast<uint16_t>(static_cast<int16_t>(v))};
    }

    // Creates a key-range or velocity-range generator.
    static Generator Range(uint16_t oper, int lo, int hi)
    {
        return {oper, static_cast<uint16_t>((lo & 0xff) | ((hi & 0xff) << 8))};
    }

    uint16_t oper = 0;
    uint16_t amount = 0; // signed values as two's complement; ranges as lo | hi << 8
};

// Modulator (section 8.2): source, destination generator, amount, optional amount-scaling source and transform.
struct Modulator
{
    uint16_t source = 0;
    uint16_t dest = 0;
    int16_t amount = 0;
    uint16_t amount_source = 0;
    uint16_t transform = kLinear;
};

// The sources of the default modulators of SoundFont 2.01 and of FluidSynth, built from the Source flags. A source
// without a curve flag is linear, a source without kBipolar is unipolar, and a source without kNegative is positive.
inline constexpr uint16_t kSrcVelocityLinearNegative = kNoteOnVelocity | kNegative; // 0x0102
inline constexpr uint16_t kSrcVelocitySwitch = kNoteOnVelocity | kSwitch;           // 0x0C02
inline constexpr uint16_t kSrcModWheel = kController | 1;                           // 0x0081: CC1
inline constexpr uint16_t kSrcPitchWheel = kPitchWheel | kBipolar;                  // 0x020E
inline constexpr uint16_t kSrcPitchWheelSensitivity = kPitchWheelSensitivity;       // 0x0010: RPN 0

// The modulators that a global zone starts with, to turn velocity's filtering off and give the pitch wheel its full
// range. A zone's modulator replaces the default modulator that has the same source, destination, amount source and
// transform.
// - The first two set the default velocity-to-cutoff modulator's amount to 0, in the versions of SoundFont 2.01 and of
//   FluidSynth. FluidSynth's version has a velocity switch as its amount source. Every velocity then plays with the
//   same filter.
// - The third replaces the default pitch wheel modulator. That one gives 12700 cents times the bend range / 128. A
//   7-semitone bend set through RPN 0 comes out about 5.5 cents flat. This one follows FluidSynth: it gives 12800 cents
//   and targets fineTune. The bend then covers the full range.
inline constexpr std::array<Modulator, 3> kNoVelocityFilterFullBend = {{
    {kSrcVelocityLinearNegative, kInitialFilterFc, 0, 0},
    {kSrcVelocityLinearNegative, kInitialFilterFc, 0, kSrcVelocitySwitch},
    {kSrcPitchWheel, kFineTune, 12800, kSrcPitchWheelSensitivity},
}};

struct Zone
{
    std::vector<Generator> generators;
    std::vector<Modulator> modulators;
};

struct Instrument
{
    std::string name;
    std::vector<Zone> zones; // a leading zone without a sample is global
};

struct Preset
{
    std::string name;
    uint16_t program = 0;
    uint16_t bank = 0;
    std::vector<Zone> zones;
};

enum SampleType : uint16_t
{
    kMono = 1,
    kRight = 2,
    kLeft = 4
};

struct Sample
{
    std::string name;
    std::vector<int16_t> data;
    uint32_t sample_rate = 44100;
    uint8_t original_key = 60;
    uint32_t loop_start = 0; // relative to the sample's start
    uint32_t loop_end = 0;   // relative to the sample's start
    uint16_t link = 0;       // sample index of the other stereo channel
    uint16_t type = kMono;
};

// A sub-chunk of the INFO list, such as INAM (the name), ICMT (a comment) or ISFT (the software that wrote the file).
struct InfoField
{
    std::string id; // four characters
    std::string text;
};

struct SoundFont
{
    std::string sound_engine = "EMU8000"; // isng

    // The INFO sub-chunks after ifil and isng, in the order given. INAM, the name, is required. Without it, the writer
    // writes an empty one first. The writer doesn't shorten them. The format allows at most 256 bytes for INAM and
    // 65,536 for ICMT, counting the terminating zeros.
    std::vector<InfoField> info;

    std::vector<Sample> samples;
    std::vector<Instrument> instruments;
    std::vector<Preset> presets;
};

} // namespace seqlib::sf2
