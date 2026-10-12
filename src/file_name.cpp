// SPDX-License-Identifier: MIT

#include "seqlib/file_name.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace seqlib
{
namespace
{

// Device names that every version of Windows reserves. The superscript digits are in UTF-8.
constexpr std::string_view kDevices[] = {
    "CON",  "PRN",  "AUX",  "NUL",         "COM1",        "COM2",        "COM3",        "COM4",       "COM5", "COM6",
    "COM7", "COM8", "COM9", "COM\xc2\xb9", "COM\xc2\xb2", "COM\xc2\xb3", "LPT1",        "LPT2",       "LPT3", "LPT4",
    "LPT5", "LPT6", "LPT7", "LPT8",        "LPT9",        "LPT\xc2\xb9", "LPT\xc2\xb2", "LPT\xc2\xb3"};

// Device names that FileNameRules::all_devices adds.
constexpr std::string_view kMoreDevices[] = {"CONIN$", "CONOUT$", "COM0", "LPT0"};

// The length of the UTF-8 character at `name[i]`, or 0 if the bytes there aren't a valid one: an overlong form, a
// surrogate, a code point above U+10FFFF or a cut sequence (RFC 3629).
std::size_t Utf8Length(const std::string& name, std::size_t i)
{
    const auto byte = [&name](std::size_t k) -> unsigned
    {
        return k < name.size() ? static_cast<unsigned char>(name[k]) : 0;
    };
    const unsigned lead = byte(i);
    if (lead < 0x80)
    {
        return 1;
    }

    // The character's length, and the range of its second byte, from its first byte.
    std::size_t length = 0;
    unsigned low = 0x80;
    unsigned high = 0xbf;
    if (lead >= 0xc2 && lead <= 0xdf)
    {
        length = 2;
    }
    else if (lead >= 0xe0 && lead <= 0xef)
    {
        length = 3;
        low = lead == 0xe0 ? 0xa0 : low;
        high = lead == 0xed ? 0x9f : high;
    }
    else if (lead >= 0xf0 && lead <= 0xf4)
    {
        length = 4;
        low = lead == 0xf0 ? 0x90 : low;
        high = lead == 0xf4 ? 0x8f : high;
    }
    else
    {
        return 0;
    }

    if (byte(i + 1) < low || byte(i + 1) > high)
    {
        return 0;
    }
    for (std::size_t k = 2; k < length; k++)
    {
        if (byte(i + k) < 0x80 || byte(i + k) > 0xbf)
        {
            return 0;
        }
    }

    return length;
}

} // namespace

std::string SafeFileName(std::string name, const FileNameRules& rules)
{
    // UTF-8 characters stay. Each byte that isn't part of a valid one becomes '_'.
    for (std::size_t i = 0; i < name.size();)
    {
        const std::size_t length = Utf8Length(name, i);
        if (length == 0)
        {
            name[i] = '_';
            i++;
        }
        else
        {
            i += length;
        }
    }

    constexpr std::string_view kNotAllowed = "<>:\"/\\|?*";
    for (char& c : name)
    {
        if (static_cast<unsigned char>(c) < 0x20 || kNotAllowed.find(c) != std::string_view::npos)
        {
            c = '_';
        }
    }

    // The name can also be a directory name. Windows strips trailing dots and spaces from directory names, and "." and
    // ".." refer to existing directories.
    if (rules.trailing == TrailingRule::kReplaceLast)
    {
        if (!name.empty() && (name.back() == '.' || name.back() == ' '))
        {
            name.back() = '_';
        }
    }
    else
    {
        while (!name.empty() && (name.back() == '.' || name.back() == ' '))
        {
            name.pop_back();
        }
    }
    if (name.empty())
    {
        return rules.empty_name;
    }

    // Windows reserves device names regardless of extension: "NUL.mid" and "NUL.x.mid" are invalid, but "NUL_.x.mid"
    // and "_NUL.x.mid" are allowed.
    const std::size_t stem_end = std::min(name.find('.'), name.size());
    if (IsDeviceName(name.substr(0, stem_end), rules))
    {
        name.insert(rules.device_mark == DeviceMark::kAfterStem ? stem_end : 0, "_");
    }

    return name;
}

bool IsDeviceName(std::string stem, const FileNameRules& rules)
{
    if (rules.all_devices)
    {
        while (!stem.empty() && stem.back() == ' ')
        {
            stem.pop_back();
        }
    }

    for (char& c : stem)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }

    if (std::find(std::begin(kDevices), std::end(kDevices), stem) != std::end(kDevices))
    {
        return true;
    }

    return rules.all_devices &&
           std::find(std::begin(kMoreDevices), std::end(kMoreDevices), stem) != std::end(kMoreDevices);
}

} // namespace seqlib
