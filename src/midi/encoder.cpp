// SPDX-License-Identifier: MIT

// Standard MIDI File encoding (see encoder.h).

#include "seqlib/midi/encoder.h"

#include <algorithm>
#include <cmath>

#include "bytes.h"

namespace seqlib::midi
{

using detail::AppendBe16;
using detail::AppendBe32;

void AppendVlq(std::vector<uint8_t>& out, uint32_t v)
{
    uint8_t buf[5];
    int n = 0;
    buf[n++] = v & 0x7f;
    while (v >>= 7)
    {
        buf[n++] = static_cast<uint8_t>(0x80 | (v & 0x7f));
    }

    while (n)
    {
        out.push_back(buf[--n]);
    }
}

void AppendHeader(std::vector<uint8_t>& out, uint16_t format, uint16_t tracks, uint16_t division)
{
    out.insert(out.end(), {'M', 'T', 'h', 'd'});
    AppendBe32(out, 6);
    AppendBe16(out, format);
    AppendBe16(out, tracks);
    AppendBe16(out, division);
}

void AppendTrack(std::vector<uint8_t>& out, std::span<const uint8_t> body)
{
    out.insert(out.end(), {'M', 'T', 'r', 'k'});
    AppendBe32(out, static_cast<uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

std::vector<uint8_t> MetaEvent(uint8_t type, const std::string& text)
{
    std::vector<uint8_t> bytes = {0xff, type};
    AppendVlq(bytes, static_cast<uint32_t>(text.size()));
    bytes.insert(bytes.end(), text.begin(), text.end());

    return bytes;
}

std::vector<uint8_t> TempoEvent(uint32_t micros_per_quarter)
{
    const uint32_t us = micros_per_quarter;
    return {0xff, 0x51, 0x03, static_cast<uint8_t>(us >> 16), static_cast<uint8_t>(us >> 8), static_cast<uint8_t>(us)};
}

void TrackEncoder::Event(uint64_t tick, std::initializer_list<uint8_t> bytes)
{
    Delta(tick);
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void TrackEncoder::Event(uint64_t tick, std::span<const uint8_t> bytes)
{
    Delta(tick);
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void TrackEncoder::Meta(uint64_t tick, uint8_t type, const std::string& text)
{
    Event(tick, MetaEvent(type, text));
}

void TrackEncoder::Tempo(uint64_t tick, double bpm)
{
    const uint32_t us = static_cast<uint32_t>(std::llround(std::clamp(60000000.0 / bpm, 1.0, 16777215.0)));
    Event(tick, TempoEvent(us));
}

void TrackEncoder::End(uint64_t tick)
{
    Event(tick, {0xff, 0x2f, 0x00});
}

const std::vector<uint8_t>& TrackEncoder::Data() const
{
    return data_;
}

void TrackEncoder::AppendTo(std::vector<uint8_t>& out) const
{
    AppendTrack(out, data_);
}

void TrackEncoder::Delta(uint64_t tick)
{
    const uint64_t midi_tick = std::max(tick, last_);
    uint64_t delta = midi_tick - last_;
    while (delta > kMaxDelta)
    {
        AppendVlq(data_, kMaxDelta);
        data_.insert(data_.end(), {0xff, 0x01, 0x00});
        delta -= kMaxDelta;
    }

    AppendVlq(data_, static_cast<uint32_t>(delta));
    last_ = midi_tick;
}

} // namespace seqlib::midi
