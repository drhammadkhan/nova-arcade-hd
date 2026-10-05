// The Nova HD synth. See audio.h for the song format.
#include "audio.h"
#include <SDL3/SDL.h>
#include <vector>
#include <string>

namespace nova::audio {

int mute = 0;
static const float SR = (float)RATE;
static const float TAU = 6.28318531f;
static SDL_AudioStream* stream = nullptr;
static SDL_Mutex* lock = nullptr;
static int vol = 7;
static bool ducked = false;

// ------------------------------------------------------------ instruments
struct InstDef {
  float a, d, s, r;     // envelope: attack, decay (time constant), sustain level, release (time constant)
  float gain, rev, echo;
};
static const InstDef INST[I_INST_COUNT] = {
  /*NONE     */ {0.01f, 0.1f, 0, 0.1f, 0, 0, 0},
  /*KOTO     */ {0.001f, 9.0f, 0, 0.30f, 0.55f, 0.30f, 0.12f},
  /*FLUTE    */ {0.06f, 0.6f, 0.85f, 0.10f, 0.32f, 0.35f, 0.18f},
  /*BELL     */ {0.002f, 0.9f, 0, 0.5f, 0.30f, 0.40f, 0.10f},
  /*MARIMBA  */ {0.002f, 0.28f, 0, 0.12f, 0.45f, 0.25f, 0.05f},
  /*PAD      */ {0.7f, 2.0f, 0.75f, 0.9f, 0.13f, 0.50f, 0},
  /*STRINGS  */ {0.25f, 1.5f, 0.8f, 0.5f, 0.12f, 0.45f, 0},
  /*SAWLEAD  */ {0.01f, 0.5f, 0.7f, 0.10f, 0.20f, 0.25f, 0.22f},
  /*SQUARE   */ {0.005f, 0.4f, 0.6f, 0.06f, 0.16f, 0.20f, 0.15f},
  /*PLUCKBASS*/ {0.003f, 0.25f, 0.35f, 0.06f, 0.42f, 0.05f, 0},
  /*SUBBASS  */ {0.006f, 0.6f, 0.85f, 0.08f, 0.50f, 0.03f, 0},
  /*CHOIR    */ {0.35f, 2.0f, 0.8f, 0.7f, 0.16f, 0.55f, 0},
};

enum Kind : uint8_t {
  K_INST,                                  // a pitched instrument note (inst says which)
  K_KICK, K_SNARE, K_CLAP, K_HAT, K_OHAT, K_TAIKO, K_STAIKO, K_WOOD, K_SHAKER,
  K_TONE,                                  // a sound effect sweep
};
enum Wave : uint8_t { W_SINE, W_TRI, W_SQUARE, W_SAW, W_NOISE };

struct Voice {
  bool on;
  Kind kind;
  Inst inst;
  int8_t owner;            // -1 sound effect, 0.. song tracks
  bool music;
  float freq, gain, pan;
  float t, delay;          // seconds since it started sounding; seconds before it starts
  bool gate;
  float relT, relAmp, lastAmp;
  float ph[3], fmph;
  float lp, bp, lp2, bp2;  // two state-variable filters
  // Karplus-Strong string
  float ks[2400]; int ksLen, ksPos; float apC, apX, apY;
  // sweeps for sound effects
  Wave wave; float f1, sweep, dur, decay, cut, noiseMix;
  uint32_t nseed;
  uint32_t age;
};
static const int NV = 40;
static Voice voices[NV];
static uint32_t ageCounter = 0;

static inline float noise(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFF) / 32768.0f - 1.0f; }
static inline float mtof(float m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }
// one state-variable filter step: returns low-pass; band-pass left in bp
static inline float svf(float in, float cutoff, float q, float& lp, float& bp) {
  float f = 2 * sinf(3.14159265f * fminf(cutoff, SR * 0.22f) / SR);
  float hp = in - lp - q * bp;
  bp += f * hp;
  lp += f * bp;
  return lp;
}
static inline float polyblep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1; }
  if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
  return 0;
}
static inline float saw(float& ph, float f) {
  float dt = f / SR;
  float v = 2 * ph - 1 - polyblep(ph, dt);
  ph += dt; if (ph >= 1) ph -= 1;
  return v;
}
static inline float square(float& ph, float f) {
  float dt = f / SR;
  float v = (ph < 0.5f ? 1 : -1) + polyblep(ph, dt) - polyblep(fmodf(ph + 0.5f, 1), dt);
  ph += dt; if (ph >= 1) ph -= 1;
  return v;
}
static inline float sine(float& ph, float f) {
  float v = sinf(ph * TAU);
  ph += f / SR; if (ph >= 1) ph -= 1;
  return v;
}

