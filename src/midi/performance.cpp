// SPDX-License-Identifier: MIT

// A performance (see performance.h).

#include "seqlib/midi/performance.h"

namespace seqlib::midi
{

uint32_t NoteCount(const Performance& perf)
{
    uint32_t notes = 0;
    for (const std::vector<TrackEvent>& events : perf.tracks)
    {
        for (const TrackEvent& e : events)
        {
            if (e.kind == EventKind::kNoteOn)
            {
                notes++;
            }
        }
    }

    return notes;
}

} // namespace seqlib::midi
