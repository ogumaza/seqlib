// SPDX-License-Identifier: MIT

// A small test runner. Each test registers under its module's name. test_main.cpp runs the modules named on the command
// line, or every test.

#pragma once

#include "seqlib/testing/check.h"

namespace seqlib::test
{

// The tests call the shared checks' SetContext (seqlib/testing/check.h) by this name.
using seqlib::testing::SetContext;

using TestFunction = void (*)();

// Registers `function` as test `name` of `module`. Returns true, for a variable at namespace scope to hold.
bool Register(const char* module, const char* name, TestFunction function);

// Runs the tests of the modules named in `argv`, or every test with no names. Returns the exit status.
int Run(int argc, char** argv);

} // namespace seqlib::test

#define SEQLIB_TEST(module, name)                                                                     \
    void name();                                                                                      \
    [[maybe_unused]] const bool k##name##Registered = ::seqlib::test::Register(#module, #name, name); \
    void name()
