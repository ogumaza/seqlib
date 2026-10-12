// SPDX-License-Identifier: MIT

// Checks SoundFont 2 and MIDI files: seqlib::sf2::Check on each .sf2 file and seqlib::midi::Problems on each .mid file.
// Prints each file's problems, and exits with 1 if any file has one.
//
// usage: seqlib_check <file.sf2|file.mid>...

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "seqlib/midi/reader.h"
#include "seqlib/sf2/reader.h"

namespace
{

// Returns the bytes of the file at `path`, or none if it can't be read.
std::optional<std::vector<uint8_t>> ReadFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad())
    {
        return std::nullopt;
    }

    return bytes;
}

// Returns the problems of the file at `path`, by its extension.
std::vector<std::string> FileProblems(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".sf2" && extension != ".mid" && extension != ".midi")
    {
        return {"not a .sf2 or .mid file"};
    }

    const std::optional<std::vector<uint8_t>> bytes = ReadFile(path);
    if (!bytes)
    {
        return {"can't be read"};
    }

    return extension == ".sf2" ? seqlib::sf2::Check(*bytes) : seqlib::midi::Problems(*bytes);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fputs("usage: seqlib_check <file.sf2|file.mid>...\n", stderr);
        return 2;
    }

    int bad = 0;
    for (int i = 1; i < argc; i++)
    {
        const std::vector<std::string> problems = FileProblems(argv[i]);
        if (problems.empty())
        {
            continue;
        }

        bad++;
        std::printf("%s\n", argv[i]);
        for (const std::string& problem : problems)
        {
            std::printf("  %s\n", problem.c_str());
        }
    }

    std::printf("%d files, %d with problems\n", argc - 1, bad);

    return bad == 0 ? 0 : 1;
}
