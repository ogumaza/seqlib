// SPDX-License-Identifier: MIT

// Reading SoundFont 2 files: the bytes of each chunk, the records in them, and a check of their structure.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace seqlib::sf2
{

// A SoundFont's chunks: the sub-chunks of its INFO, sdta and pdta lists, by ID, such as "INAM", "smpl" or "shdr".
struct Records
{
    // Returns record `index` of chunk `chunk`. The chunk's records are `size` bytes each. Throws std::out_of_range if
    // the file has no such chunk or the record lies past its end.
    const uint8_t* Record(const std::string& chunk, std::size_t size, std::size_t index) const;

    // Returns the number of whole records of `size` bytes in chunk `chunk`, or 0 if the file has no such chunk.
    std::size_t Count(const std::string& chunk, std::size_t size) const;

    // Returns the largest magnitude of any point of sample `sample`, from its header's start to its end.
    int SamplePeak(std::size_t sample) const;

    // Returns the generators of instrument zone `zone`: a map from operator to amount.
    std::map<uint16_t, uint16_t> ZoneGens(std::size_t zone) const;

    std::map<std::string, std::vector<uint8_t>> chunks;
};

// Reads a file's chunks. Throws ReadError for a header that isn't RIFF sfbk with the data's size, a top-level chunk
// that isn't a LIST, a chunk that runs past the end of its list or of the data, and a list whose sub-chunks don't end
// where it ends.
Records Read(std::span<const uint8_t> data);

// Returns each problem in the structure of the SoundFont in `data`, one line each. The check covers the RIFF chunks,
// the record sizes and terminal records, the bag and generator indexes, the order of the generators, duplicate presets,
// the samples' bounds, the 46 zero points after each sample, the stereo links, the sample types and rates, and the loop
// padding of looping zones. Five of the rules are stricter than the SoundFont 2 specification. The smpl chunk is
// required. The terminal records have to be named EOP, EOI and EOS. No two presets may share a bank and program. A
// sample has to be mono, right or left. Its rate has to be from 400 to 200,000 Hz.
std::vector<std::string> Check(std::span<const uint8_t> data);

} // namespace seqlib::sf2