static Voice* alloc() {
  Voice* best = nullptr;
  for (auto& v : voices) if (!v.on) { best = &v; break; }
  if (!best) {   // steal the oldest released voice, else the oldest
    for (auto& v : voices) if (!v.gate && (!best || v.age < best->age)) best = &v;
    if (!best) for (auto& v : voices) if (!best || v.age < best->age) best = &v;
  }
  memset((void*)best, 0, offsetof(Voice, ks));
  best->on = true; best->gate = true; best->age = ++ageCounter; best->owner = -1;
  best->nseed = 0x12345u + ageCounter * 2654435761u;
  return best;
}

static Voice* noteOn(Inst inst, float freq, float gain, float pan, float delay = 0, int owner = -1, bool music = false) {
  Voice* v = alloc();
  v->kind = K_INST; v->inst = inst; v->freq = freq; v->gain = gain; v->pan = pan; v->delay = delay;
  v->owner = owner; v->music = music;
  if (inst == I_KOTO) {
    // Karplus-Strong: a delay line one period long, filled with a bright burst of noise
    float period = SR / freq;
    int n = (int)floorf(period - 0.5f);
    n = clampv(n, 2, 2399);
    float frac = period - n - 0.5f;
    v->ksLen = n; v->ksPos = 0;
    v->apC = (1 - frac) / (1 + frac);
    v->apX = v->apY = 0;
    float lp = 0;
    for (int i = 0; i < n; i++) {
      float x = noise(v->nseed);
      lp += (x - lp) * 0.6f;          // soften the pluck a touch
      v->ks[i] = lp * 0.9f + x * 0.1f;
    }
  }
  return v;
}
static void noteOff(Voice* v) {
  if (v && v->on && v->gate) { v->gate = false; v->relT = v->t; v->relAmp = v->lastAmp; }
}

static Voice* drum(Kind k, float gain, float pan, float delay = 0, bool music = true) {
  Voice* v = alloc();
  v->kind = k; v->gain = gain; v->pan = pan; v->delay = delay; v->music = music; v->gate = false;
  return v;
}

static Voice* tone(Wave w, float f0, float f1, float sweep, float decay, float dur, float gain, float delay = 0, float cut = 12000, float noiseMix = 0) {
  Voice* v = alloc();
  v->kind = K_TONE; v->wave = w; v->freq = f0; v->f1 = f1; v->sweep = sweep; v->decay = decay; v->dur = dur;
  v->gain = gain; v->delay = delay; v->cut = cut; v->noiseMix = noiseMix; v->gate = false;
  return v;
}

