// SPDX-License-Identifier: MIT

// The test runner: the tests' registry and main (see test.h).

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "test.h"

namespace seqlib::test
{
namespace
{

struct Test
{
    std::string module;
    const char* name;
    TestFunction function;
};

// Tests register while static objects start up, in any order of files. A function's static is ready for the first.
std::vector<Test>& Tests()
{
    static std::vector<Test> tests;
    return tests;
}

} // namespace

bool Register(const char* module, const char* name, TestFunction function)
{
    Tests().push_back({module, name, function});

    return true;
}

int Run(int argc, char** argv)
{
    // Each name on the command line has to name a module.
    std::vector<std::string> modules(argv + 1, argv + argc);
    bool unknown = false;
    for (const std::string& m : modules)
    {
        const auto in_module = [&m](const Test& t)
        {
            return t.module == m;
        };
        if (std::ranges::none_of(Tests(), in_module))
        {
            std::fprintf(stderr, "No tests in module %s.\n", m.c_str());
            unknown = true;
        }
    }

    int run = 0;
    for (const Test& t : Tests())
    {
        bool wanted = modules.empty();
        for (const std::string& m : modules)
        {
            wanted = wanted || m == t.module;
        }
        if (!wanted)
        {
            continue;
        }

        // An exception that leaves a test fails it. The later tests still run.
        seqlib::testing::test_name = t.name;
        seqlib::testing::context.clear();
        try
        {
            t.function();
        }
        catch (const std::exception& e)
        {
            seqlib::testing::FailWith(__FILE__, __LINE__, std::string("exception: ") + e.what());
        }
        catch (...)
        {
            seqlib::testing::FailWith(__FILE__, __LINE__, "unknown exception");
        }

        run++;
    }

    seqlib::testing::test_name.clear();
    std::printf("%d tests, %d failed checks\n", run, seqlib::testing::failures);

    return run > 0 && !unknown && seqlib::testing::failures == 0 ? 0 : 1;
}

} // namespace seqlib::test

int main(int argc, char** argv)
{
    return seqlib::test::Run(argc, argv);
}
