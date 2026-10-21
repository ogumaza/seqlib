// SPDX-License-Identifier: MIT

// Standard MIDI File (format 1) builder: events added in any order, sorted when the file is encoded.

#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "seqlib/midi/controllers.h"

namespace seqlib::midi
{

// Sets CC10 and CC11 for the levels `left` and `right` (full scale 128). CC11 sets the loudness. Under the SF2 default
// modulator, a controller value c scales the amplitude by (c/127)^2. CC10 sets the ratio between the sides, for a
// constant-power pan law. When both levels are 0, only CC11 changes.
void LevelsToControllers(double left, double right, int& cc10, int& cc11);

// A track of a Standard MIDI File. Events can be added in any order. They're written by tick. At the same tick, meta
// events come first, then system exclusive messages, note-offs, key pressures, bank selects, program changes,
// controllers, pitch bends and note-ons. Events of one kind keep the order they were added in. A tick's events can also
// take steps. Each step's events come after those of the steps before it. Steps allow a note to play between two
// program changes at its tick.
class TrackBuilder
{
public:
    void Meta(uint32_t tick, uint8_t type, const std::string& text);

    void Name(const std::string& text)
    {
        Meta(0, 0x03, text);
    }

    void Tempo(uint32_t tick, uint32_t micros_per_quarter);
    void TimeSignature(uint32_t tick, int numerator, int denominator_pow2);

    // Adds a tempo change at `tick` to the tempo that the track's tempo changes give there, if the track has any. The
    // change goes after the meta events added at `tick` so far, such as a loopStart marker. A player that jumps back to
    // the marker and keeps the tempo it was playing then takes the tempo that the tick had the first time.
    void RepeatTempo(uint32_t tick);

    // Adds the system exclusive message `message`, from its F0 to its F7. The event holds F0, the number of bytes after
    // F0 as a variable-length quantity, then those bytes. Throws std::invalid_argument for a message that doesn't start
    // with F0 and end with F7, or that has a byte above 127 between them.
    void SysEx(uint32_t tick, const std::vector<uint8_t>& message);

    // Adds a note-on at step `step` of its tick. With `before_programs`, the note-on goes before the bank selects and
    // program changes at the same step. Such a note plays with the program from before them.
    void NoteOn(uint32_t tick, int ch, int key, int velocity, bool before_programs = false, int step = 0);

    void NoteOff(uint32_t tick, int ch, int key);

    // Adds a polyphonic key pressure of `value` for `key` on channel `ch`, at step `step` of its tick. It goes before
    // every note-on at the same step. That includes a note-on that goes before program changes. A note of that key that
    // starts at the same step then starts with the new pressure.
    void KeyPressure(uint32_t tick, int ch, int key, int value, int step = 0);

    void Control(uint32_t tick, int ch, int cc, int value);

    // Adds a program change, at step `step` of its tick.
    void Program(uint32_t tick, int ch, int program, int step = 0);

    // Adds a bank select (CC0 + CC32). It goes before program changes at the same tick and step.
    void Bank(uint32_t tick, int ch, int bank, int step = 0);

    // Adds a pitch bend of `value`, from 0 to 16383 with the centre at 8192.
    void PitchBend(uint32_t tick, int ch, int value);

    // Sets channel `ch`'s pitch bend range to `semitones` with RPN 0, then selects the null RPN. With the null RPN
    // selected, later data entry changes nothing. The six controllers sort with the tick's other controllers. No pitch
    // bend is added.
    void BendRange(uint32_t tick, int ch, int semitones);

    // Makes the track last at least until `tick`.
    void SetEnd(uint32_t tick)
    {
        end_ = std::max(end_, tick);
    }

    // Moves each event, and the track's end, to the tick that `place` returns for the event's tick. `place` mustn't put
    // any two ticks out of order. A note whose note-on and note-off then fall on the same tick would no longer play.
    // The note is left out.
    void Retime(const std::function<uint32_t(uint32_t)>& place);

    // Returns the track's events as the data of an MTrk chunk.
    std::vector<uint8_t> Encode() const;

private:
    // The order of events at the same tick.
    enum Order : int
    {
        kMeta = 0,
        kSysEx = 5,
        kNoteOff = 10,
        kKeyPressure = 11,
        kNoteOnFirst = 12,
        kBank = 15,
        kProgram = 20,
        kControl = 30,
        kBend = 40,
        kNoteOn = 50
    };

    struct Event
    {
        uint32_t tick;
        int step; // its step at the tick. Steps sort before `order`.
        int order;
        std::vector<uint8_t> bytes;
    };

    void AddEvent(uint32_t tick, int order, std::vector<uint8_t> bytes, int step = 0);

    std::vector<Event> events_;
    uint32_t end_ = 0;
};

// A Standard MIDI File of format 1: tracks played together, timed in ticks of 1/`division` of a quarter note.
class FileBuilder
{
public:
    explicit FileBuilder(uint16_t division) : division_(division)
    {
    }

    TrackBuilder& AddTrack()
    {
        tracks_.emplace_back();
        return tracks_.back();
    }

    // Sets the ticks in a quarter note.
    void SetDivision(uint16_t division)
    {
        division_ = division;
    }

    // Moves the events of every track with TrackBuilder::Retime().
    void Retime(const std::function<uint32_t(uint32_t)>& place)
    {
        for (TrackBuilder& track : tracks_)
        {
            track.Retime(place);
        }
    }

    // Returns the bytes of the file.
    std::vector<uint8_t> Encode() const;

private:
    uint16_t division_;
    std::deque<TrackBuilder> tracks_; // deque: AddTrack() references stay valid
};

} // namespace seqlib::midi
