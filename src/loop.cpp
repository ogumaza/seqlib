// SPDX-License-Identifier: MIT

#include "seqlib/loop.h"

#include <algorithm>
#include <utility>

namespace seqlib
{

PreparedWave UnrollLoop(const Pcm& pcm, uint32_t min_start, uint32_t min_before, uint32_t min_length, uint32_t after)
{
    PreparedWave w;
    w.loop = pcm.loop;
    for (const auto& ch : pcm.channels)
    {
        std::vector<int16_t> src = ch;
        if (src.empty())
        {
            src.push_back(0); // a sample requires at least one point
        }

        if (!pcm.loop)
        {
            w.channels.push_back(std::move(src));
            continue;
        }

        const uint32_t ls = pcm.loop_start, le = pcm.loop_end, len = le - ls;
        const std::vector<int16_t> body(src.begin() + ls, src.begin() + le);
        uint32_t before = 0;
        while (ls + before * len < min_start || before * len < min_before)
        {
            before++;
        }

        uint32_t reps = 1;
        while (reps * len < min_length)
        {
            reps++;
        }

        std::vector<int16_t> out(src.begin(), src.begin() + ls);
        for (uint32_t i = 0; i < before; i++)
        {
            out.insert(out.end(), body.begin(), body.end());
        }

        w.loop_start = static_cast<uint32_t>(out.size());
        for (uint32_t i = 0; i < reps; i++)
        {
            out.insert(out.end(), body.begin(), body.end());
        }
        w.loop_end = static_cast<uint32_t>(out.size());

        for (uint32_t i = 0; i < after; i++)
        {
            out.push_back(body[i % len]);
        }

        w.channels.push_back(std::move(out));
    }

    return w;
}

PreparedWave PrepareForSoundFont(const Pcm& pcm)
{
    return UnrollLoop(pcm, 8, 0, 32, 32);
}

PreparedWave PrepareForSfz(const Pcm& pcm, uint32_t crossfade)
{
    return UnrollLoop(pcm, 0, std::max<uint32_t>(crossfade, 64), 64, 64);
}

} // namespace seqlib
