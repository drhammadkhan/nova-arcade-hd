// =====================================================================
//  TURBO HORIZON HD  -  a pseudo-3D road racer (Nova Arcade HD)
//  Race the clock through three stages: Sunset Coast, Canyon Run and
//  Neon Night. Reach each checkpoint before the timer runs out to earn
//  more time. Weave through traffic, stay on the tarmac (the verge slows
//  you down and roadside scenery stops you dead) and keep your foot down.
//
//  The original's track, physics, traffic and timing, kept in its units.
//  The picture is new: the road is projected natively at 1920 x 1080,
//  segment by segment, and drawn as one batch of triangles (grass bands,
//  rumble strips, tarmac with a sheen, lane and edge lines) with smooth
//  per-vertex fog. Vector cars and scenery (tools/make_art.py) are scaled
//  by depth and clipped at the crest of the hill in front of them.
//  Parallax skylines per stage, a striped sunset sun, speed lines, tyre
//  smoke, sparks and an engine drone (audio::engine).
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include "art.h"
#include <stdio.h>
#include <algorithm>
#include <vector>

using namespace nova;
using namespace nova::audio;

namespace th {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  112, 4, "C G Am F",
  {I_SAWLEAD, 0.55f, 0.1f,
   "C5 - - E5 G5 - - - C6 - B5 - G5 - - - | G5 - - A5 B5 - - - D6 - C6 - B5 - - - |"
   "A5 - - G5 E5 - - - C5 - D5 - E5 - - - | F5 - E5 - D5 - C5 - D5 - - - . . . ."},
  {}, I_PLUCKBASS, "1.3.5.3.8.5.3.5.", 0.22f, I_SUBBASS, "R.......R...R...", 0.5f, I_PAD, 0.4f,
  "K...h...K...h.h.", 0.6f, 0, true};
static const Song SONG_RACE = {
  150, 8, "C G Am F C G Dm E",
  {I_SQUARE, 0.55f, 0.05f,
   "C6 . C6 . B5 - C6 - G5 - - - E5 - G5 - | B5 . B5 . A5 - B5 - D6 - - - B5 - - - |"
   "A5 . A5 . G5 - A5 - E5 - - - C5 - E5 - | F5 - G5 - A5 - C6 - D6 - C6 - A5 - G5 - |"
   "C6 . C6 . B5 - C6 - G5 - - - E5 - G5 - | B5 . B5 . A5 - B5 - D6 - - - B5 - - - |"
   "D5 - F5 - A5 - - - G5 - F5 - D5 - F5 - | E5 - Ab5 - B5 - - - E6 - - - D6 - B5 -"},
  {}, I_NONE, nullptr, 0, I_PLUCKBASS, "R.ROR.ROR.ROR.RO", 0.5f, I_STRINGS, 0.25f,
  "K.hhS.hhK.hhS.hh|K.hhS.hhK.hhS.hh|K.hhS.hhK.hhS.hh|K.hhS.hKK.hhS.SS", 0.8f, 0, true};
