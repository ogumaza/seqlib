// SPDX-License-Identifier: MIT

#include <cstdio>
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
    std::vector<std::string> modules(argv + 1, argv + argc);
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

        seqlib::testing::test_name = t.name;
        seqlib::testing::context.clear();
        t.function();
        run++;
    }

    seqlib::testing::test_name.clear();
    std::printf("%d tests, %d failed checks\n", run, seqlib::testing::failures);

    return run > 0 && seqlib::testing::failures == 0 ? 0 : 1;
}

} // namespace seqlib::test

int main(int argc, char** argv)
{
    return seqlib::test::Run(argc, argv);
}
