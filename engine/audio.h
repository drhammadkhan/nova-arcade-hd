// =====================================================================
//  The Nova HD synth: 48 kHz stereo, everything synthesised (no samples).
//  Instruments: a plucked koto (Karplus-Strong), a breathy bamboo flute,
//  FM bells, marimba, warm pads and strings, basses, and synthesised drums
//  including taiko. A shared reverb and echo glue it together.
//
//  A song is a small tracker score, written as text:
//    chords  one chord per bar: "Am F C G" (two in a bar: "Am,F")
//            qualities: (none) major, m, 7, m7, maj7, sus2, sus4
//    lead /  16 tokens per bar, space separated, '|' between bars is ignored:
//    counter "E5" note, "-" hold, "." rest
//    arp     16 characters per bar: 1-4 chord tones, 5-8 the same an octave up, '-' hold, '.' rest
//    bass    16 characters per bar: R root, 3 third, 5 fifth, O octave, L low fifth, '-' hold, '.' rest
//    drums   16 characters per bar: K kick, S snare, C clap, h hat, o open hat,
//            T taiko, t small taiko, w woodblock, s shaker, '.' nothing
//  Pattern strings may hold several bars separated by '|'; they cycle.
// =====================================================================
#pragma once
#include "nova.h"

namespace nova::audio {

enum Inst : uint8_t {
  I_NONE, I_KOTO, I_FLUTE, I_BELL, I_MARIMBA, I_PAD, I_STRINGS, I_SAWLEAD, I_SQUARE,
  I_PLUCKBASS, I_SUBBASS, I_CHOIR, I_INST_COUNT
};

struct Track { Inst inst = I_NONE; float vol = 1; float pan = 0; const char* notes = nullptr; };

struct Song {
  float bpm;
  int bars;
  const char* chords;
  Track lead, counter;
  Inst arpInst; const char* arp; float arpVol;
  Inst bassInst; const char* bass; float bassVol;
  Inst padInst; float padVol;
  const char* drums; float drumVol;
  float swing;      // 0..0.3: delays every other 16th
  bool loop;
};

// A continuous engine drone for racing games: pitch in Hz, volume 0..1. Call it every frame while the
// engine should sound; it fades out a moment after the calls stop (pause, leaving the game).
void engine(float hz, float vol);

// Render audio without a device (tests and tools): interleaved stereo float, 48 kHz.
void renderOffline(float* out, int frames);
void init(bool openDevice);
const int RATE = 48000;

}  // namespace nova::audio
