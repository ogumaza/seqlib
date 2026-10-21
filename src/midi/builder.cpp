// SPDX-License-Identifier: MIT

// Standard MIDI File builder (see builder.h).

#include "seqlib/midi/builder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <numbers>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "seqlib/midi/encoder.h"

namespace seqlib::midi
{
namespace
{

uint8_t Clamp7(int v)
{
    return static_cast<uint8_t>(std::clamp(v, 0, 127));
}

} // namespace

void LevelsToControllers(double left, double right, int& cc10, int& cc11)
{
    const double gl = left / 128.0, gr = right / 128.0;
    const double amp = std::sqrt(gl * gl + gr * gr);
    if (amp <= 0)
    {
        cc11 = 0;
        return;
    }

    // Equal levels are centred. The formula puts them at exactly 63.5. Rounding that would depend on atan2() returning
    // exactly the double nearest pi/4.
    if (left == right)
    {
        cc10 = 64;
    }
    else
    {
        cc10 = std::clamp(static_cast<int>(std::lround(std::atan2(gr, gl) / (std::numbers::pi / 2) * 127)), 0, 127);
    }

    const double kLoudestCentred = std::sqrt(2.0) * 127.0 / 128.0; // amplitude of the loudest centred voice
    cc11 = std::clamp(static_cast<int>(std::lround(127 * std::sqrt(amp / kLoudestCentred))), 0, 127);
}

void TrackBuilder::AddEvent(uint32_t tick, int order, std::vector<uint8_t> bytes, int step)
{
    events_.push_back({tick, step, order, std::move(bytes)});
    end_ = std::max(end_, tick);
}

void TrackBuilder::Meta(uint32_t tick, uint8_t type, const std::string& text)
{
    AddEvent(tick, kMeta, MetaEvent(type, text));
}

void TrackBuilder::Tempo(uint32_t tick, uint32_t micros_per_quarter)
{
    AddEvent(tick, kMeta, TempoEvent(micros_per_quarter));
}

void TrackBuilder::RepeatTempo(uint32_t tick)
{
    // The tempo at `tick` is the last change added at the latest tick up to it.
    const Event* last = nullptr;
    for (const Event& e : events_)
    {
        const bool tempo = e.bytes.size() == 6 && e.bytes[0] == 0xFF && e.bytes[1] == 0x51;
        if (tempo && e.tick <= tick && (!last || e.tick >= last->tick))
        {
            last = &e;
        }
    }

    if (last)
    {
        AddEvent(tick, kMeta, last->bytes);
    }
}

void TrackBuilder::TimeSignature(uint32_t tick, int numerator, int denominator_pow2)
{
    AddEvent(tick, kMeta,
             {0xFF, 0x58, 0x04, static_cast<uint8_t>(numerator), static_cast<uint8_t>(denominator_pow2), 24, 8});
}

void TrackBuilder::SysEx(uint32_t tick, const std::vector<uint8_t>& message)
{
    const bool framed = message.size() >= 2 && message.front() == 0xF0 && message.back() == 0xF7;
    if (!framed || std::any_of(message.begin() + 1, message.end() - 1, [](uint8_t b) { return b > 127; }))
    {
        throw std::invalid_argument("a system exclusive message runs from F0 to F7 with only data bytes between them");
    }

    // F0, the number of bytes after it as a variable-length quantity, then those bytes.
    std::vector<uint8_t> bytes = {0xF0};
    AppendVlq(bytes, static_cast<uint32_t>(message.size() - 1));
    bytes.insert(bytes.end(), message.begin() + 1, message.end());
    AddEvent(tick, kSysEx, std::move(bytes));
}

void TrackBuilder::NoteOn(uint32_t tick, int ch, int key, int velocity, bool before_programs, int step)
{
    AddEvent(tick, before_programs ? kNoteOnFirst : kNoteOn,
             {static_cast<uint8_t>(0x90 | ch), Clamp7(key), std::max<uint8_t>(1, Clamp7(velocity))}, step);
}

void TrackBuilder::NoteOff(uint32_t tick, int ch, int key)
{
    AddEvent(tick, kNoteOff, {static_cast<uint8_t>(0x80 | ch), Clamp7(key), 0});
}

void TrackBuilder::KeyPressure(uint32_t tick, int ch, int key, int value, int step)
{
    AddEvent(tick, kKeyPressure, {static_cast<uint8_t>(0xA0 | ch), Clamp7(key), Clamp7(value)}, step);
}

void TrackBuilder::Control(uint32_t tick, int ch, int cc, int value)
{
    AddEvent(tick, kControl, {static_cast<uint8_t>(0xB0 | ch), static_cast<uint8_t>(cc & 0x7F), Clamp7(value)});
}

void TrackBuilder::BendRange(uint32_t tick, int ch, int semitones)
{
    Control(tick, ch, cc::kRpnMsb, 0);
    Control(tick, ch, cc::kRpnLsb, 0);
    Control(tick, ch, cc::kDataEntry, semitones);
    Control(tick, ch, cc::kDataEntryLsb, 0);
    Control(tick, ch, cc::kRpnMsb, 127);
    Control(tick, ch, cc::kRpnLsb, 127);
}

void TrackBuilder::Program(uint32_t tick, int ch, int program, int step)
{
    AddEvent(tick, kProgram, {static_cast<uint8_t>(0xC0 | ch), Clamp7(program)}, step);
}

void TrackBuilder::Bank(uint32_t tick, int ch, int bank, int step)
{
    AddEvent(tick, kBank, {static_cast<uint8_t>(0xB0 | ch), cc::kBankSelect, static_cast<uint8_t>(bank & 0x7F)}, step);
    AddEvent(tick, kBank, {static_cast<uint8_t>(0xB0 | ch), cc::kBankSelectLsb, 0}, step);
}

void TrackBuilder::PitchBend(uint32_t tick, int ch, int value)
{
    value = std::clamp(value, 0, 16383);
    AddEvent(tick, kBend,
             {static_cast<uint8_t>(0xE0 | ch), static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>(value >> 7)});
}

void TrackBuilder::Retime(const std::function<uint32_t(uint32_t)>& place)
{
    for (Event& e : events_)
    {
        e.tick = place(e.tick);
    }
    end_ = place(end_);

    // Each note off ends the latest note on of its key on its channel, in the order they were added.
    std::map<std::pair<int, int>, std::size_t> sounding;
    std::vector<bool> left_out(events_.size());
    for (std::size_t i = 0; i < events_.size(); i++)
    {
        const std::vector<uint8_t>& b = events_[i].bytes;
        const int kind = b[0] & 0xF0;
        if (kind != 0x80 && kind != 0x90)
        {
            continue;
        }

        const std::pair<int, int> key = {b[0] & 0x0F, b[1]};
        if (kind == 0x90 && b[2])
        {
            sounding[key] = i;
            continue;
        }

        const auto it = sounding.find(key);
        if (it != sounding.end())
        {
            if (events_[it->second].tick == events_[i].tick)
            {
                left_out[it->second] = left_out[i] = true;
            }
            sounding.erase(it);
        }
    }

    std::size_t kept = 0;
    for (std::size_t i = 0; i < events_.size(); i++)
    {
        if (left_out[i])
        {
            continue;
        }

        if (kept != i)
        {
            events_[kept] = std::move(events_[i]);
        }
        kept++;
    }
    events_.resize(kept);
}

std::vector<uint8_t> TrackBuilder::Encode() const
{
    std::vector<const Event*> sorted;
    sorted.reserve(events_.size());
    for (const Event& e : events_)
    {
        sorted.push_back(&e);
    }

    // Stable: events of the same tick, step and order stay in the order they were added.
    auto by_tick_step_and_order = [](const Event* a, const Event* b)
    {
        return std::tie(a->tick, a->step, a->order) < std::tie(b->tick, b->step, b->order);
    };
    std::stable_sort(sorted.begin(), sorted.end(), by_tick_step_and_order);

    // The events are sorted, and none comes after the track's end.
    TrackEncoder encoder;
    for (const Event* e : sorted)
    {
        encoder.Event(e->tick, e->bytes);
    }
    encoder.End(end_);

    return encoder.Data();
}

std::vector<uint8_t> FileBuilder::Encode() const
{
    std::vector<uint8_t> out;
    AppendHeader(out, 1, static_cast<uint16_t>(tracks_.size()), division_);
    for (const TrackBuilder& t : tracks_)
    {
        AppendTrack(out, t.Encode());
    }

    return out;
}

} // namespace seqlib::midi