static const Song SONG_OVER = {
  90, 2, "F C", {I_FLUTE, 0.8f, 0, "G5 - - F5 E5 - - D5 C5 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ road geometry (the original's units)
static const float SEGL = 200, ROADW = 1400, CAMH = 1000, CAMD = 0.84f, PLAYERZ = CAMH * 0.84f;
static const float U = 6.25f;                 // world units per original sprite pixel
static const float MAXSPD = SEGL;             // world units per frame at top speed
static const int DRAWN = 120, STRIPE = 3, MAXSEG = 2200;
// projection onto the HD frame: the original's 160 x 120 half-screen scales, times 4.5
static const float K = 4.5f, CX = W / 2, CY = H / 2, XS = 160 * K, YS = 120 * K;

enum Scenery { SC_PALM0, SC_PALM1, SC_ROCK, SC_MESA, SC_CACTUS, SC_BILL0, SC_BILL1, SC_BILL2, SC_LAMP, SC_TOWER0, SC_TOWER1,
               SC_CHEVRON, SC_BUSH, SC_BANNERC, SC_BANNERS, SC_POST, SC_COUNT };
// the original sprites' widths in pixels (collisions use them)
static const int SPR_W[SC_COUNT] = {72, 72, 72, 120, 40, 96, 96, 96, 40, 64, 64, 32, 48, 176, 176, 10};

struct Seg { float curve, y; uint8_t spr; int8_t off; };   // off: sprite offset x 0.1 road half-widths
static Seg segs[MAXSEG];
static int nseg = 0;
static float trackLen = 0;

static inline float easeIn(float a, float b, float t) { return a + (b - a) * t * t; }
static inline float easeInOutC(float a, float b, float t) { return a + (b - a) * ((-cosf(t * 3.14159f) / 2) + 0.5f); }
static inline float lastY() { return nseg ? segs[nseg - 1].y : 0; }
static void addSeg(float curve, float y) { if (nseg < MAXSEG) segs[nseg++] = {curve, y, 0, 0}; }
static void addRoad(int enter, int hold, int leave, float curve, float hill) {
  float y0 = lastY(), y1 = y0 + hill * SEGL;
  int total = enter + hold + leave;
  for (int i = 0; i < enter; i++) addSeg(easeIn(0, curve, (float)i / enter), easeInOutC(y0, y1, (float)i / total));
  for (int i = 0; i < hold; i++) addSeg(curve, easeInOutC(y0, y1, (float)(enter + i) / total));
  for (int i = 0; i < leave; i++) addSeg(easeInOutC(curve, 0, (float)i / leave), easeInOutC(y0, y1, (float)(enter + hold + i) / total));
}

// Each stage is a list of road pieces: enter, hold, leave, curve, hill.
struct Piece { int16_t e, h, l; int8_t curve; int8_t hill; };
static const Piece STAGE0[] = {   // Sunset Coast: sweeping bends and gentle rises
  {0, 80, 0, 0, 0}, {30, 60, 30, 2, 10}, {30, 80, 30, -3, -10}, {25, 60, 25, 0, 20}, {30, 90, 30, 4, 0},
  {25, 50, 25, -2, -20}, {25, 60, 25, 0, 15}, {30, 70, 30, -4, 0}, {20, 40, 20, 3, -15}, {25, 70, 25, 0, 25},
  {30, 60, 30, 5, -10}, {25, 60, 25, -3, 0}, {20, 60, 20, 0, -15},
};
static const Piece STAGE1[] = {   // Canyon Run: big crests and tighter turns
  {0, 80, 0, 0, 0}, {25, 50, 25, 0, 40}, {25, 60, 25, 5, -20}, {20, 50, 20, -5, -20}, {30, 40, 30, 0, 35},
  {25, 70, 25, 3, -35}, {20, 40, 20, -6, 0}, {20, 50, 20, 6, 20}, {25, 60, 25, 0, -30}, {25, 50, 25, -4, 25},
  {25, 70, 25, 2, -10}, {20, 40, 20, 5, 0},
};
static const Piece STAGE2[] = {   // Neon Night: city S-bends
  {0, 80, 0, 0, 0}, {20, 40, 20, 4, 0}, {20, 40, 20, -4, 10}, {20, 40, 20, 5, -10}, {25, 60, 25, 0, 20},
  {20, 30, 20, -6, 0}, {20, 30, 20, 6, -20}, {25, 80, 25, -2, 0}, {20, 40, 20, 5, 15}, {20, 40, 20, -5, -15},
  {25, 60, 25, 0, 25}, {20, 50, 20, 4, -10}, {20, 40, 20, -3, 0},
};
struct StageDef { const char* name; const Piece* pieces; int npieces; float time; };
static const StageDef STAGES[3] = {
  {"SUNSET COAST", STAGE0, sizeof(STAGE0) / sizeof(Piece), 44},
  {"CANYON RUN", STAGE1, sizeof(STAGE1) / sizeof(Piece), 32},
  {"NEON NIGHT", STAGE2, sizeof(STAGE2) / sizeof(Piece), 30},
};

// Stage colours: sky top, sky bottom, fog, grass L/D, rumble L/D, road L/D, lane, far tint, near tint
struct Theme { Color c[12]; };
static const Theme THEMES[3] = {
  {{{40, 20, 90}, {255, 120, 110}, {255, 150, 130}, {232, 196, 120}, {214, 174, 100}, {255, 255, 255}, {220, 40, 60},
    {120, 112, 128}, {112, 104, 120}, {255, 255, 255}, {120, 60, 130}, {70, 120, 170}}},
  {{{40, 110, 210}, {190, 230, 250}, {220, 210, 190}, {220, 130, 70}, {200, 112, 58}, {255, 255, 255}, {60, 60, 70},
    {130, 120, 116}, {122, 112, 108}, {255, 230, 120}, {200, 96, 60}, {160, 70, 50}}},
  {{{6, 4, 24}, {60, 20, 90}, {50, 20, 80}, {24, 20, 50}, {18, 14, 40}, {255, 60, 190}, {60, 230, 255},
    {44, 42, 62}, {38, 36, 56}, {255, 220, 120}, {30, 22, 60}, {16, 12, 36}}},
};
enum { TC_SKY0, TC_SKY1, TC_FOG, TC_GRASS0, TC_GRASS1, TC_RUMB0, TC_RUMB1, TC_ROAD0, TC_ROAD1, TC_LANE, TC_FAR, TC_NEAR };

// ------------------------------------------------------------ cars and player
struct Car { float z, off, target, speed; uint8_t type; int8_t ahead; };
static Car cars[24];
static int ncars = 0;
static float position = 0, speedv = 0, playerX = 0, playerY = 0, bgFar = 0, bgNear = 0;
static float timeLeft = 0;
static int stage = 0, lap = 0, crashT = 0, offroadT = 0, steerDir = 0, checkT = 0, passes = 0, crashes = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 30000;
static bool newHi = false, braking = false, skidding = false;
static float scoreAcc = 0;

enum State { ST_TITLE, ST_COUNT, ST_RACE, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

// cosmetics (screen space, outside the gameplay state)
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; uint8_t kind; };   // 0 puff, 1 spark, 2 speed line
static Part parts[220];
struct Popup { bool on; int t; char txt[16]; };
static Popup popups[4];
static float shake = 0, shakeX = 0, shakeY = 0, flash = 0;

static void popup(const char* s) {
  for (auto& p : popups) if (!p.on) { p.on = true; p.t = 90; snprintf(p.txt, sizeof(p.txt), "%s", s); return; }
}
static Part* spawn() { for (auto& p : parts) if (!p.on) { p = Part(); p.on = true; return &p; } return nullptr; }
static void puff(float x, float y, Color c, float size, float vx, float vy) {
  Part* p = spawn();
  if (!p) return;
  p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->life = p->max = frange(30, 46); p->size = size; p->col = c; p->kind = 0;
}
static void sparks(float x, float y, int n, Color c) {
  for (int i = 0; i < n; i++) {
    Part* p = spawn();
    if (!p) return;
    float a = frange(3.4f, 6.0f), s = frange(6, 20);
    p->x = x; p->y = y; p->vx = cosf(a) * s; p->vy = sinf(a) * s; p->life = p->max = frange(14, 30); p->size = 1; p->col = c; p->kind = 1;
  }
}
static void addScore(uint32_t v) { score += v; if (score > hiscore) { hiscore = score; newHi = true; } }

static inline int segIndex(float z) { int i = (int)floorf(z / SEGL) % nseg; return i < 0 ? i + nseg : i; }
static inline float wrapDz(float dz) { while (dz > trackLen / 2) dz -= trackLen; while (dz < -trackLen / 2) dz += trackLen; return dz; }

static void buildStage(int st) {
  st %= 3;
  const StageDef& sd = STAGES[st];
  nseg = 0;
  for (int i = 0; i < sd.npieces; i++) { const Piece& p = sd.pieces[i]; addRoad(p.e, p.h, p.l, p.curve, p.hill); }
  addRoad(40, 40, 40, 0, -lastY() / SEGL);    // level out so stages join up
  addRoad(0, 140, 0, 0, 0);                   // run-out to the checkpoint
  trackLen = nseg * SEGL;
  for (int i = 0; i < nseg; i++) {   // scenery
    Seg& s = segs[i];
    int side = (i / 2) & 1 ? 1 : -1;
    uint32_t h = (uint32_t)i * 2654435761u >> 16;
    if (fabsf(s.curve) > 2.5f && i % 6 == 0) { s.spr = SC_CHEVRON + 1; s.off = (int8_t)(s.curve > 0 ? -13 : 13); continue; }
    if (st == 0) {
      if (i % 9 == 0) { s.spr = (h & 1 ? SC_PALM0 : SC_PALM1) + 1; s.off = (int8_t)(side * (14 + h % 8)); }
      else if (i % 97 == 40) { s.spr = SC_BILL0 + 1 + (i / 97) % 3; s.off = (int8_t)(side * 18); }
      else if (i % 9 == 5 && h % 3 == 0) { s.spr = SC_BUSH + 1; s.off = (int8_t)(-side * (13 + h % 10)); }
    } else if (st == 1) {
      if (i % 11 == 0) { s.spr = SC_CACTUS + 1; s.off = (int8_t)(side * (13 + h % 12)); }
      else if (i % 23 == 7) { s.spr = SC_ROCK + 1; s.off = (int8_t)(side * (15 + h % 10)); }
      else if (i % 61 == 30) { s.spr = SC_MESA + 1; s.off = (int8_t)(side * (30 + h % 20)); }
      else if (i % 131 == 70) { s.spr = SC_BILL0 + 1 + (i / 131) % 3; s.off = (int8_t)(side * 18); }
    } else {
      if (i % 8 == 0) { s.spr = SC_LAMP + 1; s.off = (int8_t)(side * 12); }
      else if (i % 14 == 4) { s.spr = (h & 1 ? SC_TOWER0 : SC_TOWER1) + 1; s.off = (int8_t)(side * (24 + h % 16)); }
      else if (i % 89 == 45) { s.spr = SC_BILL0 + 1 + (i / 89) % 3; s.off = (int8_t)(side * 18); }
    }
  }
  segs[nseg - 20].spr = SC_BANNERC + 1;
}

static void spawnCars() {
  int want = difficulty() == EASY ? 12 : difficulty() == NORMAL ? 16 : 22;
  ncars = std::min(24, want + lap * 2);
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    c.z = fmodf(position + PLAYERZ + 3000 + (trackLen - 6000) * i / ncars + frange(0, 800), trackLen);
    static const float LANES[3] = {-0.66f, 0, 0.66f};
    c.off = c.target = LANES[rnd() % 3];
    c.speed = MAXSPD * frange(0.28f, 0.55f);
    c.type = rnd() % 4;
    c.ahead = 1;
  }
}

static void startRace() {
  stage = 0; lap = 0; position = 0; speedv = 0; playerX = 0; score = 0; newHi = false; scoreAcc = 0; passes = 0; crashes = 0;
  crashT = 0; checkT = 0;
  buildStage(0);
  segs[10].spr = SC_BANNERS + 1;
  spawnCars();
  timeLeft = STAGES[0].time / nova::speed();
  hiscore = loadHi(HI_DEFAULT);
  for (auto& p : popups) p.on = false;
  state = ST_COUNT; stateT = 0;
  music(nullptr);
}

static void reachCheckpoint() {
  float oldLen = trackLen;
  stage++;
  if (stage % 3 == 0) lap++;
  buildStage(stage);
  position -= oldLen;
  spawnCars();
  float bonus = STAGES[stage % 3].time / nova::speed() * (lap ? 0.9f : 1.0f);
  timeLeft += bonus;
  checkT = 150;
  addScore(5000 + 1000 * stage);
  char b[16]; snprintf(b, sizeof(b), "+%d SEC", (int)bonus);
  popup(b);
  sfx(SFX_POWERUP);
  sfx(SFX_CHECKPOINT);
  flash = 0.35f;
}

// ------------------------------------------------------------ driving
static void crash() {
  crashT = 50;
  crashes++;
  speedv *= 0.15f;
  sfx(SFX_EXPLODE, 0, 0.8f); sfx(SFX_ROLL);
  rumble(1.0f, 300);
  shake = 40; flash = 0.25f;
  sparks(W / 2, H - 120, 40, Color(255, 200, 90));
  for (int i = 0; i < 10; i++) puff(W / 2 + frange(-160, 160), H - 90, Color(200, 190, 190), frange(1.2f, 2.2f), frange(-3, 3), frange(-4, -1));
}

// The original's autopilot (title screen attract mode and the test bot): pick the safest lane,
// follow the bends, lift off behind slower cars.
static void autopilot(float* steer, bool* gas, bool* brake) {
  static const float LANES[3] = {-0.66f, 0, 0.66f};
  float pz = position + PLAYERZ, best = 1e9, tgt = 0;
  for (float l : LANES) {
    float danger = fabsf(l - playerX) * 0.6f;
    for (int i = 0; i < ncars; i++) {
      float dz = wrapDz(cars[i].z - pz);
      if (dz < -100 || dz > 4500) continue;
      if (fabsf(cars[i].off - l) < 0.45f) danger += 4.0f * (1 - dz / 4500);
    }
    if (danger < best) { best = danger; tgt = l; }
  }
  const Seg& s = segs[segIndex(pz)];
  float push = (speedv / MAXSPD) * s.curve * 0.12f;
  *steer = clampv((tgt - playerX) * 4.0f + push, -1.0f, 1.0f);   // feed-forward for the bend
  *gas = true; *brake = false;
  for (int i = 0; i < ncars; i++) {
    float dz = wrapDz(cars[i].z - pz);
    if (dz > 0 && dz < 900 && fabsf(cars[i].off - playerX) < 0.4f && speedv > cars[i].speed) { *gas = false; *brake = dz < 500; }
  }
}

// rear wheels on screen, for smoke and dust
static const float WHEEL_DX = 140, WHEEL_Y = H - 40;

static void drive(float steer, bool gas, bool brake) {
  float sp = speedv / MAXSPD;
  int pseg = segIndex(position + PLAYERZ);
  const Seg& s = segs[pseg];
  float dx = 2.0f / 60 * sp;
  if (crashT) { crashT--; steer = 0; gas = false; playerX += (playerX > 0 ? -1 : 1) * 0.02f * (fabsf(playerX) > 0.8f); }
  playerX += dx * steer;
  playerX -= dx * sp * s.curve * 0.12f;          // the bend pushes you outwards
  steerDir = steer > 0.3f ? 1 : steer < -0.3f ? -1 : 0;
  braking = brake;
  if (gas) speedv += MAXSPD / 330 * (1.15f - sp * 0.5f);
  else if (brake) speedv -= MAXSPD / 70;
  else speedv -= MAXSPD / 500;
  bool off = fabsf(playerX) > 1.0f;
  if (off) {
    offroadT++;
    if (speedv > MAXSPD / 3) speedv -= MAXSPD / 90;
    if ((frameNo & 1) == 0 && speedv > 10) {
      const Color& g = THEMES[stage % 3].c[TC_GRASS1];
      puff(W / 2 + frange(-WHEEL_DX, WHEEL_DX), WHEEL_Y, mix(g, Color(255, 240, 210), 0.3f), frange(0.8f, 1.4f), frange(-4, 4), frange(-5, -2));
    }
  } else offroadT = 0;
  playerX = clampv(playerX, -2.6f, 2.6f);
  speedv = clampv(speedv, 0.0f, MAXSPD);
  // tyre squeal on hard cornering
  skidding = fabsf(s.curve) > 3.5f && sp > 0.8f && fabsf(steer) > 0.6f;
  if (skidding) {
    if ((frameNo % 20) == 0) sfx(SFX_ROLL, steer * 0.4f);
    if ((frameNo % 3) == 0)
      for (int k = -1; k <= 1; k += 2) puff(W / 2 + k * WHEEL_DX, WHEEL_Y, Color(230, 230, 240), frange(0.7f, 1.1f), frange(-2, 2) - steer * 3, frange(-3, -1));
  }
  // collisions with roadside scenery
  if (off && !crashT) {
    for (int n = 0; n < 2; n++) {
      const Seg& q = segs[(pseg + n) % nseg];
      if (!q.spr || q.spr - 1 == SC_BUSH || q.spr - 1 >= SC_BANNERC) continue;
      float so = q.off / 10.0f, half = SPR_W[q.spr - 1] * U / ROADW / 2;
      if (q.spr - 1 == SC_MESA) half *= 0.8f;
      if (fabsf(so - playerX) < half + 0.25f) { crash(); break; }
    }
  }
  // cars
  float pz = position + PLAYERZ;
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    float dz = wrapDz(c.z - pz);
    if (!crashT && dz > 0 && dz < 140 && speedv > c.speed && fabsf(c.off - playerX) < 0.62f) {
      speedv = c.speed * 0.6f;
      position = c.z - PLAYERZ - 160;
      sfx(SFX_BUMP); sfx(SFX_LAND);
      rumble(0.6f, 150);
      offroadT = 8;
      shake = 18;
      sparks(W / 2 + (c.off - playerX) * 300, H - 200, 16, Color(255, 230, 140));
    }
    int8_t ahead = dz > 0 ? 1 : -1;
    if (c.ahead == 1 && ahead == -1 && fabsf(dz) < 2000 && state == ST_RACE) { passes++; addScore(200); }
    c.ahead = ahead;
  }
  // move on
  position += speedv;
  float pz2 = position + PLAYERZ;
  if (state == ST_RACE && pz2 >= trackLen) reachCheckpoint();
  else if (pz2 >= trackLen) position -= trackLen;
  int ps = segIndex(position + PLAYERZ);
  float pct = fmodf(position + PLAYERZ, SEGL) / SEGL; if (pct < 0) pct += 1;
  playerY = segs[ps].y + (segs[(ps + 1) % nseg].y - segs[ps].y) * pct;
  bgFar += segs[ps].curve * sp * 0.25f;
  bgNear += segs[ps].curve * sp * 0.6f;
  if (state == ST_RACE) {
    scoreAcc += speedv * 0.02f;
    if (scoreAcc >= 1) { addScore((uint32_t)scoreAcc); scoreAcc -= (int)scoreAcc; }
  }
}

