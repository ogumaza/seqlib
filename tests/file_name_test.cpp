// SPDX-License-Identifier: MIT

// Golden tests for seqlib/file_name.h. Each case gives the name with the default rules and with kOtherRules. The
// expected names are fixed: a change to them is a change of seqlib's output.

#include "seqlib/file_name.h"

#include <cstddef>
#include <string>

#include "test.h"

namespace
{

struct NameCase
{
    const char* name;
    std::size_t size; // the name can hold a NUL
    const char* with_defaults;
    const char* with_other_rules;
};

// Forbidden characters, trailing dots and spaces, every device name in both cases, with an extension and with spaces,
// superscript digits and UTF-8.
constexpr NameCase kNames[] = {
    {"", 0, "", "output"},
    {".", 1, "_", "output"},
    {"..", 2, "._", "output"},
    {"...", 3, ".._", "output"},
    {" ", 1, "_", "output"},
    {"  ", 2, " _", "output"},
    {"a.", 2, "a_", "a"},
    {"a ", 2, "a_", "a"},
    {"a. ", 3, "a._", "a"},
    {"a .", 3, "a _", "a"},
    {"a..", 3, "a._", "a"},
    {".x. y", 5, ".x. y", ".x. y"},
    {"BGM.", 4, "BGM_", "BGM"},
    {"BGM ", 4, "BGM_", "BGM"},
    {"BGM_01", 6, "BGM_01", "BGM_01"},
    {"a<b>c:d\"e/f\\g|h?i*j", 19, "a_b_c_d_e_f_g_h_i_j", "a_b_c_d_e_f_g_h_i_j"},
    {"../x", 4, ".._x", ".._x"},
    {"a\x09"
     "b\x00"
     "c",
     5, "a_b_c", "a_b_c"},
    {"\x01"
     "\x1f"
     "\x7f",
     3, "__\x7f", "__\x7f"},
    {"x.CON", 5, "x.CON", "x.CON"},
    {".CON", 4, ".CON", ".CON"},
    {" CON", 4, " CON", " CON"},
    {"CON..mid", 8, "CON_..mid", "_CON..mid"},
    {"CON.", 4, "CON_", "_CON"},
    {"CON ", 4, "CON_", "_CON"},
    {"CON  .mid", 9, "CON  _.mid", "CON  .mid"},
    {"CON .mid", 8, "CON _.mid", "CON .mid"},
    {"CON. mid", 8, "CON_. mid", "_CON. mid"},
    {"COM1 .x", 7, "COM1 _.x", "COM1 .x"},
    {"CLOCK$", 6, "CLOCK$", "CLOCK$"},
    {"COM10", 5, "COM10", "COM10"},
    {"LPT10", 5, "LPT10", "LPT10"},
    {"COM", 3, "COM", "COM"},
    {"CO", 2, "CO", "CO"},
    {"COM\xe2"
     "\x81"
     "\xb4",
     6,
     "COM\xe2"
     "\x81"
     "\xb4",
     "COM\xe2"
     "\x81"
     "\xb4"},
    {"\xe6"
     "\x97"
     "\xa5"
     "\xe6"
     "\x9c"
     "\xac"
     "\xe8"
     "\xaa"
     "\x9e",
     9,
     "\xe6"
     "\x97"
     "\xa5"
     "\xe6"
     "\x9c"
     "\xac"
     "\xe8"
     "\xaa"
     "\x9e",
     "\xe6"
     "\x97"
     "\xa5"
     "\xe6"
     "\x9c"
     "\xac"
     "\xe8"
     "\xaa"
     "\x9e"},
    {"\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"
     ".mid",
     9,
     "\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"
     ".mid",
     "\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"
     ".mid"},
    {"\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"
     ".",
     6,
     "\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"
     "_",
     "\xc3"
     "\xa9"
     "t\xc3"
     "\xa9"},
    {"caf\xc3"
     "\xa9"
     " ?.mid",
     11,
     "caf\xc3"
     "\xa9"
     " _.mid",
     "caf\xc3"
     "\xa9"
     " _.mid"},
    {"CON", 3, "CON_", "_CON"},
    {"con.mid", 7, "con_.mid", "_con.mid"},
    {"CON.x.sf2", 9, "CON_.x.sf2", "_CON.x.sf2"},
    {"CON ", 4, "CON_", "_CON"},
    {"CON .sfz", 8, "CON _.sfz", "CON .sfz"},
    {"PRN", 3, "PRN_", "_PRN"},
    {"prn.mid", 7, "prn_.mid", "_prn.mid"},
    {"PRN.x.sf2", 9, "PRN_.x.sf2", "_PRN.x.sf2"},
    {"PRN ", 4, "PRN_", "_PRN"},
    {"PRN .sfz", 8, "PRN _.sfz", "PRN .sfz"},
    {"AUX", 3, "AUX_", "_AUX"},
    {"aux.mid", 7, "aux_.mid", "_aux.mid"},
    {"AUX.x.sf2", 9, "AUX_.x.sf2", "_AUX.x.sf2"},
    {"AUX ", 4, "AUX_", "_AUX"},
    {"AUX .sfz", 8, "AUX _.sfz", "AUX .sfz"},
    {"NUL", 3, "NUL_", "_NUL"},
    {"nul.mid", 7, "nul_.mid", "_nul.mid"},
    {"NUL.x.sf2", 9, "NUL_.x.sf2", "_NUL.x.sf2"},
    {"NUL ", 4, "NUL_", "_NUL"},
    {"NUL .sfz", 8, "NUL _.sfz", "NUL .sfz"},
    {"CONIN$", 6, "CONIN$_", "CONIN$"},
    {"conin$.mid", 10, "conin$_.mid", "conin$.mid"},
    {"CONIN$.x.sf2", 12, "CONIN$_.x.sf2", "CONIN$.x.sf2"},
    {"CONIN$ ", 7, "CONIN$_", "CONIN$"},
    {"CONIN$ .sfz", 11, "CONIN$ _.sfz", "CONIN$ .sfz"},
    {"CONOUT$", 7, "CONOUT$_", "CONOUT$"},
    {"conout$.mid", 11, "conout$_.mid", "conout$.mid"},
    {"CONOUT$.x.sf2", 13, "CONOUT$_.x.sf2", "CONOUT$.x.sf2"},
    {"CONOUT$ ", 8, "CONOUT$_", "CONOUT$"},
    {"CONOUT$ .sfz", 12, "CONOUT$ _.sfz", "CONOUT$ .sfz"},
    {"COM0", 4, "COM0_", "COM0"},
    {"com0.mid", 8, "com0_.mid", "com0.mid"},
    {"COM0.x.sf2", 10, "COM0_.x.sf2", "COM0.x.sf2"},
    {"COM0 ", 5, "COM0_", "COM0"},
    {"COM0 .sfz", 9, "COM0 _.sfz", "COM0 .sfz"},
    {"COM1", 4, "COM1_", "_COM1"},
    {"com1.mid", 8, "com1_.mid", "_com1.mid"},
    {"COM1.x.sf2", 10, "COM1_.x.sf2", "_COM1.x.sf2"},
    {"COM1 ", 5, "COM1_", "_COM1"},
    {"COM1 .sfz", 9, "COM1 _.sfz", "COM1 .sfz"},
    {"COM2", 4, "COM2_", "_COM2"},
    {"com2.mid", 8, "com2_.mid", "_com2.mid"},
    {"COM2.x.sf2", 10, "COM2_.x.sf2", "_COM2.x.sf2"},
    {"COM2 ", 5, "COM2_", "_COM2"},
    {"COM2 .sfz", 9, "COM2 _.sfz", "COM2 .sfz"},
    {"COM3", 4, "COM3_", "_COM3"},
    {"com3.mid", 8, "com3_.mid", "_com3.mid"},
    {"COM3.x.sf2", 10, "COM3_.x.sf2", "_COM3.x.sf2"},
    {"COM3 ", 5, "COM3_", "_COM3"},
    {"COM3 .sfz", 9, "COM3 _.sfz", "COM3 .sfz"},
    {"COM4", 4, "COM4_", "_COM4"},
    {"com4.mid", 8, "com4_.mid", "_com4.mid"},
    {"COM4.x.sf2", 10, "COM4_.x.sf2", "_COM4.x.sf2"},
    {"COM4 ", 5, "COM4_", "_COM4"},
    {"COM4 .sfz", 9, "COM4 _.sfz", "COM4 .sfz"},
    {"COM5", 4, "COM5_", "_COM5"},
    {"com5.mid", 8, "com5_.mid", "_com5.mid"},
    {"COM5.x.sf2", 10, "COM5_.x.sf2", "_COM5.x.sf2"},
    {"COM5 ", 5, "COM5_", "_COM5"},
    {"COM5 .sfz", 9, "COM5 _.sfz", "COM5 .sfz"},
    {"COM6", 4, "COM6_", "_COM6"},
    {"com6.mid", 8, "com6_.mid", "_com6.mid"},
    {"COM6.x.sf2", 10, "COM6_.x.sf2", "_COM6.x.sf2"},
    {"COM6 ", 5, "COM6_", "_COM6"},
    {"COM6 .sfz", 9, "COM6 _.sfz", "COM6 .sfz"},
    {"COM7", 4, "COM7_", "_COM7"},
    {"com7.mid", 8, "com7_.mid", "_com7.mid"},
    {"COM7.x.sf2", 10, "COM7_.x.sf2", "_COM7.x.sf2"},
    {"COM7 ", 5, "COM7_", "_COM7"},
    {"COM7 .sfz", 9, "COM7 _.sfz", "COM7 .sfz"},
    {"COM8", 4, "COM8_", "_COM8"},
    {"com8.mid", 8, "com8_.mid", "_com8.mid"},
    {"COM8.x.sf2", 10, "COM8_.x.sf2", "_COM8.x.sf2"},
    {"COM8 ", 5, "COM8_", "_COM8"},
    {"COM8 .sfz", 9, "COM8 _.sfz", "COM8 .sfz"},
    {"COM9", 4, "COM9_", "_COM9"},
    {"com9.mid", 8, "com9_.mid", "_com9.mid"},
    {"COM9.x.sf2", 10, "COM9_.x.sf2", "_COM9.x.sf2"},
    {"COM9 ", 5, "COM9_", "_COM9"},
    {"COM9 .sfz", 9, "COM9 _.sfz", "COM9 .sfz"},
    {"LPT0", 4, "LPT0_", "LPT0"},
    {"lpt0.mid", 8, "lpt0_.mid", "lpt0.mid"},
    {"LPT0.x.sf2", 10, "LPT0_.x.sf2", "LPT0.x.sf2"},
    {"LPT0 ", 5, "LPT0_", "LPT0"},
    {"LPT0 .sfz", 9, "LPT0 _.sfz", "LPT0 .sfz"},
    {"LPT1", 4, "LPT1_", "_LPT1"},
    {"lpt1.mid", 8, "lpt1_.mid", "_lpt1.mid"},
    {"LPT1.x.sf2", 10, "LPT1_.x.sf2", "_LPT1.x.sf2"},
    {"LPT1 ", 5, "LPT1_", "_LPT1"},
    {"LPT1 .sfz", 9, "LPT1 _.sfz", "LPT1 .sfz"},
    {"LPT2", 4, "LPT2_", "_LPT2"},
    {"lpt2.mid", 8, "lpt2_.mid", "_lpt2.mid"},
    {"LPT2.x.sf2", 10, "LPT2_.x.sf2", "_LPT2.x.sf2"},
    {"LPT2 ", 5, "LPT2_", "_LPT2"},
    {"LPT2 .sfz", 9, "LPT2 _.sfz", "LPT2 .sfz"},
    {"LPT3", 4, "LPT3_", "_LPT3"},
    {"lpt3.mid", 8, "lpt3_.mid", "_lpt3.mid"},
    {"LPT3.x.sf2", 10, "LPT3_.x.sf2", "_LPT3.x.sf2"},
    {"LPT3 ", 5, "LPT3_", "_LPT3"},
    {"LPT3 .sfz", 9, "LPT3 _.sfz", "LPT3 .sfz"},
    {"LPT4", 4, "LPT4_", "_LPT4"},
    {"lpt4.mid", 8, "lpt4_.mid", "_lpt4.mid"},
    {"LPT4.x.sf2", 10, "LPT4_.x.sf2", "_LPT4.x.sf2"},
    {"LPT4 ", 5, "LPT4_", "_LPT4"},
    {"LPT4 .sfz", 9, "LPT4 _.sfz", "LPT4 .sfz"},
    {"LPT5", 4, "LPT5_", "_LPT5"},
    {"lpt5.mid", 8, "lpt5_.mid", "_lpt5.mid"},
    {"LPT5.x.sf2", 10, "LPT5_.x.sf2", "_LPT5.x.sf2"},
    {"LPT5 ", 5, "LPT5_", "_LPT5"},
    {"LPT5 .sfz", 9, "LPT5 _.sfz", "LPT5 .sfz"},
    {"LPT6", 4, "LPT6_", "_LPT6"},
    {"lpt6.mid", 8, "lpt6_.mid", "_lpt6.mid"},
    {"LPT6.x.sf2", 10, "LPT6_.x.sf2", "_LPT6.x.sf2"},
    {"LPT6 ", 5, "LPT6_", "_LPT6"},
    {"LPT6 .sfz", 9, "LPT6 _.sfz", "LPT6 .sfz"},
    {"LPT7", 4, "LPT7_", "_LPT7"},
    {"lpt7.mid", 8, "lpt7_.mid", "_lpt7.mid"},
    {"LPT7.x.sf2", 10, "LPT7_.x.sf2", "_LPT7.x.sf2"},
    {"LPT7 ", 5, "LPT7_", "_LPT7"},
    {"LPT7 .sfz", 9, "LPT7 _.sfz", "LPT7 .sfz"},
    {"LPT8", 4, "LPT8_", "_LPT8"},
    {"lpt8.mid", 8, "lpt8_.mid", "_lpt8.mid"},
    {"LPT8.x.sf2", 10, "LPT8_.x.sf2", "_LPT8.x.sf2"},
    {"LPT8 ", 5, "LPT8_", "_LPT8"},
    {"LPT8 .sfz", 9, "LPT8 _.sfz", "LPT8 .sfz"},
    {"LPT9", 4, "LPT9_", "_LPT9"},
    {"lpt9.mid", 8, "lpt9_.mid", "_lpt9.mid"},
    {"LPT9.x.sf2", 10, "LPT9_.x.sf2", "_LPT9.x.sf2"},
    {"LPT9 ", 5, "LPT9_", "_LPT9"},
    {"LPT9 .sfz", 9, "LPT9 _.sfz", "LPT9 .sfz"},
    {"COM\xc2"
     "\xb9",
     5,
     "COM\xc2"
     "\xb9"
     "_",
     "_COM\xc2"
     "\xb9"},
    {"com\xc2"
     "\xb9"
     ".mid",
     9,
     "com\xc2"
     "\xb9"
     "_.mid",
     "_com\xc2"
     "\xb9"
     ".mid"},
    {"COM\xc2"
     "\xb9"
     ".x.sf2",
     11,
     "COM\xc2"
     "\xb9"
     "_.x.sf2",
     "_COM\xc2"
     "\xb9"
     ".x.sf2"},
    {"COM\xc2"
     "\xb9"
     " ",
     6,
     "COM\xc2"
     "\xb9"
     "_",
     "_COM\xc2"
     "\xb9"},
    {"COM\xc2"
     "\xb9"
     " .sfz",
     10,
     "COM\xc2"
     "\xb9"
     " _.sfz",
     "COM\xc2"
     "\xb9"
     " .sfz"},
    {"COM\xc2"
     "\xb2",
     5,
     "COM\xc2"
     "\xb2"
     "_",
     "_COM\xc2"
     "\xb2"},
    {"com\xc2"
     "\xb2"
     ".mid",
     9,
     "com\xc2"
     "\xb2"
     "_.mid",
     "_com\xc2"
     "\xb2"
     ".mid"},
    {"COM\xc2"
     "\xb2"
     ".x.sf2",
     11,
     "COM\xc2"
     "\xb2"
     "_.x.sf2",
     "_COM\xc2"
     "\xb2"
     ".x.sf2"},
    {"COM\xc2"
     "\xb2"
     " ",
     6,
     "COM\xc2"
     "\xb2"
     "_",
     "_COM\xc2"
     "\xb2"},
    {"COM\xc2"
     "\xb2"
     " .sfz",
     10,
     "COM\xc2"
     "\xb2"
     " _.sfz",
     "COM\xc2"
     "\xb2"
     " .sfz"},
    {"COM\xc2"
     "\xb3",
     5,
     "COM\xc2"
     "\xb3"
     "_",
     "_COM\xc2"
     "\xb3"},
    {"com\xc2"
     "\xb3"
     ".mid",
     9,
     "com\xc2"
     "\xb3"
     "_.mid",
     "_com\xc2"
     "\xb3"
     ".mid"},
    {"COM\xc2"
     "\xb3"
     ".x.sf2",
     11,
     "COM\xc2"
     "\xb3"
     "_.x.sf2",
     "_COM\xc2"
     "\xb3"
     ".x.sf2"},
    {"COM\xc2"
     "\xb3"
     " ",
     6,
     "COM\xc2"
     "\xb3"
     "_",
     "_COM\xc2"
     "\xb3"},
    {"COM\xc2"
     "\xb3"
     " .sfz",
     10,
     "COM\xc2"
     "\xb3"
     " _.sfz",
     "COM\xc2"
     "\xb3"
     " .sfz"},
    {"LPT\xc2"
     "\xb9",
     5,
     "LPT\xc2"
     "\xb9"
     "_",
     "_LPT\xc2"
     "\xb9"},
    {"lpt\xc2"
     "\xb9"
     ".mid",
     9,
     "lpt\xc2"
     "\xb9"
     "_.mid",
     "_lpt\xc2"
     "\xb9"
     ".mid"},
    {"LPT\xc2"
     "\xb9"
     ".x.sf2",
     11,
     "LPT\xc2"
     "\xb9"
     "_.x.sf2",
     "_LPT\xc2"
     "\xb9"
     ".x.sf2"},
    {"LPT\xc2"
     "\xb9"
     " ",
     6,
     "LPT\xc2"
     "\xb9"
     "_",
     "_LPT\xc2"
     "\xb9"},
    {"LPT\xc2"
     "\xb9"
     " .sfz",
     10,
     "LPT\xc2"
     "\xb9"
     " _.sfz",
     "LPT\xc2"
     "\xb9"
     " .sfz"},
    {"LPT\xc2"
     "\xb2",
     5,
     "LPT\xc2"
     "\xb2"
     "_",
     "_LPT\xc2"
     "\xb2"},
    {"lpt\xc2"
     "\xb2"
     ".mid",
     9,
     "lpt\xc2"
     "\xb2"
     "_.mid",
     "_lpt\xc2"
     "\xb2"
     ".mid"},
    {"LPT\xc2"
     "\xb2"
     ".x.sf2",
     11,
     "LPT\xc2"
     "\xb2"
     "_.x.sf2",
     "_LPT\xc2"
     "\xb2"
     ".x.sf2"},
    {"LPT\xc2"
     "\xb2"
     " ",
     6,
     "LPT\xc2"
     "\xb2"
     "_",
     "_LPT\xc2"
     "\xb2"},
    {"LPT\xc2"
     "\xb2"
     " .sfz",
     10,
     "LPT\xc2"
     "\xb2"
     " _.sfz",
     "LPT\xc2"
     "\xb2"
     " .sfz"},
    {"LPT\xc2"
     "\xb3",
     5,
     "LPT\xc2"
     "\xb3"
     "_",
     "_LPT\xc2"
     "\xb3"},
    {"lpt\xc2"
     "\xb3"
     ".mid",
     9,
     "lpt\xc2"
     "\xb3"
     "_.mid",
     "_lpt\xc2"
     "\xb3"
     ".mid"},
    {"LPT\xc2"
     "\xb3"
     ".x.sf2",
     11,
     "LPT\xc2"
     "\xb3"
     "_.x.sf2",
     "_LPT\xc2"
     "\xb3"
     ".x.sf2"},
    {"LPT\xc2"
     "\xb3"
     " ",
     6,
     "LPT\xc2"
     "\xb3"
     "_",
     "_LPT\xc2"
     "\xb3"},
    {"LPT\xc2"
     "\xb3"
     " .sfz",
     10,
     "LPT\xc2"
     "\xb3"
     " _.sfz",
     "LPT\xc2"
     "\xb3"
     " .sfz"},
};

// Every trailing dot and space removed, the mark before a device name, all_devices off, and "output" for an empty name.
const seqlib::FileNameRules kOtherRules = {seqlib::TrailingRule::kRemoveAll, seqlib::DeviceMark::kBeforeName, false,
                                           "output"};

SEQLIB_TEST(file_name, DefaultRules)
{
    for (const NameCase& c : kNames)
    {
        seqlib::test::SetContext(c.with_defaults);
        SEQLIB_CHECK(seqlib::SafeFileName(std::string(c.name, c.size)) == c.with_defaults);
    }
}

SEQLIB_TEST(file_name, OtherRules)
{
    for (const NameCase& c : kNames)
    {
        seqlib::test::SetContext(c.with_other_rules);
        SEQLIB_CHECK(seqlib::SafeFileName(std::string(c.name, c.size), kOtherRules) == c.with_other_rules);
    }
}

// Bytes that aren't part of a valid UTF-8 character: Shift-JIS, a stray continuation byte, a cut character, an overlong
// form, a surrogate, a code point above U+10FFFF, 0xFF, and one after a valid character. Each becomes '_'. "NUL"
// followed by one then names no device. A character of four bytes stays.
constexpr NameCase kNotUtf8Names[] = {
    {"SE_\x83|\x83P", 7, "SE____P", "SE____P"},
    {"a\x80"
     "b",
     3, "a_b", "a_b"},
    {"\xe3\x81", 2, "__", "__"},
    {"\xc0\xaf", 2, "__", "__"},
    {"\xed\xa0\x80", 3, "___", "___"},
    {"\xf4\x90\x80\x80", 4, "____", "____"},
    {"\xff", 1, "_", "_"},
    {"\xc3\xa9\xe9", 3, "\xc3\xa9_", "\xc3\xa9_"},
    {"NUL\x80", 4, "NUL_", "NUL_"},
    {"\xf0\x9f\x8e\xb5.mid", 8, "\xf0\x9f\x8e\xb5.mid", "\xf0\x9f\x8e\xb5.mid"},
};

SEQLIB_TEST(file_name, BytesThatArentUtf8)
{
    for (const NameCase& c : kNotUtf8Names)
    {
        seqlib::test::SetContext(c.with_defaults);
        SEQLIB_CHECK(seqlib::SafeFileName(std::string(c.name, c.size)) == c.with_defaults);
        SEQLIB_CHECK(seqlib::SafeFileName(std::string(c.name, c.size), kOtherRules) == c.with_other_rules);
    }
}

SEQLIB_TEST(file_name, IsDeviceNameFollowsTheRules)
{
    SEQLIB_CHECK(seqlib::IsDeviceName("con"));
    SEQLIB_CHECK(seqlib::IsDeviceName("CON  "));
    SEQLIB_CHECK(!seqlib::IsDeviceName("CON  ", kOtherRules));
    SEQLIB_CHECK(seqlib::IsDeviceName("conout$"));
    SEQLIB_CHECK(!seqlib::IsDeviceName("conout$", kOtherRules));
    SEQLIB_CHECK(seqlib::IsDeviceName("lpt\xc2\xb3", kOtherRules));
    SEQLIB_CHECK(!seqlib::IsDeviceName("COM10"));
    SEQLIB_CHECK(!seqlib::IsDeviceName(""));
}

} // namespace
