// SPDX-License-Identifier: MIT

// File names that are valid on Windows, macOS and Linux.

#pragma once

#include <string>

namespace seqlib
{

// What SafeFileName does with trailing dots and spaces. Windows strips them from names. "." and ".." refer to existing
// directories.
enum class TrailingRule
{
    kReplaceLast, // the last one becomes '_'
    kRemoveAll,   // all of them go
};

// Where SafeFileName puts the '_' that keeps a name from naming a Windows device.
enum class DeviceMark
{
    kAfterStem,  // "NUL.mid" becomes "NUL_.mid"
    kBeforeName, // "NUL.mid" becomes "_NUL.mid"
};

// How SafeFileName makes a name safe.
struct FileNameRules
{
    TrailingRule trailing = TrailingRule::kReplaceLast;
    DeviceMark device_mark = DeviceMark::kAfterStem;

    // Also reserve COM0, LPT0, CONIN$ and CONOUT$, and a device name followed by spaces, such as "CON .mid".
    bool all_devices = true;

    // The name that replaces an empty result.
    std::string empty_name;
};

// Makes `name` safe for a single file or directory name on Windows, macOS and Linux. Replaces the bytes below 0x20,
// <>:"/\|?* and each byte that isn't part of a valid UTF-8 character with '_'. `rules` sets what happens to trailing
// dots and spaces and to Windows device names such as CON or NUL. A device name is reserved in any case and with any
// extension: "nul.x.mid" names the device too. Other UTF-8 characters stay unchanged.
std::string SafeFileName(std::string name, const FileNameRules& rules = {});

// Checks whether `stem`, the part of a name before its first dot, is a device name that Windows reserves. ASCII letters
// match in either case. COM and LPT followed by a superscript 1, 2 or 3 count too.
bool IsDeviceName(std::string stem, const FileNameRules& rules = {});

} // namespace seqlib