static void updateCars() {
  static const float LANES[3] = {-0.66f, 0, 0.66f};
  for (int i = 0; i < ncars; i++) {
    Car& c = cars[i];
    c.z += c.speed;
    if (c.z >= trackLen) c.z -= trackLen;
    if (rnd() % 600 == 0) c.target = LANES[rnd() % 3];
    c.off += clampv(c.target - c.off, -0.006f, 0.006f);
  }
}

static void updateFx() {
  float sp = speedv / MAXSPD;
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy;
    if (p.kind == 0) { p.vx *= 0.96f; p.vy *= 0.97f; p.size += 0.035f; }
    else if (p.kind == 1) { p.vy += 0.8f; p.vx *= 0.97f; }
    else { p.vx *= 1.08f; p.vy *= 1.08f; }
    if (--p.life <= 0 || p.x < -300 || p.x > W + 300 || p.y > H + 300 || p.y < -300) p.on = false;
  }
  // speed lines streaming out of the vanishing point
  if (state == ST_RACE && sp > 0.72f && !crashT) {
    int n = sp > 0.92f ? 2 : 1;
    for (int i = 0; i < n; i++) {
      Part* p = spawn();
      if (!p) break;
      float a = frange(-0.6f, 0.45f) + (rnd() & 1 ? 3.14159f : 0);   // out to the sides, off the road ahead
      float r = frange(260, 520);
      p->x = W / 2 + cosf(a) * r * 1.6f; p->y = H * 0.4f + sinf(a) * r;
      p->vx = cosf(a) * 14 * sp * 1.6f; p->vy = sinf(a) * 14 * sp;
      p->life = p->max = 34; p->kind = 2; p->size = 1; p->col = WHITE;
    }
  }
  for (auto& p : popups) if (p.on && --p.t <= 0) p.on = false;
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake) * 0.6f; shake *= 0.88f; }
  else { shake = 0; shakeX = shakeY = 0; }
  flash *= 0.9f;
  if (checkT) checkT--;
}