// ------------------------------------------------------------ rendering one voice
// returns the mono sample; sets the reverb and echo send levels
static float renderVoice(Voice& v, float& rev, float& echo) {
  if (v.delay > 0) { v.delay -= 1 / SR; rev = echo = 0; return 0; }
  float t = v.t;
  v.t += 1 / SR;
  float out = 0;
  rev = 0.2f; echo = 0;
  if (v.kind == K_INST) {
    const InstDef& d = INST[v.inst];
    float amp;
    if (t < d.a) amp = t / d.a;
    else amp = d.s + (1 - d.s) * expf(-(t - d.a) / d.d);
    if (!v.gate) amp = v.relAmp * expf(-(t - v.relT) / d.r);
    v.lastAmp = v.gate ? amp : v.lastAmp;
    if (!v.gate && amp < 0.0005f) { v.on = false; return 0; }
    float f = v.freq, s = 0;
    switch (v.inst) {
      case I_KOTO: {
        float x = v.ks[v.ksPos];
        int nx = v.ksPos + 1 < v.ksLen ? v.ksPos + 1 : 0;
        float damp = v.gate ? 0.4985f : 0.47f;
        float y = (x + v.ks[nx]) * damp;
        float ap = v.apC * y + v.apX - v.apC * v.apY;   // allpass for fine tuning
        v.apX = y; v.apY = ap;
        v.ks[v.ksPos] = ap;
        v.ksPos = nx;
        s = x * 1.2f + sinf(t * f * TAU) * 0.4f * expf(-t * 18);   // a little body thump on the attack
        break;
      }
      case I_FLUTE: {
        float vib = 1 + 0.006f * fminf(1, fmaxf(0, (t - 0.25f) * 3)) * sinf(t * 5.6f * TAU);
        float ff = f * vib;
        float p = v.ph[0];
        s = sinf(p * TAU) + 0.18f * sinf(2 * p * TAU) + 0.06f * sinf(3 * p * TAU);
        v.ph[0] += ff / SR; if (v.ph[0] >= 1) v.ph[0] -= 1;
        float n = noise(v.nseed);
        svf(n, ff * 2, 0.35f, v.lp, v.bp);
        float chiff = 0.5f * expf(-t * 25);
        s = s * 0.85f + v.bp * (0.16f + chiff);
        break;
      }
      case I_BELL: {
        float idx = 2.6f * expf(-t * 5) + 0.25f;
        float m = sinf(v.fmph * TAU) * idx;
        v.fmph += f * 3.5f / SR; if (v.fmph >= 1) v.fmph -= 1;
        s = sinf((v.ph[0] + m / TAU) * TAU) * 0.8f + sinf(v.ph[1] * TAU) * 0.25f * expf(-t * 1.5f);
        v.ph[0] += f / SR; if (v.ph[0] >= 1) v.ph[0] -= 1;
        v.ph[1] += f * 2.01f / SR; if (v.ph[1] >= 1) v.ph[1] -= 1;
        break;
      }
      case I_MARIMBA: {
        float idx = 1.8f * expf(-t * 40);
        float m = sinf(v.fmph * TAU) * idx;
        v.fmph += f * 4 / SR; if (v.fmph >= 1) v.fmph -= 1;
        s = sinf((v.ph[0] + m / TAU) * TAU) + 0.2f * sinf(v.ph[1] * TAU) * expf(-t * 20);
        v.ph[0] += f / SR; if (v.ph[0] >= 1) v.ph[0] -= 1;
        v.ph[1] += f * 3.9f / SR; if (v.ph[1] >= 1) v.ph[1] -= 1;
        break;
      }
      case I_PAD: case I_STRINGS: case I_CHOIR: {
        bool str = v.inst == I_STRINGS;
        float det = str ? 0.004f : 0.007f;
        float vib = str ? 1 + 0.003f * sinf(t * 5 * TAU) : 1;
        s = (saw(v.ph[0], f * vib) + saw(v.ph[1], f * (1 + det) * vib) + saw(v.ph[2], f * (1 - det) * vib)) * 0.4f;
        if (v.inst == I_CHOIR) {   // an "aah": two formant band-passes
          svf(s, 750, 0.25f, v.lp, v.bp);
          svf(s, 1150, 0.25f, v.lp2, v.bp2);
          s = (v.bp + v.bp2 * 0.7f) * 1.4f;
        } else {
          float cut = str ? 2600 : 900 + 300 * sinf(t * 0.4f * TAU);
          s = svf(s, cut, 0.8f, v.lp, v.bp);
          s = svf(s, cut * 1.3f, 0.9f, v.lp2, v.bp2);
        }
        break;
      }
      case I_SAWLEAD: {
        float vib = 1 + 0.004f * fminf(1, t * 2) * sinf(t * 5.5f * TAU);
        s = (saw(v.ph[0], f * vib) + saw(v.ph[1], f * 1.004f * vib)) * 0.5f;
        float cut = 1800 + 3000 * expf(-t * 6);
        s = svf(s, cut, 0.6f, v.lp, v.bp);
        break;
      }
      case I_SQUARE: {
        float vib = 1 + 0.005f * fminf(1, t * 3) * sinf(t * 6 * TAU);
        s = square(v.ph[0], f * vib) * 0.6f;
        s = svf(s, 4500, 0.9f, v.lp, v.bp);
        break;
      }
      case I_PLUCKBASS: {
        s = saw(v.ph[0], f) * 0.7f + square(v.ph[1], f * 0.5f) * 0.3f;
        float cut = 250 + 1600 * expf(-t * 14);
        s = svf(s, cut, 0.5f, v.lp, v.bp);
        break;
      }
      case I_SUBBASS: {
        float x = sine(v.ph[0], f) + 0.3f * sine(v.ph[1], f * 2) * expf(-t * 8);
        s = x / (1 + fabsf(x) * 0.4f);
        break;
      }
      default: break;
    }
    out = s * amp * d.gain;
    rev = d.rev; echo = d.echo;
  } else if (v.kind == K_TONE) {
    if (t > v.dur) { v.on = false; return 0; }
    float k = v.sweep > 0 ? fminf(1, t / v.sweep) : 1;
    float f = v.freq * powf(v.f1 / v.freq, k);
    float s;
    switch (v.wave) {
      case W_SINE: s = sine(v.ph[0], f); break;
      case W_TRI: { s = 4 * fabsf(v.ph[0] - 0.5f) - 1; v.ph[0] += f / SR; if (v.ph[0] >= 1) v.ph[0] -= 1; break; }
      case W_SQUARE: s = square(v.ph[0], f) * 0.6f; break;
      case W_SAW: s = saw(v.ph[0], f) * 0.6f; break;
      default: s = noise(v.nseed); break;
    }
    if (v.noiseMix > 0) s = s * (1 - v.noiseMix) + noise(v.nseed) * v.noiseMix;
    if (v.wave == W_NOISE) { svf(s, f, 0.5f, v.lp, v.bp); s = v.bp * 1.6f; }
    else s = svf(s, v.cut, 0.8f, v.lp, v.bp);
    float amp = fminf(1, t / 0.004f) * expf(-t / v.decay);
    float tail = fminf(1, (v.dur - t) / 0.02f);
    out = s * amp * tail;
    rev = 0.25f; echo = 0.05f;
  } else {
    // drums
    float s = 0, n = noise(v.nseed);
    float life = 0.6f;
    switch (v.kind) {
      case K_KICK: {
        float f = 48 + 130 * expf(-t * 32);
        s = sine(v.ph[0], f) * expf(-t * 7.5f) + n * 0.25f * expf(-t * 260);
        s = s / (1 + fabsf(s) * 0.3f) * 1.1f;
        life = 0.5f; rev = 0.05f; break;
      }
      case K_SNARE: {
        float hp = n - svf(n, 1800, 0.7f, v.lp, v.bp);
        s = sine(v.ph[0], 185) * 0.5f * expf(-t * 28) + hp * 0.75f * expf(-t * 17);
        life = 0.35f; rev = 0.25f; break;
      }
      case K_CLAP: {
        svf(n, 1300, 0.4f, v.lp, v.bp);
        float e = t < 0.03f ? expf(-fmodf(t, 0.01f) * 300) : expf(-(t - 0.03f) * 22);
        s = v.bp * e * 1.6f;
        life = 0.3f; rev = 0.3f; break;
      }
      case K_HAT: case K_OHAT: {
        float hp = n - svf(n, 7500, 0.6f, v.lp, v.bp);
        s = hp * 0.45f * expf(-t * (v.kind == K_HAT ? 70.0f : 9.0f));
        life = v.kind == K_HAT ? 0.1f : 0.45f; rev = 0.1f; break;
      }
      case K_TAIKO: {
        float f = 62 + 55 * expf(-t * 18);
        float lowN = svf(n, 260, 0.6f, v.lp, v.bp);
        s = sine(v.ph[0], f) * expf(-t * 4.5f) * 1.1f + lowN * 1.2f * expf(-t * 22) + sine(v.ph[1], f * 1.52f) * 0.25f * expf(-t * 9);
        s = s / (1 + fabsf(s) * 0.4f);
        life = 1.0f; rev = 0.45f; break;
      }
      case K_STAIKO: {
        float f = 150 + 90 * expf(-t * 26);
        float lowN = svf(n, 600, 0.6f, v.lp, v.bp);
        s = sine(v.ph[0], f) * expf(-t * 10) * 0.8f + lowN * 0.7f * expf(-t * 35);
        life = 0.5f; rev = 0.35f; break;
      }
      case K_WOOD: {
        s = (sine(v.ph[0], 880) + 0.5f * sine(v.ph[1], 880 * 2.76f)) * expf(-t * 45) * 0.6f;
        life = 0.15f; rev = 0.25f; break;
      }
      case K_SHAKER: {
        svf(n, 6500, 0.5f, v.lp, v.bp);
        s = v.bp * fminf(1, t / 0.012f) * expf(-t * 30) * 0.8f;
        life = 0.15f; rev = 0.1f; break;
      }
      default: break;
    }
    if (t > life) { v.on = false; return 0; }
    out = s;
  }
  return out * v.gain;
}

