// SPDX-License-Identifier: MIT

// SoundFont samples from a prepared wave (see samples.h).

#include "seqlib/sf2/samples.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace seqlib::sf2
{

std::vector<uint16_t> AddSamples(SoundFont& sf, PreparedWave wave, const std::string& name, uint32_t sample_rate,
                                 uint8_t root_key)
{
    const std::size_t n = std::min<std::size_t>(wave.channels.size(), 2);
    std::vector<uint16_t> ids;
    for (std::size_t c = 0; c < n; c++)
    {
        Sample s;
        s.name = name + (n == 1 ? "" : (c == 0 ? "L" : "R"));
        s.data = std::move(wave.channels[c]);
        s.sample_rate = sample_rate;
        s.original_key = root_key;
        if (wave.loop)
        {
            s.loop_start = wave.loop_start;
            s.loop_end = wave.loop_end;
        }
        else if (s.data.size() > 1)
        {
            s.loop_end = static_cast<uint32_t>(s.data.size());
        }
        s.type = n == 1 ? kMono : (c == 0 ? kLeft : kRight);
        ids.push_back(static_cast<uint16_t>(sf.samples.size()));
        sf.samples.push_back(std::move(s));
    }

    if (n == 2)
    {
        sf.samples[ids[0]].link = ids[1];
        sf.samples[ids[1]].link = ids[0];
    }

    if (sf.samples.size() > 0xffff)
    {
        throw std::runtime_error("more than 65535 samples");
    }

    return ids;
}

} // namespace seqlib::sf2