static void step(const Pad& in) {
  stateT++;
  updateFx();
  float steer = 0; bool gas = false, brake = false;
  switch (state) {
    case ST_TITLE:
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { sfx(SFX_START); startRace(); break; }
      autopilot(&steer, &gas, &brake);
      if (speedv > MAXSPD * 0.7f) gas = false;
      drive(steer, gas, brake);
      updateCars();
      break;
    case ST_COUNT:
      if (stateT % 60 == 0 && stateT < 240) sfx(stateT == 180 ? SFX_SELECT : SFX_KNOCK);
      engine(55 + (in.down(BTN_A) ? 50 : 0), 0.45f);
      if (stateT >= 180) { state = ST_RACE; stateT = 0; music(&SONG_RACE); }
      break;
    case ST_RACE: {
      if (in.hit(BTN_START)) { nova::pause(); break; }
      steer = in.ax;
      gas = in.down(BTN_A | BTN_R) || (in.ay < -0.5f && !in.down(BTN_SELECT));
      brake = in.down(BTN_B | BTN_L) || in.ay > 0.5f;
      if (gas && brake) gas = false;
      drive(steer, gas, brake);
      updateCars();
      int prev = (int)ceilf(timeLeft);
      timeLeft -= 1.0f / 60;
      if (timeLeft < 10 && (int)ceilf(timeLeft) != prev && timeLeft > 0) sfx(SFX_KNOCK, 0, 1.5f);
      if (timeLeft <= 0) {
        timeLeft = 0; state = ST_OVER; stateT = 0;
        music(&SONG_OVER);
        if (newHi) saveHi(hiscore);
      }
      float sp = speedv / MAXSPD;
      int gear = std::min(4, (int)(sp * 5));
      float rev = sp * 5 - gear;
      engine(48 + gear * 16 + rev * 75 + (offroadT ? (frameNo & 2) * 6 : 0), 0.5f);
      break;
    }
    case ST_OVER:
      drive(0, false, true);
      updateCars();
      engine(45 + speedv / MAXSPD * 120, speedv > 2 ? 0.4f : 0.0f);
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) {
        state = ST_TITLE; stateT = 0; stage = 0; lap = 0; buildStage(0); spawnCars(); music(&SONG_TITLE);
      }
      break;
  }
}

