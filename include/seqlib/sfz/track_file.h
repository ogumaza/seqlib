// SPDX-License-Identifier: MIT

// A sequence's SFZ files: one for each track that has a MIDI channel, with the presets that the track plays, and the
// WAV samples that the files share. An sfz::Instruments holds both: the track files (sfz::File) and the samples
// (sfz::Sample). TrackFiles writes the frame of each track's file: its comments, <control>, the pan curve, <global> and
// a <master> for each preset. A hook adds the rest: other controller curves, opcodes, and each preset's groups of
// regions. The caller adds the samples.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "seqlib/midi/performance.h"
#include "seqlib/midi/performance_writer.h"

namespace seqlib::sfz
{

// An SFZ file of a sequence: its name, such as "Track 3.sfz", and its text.
struct File
{
    std::string name;
    std::string text;
};

// A WAV sample that a sequence's SFZ files share: its name, such as "w0_12.wav", and its bytes.
struct Sample
{
    std::string name;
    std::vector<uint8_t> wav;
};

// A sequence's SFZ files and the samples that they share.
struct Instruments
{
    std::vector<File> files;
    std::vector<Sample> samples;
};

// The folder of the samples, beside the sequences' folders. Each file's <control> header points there.
inline const std::string kSamplesFolder = "samples";

// The text that the hook adds to a track's file.
struct TrackText
{
    std::string curves; // controller curves, appended after the CC10 pan curve
    std::string global; // opcodes, appended to <global> after the pan and volume controllers

    // Returns the text appended after the <master> line of preset `preset`, an index into the performance's presets:
    // the preset's opcodes, and its <group> headers with their regions.
    std::function<std::string(uint32_t preset)> preset;
};

// Returns the text added to the file of track `track`.
using TrackTextFunction = std::function<TrackText(int track)>;

// What TrackFiles writes in each file.
struct TrackFileOptions
{
    std::string title;                     // the sequence's name, in each file's first comment
    std::string program;                   // the name of the program that writes the files, in the same comment
    std::vector<std::string> preset_names; // each preset's name, in a comment before its <master>
    std::size_t presets = 0;               // the number of presets. Above 128, each <master> selects its bank by CC0
    double level_cut_db = 0.0;             // the MIDI file's attenuation in dB, noted in each file's comments

    // Name each file, and the MIDI track in its first comment, after the MIDI track where it has a name. Otherwise both
    // are "Track <n>".
    bool track_names = false;
};

// Returns the file of each track that has a MIDI channel in `midi`: each track whose bend range isn't 0. `text` gives
// the text added to each track's file.
std::vector<File> TrackFiles(const midi::Performance& perf, const midi::MidiReport& midi,
                             const TrackFileOptions& options, const TrackTextFunction& text);

} // namespace seqlib::sfz
