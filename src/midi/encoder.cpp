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
using detail::AppendFourcc;

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
    AppendFourcc(out, "MThd");
    AppendBe32(out, 6);
    AppendBe16(out, format);
    AppendBe16(out, tracks);
    AppendBe16(out, division);
}

void AppendTrack(std::vector<uint8_t>& out, std::span<const uint8_t> body)
{
    AppendFourcc(out, "MTrk");
    AppendBe32(out, static_cast<uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
}

std::vector<uint8_t> MetaEvent(uint8_t type, const std::string& text)
{
    std::vector<uint8_t> bytes = {status::kMeta, type};
    AppendVlq(bytes, static_cast<uint32_t>(text.size()));
    bytes.insert(bytes.end(), text.begin(), text.end());

    return bytes;
}

std::vector<uint8_t> TempoEvent(uint32_t micros_per_quarter)
{
    return {status::kMeta,
            meta::kTempo,
            0x03,
            static_cast<uint8_t>(micros_per_quarter >> 16),
            static_cast<uint8_t>(micros_per_quarter >> 8),
            static_cast<uint8_t>(micros_per_quarter)};
}

void TrackEncoder::Event(uint64_t tick, std::initializer_list<uint8_t> bytes)
{
    Event(tick, std::span<const uint8_t>(bytes.begin(), bytes.size()));
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
    const uint32_t us =
        static_cast<uint32_t>(std::llround(std::clamp(60000000.0 / bpm, 1.0, double{meta::kMaxMicrosPerQuarter})));
    Event(tick, TempoEvent(us));
}

void TrackEncoder::End(uint64_t tick)
{
    Event(tick, {status::kMeta, meta::kEndOfTrack, 0x00});
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
        data_.insert(data_.end(), {status::kMeta, meta::kText, 0x00});
        delta -= kMaxDelta;
    }

    AppendVlq(data_, static_cast<uint32_t>(delta));
    last_ = midi_tick;
}

} // namespace seqlib::midi