// ------------------------------------------------------------ projection and the road (once per frame)
static float pX[DRAWN + 1], pY[DRAWN + 1], pS[DRAWN + 1], pClip[DRAWN + 1];
static bool pOk[DRAWN + 1];
static float horizon = 540;
static std::vector<Vtx> road;
struct Cmd { const Img* img; float z, x, yb, sx, sy, clip, alpha; bool flip; uint8_t kind; };   // kind: 0 scenery, 1 car, 2 lamp
static Cmd cmds[160];
static int ncmds = 0;

static void addCmd(const Img* img, float z, float x, float yb, float sx, float sy, float clip, bool flip, float alpha, uint8_t kind) {
  if (ncmds >= 160 || sx * img->sw() < 1.5f) return;
  float w = img->sw() * sx;
  if (x + w < 0 || x - w > W || yb - img->sh() * sy > clip) return;
  cmds[ncmds++] = {img, z, x, yb, sx, sy, clip, alpha, flip, kind};
}

static inline float fogAt(float n) { float t = n / DRAWN; return 1 - expf(-t * t * 4); }

static inline void quad(float y0, float xa0, float xb0, Color c0, float y1, float xa1, float xb1, Color c1) {
  // y0 far edge (xa0..xb0), y1 near edge (xa1..xb1)
  road.push_back({xa0, y0, c0}); road.push_back({xb0, y0, c0}); road.push_back({xb1, y1, c1});
  road.push_back({xa0, y0, c0}); road.push_back({xb1, y1, c1}); road.push_back({xa1, y1, c1});
}

// One segment's band: the far edge (xf, yf, wf) and near edge (xn, yn, wn), already clipped.
static void segmentQuads(int st, bool alt, float xf, float yf, float wf, float ff, float xn, float yn, float wn, float fn) {
  const Theme& th = THEMES[st];
  Color fog = th.c[TC_FOG];
  auto fc = [&](int k, float f) { return mix(th.c[k], fog, f); };
  Color g0 = fc(alt ? TC_GRASS1 : TC_GRASS0, ff), g1 = fc(alt ? TC_GRASS1 : TC_GRASS0, fn);
  quad(yf, 0, W, g0, yn, 0, W, g1);
  float rf = wf / 6, rn = wn / 6;
  Color r0 = fc(alt ? TC_RUMB1 : TC_RUMB0, ff), r1 = fc(alt ? TC_RUMB1 : TC_RUMB0, fn);
  quad(yf, xf - wf - rf, xf + wf + rf, r0, yn, xn - wn - rn, xn + wn + rn, r1);
  // tarmac: a little lighter down the middle
  Color e0 = fc(alt ? TC_ROAD1 : TC_ROAD0, ff), e1 = fc(alt ? TC_ROAD1 : TC_ROAD0, fn);
  Color m0 = mix(e0, Color(255, 255, 255), 0.07f * (1 - ff)), m1 = mix(e1, Color(255, 255, 255), 0.07f * (1 - fn));
  e0 = mix(e0, Color(0, 0, 0), 0.08f * (1 - ff)); e1 = mix(e1, Color(0, 0, 0), 0.08f * (1 - fn));
  road.push_back({xf - wf, yf, e0}); road.push_back({xf, yf, m0}); road.push_back({xn, yn, m1});
  road.push_back({xf - wf, yf, e0}); road.push_back({xn, yn, m1}); road.push_back({xn - wn, yn, e1});
  road.push_back({xf, yf, m0}); road.push_back({xf + wf, yf, e0}); road.push_back({xn + wn, yn, e1});
  road.push_back({xf, yf, m0}); road.push_back({xn + wn, yn, e1}); road.push_back({xn, yn, m1});
  Color l0 = fc(TC_LANE, ff), l1 = fc(TC_LANE, fn);
  if (!alt) {   // dashed lane lines
    float lf = fmaxf(0.8f, wf / 28) / 2, ln = fmaxf(0.8f, wn / 28) / 2;
    for (int l = 1; l < 3; l++) {
      float cf = xf - wf + 2 * wf * l / 3, cn = xn - wn + 2 * wn * l / 3;
      quad(yf, cf - lf, cf + lf, l0, yn, cn - ln, cn + ln, l1);
    }
  }
  if (st != 1) {   // solid edge lines
    float lf = fmaxf(0.8f, wf / 40), ln = fmaxf(0.8f, wn / 40);
    float of = wf * 0.03f, on = wn * 0.03f;
    quad(yf, xf - wf + of, xf - wf + of + lf, l0, yn, xn - wn + on, xn - wn + on + ln, l1);
    quad(yf, xf + wf - of - lf, xf + wf - of, l0, yn, xn + wn - on - ln, xn + wn - on, l1);
  }
}

