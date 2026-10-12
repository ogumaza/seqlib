// SPDX-License-Identifier: MIT

#include "seqlib/sfz/opcodes.h"

#include "seqlib/sfz/text.h"

namespace seqlib::sfz
{

std::string TuneOpcodes(int cents)
{
    std::string s;
    if (cents / 100 != 0)
    {
        s += " transpose=" + std::to_string(cents / 100);
    }
    if (cents % 100 != 0)
    {
        s += " tune=" + std::to_string(cents % 100);
    }

    return s;
}

std::string AmpEnvelopeOpcodes(const AmpEnvelope& e)
{
    std::string s;
    if (e.delay > 0.0)
    {
        s += " ampeg_delay=" + Number(e.delay, 6);
    }
    if (e.attack > 0.0)
    {
        s += " ampeg_attack=" + Number(e.attack, 6);
    }
    if (e.hold > 0.0)
    {
        s += " ampeg_hold=" + Number(e.hold, 6);
    }
    if (e.sustain < 100.0)
    {
        s += " ampeg_decay=" + Number(e.decay, 6) + " ampeg_sustain=" + Number(e.sustain, 6);
    }
    s += " ampeg_release=" + Number(e.release, 6);

    return s;
}

std::string LoopOpcodes(bool loop, uint32_t start, uint32_t end)
{
    if (!loop)
    {
        return " loop_mode=no_loop";
    }

    return " loop_mode=loop_continuous loop_start=" + std::to_string(start) + " loop_end=" + std::to_string(end);
}

} // namespace seqlib::sfz
