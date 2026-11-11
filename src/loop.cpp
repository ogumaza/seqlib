// SPDX-License-Identifier: MIT

// Loop preparation for SoundFont 2 and SFZ samples (see loop.h).

#include "seqlib/loop.h"

#include <algorithm>
#include <span>
#include <utility>

#include "sf2/format.h"

namespace seqlib
{
namespace
{

// The samples of an empty channel: a sample requires at least one point.
constexpr int16_t kSilence[] = {0};

// The samples that PrepareForSoundFont appends from the loop start for interpolation. The format requires 8 points
// after the loop.
constexpr uint32_t kSoundFontTail = 32;

// The fewest samples that PrepareForSfz repeats before the loop, puts in the loop and appends after it.
constexpr uint32_t kSfzMinimum = 64;

} // namespace

PreparedWave UnrollLoop(const Pcm& pcm, uint32_t min_start, uint32_t min_before, uint32_t min_length, uint32_t after)
{
    PreparedWave w;
    w.loop = pcm.loop;
    for (const auto& ch : pcm.channels)
    {
        const std::span<const int16_t> src = ch.empty() ? std::span<const int16_t>(kSilence) : std::span(ch);
        if (!pcm.loop)
        {
            w.channels.emplace_back(src.begin(), src.end());
            continue;
        }

        const uint32_t ls = pcm.loop_start, le = pcm.loop_end, len = le - ls;
        const std::span<const int16_t> body = src.subspan(ls, len);
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
    return UnrollLoop(pcm, detail::kLoopPadding, 0, detail::kMinLoop, kSoundFontTail);
}

PreparedWave PrepareForSfz(const Pcm& pcm, uint32_t crossfade)
{
    return UnrollLoop(pcm, 0, std::max(crossfade, kSfzMinimum), kSfzMinimum, kSfzMinimum);
}

} // namespace seqlib