static void project() {
  road.clear();
  ncmds = 0;
  int st = stage % 3;
  int base = segIndex(position);
  float baseZ = floorf(position / SEGL) * SEGL;
  float basePct = (position - baseZ) / SEGL;
  float camX = playerX * ROADW, camY = playerY + CAMH;
  float x = 0, dx = -segs[base].curve * basePct;
  float maxy = H + 40;
  for (int n = 0; n < DRAWN; n++) {
    int i = (base + n) % nseg, j = (i + 1) % nseg;
    float z1 = baseZ + n * SEGL - position, z2 = z1 + SEGL;
    pOk[n] = false;
    pClip[n] = maxy;
    if (z1 <= CAMD) { x += dx; dx += segs[i].curve; continue; }
    float s1 = CAMD / z1, s2 = CAMD / z2;
    float X1 = CX + s1 * (x - camX) * XS, X2 = CX + s2 * (x + dx - camX) * XS;
    float Y1 = CY - s1 * (segs[i].y - camY) * YS, Y2 = CY - s2 * (segs[j].y - camY) * YS;
    float W1 = s1 * ROADW * XS, W2 = s2 * ROADW * XS;
    pX[n] = X1; pY[n] = Y1; pS[n] = s1; pOk[n] = true;
    pX[n + 1] = X2; pY[n + 1] = Y2; pS[n + 1] = s2;
    x += dx; dx += segs[i].curve;
    if (Y2 >= Y1 || Y2 >= maxy) continue;
    // clip the near edge to the crest in front (or the bottom of the screen)
    float f1 = fogAt((float)n), f2 = fogAt((float)n + 1);
    float yn = Y1, xn = X1, wn = W1, fn = f1;
    if (Y1 > maxy) {
      float t = (Y1 - maxy) / (Y1 - Y2);
      yn = maxy; xn = X1 + (X2 - X1) * t; wn = W1 + (W2 - W1) * t; fn = f1 + (f2 - f1) * t;
    }
    bool alt = ((base + n) / STRIPE) & 1;
    segmentQuads(st, alt, X2, Y2, W2, f2, xn, yn, wn, fn);
    maxy = Y2;
  }
  horizon = clampv(maxy, 180.0f, 900.0f);
  // scenery, far to near
  for (int n = DRAWN - 1; n >= 1; n--) {
    if (!pOk[n]) continue;
    const Seg& sg = segs[(base + n) % nseg];
    if (!sg.spr) continue;
    int t = sg.spr - 1;
    float s = pS[n];
    float alpha = clampv((DRAWN - 4 - n) / 16.0f, 0.0f, 1.0f);
    if (t == SC_BANNERC || t == SC_BANNERS) {
      float postH = 900 * s * XS, postW = 70 * s * XS, span = 1.15f * ROADW * s * XS, cx = pX[n];
      const Img& post = thart::IMG_POST;
      float psx = postW / (10 * K), psy = postH / (90 * K);
      addCmd(&post, n * SEGL, cx - span, pY[n], psx, psy, pClip[n], false, alpha, 0);
      addCmd(&post, n * SEGL, cx + span, pY[n], psx, psy, pClip[n], false, alpha, 0);
      float bw = 2 * span + postW, bsx = bw / (176 * K), bh = 26 * K * bsx;
      addCmd(thart::SCENERY[t], n * SEGL - 1, cx, pY[n] - postH + bh * 0.9f, bsx, bsx, pClip[n], false, alpha, 0);
      continue;
    }
    float sx = pX[n] + s * (sg.off / 10.0f) * ROADW * XS;
    bool flip = (t == SC_LAMP && sg.off > 0) || (t == SC_CHEVRON && sg.off < 0);
    float k = U * s * 160;
    addCmd(thart::SCENERY[t], n * SEGL, sx, pY[n], k, k, pClip[n], flip, alpha, t == SC_LAMP ? 2 : 0);
  }
  // traffic
  for (int c = 0; c < ncars; c++) {
    float dz = cars[c].z - baseZ;
    if (dz < 0) dz += trackLen;
    int n = (int)(dz / SEGL);
    if (n < 1 || n >= DRAWN - 1 || !pOk[n]) continue;
    float pct = fmodf(dz, SEGL) / SEGL;
    float s = pS[n] + (pS[n + 1] - pS[n]) * pct;
    float cx = pX[n] + (pX[n + 1] - pX[n]) * pct + s * cars[c].off * ROADW * XS;
    float cy = pY[n] + (pY[n + 1] - pY[n]) * pct;
    float k = U * s * 160;
    addCmd(thart::RIVAL[cars[c].type], dz, cx, cy, k, k, pClip[n], false, clampv((DRAWN - 4 - n) / 16.0f, 0.0f, 1.0f), 1);
  }
  std::stable_sort(cmds, cmds + ncmds, [](const Cmd& a, const Cmd& b) { return a.z > b.z; });
}

// ------------------------------------------------------------ drawing
// A sprite with its pivot (bottom centre) at (x, yb), scaled, with everything below `clip` hidden
// (the crest of a hill in front of it).
static void drawClipped(const Img& img, float x, float yb, float sx, float sy, float clip, bool flip, Color tint) {
  float fullH = img.py * img.scale * sy;
  float top = yb - fullH;
  float bottom = fminf(yb, clip);
  if (bottom <= top + 0.5f) return;
  Img c = img;
  if (bottom < yb) c.h = img.py * (bottom - top) / fullH;
  c.py = 0;
  Fx f; f.sx = sx; f.sy = sy; f.flip = flip; f.tint = tint;
  draw(c, x, top, f);
}

static void drawSky(int st) {
  const Theme& th = THEMES[st];
  Color mid = mix(th.c[TC_SKY0], th.c[TC_SKY1], 0.3f);
  rectGrad(0, 0, W, horizon * 0.55f, th.c[TC_SKY0], mid);
  rectGrad(0, horizon * 0.55f - 1, W, horizon * 0.45f + 120, mid, th.c[TC_SKY1]);
  float ox = -fmodf(bgFar * 0.5f * K, 640 * K);
  float sunX = W / 2 + (230 - 160) * K + ox;
  if (sunX < -60 * K) sunX += 640 * K;
  if (st == 0) {   // a big striped sunset sun
    float r = 36 * K, sy = horizon - 40 * K;
    glow(sunX, sy, r * 3.2f, Color(255, 140, 120, 120));
    const int N = 48;
    static Vtx v[N * 6];
    int k = 0;
    for (int i = 0; i < N; i++) {
      float y0 = sy - r + 2 * r * i / N, y1 = sy - r + 2 * r * (i + 1) / N;
      float dy = (y0 + y1) / 2 - sy;
      if (dy > 0) {   // stripes that drift down through the lower half
        float ph = fmodf((dy + frameNo * 0.4f) / (r * 0.16f), 1.0f);
        if (ph < 0.15f + dy / r * 0.45f) continue;
      }
      auto hw = [&](float y) { float d = y - sy; return sqrtf(fmaxf(0, r * r - d * d)); };
      Color c0 = mix(Color(255, 245, 140), Color(255, 70, 150), (y0 - (sy - r)) / (2 * r));
      Color c1 = mix(Color(255, 245, 140), Color(255, 70, 150), (y1 - (sy - r)) / (2 * r));
      float a0 = hw(y0), a1 = hw(y1);
      v[k++] = {sunX - a0, y0, c0}; v[k++] = {sunX + a0, y0, c0}; v[k++] = {sunX + a1, y1, c1};
      v[k++] = {sunX - a0, y0, c0}; v[k++] = {sunX + a1, y1, c1}; v[k++] = {sunX - a1, y1, c1};
    }
    tris(v, k);
  } else if (st == 1) {   // a white-hot desert sun
    float sy = horizon - 100 * K;
    glow(sunX, sy, 420, Color(255, 250, 220, 140));
    glow(sunX, sy, 160, Color(255, 255, 240, 200));
    disc(sunX, sy, 12 * K, Color(255, 252, 235));
  } else {   // stars and a crescent moon
    for (int i = 0; i < 90; i++) {
      float x = fmodf(i * 397.0f + 4000 - bgFar * 0.3f * K, W + 40) - 20;
      float y = fmodf(i * 211.0f, horizon - 40);
      float tw = 0.5f + 0.5f * sinf(frameNo * 0.05f + i);
      disc(x, y, 1.5f + (i % 3), Color(210, 210, 255, (uint8_t)(120 + 120 * tw)));
    }
    float sy = horizon - 80 * K;
    glow(sunX, sy, 260, Color(160, 150, 255, 90));
    disc(sunX, sy, 10 * K, Color(235, 235, 255));
    disc(sunX + 16, sy - 10, 9 * K, mix(th.c[TC_SKY0], th.c[TC_SKY1], (sy - 40) / horizon));
  }
}

