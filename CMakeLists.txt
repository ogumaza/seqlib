# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)

project(seqlib VERSION 1.1.0 DESCRIPTION "MIDI, SoundFont 2 and SFZ files" LANGUAGES CXX)

include(GNUInstallDirs)

# The tests, the tools and the install rules are on when seqlib is the top-level project, and off when another project
# includes it. CMake 3.20 lacks PROJECT_IS_TOP_LEVEL.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SEQLIB_TOP_LEVEL ON)
else()
  set(SEQLIB_TOP_LEVEL OFF)
endif()
option(SEQLIB_BUILD_TESTS "Build seqlib's tests" ${SEQLIB_TOP_LEVEL})
option(SEQLIB_BUILD_TOOLS "Build seqlib's tools" ${SEQLIB_TOP_LEVEL})
option(SEQLIB_INSTALL "Install seqlib and its CMake package" ${SEQLIB_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(SEQLIB_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Floating-point flags that keep results the same on every platform: no FMA contraction, and SSE2 rather than x87's
# extra precision on 32-bit x86. Clang enables FMA contraction by default on some CPUs. seqlib's sources use the flags.
# A project whose output has to match seqlib's output on every platform can link seqlib::reproducible_fp too.
add_library(seqlib_reproducible_fp INTERFACE)
add_library(seqlib::reproducible_fp ALIAS seqlib_reproducible_fp)
set_target_properties(seqlib_reproducible_fp PROPERTIES EXPORT_NAME reproducible_fp)
if(MSVC)
  # clang-cl takes Clang's option through /clang:. MSVC itself doesn't contract by default.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(seqlib_reproducible_fp INTERFACE /clang:-ffp-contract=off)
  endif()
else()
  target_compile_options(seqlib_reproducible_fp INTERFACE -ffp-contract=off)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86|x86_64|AMD64)$")
    target_compile_options(seqlib_reproducible_fp INTERFACE -msse2 -mfpmath=sse)
  endif()
endif()

# Warnings and flags for seqlib's targets.
function(seqlib_set_flags target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
  target_link_libraries(${target} PRIVATE seqlib::reproducible_fp)
endfunction()

add_library(seqlib STATIC
  src/file_name.cpp
  src/loop.cpp
  src/midi/builder.cpp
  src/midi/encoder.cpp
  src/midi/performance.cpp
  src/midi/performance_writer.cpp
  src/midi/reader.cpp
  src/midi/recorder.cpp
  src/read_error.cpp
  src/sf2/presets.cpp
  src/sf2/reader.cpp
  src/sf2/samples.cpp
  src/sf2/units.cpp
  src/sf2/writer.cpp
  src/sfz/opcodes.cpp
  src/sfz/text.cpp
  src/sfz/track_file.cpp
  src/wav.cpp)
add_library(seqlib::seqlib ALIAS seqlib)
target_include_directories(seqlib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# The private header, bytes.h, is in src.
target_include_directories(seqlib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(seqlib PUBLIC cxx_std_20)
seqlib_set_flags(seqlib)

# The checks that the unit tests of seqlib and of programs that use seqlib share: seqlib/testing/check.h.
add_library(seqlib_testing INTERFACE)
add_library(seqlib::testing ALIAS seqlib_testing)
set_target_properties(seqlib_testing PROPERTIES EXPORT_NAME testing)
target_include_directories(seqlib_testing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(seqlib_testing INTERFACE cxx_std_20)

if(SEQLIB_BUILD_TESTS)
  enable_testing()
  add_executable(seqlib_tests
    tests/builder_test.cpp
    tests/file_name_test.cpp
    tests/loop_test.cpp
    tests/midi_reader_test.cpp
    tests/midi_recorder_test.cpp
    tests/midi_writer_test.cpp
    tests/presets_test.cpp
    tests/sf2_reader_test.cpp
    tests/sf2_samples_test.cpp
    tests/sf2_writer_test.cpp
    tests/sfz_opcodes_test.cpp
    tests/sfz_text_test.cpp
    tests/sfz_track_file_test.cpp
    tests/test_data.cpp
    tests/test_main.cpp
    tests/testing_test.cpp
    tests/units_test.cpp
    tests/wav_test.cpp)
  target_link_libraries(seqlib_tests PRIVATE seqlib::seqlib seqlib::testing)
  target_compile_definitions(seqlib_tests PRIVATE SEQLIB_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
  seqlib_set_flags(seqlib_tests)
  # One test for each module, run by the module's name.
  foreach(module builder file_name loop midi_reader midi_recorder midi_writer presets sf2_reader sf2_samples sf2_writer
                 sfz_opcodes sfz_text sfz_track_file testing units wav)
    add_test(NAME seqlib_${module} COMMAND seqlib_tests ${module})
  endforeach()
endif()

if(SEQLIB_BUILD_TOOLS)
  # Checks SoundFont 2 and MIDI files from the command line.
  add_executable(seqlib_check tools/seqlib_check.cpp)
  target_link_libraries(seqlib_check PRIVATE seqlib::seqlib)
  seqlib_set_flags(seqlib_check)
endif()

if(SEQLIB_INSTALL)
  include(CMakePackageConfigHelpers)
  install(TARGETS seqlib seqlib_reproducible_fp seqlib_testing EXPORT seqlibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT seqlibTargets NAMESPACE seqlib:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/seqlib)
  configure_package_config_file(cmake/seqlibConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/seqlibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/seqlib)
  write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/seqlibConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/seqlibConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/seqlibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/seqlib)
endif()
