// SPDX-License-Identifier: MIT

// Text for SFZ files: numbers, controller curves and comments.

#pragma once

#include <cstdio>
#include <string>

namespace seqlib::sfz
{

// Formats `v` with at most `decimals` fractional digits. Trailing zeros go, and "-0" becomes "0". The decimal point is
// '.' in any locale.
std::string Number(double v, int decimals);

// Writes a <curve> with index `index` that maps controller values 0-127 through `f`, after a comment line with `what`.
// Each value has at most 6 fractional digits.
template <typename F>
std::string Curve(int index, const char* what, F f)
{
    std::string s = "// " + std::string(what) + "\n<curve> curve_index=" + std::to_string(index);
    for (int v = 0; v < 128; v++)
    {
        char name[8];
        std::snprintf(name, sizeof(name), "v%03d", v);
        s += (v % 8 == 0 ? "\n" : " ") + std::string(name) + "=" + Number(f(v), 6);
    }

    return s + "\n\n";
}

// Makes `text` safe for a single-line comment by replacing control characters, including line breaks, with spaces.
std::string CommentText(std::string text);

} // namespace seqlib::sfz