static void drawLayers(int st) {
  static const Img* FAR[3] = {&thart::IMG_FAR0, &thart::IMG_FAR1, &thart::IMG_FAR2};
  static const Img* NEAR[3] = {&thart::IMG_NEAR0, &thart::IMG_NEAR1, &thart::IMG_NEAR2};
  static const float FARB[3] = {60, 50, 40}, NEARB[3] = {40, 40, 40};
  const Img& f = *FAR[st];
  const Img& n = *NEAR[st];
  drawStrip(f, -bgFar * K, horizon + FARB[st] - f.sh());
  if (st == 2) glow(W / 2, horizon, 1100, Color(255, 60, 190, 50));
  drawStrip(n, -bgNear * K, horizon + NEARB[st] - n.sh());
  // haze where the land meets the sky
  Color fog = THEMES[st].c[TC_FOG];
  rectGrad(0, horizon - 120, W, 160, fog.alpha(0), fog.alpha(st == 2 ? 0.5f : 0.75f));
}

static void drawSprites(int st) {
  for (int i = 0; i < ncmds; i++) {
    const Cmd& c = cmds[i];
    Color tint = WHITE;
    if (st == 2 && c.kind != 2) tint = Color(190, 180, 225);
    tint = tint.alpha(c.alpha);
    if (c.kind == 1)   // a soft shadow under each car
      glow(c.x, c.yb, c.img->sw() * c.sx * 0.5f, Color(0, 0, 0, (uint8_t)(70 * c.alpha)), BLEND_ALPHA);
    drawClipped(*c.img, c.x, c.yb, c.sx, c.sy, c.clip, c.flip, tint);
    if (c.yb - 4 > c.clip) continue;
    float w = c.img->sw() * c.sx, h = c.img->sh() * c.sy;
    if (c.kind == 1 && (st != 1)) {   // tail lights in the dusk and the dark
      for (int s = -1; s <= 1; s += 2)
        glow(c.x + s * w * 0.32f, c.yb - h * 0.5f, fmaxf(6, w * 0.22f), Color(255, 40, 50, (uint8_t)((st == 2 ? 200 : 110) * c.alpha)));
    }
    if (c.kind == 2 && st == 2) {   // street lamps light up the night
      float lx = c.x + (c.flip ? -1 : 1) * 49.5f * c.sx, ly = c.yb - 486 * c.sy;
      glow(lx, ly, fmaxf(10, 260 * c.sx), Color(255, 220, 140, (uint8_t)(170 * c.alpha)));
      glow(lx, c.yb, fmaxf(10, 420 * c.sx), Color(255, 200, 120, (uint8_t)(50 * c.alpha)));
    }
  }
}

static void drawPlayer() {
  float sp = speedv / MAXSPD;
  float bounce = (offroadT && speedv > 5) ? frange(0, 10) : (sp > 0.1f ? 3 * sinf(frameNo * 0.9f) * fminf(1, sp * 2) : 0);
  int frame = steerDir ? (speedv > MAXSPD * 0.5f ? 2 : 1) : 0;
  if (crashT) frame = ((crashT >> 2) & 1) ? 2 : 1;
  bool flip = crashT ? ((crashT >> 3) & 1) : steerDir < 0;
  float x = W / 2 + shakeX, yb = H - 27 + bounce + shakeY;
  glow(x, yb - 6, 230, Color(0, 0, 0, 120), BLEND_ALPHA);
  Fx f; f.flip = flip;
  draw(*thart::PCAR[frame], x, yb, f);
  int st = stage % 3;
  float lightA = braking ? 1.0f : (st == 2 ? 0.45f : st == 0 ? 0.2f : 0);
  if (lightA > 0)
    for (int s = -1; s <= 1; s += 2) {
      float lx = x + s * 115 + (flip ? -1 : 1) * frame * 6, ly = yb - 74;
      glow(lx, ly, braking ? 90 : 60, Color(255, 40, 40, (uint8_t)(200 * lightA)));
      if (braking) glow(lx, ly, 30, Color(255, 200, 180, 220));
    }
}

static void drawParts() {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    if (p.kind == 0) {
      Fx f; f.sx = f.sy = p.size; f.tint = p.col.alpha(t * 0.7f); f.rot = p.life * 3;
      draw(cart::IMG_PUFF, p.x, p.y, f);
    } else if (p.kind == 1) {
      line(p.x, p.y, p.x - p.vx * 1.5f, p.y - p.vy * 1.5f, 4, p.col.alpha(t), BLEND_ADD);
    } else {
      float l = 4.0f;
      line(p.x, p.y, p.x - p.vx * l, p.y - p.vy * l, 3, Color(255, 255, 255, (uint8_t)(80 * sinf(t * 3.14159f))), BLEND_ADD);
    }
  }
}

