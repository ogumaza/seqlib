// SPDX-License-Identifier: MIT

// The tests' data (see test_data.h).

#include "test_data.h"

#include <cctype>
#include <fstream>
#include <ios>
#include <iterator>

namespace seqlib::test
{

std::filesystem::path TestDataPath(const std::string& name)
{
    const std::string kRoot = SEQLIB_TEST_DATA;
    const std::filesystem::path base(std::u8string(kRoot.begin(), kRoot.end()));
    return base / std::filesystem::path(std::u8string(name.begin(), name.end()));
}

std::vector<uint8_t> ReadTestData(const std::string& name)
{
    std::ifstream f(TestDataPath(name), std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::vector<uint8_t> FromHex(std::string_view hex)
{
    auto digit = [](uint8_t c)
    {
        return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10;
    };

    std::vector<uint8_t> bytes;
    int high = -1;
    for (const char character : hex)
    {
        const auto c = static_cast<uint8_t>(character);
        if (!std::isxdigit(c))
        {
            continue;
        }

        if (high < 0)
        {
            high = digit(c);
        }
        else
        {
            bytes.push_back(static_cast<uint8_t>(high << 4 | digit(c)));
            high = -1;
        }
    }

    return bytes;
}

std::vector<uint8_t> ReadHexTestData(const std::string& name)
{
    const std::vector<uint8_t> text = ReadTestData(name);
    return FromHex(std::string(text.begin(), text.end()));
}

} // namespace seqlib::test
