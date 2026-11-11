// SPDX-License-Identifier: MIT

// Tests of the shared checks (seqlib/testing/check.h): what a failure reports, the counts, REQUIRE, and Summary. Each
// test fails checks on purpose into a string, and then puts the counts back.

#include "test.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>

#include "seqlib/testing/check.h"

namespace seqlib::test
{
namespace
{

// The state of the checks that Capture sets aside.
struct State
{
    int checks = 0;
    int failures = 0;
    std::string context;
    std::string test_name;
    std::ostream* failure_stream = nullptr;
    std::ostream* success_stream = nullptr;

    bool operator==(const State&) const = default;
};

State CurrentState()
{
    return {seqlib::testing::checks,    seqlib::testing::failures,       seqlib::testing::context,
            seqlib::testing::test_name, seqlib::testing::failure_stream, seqlib::testing::success_stream};
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

// Throws std::invalid_argument when `fail` is true, as ThrowIf throws std::runtime_error.
void RefuseIf(bool fail)
{
    if (fail)
    {
        throw std::invalid_argument("bad value");
    }
}

SEQLIB_TEST(testing, CheckGivesItsResult)
{
    // A failure gives the file and the line of the call.
    bool passed = false;
    bool failed = true;
    int line = 0;

    const seqlib::testing::Captured c = seqlib::testing::Capture(
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
    const seqlib::testing::Captured c = seqlib::testing::Capture(
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
    const seqlib::testing::Captured big = seqlib::testing::Capture(
        []
        {
            SEQLIB_CHECK_EQ(UINT64_MAX, -1);
            SEQLIB_CHECK_EQ(uint64_t{1} << 63, 0);
        });

    // bool, characters and enumerations compare by their values.
    const seqlib::testing::Captured kinds = seqlib::testing::Capture(
        []
        {
            SEQLIB_CHECK_EQ(true, 1);
            SEQLIB_CHECK_EQ('A', 65);
            SEQLIB_CHECK_EQ(Color::kBlue, 2);
            SEQLIB_CHECK_EQ(Color::kRed, Color::kBlue);
        });

    SEQLIB_CHECK(big.checks == 2 && big.failures == 2);
    SEQLIB_CHECK(big.text.find("(18446744073709551615 vs -1)") != std::string::npos);
    SEQLIB_CHECK(big.text.find("(9223372036854775808 vs 0)") != std::string::npos);
    SEQLIB_CHECK(kinds.checks == 4 && kinds.failures == 1);
    SEQLIB_CHECK(kinds.text.ends_with(": CHECK_EQ failed: Color::kRed == Color::kBlue (1 vs 2)\n"));
}

SEQLIB_TEST(testing, RequireEndsTheFunction)
{
    // A failed REQUIRE ends the function. So does a failed REQUIRE_EQ.
    bool plain_reached = false;
    bool equal_reached = false;
    bool passing_reached = false;

    const seqlib::testing::Captured plain = seqlib::testing::Capture([&] { RequireAll(false, 3, 3, plain_reached); });
    const seqlib::testing::Captured equal = seqlib::testing::Capture([&] { RequireAll(true, 3, 4, equal_reached); });
    const seqlib::testing::Captured passing =
        seqlib::testing::Capture([&] { RequireAll(true, 3, 3, passing_reached); });

    SEQLIB_CHECK(!plain_reached && plain.checks == 1 && plain.failures == 1);
    SEQLIB_CHECK(!equal_reached && equal.checks == 2 && equal.failures == 1);
    SEQLIB_CHECK(passing_reached && passing.checks == 2 && passing.failures == 0);
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

    const seqlib::testing::Captured c = seqlib::testing::Capture([&] { RequireAfterAFailure(reached); });

    SEQLIB_CHECK(reached && c.checks == 2 && c.failures == 1);
}

SEQLIB_TEST(testing, FailuresGiveTheContextAndTheTest)
{
    // A failed SEQLIB_CHECK and a failed Check give the same parts.
    const seqlib::testing::Captured c = seqlib::testing::Capture(
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

    const std::string error = seqlib::testing::ErrorOf([&] { ThrowIf(fail); });
    fail = false;
    const std::string none = seqlib::testing::ErrorOf([&] { ThrowIf(fail); });

    SEQLIB_CHECK(error == "no such file");
    SEQLIB_CHECK(none.empty());
}

SEQLIB_TEST(testing, ErrorOfCatchesOnlyItsType)
{
    // An exception of the type, or of a type derived from it, gives its message. The inner ErrorOf doesn't catch the
    // std::invalid_argument. The outer one catches it.
    bool fail = true;
    const auto refuse = [&]
    {
        RefuseIf(fail);
    };
    const auto inner = [&]
    {
        seqlib::testing::ErrorOf<std::runtime_error>(refuse);
    };

    const std::string exact = seqlib::testing::ErrorOf<std::invalid_argument>(refuse);
    const std::string base = seqlib::testing::ErrorOf<std::logic_error>(refuse);
    const std::string outer = seqlib::testing::ErrorOf<std::invalid_argument>(inner);
    fail = false;
    const std::string none = seqlib::testing::ErrorOf<std::invalid_argument>(refuse);

    SEQLIB_CHECK(exact == "bad value");
    SEQLIB_CHECK(base == "bad value");
    SEQLIB_CHECK(outer == "bad value");
    SEQLIB_CHECK(none.empty());
}

SEQLIB_TEST(testing, SummaryGivesTheResult)
{
    int failed_status = 0;
    int passed_status = 1;

    const seqlib::testing::Captured failed = seqlib::testing::Capture(
        [&]
        {
            seqlib::testing::failures = 2;
            failed_status = seqlib::testing::Summary("demo");
        });
    const seqlib::testing::Captured passed =
        seqlib::testing::Capture([&] { passed_status = seqlib::testing::Summary("demo"); });

    SEQLIB_CHECK(failed_status == 1 && failed.text == "2 check(s) failed\n");
    SEQLIB_CHECK(passed_status == 0 && passed.text == "demo tests passed\n");
}

SEQLIB_TEST(testing, CaptureRestoresTheStateAfterAnException)
{
    // The function fails a check and changes the context and the test's name before it throws.
    const auto fail_and_throw = []
    {
        seqlib::testing::SetContext("inner");
        seqlib::testing::test_name = "Inner";
        SEQLIB_CHECK(1 + 1 == 3);
        throw std::runtime_error("bad input");
    };

    seqlib::testing::SetContext("outer");
    const State before = CurrentState();

    const std::string error = seqlib::testing::ErrorOf([&] { seqlib::testing::Capture(fail_and_throw); });
    const State after = CurrentState();
    seqlib::testing::SetContext("");

    SEQLIB_CHECK(error == "bad input");
    SEQLIB_CHECK(after == before);
}

} // namespace
} // namespace seqlib::test
