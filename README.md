# seqlib

seqlib is a C++20 library for MIDI, SoundFont 2 and SFZ files. It turns a performance, the notes and controllers that a
sequence player plays on each of its tracks, into a Standard MIDI File. The library also writes SoundFont 2 banks from
samples, instruments and presets.

seqlib depends on nothing but the C++ standard library.

## What it does

- **MIDI encoding** (`seqlib/midi/encoder.h`): variable-length quantities, header and track chunks, and a track
  encoder for events in time order. A gap too long for one delta time is split with empty text events.
- **Performance to MIDI** (`seqlib/midi/performance.h`, `seqlib/midi/performance_writer.h`): `WriteMidi` writes a
  format 1 file from a `Performance` of up to 16 tracks. It gives each track a MIDI channel, and puts notes with other
  settings on another part with another channel. Overlapping notes of one key go on extra channels (layers)
  while channels last. General MIDI's drum channel is used last. `WriteMidi` sets each track's pitch bend range to the
  largest bend that the track plays. It writes loop markers (`loopStart` and `loopEnd`, and CC111 at the loop's start).
  Events can be timed to a player's frames, between the sequence's ticks.
- **Recording a performance** (`seqlib/midi/recorder.h`): a `Recorder` that puts a sequence player's notes, settings,
  presets, pitch and tempo on a `Performance`'s tracks, at the sequence's ticks and between them. A setting goes before
  the note-ons at the end of its tick. Only a change goes out. A track can switch its new notes to another part. Options
  cover the rules in which sequence players record differently, such as whether a switch sends the new part the old
  part's settings.
- **MIDI file building** (`seqlib/midi/builder.h`): tracks whose events can be added in any order. At one tick,
  events sort by step and then by kind. A track can be retimed. `LevelsToControllers` turns left and right levels into
  pan and expression.
- **SoundFont 2 writing** (`seqlib/sf2/soundfont.h`, `seqlib/sf2/writer.h`): samples, instruments, presets, zones,
  generators and modulators, with stereo links, INFO fields in the caller's order, and the 46 zero points that the
  format requires after each sample.
- **SoundFont presets** (`seqlib/sf2/presets.h`): preset names by bank and program, and the copies of presets that
  MIDI channel 10 plays from bank 128.
- **SoundFont samples** (`seqlib/sf2/samples.h`): the samples that a prepared wave becomes, mono or a linked stereo
  pair, with loop points.
- **SoundFont units** (`seqlib/sf2/units.h`): timecents, and a square-root pan law converted to SoundFont's pan law.
- **SFZ text** (`seqlib/sfz/text.h`): numbers, controller curves and comments.
- **SFZ opcodes** (`seqlib/sfz/opcodes.h`): a region's tuning, amplitude envelope and loop.
- **SFZ track files** (`seqlib/sfz/track_file.h`): a sequence's SFZ files, one for each track that has a MIDI channel.
  `TrackFiles` writes the parts of each file that don't depend on the instrument model, such as the pan curve. A hook
  adds the rest: other controller curves, opcodes and each preset's regions.
- **WAV files** (`seqlib/wav.h`): mono 16-bit PCM.
- **Loops** (`seqlib/loop.h`): a sample's loop unrolled to meet the requirements of SoundFont players and sfizz.
  Playback stays the same.
- **File names** (`seqlib/file_name.h`): names that are valid on Windows, macOS and Linux, with rules for trailing
  dots and Windows device names.
- **Reading and checking** (`seqlib/midi/reader.h`, `seqlib/sf2/reader.h`): a MIDI file's tracks and events, with its
  notes, track names, markers and bends, and a SoundFont's chunks and records. `seqlib::midi::Problems` lists the
  problems in a MIDI file, such as a note that never ends. `seqlib::sf2::Check` lists the problems in a SoundFont's
  structure, such as a broken stereo link or a loop without its padding. Each check has a few rules that are stricter
  than the specification of its format. `Problems` reports running status, for example. The `seqlib_check` program
  runs both checks on the files it's given.

Writers return the file's bytes, and readers take them. Your program reads and writes the files.

## Building

seqlib supports macOS on Apple Silicon and Windows x64 built with clang-cl. Other platforms and compilers aren't
supported. You need CMake 3.20 or later and a C++20 compiler:

- **Windows x64**: Visual Studio 2022 with the "Desktop development with C++" workload and its "C++ Clang tools for
  Windows" component. The workload includes CMake. Configure with `cmake -S . -B build -A x64 -T ClangCL` in place of
  the first command below.
- **macOS on Apple Silicon**: the Xcode command line tools (`xcode-select --install`) and CMake (for example
  `brew install cmake`).

Then:

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

The tests use synthetic files only.

These options are on when seqlib is the project you build, and off when another project includes it:

- `SEQLIB_BUILD_TESTS`: the tests, one CTest test for each module.
- `SEQLIB_BUILD_TOOLS`: `seqlib_check`.
- `SEQLIB_INSTALL`: the install rules and the CMake package for `find_package`.

`seqlib_check` checks MIDI and SoundFont files. It reads each file it's given, prints the problems it finds, and
exits with status 1 if it found any:

```
seqlib_check song.mid bank.sf2
```

## Using it

seqlib is a CMake package with the target `seqlib::seqlib`. Take it in any of three ways:

```cmake
# A copy in your source tree, such as a Git submodule:
add_subdirectory(externals/seqlib)

# Or fetched at configure time:
include(FetchContent)
FetchContent_Declare(seqlib GIT_REPOSITORY <seqlib's repository URL> GIT_TAG <a commit or tag>)
FetchContent_MakeAvailable(seqlib)

# Or installed with `cmake --install build`:
find_package(seqlib 1.0 REQUIRED)

target_link_libraries(your_program PRIVATE seqlib::seqlib)
```

seqlib computes some values in floating point. For results that are the same on both platforms, it builds without FMA
contraction. Link `seqlib::reproducible_fp` to build your sources the same way.

`seqlib::testing` holds the checks that the unit tests of seqlib and of programs that use seqlib share. Its header is
`seqlib/testing/check.h`. A failed check is reported and counted. The test goes on after it. A failed `SEQLIB_REQUIRE`
or `SEQLIB_REQUIRE_EQ` returns from the function that contains it instead. Link `seqlib::testing` to your tests only.

A small example:

```cpp
#include "seqlib/midi/performance_writer.h"

seqlib::midi::Performance perf;
perf.timebase = 48;
perf.tempo = {{0, 120.0}};
perf.tracks[0] = {
    {.tick = 0, .kind = seqlib::midi::EventKind::kNoteOn, .key = 60, .value = 100, .note = 1},
    {.tick = 48, .kind = seqlib::midi::EventKind::kNoteOff, .key = 60, .note = 1},
};
perf.end_tick = 96;

seqlib::midi::MidiReport report;
const std::vector<uint8_t> file = seqlib::midi::WriteMidi(perf, "Example", report);
```

A SoundFont with one sample, one instrument and one preset, checked after it's written:

```cpp
#include "seqlib/sf2/reader.h"
#include "seqlib/sf2/writer.h"

namespace sf2 = seqlib::sf2;

sf2::SoundFont sf;
sf.info = {{"INAM", "Example"}, {"ISFT", "my program"}};

sf2::Sample sample;
sample.name = "Sine";
sample.sample_rate = 32000;
sample.data.assign(64, 0); // 16-bit PCM
sf.samples = {sample};

sf2::Instrument instrument;
instrument.name = "Sine";
instrument.zones = {{{sf2::Generator::Value(sf2::kSampleId, 0)}, {}}};
sf.instruments = {instrument};

sf2::Preset preset;
preset.name = "Sine";
preset.zones = {{{sf2::Generator::Value(sf2::kInstrument, 0)}, {}}};
sf.presets = {preset};

const std::vector<uint8_t> bank = sf2::Write(sf);
for (const std::string& problem : sf2::Check(bank))
{
    std::printf("%s\n", problem.c_str());
}
```

## Versions

seqlib uses semantic version numbers. The CMake package's version file accepts any 1.x release for
`find_package(seqlib 1.0)`.

## Licence

seqlib is released under the MIT licence. See `LICENSE`.
