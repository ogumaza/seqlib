// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sfz/text.h. The expected values are fixed: a change to them is a change of seqlib's output.

#include <clocale>
#include <cstddef>
#include <cstdio>
#include <locale>
#include <stdexcept>
#include <string>

#include "seqlib/sfz/text.h"
#include "test.h"

namespace
{

struct NumberCase
{
    double value;
    int decimals;
    const char* expected;
};

constexpr NumberCase kNumbers[] = {
    {0x0p+0, 6, "0"},
    {-0x0p+0, 6, "0"},
    {0x1p+0, 6, "1"},
    {-0x1p+0, 6, "-1"},
    {0x1p-1, 6, "0.5"},
    {-0x1.ad7f29abcaf48p-22, 6, "0"},
    {0x1.ad7f29abcaf48p-22, 6, "0"},
    {0x1.5555555555555p-2, 6, "0.333333"},
    {0x1.5555555555555p-1, 6, "0.666667"},
    {-0x1.5555555555555p-1, 6, "-0.666667"},
    {0x1.edd3c07ee0b0bp+6, 2, "123.46"},
    {0x1.9p+6, 0, "100"},
    {0x1.c6bf52634p+49, 6, "1000000000000000"},
    {0x1.999999999999ap-4, 1, "0.1"},
    {0x1.34a456d5cfaadp+10, 3, "1234.568"},
    {-0x1.47ae147ae147bp-5, 1, "0"},
    {0x1.8fffffe5280d6p+6, 6, "100"},
    {-0x1.8fffffe5280d6p+6, 6, "-100"},
    {0x1.fffp-1, 6, "0.999878"},
    {0x1.ad7f29abcaf48p-24, 6, "0"},
    {-0x1.ad7f29abcaf48p-24, 6, "0"},
    {0x1.fffcp+14, 6, "32767"},
    {-0x1.77p+13, 0, "-12000"},
    {0x1.01f31f46ed246p-13, 6, "0.000123"},
};

struct CommentCase
{
    const char* text;
    std::size_t size; // the text can hold a NUL
    const char* expected;
};

constexpr CommentCase kComments[] = {
    {"", 0, ""},
    {"plain text", 10, "plain text"},
    {"line\x0a"
     "break",
     10, "line break"},
    {"tab\x09here", 8, "tab here"},
    {"\x0d\x0a", 2, "  "},
    {"nul\x00inside", 10, "nul inside"},
    {"\x01\x1f ~\x7f", 5, "   ~ "},
    {"UTF-8: \xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x8e\xb5", 21,
     "UTF-8: \xc3\xa9 \xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x8e\xb5"},
};

// The curve from CC10 to pan.
const char* const kPanCurve = R"(// CC10 to pan
<curve> curve_index=7
v000=-1 v001=-0.984375 v002=-0.96875 v003=-0.953125 v004=-0.9375 v005=-0.921875 v006=-0.90625 v007=-0.890625
v008=-0.875 v009=-0.859375 v010=-0.84375 v011=-0.828125 v012=-0.8125 v013=-0.796875 v014=-0.78125 v015=-0.765625
v016=-0.75 v017=-0.734375 v018=-0.71875 v019=-0.703125 v020=-0.6875 v021=-0.671875 v022=-0.65625 v023=-0.640625
v024=-0.625 v025=-0.609375 v026=-0.59375 v027=-0.578125 v028=-0.5625 v029=-0.546875 v030=-0.53125 v031=-0.515625
v032=-0.5 v033=-0.484375 v034=-0.46875 v035=-0.453125 v036=-0.4375 v037=-0.421875 v038=-0.40625 v039=-0.390625
v040=-0.375 v041=-0.359375 v042=-0.34375 v043=-0.328125 v044=-0.3125 v045=-0.296875 v046=-0.28125 v047=-0.265625
v048=-0.25 v049=-0.234375 v050=-0.21875 v051=-0.203125 v052=-0.1875 v053=-0.171875 v054=-0.15625 v055=-0.140625
v056=-0.125 v057=-0.109375 v058=-0.09375 v059=-0.078125 v060=-0.0625 v061=-0.046875 v062=-0.03125 v063=-0.015625
v064=0 v065=0.015625 v066=0.03125 v067=0.046875 v068=0.0625 v069=0.078125 v070=0.09375 v071=0.109375
v072=0.125 v073=0.140625 v074=0.15625 v075=0.171875 v076=0.1875 v077=0.203125 v078=0.21875 v079=0.234375
v080=0.25 v081=0.265625 v082=0.28125 v083=0.296875 v084=0.3125 v085=0.328125 v086=0.34375 v087=0.359375
v088=0.375 v089=0.390625 v090=0.40625 v091=0.421875 v092=0.4375 v093=0.453125 v094=0.46875 v095=0.484375
v096=0.5 v097=0.515625 v098=0.53125 v099=0.546875 v100=0.5625 v101=0.578125 v102=0.59375 v103=0.609375
v104=0.625 v105=0.640625 v106=0.65625 v107=0.671875 v108=0.6875 v109=0.703125 v110=0.71875 v111=0.734375
v112=0.75 v113=0.765625 v114=0.78125 v115=0.796875 v116=0.8125 v117=0.828125 v118=0.84375 v119=0.859375
v120=0.875 v121=0.890625 v122=0.90625 v123=0.921875 v124=0.9375 v125=0.953125 v126=0.96875 v127=0.984375

)";

