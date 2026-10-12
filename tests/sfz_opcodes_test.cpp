// SPDX-License-Identifier: MIT

// Golden tests for seqlib/sfz/opcodes.h. The expected text is fixed: a change to it is a change of seqlib's output.

#include <string>

#include "seqlib/sfz/opcodes.h"
#include "test.h"

namespace
{

namespace sfz = seqlib::sfz;

SEQLIB_TEST(sfz_opcodes, TuneOpcodes)
{
    SEQLIB_CHECK(sfz::TuneOpcodes(0).empty());
    SEQLIB_CHECK(sfz::TuneOpcodes(99) == " tune=99");
    SEQLIB_CHECK(sfz::TuneOpcodes(1200) == " transpose=12");
    SEQLIB_CHECK(sfz::TuneOpcodes(150) == " transpose=1 tune=50");
    SEQLIB_CHECK(sfz::TuneOpcodes(-250) == " transpose=-2 tune=-50");
}

SEQLIB_TEST(sfz_opcodes, AmpEnvelopeOpcodesLeaveOutDefaults)
{
    SEQLIB_CHECK(sfz::AmpEnvelopeOpcodes({}) == " ampeg_release=0");
    SEQLIB_CHECK(sfz::AmpEnvelopeOpcodes({.release = 0.125}) == " ampeg_release=0.125");
}

SEQLIB_TEST(sfz_opcodes, AmpEnvelopeOpcodesWriteEveryStage)
{
    const sfz::AmpEnvelope kEnvelope{
        .delay = 0.5, .attack = 0.01, .hold = 0.25, .decay = 1.5, .sustain = 50.0, .release = 1.0 / 3.0};

    SEQLIB_CHECK(sfz::AmpEnvelopeOpcodes(kEnvelope) ==
                 " ampeg_delay=0.5 ampeg_attack=0.01 ampeg_hold=0.25 ampeg_decay=1.5 "
                 "ampeg_sustain=50 ampeg_release=0.333333");
}

SEQLIB_TEST(sfz_opcodes, AmpEnvelopeOpcodesDecayGoesWithSustain)
{
    // A decay to full sustain changes nothing. Both opcodes are left out.
    SEQLIB_CHECK(sfz::AmpEnvelopeOpcodes({.decay = 2.0, .release = 0.5}) == " ampeg_release=0.5");
    SEQLIB_CHECK(sfz::AmpEnvelopeOpcodes({.decay = 0.0, .sustain = 0.0}) ==
                 " ampeg_decay=0 ampeg_sustain=0 ampeg_release=0");
}

SEQLIB_TEST(sfz_opcodes, LoopOpcodes)
{
    SEQLIB_CHECK(sfz::LoopOpcodes(false, 8, 39) == " loop_mode=no_loop");
    SEQLIB_CHECK(sfz::LoopOpcodes(true, 8, 39) == " loop_mode=loop_continuous loop_start=8 loop_end=39");
}

} // namespace
