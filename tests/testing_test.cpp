// SPDX-License-Identifier: MIT

// Tests of the shared checks (seqlib/testing/check.h): what a failure reports, the counts, REQUIRE, and Summary. Each
// test fails checks on purpose into a string, and then puts the counts back.

#include "test.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

#include "seqlib/testing/check.h"

namespace seqlib::test
{
namespace
{

// What the checks wrote while `f` ran, and the checks and failures that it added.
struct Captured
{
    std::string text;
    int checks = 0;
    int failures = 0;
};

// Runs `f` with the checks' output in a string, and with the counts, the context and the test's name set aside.
template <typename F>
Captured Capture(F f)
{
    namespace t = seqlib::testing;
    const int checks = t::checks;
    const int failures = t::failures;
    const std::string context = t::context;
    const std::string name = t::test_name;
    std::ostream* failure_stream = t::failure_stream;
    std::ostream* success_stream = t::success_stream;
    std::ostringstream out;
    t::checks = 0;
    t::failures = 0;
    t::context.clear();
    t::test_name.clear();
    t::failure_stream = &out;
    t::success_stream = &out;

    f();

    Captured captured;
    captured.text = out.str();
    captured.checks = t::checks;
    captured.failures = t::failures;
    t::checks = checks;
    t::failures = failures;
    t::context = context;
    t::test_name = name;
    t::failure_stream = failure_stream;
    t::success_stream = success_stream;

    return captured;
}

// A check that passes after a failed check inside it.
bool PassesAfterAFailure()
{
    SEQLIB_CHECK(1 == 2);

    return true;
}

// A test function that requires each check in turn. `reached` is set if the function goes past them all.
void RequireAll(bool first, int a, int b, bool& reached)
{
    SEQLIB_REQUIRE(first);
    SEQLIB_REQUIRE_EQ(a, b);
    reached = true;
}

enum class Color : uint8_t
{
    kRed = 1,
    kBlue = 2,
};

// Throws when `fail` is true. The lambdas in the test call it with a value that isn't a compile-time constant.
void ThrowIf(bool fail)
{
    if (fail)
    {
        throw std::runtime_error("no such file");
    }
}

SEQLIB_TEST(testing, CheckGivesItsResult)
{
    // A failure gives the file and the line of the call.
    bool passed = false;
    bool failed = true;
    int line = 0;
    const Captured c = Capture(
        [&]
        {
            passed = seqlib::testing::Check(true, "never shown");
            line = __LINE__ + 1;
            failed = seqlib::testing::Check(false, "case 3 of 7");
        });

    SEQLIB_CHECK(passed && !failed);
    SEQLIB_CHECK(c.checks == 2 && c.failures == 1);
    SEQLIB_CHECK(c.text.ends_with("testing_test.cpp:" + std::to_string(line) + ": CHECK failed: case 3 of 7\n"));
    SEQLIB_CHECK(c.text.find('\n') == c.text.size() - 1);
}

SEQLIB_TEST(testing, CheckEqGivesBothValues)
{
    const Captured c = Capture(
        []
        {
            SEQLIB_CHECK_EQ(2 + 2, 4);
            SEQLIB_CHECK_EQ(2 + 2, 5);
        });

    SEQLIB_CHECK(c.checks == 2 && c.failures == 1);
    SEQLIB_CHECK(c.text.ends_with(": CHECK_EQ failed: 2 + 2 == 5 (4 vs 5)\n"));
}

SEQLIB_TEST(testing, CheckEqComparesValues)
{
    // Large unsigned numbers print as unsigned, and a signed -1 doesn't equal an unsigned maximum.
    const Captured big = Capture(
        []
        {
            SEQLIB_CHECK_EQ(UINT64_MAX, -1);
            SEQLIB_CHECK_EQ(uint64_t{1} << 63, 0);
        });
    SEQLIB_CHECK(big.checks == 2 && big.failures == 2);
    SEQLIB_CHECK(big.text.find("(18446744073709551615 vs -1)") != std::string::npos);
    SEQLIB_CHECK(big.text.find("(9223372036854775808 vs 0)") != std::string::npos);

    // bool, characters and enumerations compare by their values.
    const Captured kinds = Capture(
        []
        {
            SEQLIB_CHECK_EQ(true, 1);
            SEQLIB_CHECK_EQ('A', 65);
            SEQLIB_CHECK_EQ(Color::kBlue, 2);
            SEQLIB_CHECK_EQ(Color::kRed, Color::kBlue);
        });
    SEQLIB_CHECK(kinds.checks == 4 && kinds.failures == 1);
    SEQLIB_CHECK(kinds.text.ends_with(": CHECK_EQ failed: Color::kRed == Color::kBlue (1 vs 2)\n"));
}

SEQLIB_TEST(testing, RequireEndsTheFunction)
{
    // A failed REQUIRE ends the function. So does a failed REQUIRE_EQ.
    bool reached = false;
    const Captured plain = Capture([&] { RequireAll(false, 3, 3, reached); });
    SEQLIB_CHECK(!reached && plain.checks == 1 && plain.failures == 1);

    reached = false;
    const Captured equal = Capture([&] { RequireAll(true, 3, 4, reached); });
    SEQLIB_CHECK(!reached && equal.checks == 2 && equal.failures == 1);

    reached = false;
    const Captured passing = Capture([&] { RequireAll(true, 3, 3, reached); });
    SEQLIB_CHECK(reached && passing.checks == 2 && passing.failures == 0);
}

// Uses a REQUIRE whose condition holds, after a failed check inside it. `reached` is set if the function goes on.
void RequireAfterAFailure(bool& reached)
{
    SEQLIB_REQUIRE(PassesAfterAFailure());
    reached = true;
}

SEQLIB_TEST(testing, RequireLooksAtItsCondition)
{
    // A failure inside the condition doesn't end the function. Only a false condition does.
    bool reached = false;
    const Captured c = Capture([&] { RequireAfterAFailure(reached); });
    SEQLIB_CHECK(reached && c.checks == 2 && c.failures == 1);
}

SEQLIB_TEST(testing, FailuresGiveTheContextAndTheTest)
{
    // A failed SEQLIB_CHECK and a failed Check give the same parts.
    const Captured c = Capture(
        []
        {
            seqlib::testing::SetContext("case 7");
            seqlib::testing::test_name = "SomeTest";
            SEQLIB_CHECK(1 + 1 == 3);
            seqlib::testing::Check(false, "the sum is 3");
        });

    SEQLIB_CHECK(c.failures == 2);
    const std::size_t second = c.text.find('\n') + 1;
    const std::string macro = c.text.substr(0, second);
    const std::string check = c.text.substr(second);
    SEQLIB_CHECK(macro.ends_with(": SomeTest: CHECK failed: 1 + 1 == 3 (case 7)\n"));
    SEQLIB_CHECK(check.ends_with(": SomeTest: CHECK failed: the sum is 3 (case 7)\n"));
    SEQLIB_CHECK(macro.find("testing_test.cpp:") != std::string::npos);
    SEQLIB_CHECK(check.find("testing_test.cpp:") != std::string::npos);
}

SEQLIB_TEST(testing, ErrorOfGivesTheMessage)
{
    bool fail = true;
    SEQLIB_CHECK(seqlib::testing::ErrorOf([&] { ThrowIf(fail); }) == "no such file");
    fail = false;
    SEQLIB_CHECK(seqlib::testing::ErrorOf([&] { ThrowIf(fail); }).empty());
}

SEQLIB_TEST(testing, SummaryGivesTheResult)
{
    int failed_status = 0;
    const Captured failed = Capture(
        [&]
        {
            seqlib::testing::failures = 2;
            failed_status = seqlib::testing::Summary("demo");
        });
    SEQLIB_CHECK(failed_status == 1 && failed.text == "2 check(s) failed\n");

    int passed_status = 1;
    const Captured passed = Capture([&] { passed_status = seqlib::testing::Summary("demo"); });
    SEQLIB_CHECK(passed_status == 0 && passed.text == "demo tests passed\n");
}

} // namespace
} // namespace seqlib::test