// Values less than a ten-millionth from 0, on both sides of it, print as "0", never "-0".
const char* const kTinyCurve = R"(// Tiny values
<curve> curve_index=12
v000=0 v001=0 v002=0 v003=0 v004=0 v005=0 v006=0 v007=0
v008=0 v009=0 v010=0 v011=0 v012=0 v013=0 v014=0 v015=0
v016=0 v017=0 v018=0 v019=0 v020=0 v021=0 v022=0 v023=0
v024=0 v025=0 v026=0 v027=0 v028=0 v029=0 v030=0 v031=0
v032=0 v033=0 v034=0 v035=0 v036=0 v037=0 v038=0 v039=0
v040=0 v041=0 v042=0 v043=0 v044=0 v045=0 v046=0 v047=0
v048=0 v049=0 v050=0 v051=0 v052=0 v053=0 v054=0 v055=0
v056=0 v057=0 v058=0 v059=0 v060=0 v061=0 v062=0 v063=0
v064=0 v065=0 v066=0 v067=0 v068=0 v069=0 v070=0 v071=0
v072=0 v073=0 v074=0 v075=0 v076=0 v077=0 v078=0 v079=0
v080=0 v081=0 v082=0 v083=0 v084=0 v085=0 v086=0 v087=0
v088=0 v089=0 v090=0 v091=0 v092=0 v093=0 v094=0 v095=0
v096=0 v097=0 v098=0 v099=0 v100=0 v101=0 v102=0 v103=0
v104=0 v105=0 v106=0 v107=0 v108=0 v109=0 v110=0 v111=0
v112=0 v113=0 v114=0 v115=0 v116=0 v117=0 v118=0 v119=0
v120=0 v121=0 v122=0 v123=0 v124=0 v125=0 v126=0 v127=0

)";