static void drawHud() {
  bool blink = (frameNo >> 4) & 1;
  // time
  TextStyle lab = ui::style(30, Color(255, 216, 74)); lab.outline = Color(30, 16, 50);
  text("TIME", W / 2, 22, lab);
  bool low = timeLeft < 10 && state == ST_RACE;
  TextStyle tt = ui::style(110, low && blink ? Color(255, 70, 80) : WHITE); tt.shadow = 5;
  textf(W / 2, 58, tt, "%d", (int)ceilf(timeLeft));
  // score
  TextStyle l2 = ui::style(28, Color(184, 220, 255), LEFT); l2.outline = Color(30, 16, 50);
  text("SCORE", 44, 26, l2);
  TextStyle sv = ui::style(56, WHITE, LEFT); sv.shadow = 4;
  textf(44, 60, sv, "%07lu", (unsigned long)score);
  // stage and the progress bar to the checkpoint
  TextStyle l3 = ui::style(28, Color(184, 220, 255), RIGHT); l3.outline = Color(30, 16, 50);
  textf(W - 44, 26, l3, "STAGE %d", stage + 1);
  TextStyle sn = ui::style(40, WHITE, RIGHT); sn.shadow = 3;
  text(STAGES[stage % 3].name, W - 44, 60, sn);
  float prog = clampv((position + PLAYERZ) / trackLen, 0.0f, 1.0f);
  float bx = W - 44 - 380, by = 118;
  roundRect(bx - 4, by - 4, 388, 26, 13, Color(20, 16, 40, 200));
  if (prog > 0.01f) roundRect(bx, by, 380 * prog, 18, 9, Color(255, 200, 60));
  disc(bx + 380 * prog, by + 9, 16, WHITE);
  disc(bx + 380 * prog, by + 9, 10, Color(220, 40, 60));
  // speedometer: digits on the right, a rising bar graph on the left
  int kmh = (int)(speedv / MAXSPD * 293);
  TextStyle kv = ui::style(96, WHITE, RIGHT); kv.shadow = 5;
  textf(W - 60, H - 170, kv, "%d", kmh);
  TextStyle kl = ui::style(30, Color(184, 220, 255), RIGHT); kl.outline = Color(30, 16, 50);
  text("KM/H", W - 60, H - 64, kl);
  for (int i = 0; i < 12; i++) {
    bool on = i < kmh * 12 / 293 + (kmh > 0);
    Color c = on ? (i < 7 ? Color(90, 230, 120) : i < 10 ? Color(255, 210, 60) : Color(255, 70, 70)) : Color(30, 28, 54, 200);
    float h = 18 + i * 6;
    roundRect(40 + i * 26, H - 44 - h, 18, h, 5, c);
    if (on) glow(40 + i * 26 + 9, H - 44 - h / 2, 26, c.alpha(0.35f));
  }
}

static void draw() {
  project();
  int st = stage % 3;
  drawSky(st);
  drawLayers(st);
  rect(0, horizon + 30, W, H, mix(THEMES[st].c[TC_GRASS0], THEMES[st].c[TC_FOG], 0.6f));
  if (shakeX != 0 || shakeY != 0)
    for (auto& v : road) { v.x += shakeX * 0.5f; v.y += shakeY * 0.5f; }
  if (!road.empty()) tris(road.data(), (int)road.size());
  drawSprites(st);
  drawPlayer();
  drawParts();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 250, 230, (uint8_t)(flash * 255)), BLEND_ADD);
  if (state == ST_TITLE) {
    static const char* HELP[] = {"A ACCELERATE    B BRAKE    LEFT / RIGHT STEER", "BEAT THE CLOCK - REACH EVERY CHECKPOINT"};
    char h0[64];
    snprintf(h0, sizeof(h0), "%s ACCELERATE    %s BRAKE    LEFT / RIGHT STEER", btnName(BTN_A), btnName(BTN_B));
    const char* help[2] = {h0, HELP[1]};
    ui::titleScreen(cart::IMG_LOGO_TURBOHORIZON, "RACE THE CLOCK TO THE HORIZON", hiscore, help, 2, Color(255, 200, 150));
    return;
  }
  drawHud();
  if (state == ST_COUNT) {
    int n = 3 - stateT / 60;
    ui::panel(W / 2 - 230, 250, 460, 150, Color(18, 14, 40, 210), 75);
    for (int i = 0; i < 3; i++) {
      bool lit = i < 3 - n || stateT >= 180;
      Color c = stateT >= 180 ? Color(80, 255, 120) : lit ? Color(255, 60, 60) : Color(70, 36, 50);
      float x = W / 2 - 140 + i * 140;
      if (lit) glow(x, 325, 120, c.alpha(0.6f));
      disc(x, 325, 52, c);
      disc(x - 14, 310, 14, Color(255, 255, 255, lit ? 110 : 30));
    }
    TextStyle s = ui::style(64); s.shadow = 4;
    text(STAGES[0].name, W / 2, 440, s);
  }
  if (state == ST_RACE && stateT < 60) {
    float k = 1 + 0.5f * (1 - easeOut(stateT / 20.0f));
    TextStyle s = ui::style(200 * k, Color(80, 255, 120).alpha(clampv((60 - stateT) / 20.0f, 0.0f, 1.0f))); s.shadow = 8;
    text("GO!", W / 2, 330 - 100 * (k - 1), s);
  }
  if (checkT) {
    ui::banner("CHECKPOINT!", W / 2, 300, checkT, 150, Color(255, 216, 74), 110);
    TextStyle s = ui::style(54, WHITE.alpha(clampv(checkT / 15.0f, 0.0f, 1.0f))); s.shadow = 4;
    text(STAGES[stage % 3].name, W / 2, 430, s);
  }
  int pi = 0;
  for (auto& p : popups) {
    if (!p.on) continue;
    float age = (90 - p.t) / 90.0f;
    TextStyle s = ui::style(72, ((p.t >> 2) & 1) ? Color(130, 255, 160) : WHITE); s.shadow = 4;
    s.color = s.color.alpha(clampv(p.t / 20.0f, 0.0f, 1.0f));
    text(p.txt, W / 2, 520 - age * 60 + pi * 80, s);
    pi++;
  }
  if (state == ST_OVER) {
    ui::gameOver(stateT, newHi, "TIME UP");
    if (stateT > 30) {
      TextStyle s = ui::style(48);
      textf(W / 2, 770, s, "SCORE %lu    STAGE %d", (unsigned long)score, stage + 1);
      textf(W / 2, 840, ui::style(40, Color(184, 220, 255)), "CARS PASSED %d", passes);
    }
  }
}

// ------------------------------------------------------------ bot: the original's autopilot
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; p.pressed = p.held; return; }
  if (state == ST_COUNT) { p.held = BTN_A; return; }
  float steer; bool gas, brake;
  autopilot(&steer, &gas, &brake);
  p.ax = steer;
  if (gas) p.held |= BTN_A;
  if (brake) p.held |= BTN_B;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d stage %d score %lu time %.1f kmh %d passed %d crashes %d", state, stage + 1, (unsigned long)score,
           timeLeft, (int)(speedv / MAXSPD * 293), passes, crashes);
}

static void init() { loadAtlas(thart::TEX_FILES, thart::TEX, thart::NTEX); road.reserve(DRAWN * 60); }
static void enter() {
  state = ST_TITLE; stateT = 0; stage = 0; lap = 0;
  position = 0; speedv = 0; playerX = 0;
  buildStage(0);
  spawnCars();
  for (auto& p : parts) p.on = false;
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state == ST_RACE || state == ST_COUNT; }

}  // namespace th

extern const nova::Game TURBO_HORIZON;
const nova::Game TURBO_HORIZON = {
  "turbohorizon", "TURBO HORIZON", "RACE THE CLOCK TO THE HORIZON",
  th::init, th::enter, th::step, th::draw, th::canPause, nullptr, th::bot, th::debugInfo,
};
