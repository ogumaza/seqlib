// SPDX-License-Identifier: MIT

// Text for SFZ files (see text.h).

#include "seqlib/sfz/text.h"

#include <iomanip>
#include <ios>
#include <locale>
#include <sstream>

namespace seqlib::sfz
{

std::string Number(double v, int decimals)
{
    // A GUI toolkit can set the C locale, and a program can set the global C++ locale. Either can make the decimal
    // point a comma. The stream formats in the classic locale and writes a '.' under both. std::to_chars would too.
    // Apple's libc++ has std::to_chars for doubles only from macOS 13.3. As in printf, a negative precision means 6
    // digits after the point.
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(decimals) << v;
    std::string s = out.str();

    if (s.find('.') != std::string::npos)
    {
        while (s.back() == '0')
        {
            s.pop_back();
        }
        if (s.back() == '.')
        {
            s.pop_back();
        }
    }

    return s == "-0" ? "0" : s;
}

std::string CommentText(std::string text)
{
    for (char& c : text)
    {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
        {
            c = ' ';
        }
    }

    return text;
}

} // namespace seqlib::sfz