const char* const kSquareCurve = R"(// Square
<curve> curve_index=255
v000=0 v001=0.000062 v002=0.000248 v003=0.000558 v004=0.000992 v005=0.00155 v006=0.002232 v007=0.003038
v008=0.003968 v009=0.005022 v010=0.0062 v011=0.007502 v012=0.008928 v013=0.010478 v014=0.012152 v015=0.01395
v016=0.015872 v017=0.017918 v018=0.020088 v019=0.022382 v020=0.0248 v021=0.027342 v022=0.030008 v023=0.032798
v024=0.035712 v025=0.03875 v026=0.041912 v027=0.045198 v028=0.048608 v029=0.052142 v030=0.0558 v031=0.059582
v032=0.063488 v033=0.067518 v034=0.071672 v035=0.07595 v036=0.080352 v037=0.084878 v038=0.089528 v039=0.094302
v040=0.0992 v041=0.104222 v042=0.109368 v043=0.114638 v044=0.120032 v045=0.12555 v046=0.131192 v047=0.136958
v048=0.142848 v049=0.148862 v050=0.155 v051=0.161262 v052=0.167648 v053=0.174158 v054=0.180792 v055=0.18755
v056=0.194432 v057=0.201438 v058=0.208568 v059=0.215822 v060=0.2232 v061=0.230702 v062=0.238328 v063=0.246078
v064=0.253953 v065=0.261951 v066=0.270073 v067=0.278319 v068=0.286689 v069=0.295183 v070=0.303801 v071=0.312543
v072=0.321409 v073=0.330399 v074=0.339513 v075=0.348751 v076=0.358113 v077=0.367599 v078=0.377209 v079=0.386943
v080=0.396801 v081=0.406783 v082=0.416889 v083=0.427119 v084=0.437473 v085=0.447951 v086=0.458553 v087=0.469279
v088=0.480129 v089=0.491103 v090=0.502201 v091=0.513423 v092=0.524769 v093=0.536239 v094=0.547833 v095=0.559551
v096=0.571393 v097=0.583359 v098=0.595449 v099=0.607663 v100=0.620001 v101=0.632463 v102=0.645049 v103=0.657759
v104=0.670593 v105=0.683551 v106=0.696633 v107=0.709839 v108=0.723169 v109=0.736623 v110=0.750202 v111=0.763904
v112=0.77773 v113=0.79168 v114=0.805754 v115=0.819952 v116=0.834274 v117=0.84872 v118=0.86329 v119=0.877984
v120=0.892802 v121=0.907744 v122=0.92281 v123=0.938 v124=0.953314 v125=0.968752 v126=0.984314 v127=1

)";

SEQLIB_TEST(sfz_text, NumberText)
{
    for (const NumberCase& c : kNumbers)
    {
        seqlib::test::SetContext(c.expected);
        SEQLIB_CHECK(seqlib::sfz::Number(c.value, c.decimals) == c.expected);
    }
}

SEQLIB_TEST(sfz_text, NumberInACommaLocale)
{
    // A GUI toolkit can set the C locale, and a program can set the global C++ locale. Either can make the decimal
    // point a comma. The test sets both to the first locale with a comma that the machine has. Without one, the test
    // checks nothing.
    const char* const kCommaLocales[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8",
                                         "fr_FR.utf8",  "de_DE",      "German_Germany.1252"};
    const std::string c_before = std::setlocale(LC_ALL, nullptr);
    const std::locale before;

    bool comma = false;
    for (const char* name : kCommaLocales)
    {
        try
        {
            // A locale with a name sets the C locale too.
            std::locale::global(std::locale(name));
        }
        catch (const std::runtime_error&)
        {
            continue;
        }
        if (std::string(std::localeconv()->decimal_point) == "," &&
            std::use_facet<std::numpunct<char>>(std::locale()).decimal_point() == ',')
        {
            comma = true;
            break;
        }
    }

    const std::string half = seqlib::sfz::Number(0.5, 6);
    const std::string negative = seqlib::sfz::Number(-1.25, 2);
    std::locale::global(before);
    std::setlocale(LC_ALL, c_before.c_str());

    if (!comma)
    {
        std::printf("note: NumberInACommaLocale checked nothing. No locale here has a decimal comma.\n");
        return;
    }

    SEQLIB_CHECK(half == "0.5");
    SEQLIB_CHECK(negative == "-1.25");
}

SEQLIB_TEST(sfz_text, CurveText)
{
    auto pan = [](int v)
    {
        return (v - 64) / 64.0;
    };
    auto tiny = [](int v)
    {
        return (v - 64) * -1e-9;
    };
    auto square = [](int v)
    {
        return v * v / 16129.0;
    };

    SEQLIB_CHECK(seqlib::sfz::Curve(7, "CC10 to pan", pan) == kPanCurve);
    SEQLIB_CHECK(seqlib::sfz::Curve(12, "Tiny values", tiny) == kTinyCurve);
    SEQLIB_CHECK(seqlib::sfz::Curve(255, "Square", square) == kSquareCurve);
}

SEQLIB_TEST(sfz_text, CommentTexts)
{
    for (const CommentCase& c : kComments)
    {
        const std::string text(c.text, c.size);
        SEQLIB_CHECK(seqlib::sfz::CommentText(text) == std::string(c.expected, c.size));
    }
}

} // namespace
