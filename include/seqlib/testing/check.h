// SPDX-License-Identifier: MIT

// The checks that the unit tests of seqlib and programs that use seqlib share. A failed check is reported and counted.
// The test goes on after it. One run can then report several failures. A failed SEQLIB_REQUIRE or SEQLIB_REQUIRE_EQ
// returns from the function that contains it instead. A test's main can end with Summary.

#pragma once

#include <exception>
#include <iostream>
#include <ostream>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

namespace seqlib::testing
{

// The checks made so far, and those that failed. They're plain ints. A test's main may print them.
inline int checks = 0;
inline int failures = 0;

// The text that failures report, such as the case that a loop checks. SetContext sets it. The text stays until the next
// call. Empty for none.
inline std::string context;

// The test that runs. A runner of named tests sets it. Failures report it. Empty for none.
inline std::string test_name;

// Where the checks write: failures and Summary's count of them to `failure_stream`, and Summary's success to
// `success_stream`. They're std::cerr and std::cout. A test of the checks themselves points the two streams at strings.
inline std::ostream* failure_stream = &std::cerr;
inline std::ostream* success_stream = &std::cout;

inline void SetContext(std::string text)
{
    context = std::move(text);
}

// Reports and counts a failure: `what` at `file`:`line`, after the test's name and before the context.
inline void FailWith(const char* file, int line, const std::string& what)
{
    failures++;

    std::ostream& out = *failure_stream;
    out << file << ':' << line << ": ";
    if (!test_name.empty())
    {
        out << test_name << ": ";
    }
    out << what;
    if (!context.empty())
    {
        out << " (" << context << ')';
    }
    out << '\n' << std::flush;
}

// Reports and counts the failed check of `expression` at `file`:`line`.
inline void Fail(const char* expression, const char* file, int line)
{
    FailWith(file, line, std::string("CHECK failed: ") + expression);
}

// Counts a check. If `ok` is false, it also reports and counts a failure that `what` describes, at `where`, as
// FailWith does. `where` is the place of the call by default. This is for a check whose message has to say more than
// its condition, such as which case failed. Returns `ok`. A test can then skip what depends on it.
inline bool Check(bool ok, const std::string& what, std::source_location where = std::source_location::current())
{
    checks++;

    if (!ok)
    {
        FailWith(where.file_name(), static_cast<int>(where.line()), "CHECK failed: " + what);
    }

    return ok;
}

// Returns the message of the exception that `f` throws, or "" if it throws none.
template <typename F>
std::string ErrorOf(F f)
{
    std::string message;
    try
    {
        f();
    }
    catch (const std::exception& e)
    {
        message = e.what();
    }

    return message;
}

namespace detail
{

// A whole number of any kind as an integer that std::cmp_equal takes: an enumeration as its underlying type, and bool
// and the character types as long long or unsigned long long.
template <typename T>
auto AsInteger(T value)
{
    if constexpr (std::is_enum_v<T>)
    {
        return AsInteger(static_cast<std::underlying_type_t<T>>(value));
    }
    else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
                       std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>)
    {
        if constexpr (std::is_signed_v<T>)
        {
            return static_cast<long long>(value);
        }
        else
        {
            return static_cast<unsigned long long>(value);
        }
    }
    else
    {
        return value;
    }
}

// A whole number as text: a signed one as signed, and an unsigned one as unsigned.
template <typename T>
std::string IntegerText(T value)
{
    const auto integer = AsInteger(value);
    if constexpr (std::is_signed_v<decltype(integer)>)
    {
        return std::to_string(static_cast<long long>(integer));
    }
    else
    {
        return std::to_string(static_cast<unsigned long long>(integer));
    }
}

} // namespace detail

// Counts a check that two whole numbers are equal, by their values whatever their types. On a failure, reports both.
// Returns whether they're equal.
template <typename A, typename B>
bool CheckEqual(const A& a, const B& b, const char* a_text, const char* b_text, const char* file, int line)
{
    static_assert(!std::is_floating_point_v<A> && !std::is_floating_point_v<B>,
                  "SEQLIB_CHECK_EQ compares whole numbers; use SEQLIB_CHECK for floating-point values");

    checks++;
    if (std::cmp_equal(detail::AsInteger(a), detail::AsInteger(b)))
    {
        return true;
    }

    FailWith(file, line,
             std::string("CHECK_EQ failed: ") + a_text + " == " + b_text + " (" + detail::IntegerText(a) + " vs " +
                 detail::IntegerText(b) + ")");

    return false;
}

// Prints the run's result and returns its exit status. After a failure, Summary prints "N check(s) failed" and returns
// 1. Otherwise it prints "`name` tests passed" and returns 0.
inline int Summary(const char* name)
{
    if (failures)
    {
        *failure_stream << failures << " check(s) failed\n" << std::flush;
        return 1;
    }

    *success_stream << name << " tests passed\n" << std::flush;

    return 0;
}

} // namespace seqlib::testing

// Checks `expression`. SEQLIB_CHECK is an expression itself, and can stand where one can.
#define SEQLIB_CHECK(expression)  \
    (::seqlib::testing::checks++, \
     (expression) ? static_cast<void>(0) : ::seqlib::testing::Fail(#expression, __FILE__, __LINE__))

// Checks that two whole numbers are equal. A failure gives both values. SEQLIB_CHECK_EQ is an expression too.
#define SEQLIB_CHECK_EQ(a, b) static_cast<void>(::seqlib::testing::CheckEqual((a), (b), #a, #b, __FILE__, __LINE__))

// Like SEQLIB_CHECK and SEQLIB_CHECK_EQ, but a failed check also returns from the function that contains it.
// SEQLIB_REQUIRE and SEQLIB_REQUIRE_EQ work only in a function that returns void. A test checks a count with them
// before it reads the counted items by their index.
#define SEQLIB_REQUIRE(expression)                                    \
    do                                                                \
    {                                                                 \
        ::seqlib::testing::checks++;                                  \
        if (!(expression))                                            \
        {                                                             \
            ::seqlib::testing::Fail(#expression, __FILE__, __LINE__); \
            return;                                                   \
        }                                                             \
    } while (0)

#define SEQLIB_REQUIRE_EQ(a, b)                                                   \
    do                                                                            \
    {                                                                             \
        if (!::seqlib::testing::CheckEqual((a), (b), #a, #b, __FILE__, __LINE__)) \
        {                                                                         \
            return;                                                               \
        }                                                                         \
    } while (0)