// ------------------------------------------------------------ reverb (Freeverb) and echo
struct Comb {
  std::vector<float> buf; int pos = 0; float store = 0;
  float run(float in, float fb, float damp) {
    float o = buf[pos];
    store = o * (1 - damp) + store * damp;
    buf[pos] = in + store * fb;
    if (++pos >= (int)buf.size()) pos = 0;
    return o;
  }
};
struct Allpass {
  std::vector<float> buf; int pos = 0;
  float run(float in) {
    float b = buf[pos];
    float o = -in + b;
    buf[pos] = in + b * 0.5f;
    if (++pos >= (int)buf.size()) pos = 0;
    return o;
  }
};
static Comb combs[2][8];
static Allpass alls[2][4];
static std::vector<float> echoBuf[2];
static int echoPos = 0;

static void initFx() {
  static const int CT[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
  static const int AT[4] = {556, 441, 341, 225};
  float k = SR / 44100.0f;
  for (int c = 0; c < 2; c++) {
    for (int i = 0; i < 8; i++) combs[c][i].buf.assign((int)((CT[i] + c * 23) * k), 0);
    for (int i = 0; i < 4; i++) alls[c][i].buf.assign((int)((AT[i] + c * 23) * k), 0);
    echoBuf[c].assign((int)(SR * 0.75f), 0);
  }
}

// ------------------------------------------------------------ the sequencer
struct Pattern { std::vector<std::string> bars; bool empty() const { return bars.empty(); } };
struct Chord { int root; int iv[4]; int n; };
struct Seq {
  const Song* song = nullptr;
  std::vector<int16_t> lead, counter;      // per 16th: midi note, -1 rest, -2 hold
  std::vector<Chord> chords;               // per half bar
  Pattern arp, bass, drums;
  int step = 0;                            // 16ths since the start
  double untilNext = 0;                    // samples
  Voice* held[4] = {};                     // lead, counter, arp, bass
  Voice* pad[4] = {};
  bool done = true;
};
static Seq seq;

static int parseNote(const char*& p) {
  static const int SEMI[7] = {9, 11, 0, 2, 4, 5, 7};   // A B C D E F G
  char c = *p;
  if (c >= 'a' && c <= 'g') c -= 32;
  if (c < 'A' || c > 'G') return -1;
  int n = SEMI[c - 'A'];
  p++;
  if (*p == '#') { n++; p++; } else if (*p == 'b') { n--; p++; }
  int oct = 4;
  if (*p >= '0' && *p <= '9') { oct = *p - '0'; p++; }
  return 12 * (oct + 1) + n;
}

static std::vector<int16_t> parseNotes(const char* s, int steps) {
  std::vector<int16_t> out;
  if (!s) return out;
  const char* p = s;
  while (*p && (int)out.size() < steps) {
    while (*p == ' ' || *p == '|' || *p == '\n') p++;
    if (!*p) break;
    if (*p == '-') { out.push_back(-2); p++; }
    else if (*p == '.') { out.push_back(-1); p++; }
    else {
      int n = parseNote(p);
      out.push_back((int16_t)n);
      while (*p && *p != ' ' && *p != '|') p++;
    }
  }
  while ((int)out.size() < steps) out.push_back(-2);
  return out;
}

static Pattern parsePattern(const char* s) {
  Pattern pt;
  if (!s) return pt;
  std::string cur;
  for (const char* p = s; ; p++) {
    if (*p == '|' || *p == 0) { if (!cur.empty()) pt.bars.push_back(cur); cur.clear(); if (!*p) break; }
    else if (*p != ' ') cur += *p;
  }
  return pt;
}

static Chord parseChord(const std::string& tok) {
  Chord c = {0, {0, 4, 7, 0}, 3};
  const char* p = tok.c_str();
  static const int SEMI[7] = {9, 11, 0, 2, 4, 5, 7};
  char r = *p;
  if (r >= 'A' && r <= 'G') { c.root = SEMI[r - 'A']; p++; }
  if (*p == '#') { c.root++; p++; } else if (*p == 'b') { c.root--; p++; }
  c.root = (c.root + 12) % 12;
  std::string q = p;
  auto set = [&](std::initializer_list<int> l) { c.n = 0; for (int x : l) c.iv[c.n++] = x; };
  if (q == "m") set({0, 3, 7});
  else if (q == "7") set({0, 4, 7, 10});
  else if (q == "m7") set({0, 3, 7, 10});
  else if (q == "maj7") set({0, 4, 7, 11});
  else if (q == "sus2") set({0, 2, 7});
  else if (q == "sus4") set({0, 5, 7});
  else set({0, 4, 7});
  return c;
}

static void stopSong() {
  for (auto& v : voices) if (v.on && v.music) noteOff(&v);
  seq = Seq();
}

static void startSong(const Song* s) {
  stopSong();
  if (!s) return;
  seq.song = s;
  int steps = s->bars * 16;
  seq.lead = parseNotes(s->lead.notes, steps);
  seq.counter = parseNotes(s->counter.notes, steps);
  std::string tok;
  std::vector<std::string> toks;
  for (const char* p = s->chords; ; p++) {
    if (*p == ' ' || *p == 0) { if (!tok.empty()) toks.push_back(tok); tok.clear(); if (!*p) break; }
    else tok += *p;
  }
  for (int b = 0; b < s->bars; b++) {
    std::string t = toks.empty() ? "C" : toks[b % toks.size()];
    size_t comma = t.find(',');
    Chord a = parseChord(t.substr(0, comma));
    Chord c = comma == std::string::npos ? a : parseChord(t.substr(comma + 1));
    seq.chords.push_back(a); seq.chords.push_back(c);
  }
  seq.arp = parsePattern(s->arp);
  seq.bass = parsePattern(s->bass);
  seq.drums = parsePattern(s->drums);
  seq.step = 0; seq.untilNext = 0; seq.done = false;
}

static char patAt(const Pattern& p, int bar, int i) {
  if (p.empty()) return '.';
  const std::string& s = p.bars[bar % p.bars.size()];
  return i < (int)s.size() ? s[i] : '.';
}

static void melody(int track, const Track& tr, const std::vector<int16_t>& notes, int step) {
  if (tr.inst == I_NONE || notes.empty()) return;
  int n = notes[step];
  if (n == -2) return;
  noteOff(seq.held[track]); seq.held[track] = nullptr;
  if (n >= 0) seq.held[track] = noteOn(tr.inst, mtof(n), tr.vol, tr.pan, 0, track, true);
}

static void seqStep() {
  const Song& s = *seq.song;
  int steps = s.bars * 16;
  if (seq.step >= steps) {
    if (!s.loop) { for (int i = 0; i < 4; i++) noteOff(seq.held[i]); for (auto* p : seq.pad) noteOff(p); seq.done = true; return; }
    seq.step = 0;
  }
  int st = seq.step, bar = st / 16, i = st % 16;
  const Chord& ch = seq.chords[bar * 2 + (i >= 8 ? 1 : 0)];
  // pad: restrike when the chord changes
  if (s.padInst != I_NONE && (i == 0 || i == 8)) {
    const Chord& prev = seq.chords[(bar * 2 + (i >= 8 ? 1 : 0) + seq.chords.size() - 1) % seq.chords.size()];
    bool changed = st == 0 || prev.root != ch.root || prev.n != ch.n || memcmp(prev.iv, ch.iv, sizeof(ch.iv)) != 0;
    if (changed) {
      for (auto*& p : seq.pad) { noteOff(p); p = nullptr; }
      int r = 48 + ch.root; if (r > 55) r -= 12;
      for (int k = 0; k < ch.n && k < 4; k++)
        seq.pad[k] = noteOn(s.padInst, mtof(r + ch.iv[k] + (k == 0 ? 12 : 0)), s.padVol, (k - 1.5f) * 0.35f, 0, 4, true);
    }
  }
  melody(0, s.lead, seq.lead, st);
  melody(1, s.counter, seq.counter, st);
  // arpeggio
  if (s.arpInst != I_NONE) {
    char c = patAt(seq.arp, bar, i);
    if (c >= '1' && c <= '8') {
      noteOff(seq.held[2]);
      int k = (c - '1') % 4, oct = (c - '1') / 4;
      int r = 60 + ch.root; if (r > 66) r -= 12;
      int idx = k % ch.n, extra = k / ch.n;
      int note = r + ch.iv[idx] + 12 * (oct + extra);
      seq.held[2] = noteOn(s.arpInst, mtof(note), s.arpVol, ((i * 5) % 7 - 3) * 0.12f, 0, 2, true);
    } else if (c == '.') { noteOff(seq.held[2]); seq.held[2] = nullptr; }
  }
  // bass
  if (s.bassInst != I_NONE) {
    char c = patAt(seq.bass, bar, i);
    int r = 36 + ch.root; if (r > 43) r -= 12;
    int note = -1;
    if (c == 'R') note = r;
    else if (c == '3') note = r + ch.iv[1];
    else if (c == '5') note = r + 7;
    else if (c == 'O') note = r + 12;
    else if (c == 'L') note = r - 5;
    if (note >= 0) { noteOff(seq.held[3]); seq.held[3] = noteOn(s.bassInst, mtof(note), s.bassVol, 0, 0, 3, true); }
    else if (c == '.') { noteOff(seq.held[3]); seq.held[3] = nullptr; }
  }
  // drums
  char d = patAt(seq.drums, bar, i);
  float dv = s.drumVol;
  switch (d) {
    case 'K': drum(K_KICK, 0.9f * dv, 0); break;
    case 'S': drum(K_SNARE, 0.6f * dv, 0.05f); break;
    case 'C': drum(K_CLAP, 0.55f * dv, -0.05f); break;
    case 'h': drum(K_HAT, 0.35f * dv, 0.3f); break;
    case 'o': drum(K_OHAT, 0.3f * dv, 0.3f); break;
    case 'T': drum(K_TAIKO, 0.95f * dv, -0.1f); break;
    case 't': drum(K_STAIKO, 0.7f * dv, 0.15f); break;
    case 'w': drum(K_WOOD, 0.45f * dv, -0.35f); break;
    case 's': drum(K_SHAKER, 0.4f * dv, 0.4f); break;
    case 'X': drum(K_KICK, 0.9f * dv, 0); drum(K_TAIKO, 0.7f * dv, 0); break;
    default: break;
  }
  seq.step++;
}

// ------------------------------------------------------------ mixing
static void render(float* out, int frames) {
  float master = (vol / 10.0f) * (vol / 10.0f) * 0.9f;
  float musicGain = ducked ? 0.35f : 1.0f;
  for (int f = 0; f < frames; f++) {
    if (seq.song && !seq.done) {
      if (seq.untilNext <= 0) {
        seqStep();
        double stepLen = 60.0 / seq.song->bpm / 4 * SR;
        float sw = seq.song->swing;
        seq.untilNext += stepLen * ((seq.step % 2) ? (1 + sw) : (1 - sw));
      }
      seq.untilNext -= 1;
    }
    float L = 0, Rr = 0, revIn = 0, echoL = 0, echoR = 0;
    for (auto& v : voices) {
      if (!v.on) continue;
      float rv, ec;
      float s = renderVoice(v, rv, ec);
      if (v.music) s *= musicGain;
      float pl = sqrtf(0.5f * (1 - v.pan)), pr = sqrtf(0.5f * (1 + v.pan));
      L += s * pl; Rr += s * pr;
      revIn += s * rv;
      echoL += s * ec * pl; echoR += s * ec * pr;
    }
    // echo: a dotted-eighth-ish ping-pong
    int elen = (int)echoBuf[0].size();
    float eL = echoBuf[0][echoPos], eR = echoBuf[1][echoPos];
    echoBuf[0][echoPos] = echoL + eR * 0.35f;
    echoBuf[1][echoPos] = echoR + eL * 0.35f;
    if (++echoPos >= (int)(elen * 0.5f)) echoPos = 0;
    // reverb
    float wet[2] = {0, 0};
    float in = revIn * 0.12f;
    for (int c = 0; c < 2; c++) {
      float acc = 0;
      for (auto& cb : combs[c]) acc += cb.run(in, 0.86f, 0.3f);
      for (auto& ap : alls[c]) acc = ap.run(acc);
      wet[c] = acc;
    }
    float oL = (L + eL * 0.6f + wet[0]) * master, oR = (Rr + eR * 0.6f + wet[1]) * master;
    // gentle limiter
    oL = oL / (1 + fabsf(oL) * 0.6f);
    oR = oR / (1 + fabsf(oR) * 0.6f);
    out[f * 2] = oL; out[f * 2 + 1] = oR;
  }
}

static void SDLCALL feed(void*, SDL_AudioStream* s, int additional, int) {
  static std::vector<float> buf;
  int frames = additional / (int)(sizeof(float) * 2);
  if (frames <= 0) return;
  buf.resize(frames * 2);
  SDL_LockMutex(lock);
  render(buf.data(), frames);
  SDL_UnlockMutex(lock);
  SDL_PutAudioStreamData(s, buf.data(), frames * (int)sizeof(float) * 2);
}

void renderOffline(float* out, int frames) {
  SDL_LockMutex(lock);
  render(out, frames);
  SDL_UnlockMutex(lock);
}

void init(bool openDevice) {
  lock = SDL_CreateMutex();
  initFx();
  vol = clampv(loadInt("arcade", "vol", 7), 0, 10);
  if (!openDevice) return;
  SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, RATE};
  stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, nullptr);
  if (!stream) { SDL_Log("no audio: %s", SDL_GetError()); return; }
  SDL_ResumeAudioStreamDevice(stream);
}

