// SPDX-License-Identifier: MIT

// The names of a performance's tracks, for the performance writer and the SFZ track files. The header is private and
// isn't installed.

#pragma once

#include <cstddef>
#include <string>

#include "seqlib/midi/performance.h"

namespace seqlib::detail
{

// The name of track `t` when the track has no name: "Track <t>".
inline std::string DefaultTrackName(int t)
{
    return "Track " + std::to_string(t);
}

// The name of the MIDI tracks of track `t`: the track's name in `perf`, or DefaultTrackName(t) for a track without one.
inline std::string TrackName(const midi::Performance& perf, int t)
{
    const std::string& name = perf.track_names[static_cast<std::size_t>(t)];
    return name.empty() ? DefaultTrackName(t) : name;
}

} // namespace seqlib::detail
