// SPDX-License-Identifier: MIT

// Reading Standard MIDI Files: the header, each track's events with their ticks, and the notes, names, markers and
// bends in them. Problems() lists the problems in a file, such as a note that never ends. A few of its rules are
// stricter than the Standard MIDI File specification. Problems() reports running status, for example.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace seqlib::midi
{

// An event: its tick, counted from the start of its track, and its bytes. A channel message written with running status
// gets its status byte back. Every event then starts with its status byte. A meta event's bytes are FF, its type, its
// length and its data. A system exclusive event's bytes are F0 or F7, its length and its data.
struct Event
{
    bool operator==(const Event&) const = default;

    uint64_t tick = 0;
    std::vector<uint8_t> bytes;
};

// A track chunk's events, and the tick of its last event.
struct Track
{
    std::vector<Event> events;
    uint64_t end_tick = 0;
};

// A MIDI file's header and tracks.
struct File
{
    // Returns the events of every track whose first byte is `status`, with the channel in the low nibble of a channel
    // message's status, in order of tick. Events at the same tick keep the order of their tracks.
    std::vector<Event> Find(uint8_t status) const;

    uint16_t format = 0;
    uint16_t division = 0; // ticks per quarter note, or an SMPTE timing if the top bit is set
    std::vector<Track> tracks;
};

// Reads a file. Throws ReadError for a missing or short header, fewer track chunks than the header gives, a chunk that
// runs past the end of the data, a variable-length number longer than four bytes, an event that runs past the end of
// its track, running status with no status before it, and a status byte that a file can't hold. Chunks other than MTrk
// are skipped, and so is anything after the last track.
File Read(std::span<const uint8_t> data);

// A note: its track, channel and key, its note-on's velocity, and the ticks of its note-on and note-off. A note-on of
// velocity 0 is a note-off.
struct Note
{
    bool operator==(const Note&) const = default;

    int track = 0;
    int channel = 0;
    int key = 0;
    int velocity = 0;
    uint64_t on = 0;
    uint64_t off = 0;
};

// Returns the notes of each track, in the order of their note-offs, track by track. A note-off ends the earliest note
// of its key that sounds on its channel. A note that never ends is left out.
std::vector<Note> Notes(const File& file);

// Returns each track's name: the text of its last track name event (FF 03), or an empty string.
std::vector<std::string> TrackNames(const File& file);

// A text meta event's tick and text.
struct Text
{
    bool operator==(const Text&) const = default;

    uint64_t tick = 0;
    std::string text;
};

// Returns the markers (FF 06) of every track, track by track.
std::vector<Text> Markers(const File& file);

// A pitch bend: its track, channel and tick, and its 14-bit value, 8192 at the centre.
struct Bend
{
    bool operator==(const Bend&) const = default;

    int track = 0;
    int channel = 0;
    uint64_t tick = 0;
    int value = 0;
};

// Returns the pitch bends of every track, track by track.
std::vector<Bend> Bends(const File& file);

// Returns the problems in the file `data`, one line each: malformed data (what Read throws), a header that isn't six
// bytes, a chunk other than MTrk, data after the last track, running status, a data byte above 127, a track that
// doesn't end with one end-of-track event, a note that starts again on its channel before it ends, and a note that
// never ends.
std::vector<std::string> Problems(std::span<const uint8_t> data);

} // namespace seqlib::midi