// ------------------------------------------------------------ public calls
void setVolume(int v) { SDL_LockMutex(lock); vol = clampv(v, 0, 10); SDL_UnlockMutex(lock); saveInt("arcade", "vol", vol); }
int volume() { return vol; }
void duck(bool on) { ducked = on; }
const Song* currentMusic() { return seq.song; }

void music(const Song* s) {
  if (mute) return;
  SDL_LockMutex(lock);
  if (s != seq.song || seq.done) startSong(s);
  SDL_UnlockMutex(lock);
}

static float hz(int midi) { return mtof((float)midi); }

void sfx(Sfx s, float pan, float pitch) {
  if (mute || !lock) return;
  SDL_LockMutex(lock);
  float p = pitch;
  switch (s) {
    case SFX_JUMP:
      tone(W_TRI, 330 * p, 820 * p, 0.13f, 0.09f, 0.22f, 0.32f, 0, 5000);
      tone(W_NOISE, 900 * p, 2600 * p, 0.12f, 0.06f, 0.15f, 0.10f);
      break;
    case SFX_LAND:
      tone(W_SINE, 140, 60, 0.06f, 0.04f, 0.1f, 0.35f);
      tone(W_NOISE, 500, 250, 0.05f, 0.03f, 0.08f, 0.14f);
      break;
    case SFX_COIN:
      noteOn(I_BELL, hz(83) * p, 0.55f, pan);
      noteOn(I_BELL, hz(88) * p, 0.6f, pan, 0.065f);
      break;
    case SFX_STOMP:
      tone(W_SINE, 300 * p, 70 * p, 0.12f, 0.07f, 0.18f, 0.6f);
      tone(W_NOISE, 1200, 300, 0.08f, 0.04f, 0.1f, 0.2f);
      noteOn(I_MARIMBA, hz(79) * p, 0.35f, pan, 0.03f);
      break;
    case SFX_ROLL:
      tone(W_NOISE, 500, 2600, 0.22f, 0.12f, 0.3f, 0.28f);
      tone(W_TRI, 220, 130, 0.2f, 0.1f, 0.24f, 0.16f);
      break;
    case SFX_HURT:
      tone(W_SQUARE, 520, 160, 0.25f, 0.14f, 0.32f, 0.3f, 0, 2500);
      tone(W_NOISE, 2000, 600, 0.2f, 0.08f, 0.2f, 0.12f);
      drum(K_STAIKO, 0.4f, 0, 0, false);
      break;
    case SFX_DIE: {
      static const int N[] = {76, 74, 71, 69, 64};
      for (int i = 0; i < 5; i++) noteOn(I_KOTO, hz(N[i]), 0.7f, pan, i * 0.11f);
      drum(K_TAIKO, 0.8f, 0, 0, false);
      break;
    }
    case SFX_POWERUP: {
      static const int N[] = {72, 76, 79, 84, 88};
      for (int i = 0; i < 5; i++) noteOn(I_MARIMBA, hz(N[i]) * p, 0.5f, -0.4f + i * 0.2f, i * 0.05f);
      noteOn(I_BELL, hz(96) * p, 0.25f, 0, 0.26f);
      break;
    }
    case SFX_BUMP:
      drum(K_WOOD, 0.55f, pan, 0, false);
      tone(W_SINE, 160, 90, 0.06f, 0.05f, 0.1f, 0.4f);
      break;
    case SFX_SCROLL: {
      static const int N[] = {81, 84, 88, 93};
      for (int i = 0; i < 4; i++) noteOn(I_BELL, hz(N[i]), 0.45f, -0.3f + i * 0.2f, i * 0.08f);
      Voice* c = noteOn(I_CHOIR, hz(69), 0.5f, 0); noteOff(c);
      Voice* c2 = noteOn(I_CHOIR, hz(76), 0.4f, 0); noteOff(c2);
      break;
    }
    case SFX_CHECKPOINT: {
      static const int N[] = {67, 74, 79, 83};
      for (int i = 0; i < 4; i++) noteOn(I_KOTO, hz(N[i]), 0.6f, -0.3f + i * 0.2f, i * 0.06f);
      noteOn(I_BELL, hz(91), 0.3f, 0, 0.24f);
      break;
    }
    case SFX_SPLASH:
      tone(W_NOISE, 3200, 300, 0.35f, 0.18f, 0.5f, 0.45f);
      for (int i = 0; i < 4; i++) tone(W_SINE, 500 + 100 * i, 1000 + 150 * ((i * 3) % 4), 0.05f, 0.03f, 0.06f, 0.12f, 0.1f + i * 0.07f);
      break;
    case SFX_KNOCK:
      drum(K_STAIKO, 0.6f, pan, 0, false);
      drum(K_WOOD, 0.4f, pan, 0.02f, false);
      noteOn(I_MARIMBA, hz(84) * p, 0.35f, pan, 0.04f);
      break;
    case SFX_ONEUP: {
      static const int N[] = {79, 84, 88, 91, 96};
      for (int i = 0; i < 5; i++) noteOn(I_MARIMBA, hz(N[i]), 0.5f, 0, i * 0.07f);
      break;
    }
    case SFX_MOVE: drum(K_WOOD, 0.22f, pan, 0, false); break;
    case SFX_SELECT: noteOn(I_BELL, hz(79), 0.35f, 0); noteOn(I_BELL, hz(86), 0.35f, 0, 0.06f); break;
    case SFX_BACK: noteOn(I_BELL, hz(79), 0.3f, 0); noteOn(I_BELL, hz(72), 0.3f, 0, 0.06f); break;
    case SFX_START: {
      static const int N[] = {69, 72, 76, 81, 84};
      for (int i = 0; i < 5; i++) noteOn(I_KOTO, hz(N[i]), 0.6f, -0.4f + i * 0.2f, i * 0.045f);
      drum(K_TAIKO, 0.7f, 0, 0.22f, false);
      break;
    }
    case SFX_PAUSE: noteOn(I_MARIMBA, hz(76), 0.4f, 0); noteOn(I_MARIMBA, hz(69), 0.4f, 0, 0.08f); break;
    default: break;
  }
  SDL_UnlockMutex(lock);
}

}  // namespace nova::audio
