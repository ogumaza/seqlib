// SPDX-License-Identifier: MIT

// The byte helpers of seqlib's sources: loads and appends of 16-bit and 32-bit values, big-endian for Standard MIDI
// Files and little-endian for RIFF files, and appends of four-character codes. The header is private and isn't
// installed.

#pragma once

#include <cstdint>
#include <vector>

namespace seqlib::detail
{

// The big-endian 16-bit value at `p`.
inline uint16_t LoadBe16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] << 8 | p[1]);
}

// The big-endian 32-bit value at `p`.
inline uint32_t LoadBe32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 |
           p[3];
}

// The little-endian 16-bit value at `p`.
inline uint16_t LoadLe16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | p[1] << 8);
}

// The little-endian 32-bit value at `p`.
inline uint32_t LoadLe32(const uint8_t* p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
}

// Appends `v` to `out`, big-endian or little-endian.
inline void AppendBe16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

inline void AppendBe32(std::vector<uint8_t>& out, uint32_t v)
{
    out.insert(out.end(), {static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8),
                           static_cast<uint8_t>(v)});
}

inline void AppendLe16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

inline void AppendLe32(std::vector<uint8_t>& out, uint32_t v)
{
    AppendLe16(out, static_cast<uint16_t>(v));
    AppendLe16(out, static_cast<uint16_t>(v >> 16));
}

// Appends the four characters at `id`, such as a RIFF chunk's ID.
inline void AppendFourcc(std::vector<uint8_t>& out, const char* id)
{
    out.insert(out.end(), id, id + 4);
}

} // namespace seqlib::detail
