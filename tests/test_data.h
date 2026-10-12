// SPDX-License-Identifier: MIT

// The tests' data files, in tests/data.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace seqlib::test
{

// Returns the path of the test data file `name`, a path relative to tests/data with '/' between its parts.
std::filesystem::path TestDataPath(const std::string& name);

// Returns the bytes of the test data file `name`, or none if it can't be read.
std::vector<uint8_t> ReadTestData(const std::string& name);

// Returns the bytes that the test data file `name` holds as hexadecimal text, two digits a byte. Every character that
// isn't a hexadecimal digit is skipped, and an odd last digit is dropped. Returns none if the file can't be read. The
// data is kept as text so that every file in the repository is a text file.
std::vector<uint8_t> ReadHexTestData(const std::string& name);

} // namespace seqlib::test
