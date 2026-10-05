// =====================================================================
//  CITY SHIELD HD  -  missile defence (Nova Arcade HD)
//  Enemy warheads rain down on six cities. Steer the crosshair and launch
//  interceptors from three bases: X fires from the left, A from the centre
//  (the fastest) and B from the right. Each interceptor bursts into a
//  fireball that takes out anything that flies into it, and anything it
//  destroys bursts too, so a well-placed shot can set off a chain.
//  Later waves bring splitting warheads, bombers and satellites.
//
//  The original's rules and timing on its 320 x 240 field (drawn 4.5x,
//  centred, with the scenery running out to the edges). New for HD: a night
//  skyline with lit windows (tools/make_art.py), a sky that changes every
//  wave with aurora, searchlights and drifting cloud, glowing warhead trails,
//  plasma fireballs with bloom and shock rings, turrets that turn to aim,
//  falling debris, smoke and screen flashes.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include "art.h"
#include <stdio.h>
#include <algorithm>

using namespace nova;
using namespace nova::audio;

namespace cs {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  84, 4, "Am F G E",
  {I_STRINGS, 0.7f, 0.1f, "A4 - - - E5 - - - D5 - C5 - B4 - C5 - | F5 - - - E5 - - - C5 - - - A4 - - - | D5 - - - G5 - - - F5 - E5 - D5 - B4 - | E5 - - - - - - - Ab4 - B4 - D5 - . ."},
  {}, I_BELL, "1.5.8.5.1.5.8.5.", 0.18f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.4f,
  "K.......K.....h.|K.......K.....h.|K.......K.....h.|K...K...K...K...", 0.6f, 0, true};
static const Song SONG_PLAY = {
  132, 8, "Am Am F E Am F G E",
  {I_SAWLEAD, 0.5f, 0.05f,
   ". . . . . . . . . . . . . . . . | . . . . . . . . . . . . . . . . | . . . . . . . . . . . . . . . . | . . . . . . . . . . . . . . . . |"
   "A5 . A5 . G5 - E5 - . . E5 G5 A5 - . . | C6 - B5 - A5 - F5 - . . F5 - G5 - A5 - | B5 . B5 . A5 - G5 - . . D5 F5 G5 - . . | Ab5 - - - B5 - - - D6 - C6 - B5 - Ab5 -"},
  {}, I_SQUARE, "1.5.8.5.3.5.8.5.", 0.14f, I_PLUCKBASS, "R.RRR.RRR.RRR.RO", 0.5f, I_STRINGS, 0.22f,
  "K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|KKh.S.h.KKh.S.hS|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|KKh.S.h.KKh.S.hS",
  0.8f, 0, true};
static const Song SONG_CLEAR = {
  140, 2, "A A", {I_MARIMBA, 0.8f, 0, "A4 C5 E5 A5 - - G5 - A5 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.6f, 0, false};
static const Song SONG_OVER = {
  70, 3, "Am Dm Am", {I_FLUTE, 0.8f, 0, "E5 - - - Eb5 - - - D5 - - - C#5 - - - | C5 - - - B4 - - - A4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............|R...............", 0.5f, I_PAD, 0.45f,
  "................|................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ the field (original units)
static const float K = 4.5f, OX = (W - 320 * K) / 2;
static inline float X(float x) { return OX + x * K; }
static inline float Y(float y) { return y * K; }
static const int GROUND_Y = 206, FLAT_Y = 222, CITY_W = 28;
static const int BASE_X[3] = {24, 160, 296};
static const int CITY_X[6] = {58, 90, 122, 182, 214, 246};
static const int AMMO_PER_BASE = 10, LAUNCH_Y = 198;
static const float SHOT_SPEED[3] = {4.2f, 6.2f, 4.2f};   // the centre base is the fastest

struct Warhead { bool on, targeted, mirv; float x0, y0, x, y, vx, vy; int8_t target; uint8_t splitY; };
static Warhead warheads[40];
struct Shot { bool on; float x, y, tx, ty, vx, vy, sx, sy; };
static Shot shots[12];
struct Blast { bool on; float x, y; int t; float maxR; bool big; };
static Blast blasts[40];
struct Flyer { bool on; bool sat; float x, y, vx; int dropT, frame; };
static Flyer flyers[2];

static bool cityAlive[6];
static bool baseAlive[3];
static int ammo[3];
static float crossX = 160, crossY = 110;
static int wave = 1, toSpawn = 0, spawnT = 0, bonusCities = 0, flyerT = 0;
static uint32_t score = 0, hiscore = 0, nextBonus = 10000;
static const uint32_t HI_DEFAULT = 7500;
static bool newHi = false;
static int shake = 0, flash = 0;

enum State { ST_TITLE, ST_INTRO, ST_PLAY, ST_BONUS, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static int bonusAmmo = 0, bonusCity = 0, bonusStep = 0;   // the end-of-wave tally

// ------------------------------------------------------------ effects (cosmetic, original units unless noted)
enum PartKind : uint8_t { P_SMOKE, P_FIRE, P_SPARK, P_DEBRIS, P_EMBER };
struct Part { bool on; uint8_t kind; float x, y, vx, vy, life, max, size, rot, vr; Color c; };
static Part parts[700];
struct RingFx { bool on; float x, y, t, size; Color c; };
static RingFx rings[24];
struct Popup { bool on; float x, y; int t; char txt[12]; };
static Popup popups[8];
struct Star { float x, y, z; };
static Star stars[150];
static float crossDX = 160, crossDY = 110;     // the drawn crosshair glides after the real one
static float barrelA[3] = {0, 0, 0}, recoil[3] = {0, 0, 0};
static float shakeX = 0, shakeY = 0, flashFx = 0;
static int toastT = 0;
static float cloudX = 0;

static inline int mult() { return std::min(6, (wave + 1) / 2); }
static inline float cityCX(int i) { return CITY_X[i] + CITY_W / 2; }
static inline float groundAt(float x) { return GROUND_Y + csart::GROUND_TOP[clampv((int)x, 0, 319)]; }
static int citiesLeft() { int n = 0; for (bool c : cityAlive) n += c; return n; }
static inline float panOf(float x) { return clampv((x - 160) / 160, -1.0f, 1.0f) * 0.7f; }

static Part* newPart() {
  for (auto& p : parts) if (!p.on) { p = Part(); p.on = true; return &p; }
  return nullptr;
}
static void emit(PartKind kind, float x, float y, int n, float sp, float rise, Color c, float life = 40) {
  for (int i = 0; i < n; i++) {
    Part* p = newPart();
    if (!p) return;
    float a = frand() * 6.2832f, v = frange(0.2f, 1.0f) * sp;
    p->kind = kind; p->x = x; p->y = y; p->vx = cosf(a) * v; p->vy = sinf(a) * v - rise;
    p->max = p->life = life * frange(0.6f, 1.2f);
    p->size = frange(0.6f, 1.2f); p->rot = frand() * 360; p->vr = frange(-12, 12);
    p->c = c;
  }
}
static void shockRing(float x, float y, float size, Color c) { for (auto& r : rings) if (!r.on) { r = {true, x, y, 0, size, c}; return; } }

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
  if (score >= nextBonus) { bonusCities++; nextBonus += 10000; toastT = 120; sfx(SFX_POWERUP); }
}
static void popup(float x, float y, const char* s) {
  for (auto& p : popups) if (!p.on) { p.on = true; p.x = x; p.y = y; p.t = 50; snprintf(p.txt, sizeof(p.txt), "%s", s); return; }
}
static void blast(float x, float y, float r, bool big = false) {
  for (auto& b : blasts) if (!b.on) { b = {true, x, y, 0, r, big}; break; }
  shockRing(x, y, r / 19.0f * (big ? 1.3f : 1.0f), big ? Color(255, 170, 90) : Color(170, 230, 255));
  emit(P_SPARK, x, y, big ? 14 : 8, big ? 3.0f : 2.2f, 0, big ? Color(255, 200, 120) : Color(200, 240, 255), 24);
}
static inline float blastR(const Blast& b) {
  if (b.t < 16) return b.maxR * b.t / 16.0f;
  if (b.t < 30) return b.maxR;
  return b.maxR * (46 - b.t) / 16.0f;
}

// ------------------------------------------------------------ spawning
static void targetPoint(int8_t t, float* tx, float* ty) {
  if (t < 6) { *tx = cityCX(t) + frange(-6, 6); *ty = FLAT_Y - 4; }
  else if (t < 9) { *tx = BASE_X[t - 6]; *ty = LAUNCH_Y + 2; }
  else { *tx = frange(10, 310); *ty = groundAt(*tx); }
}
static int8_t pickTarget() {
  for (int tries = 0; tries < 12; tries++) {
    int8_t t = rnd() % 10;
    if (t < 6 && cityAlive[t]) return t;
    if (t >= 6 && t < 9 && baseAlive[t - 6]) return t;
  }
  return 9;
}
static float warheadSpeed() { return std::min(1.7f, 0.30f + wave * 0.055f) * speed(); }

static void launchWarhead(float x, float y, int8_t target, bool canMirv) {
  for (auto& w : warheads) {
    if (w.on) continue;
    float tx, ty; targetPoint(target, &tx, &ty);
    float dx = tx - x, dy = ty - y, d = sqrtf(dx * dx + dy * dy);
    float v = warheadSpeed() * frange(0.85f, 1.15f);
    w = {true, false, canMirv && wave >= 3 && rnd() % 100 < (uint32_t)std::min(40, wave * 5), x, y, x, y, dx / d * v, dy / d * v, target, (uint8_t)(70 + rnd() % 50)};
    return;
  }
}

static void startWave() {
  memset(warheads, 0, sizeof(warheads)); memset(shots, 0, sizeof(shots)); memset(blasts, 0, sizeof(blasts));
  memset(flyers, 0, sizeof(flyers));
  for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = AMMO_PER_BASE; }
  // spend banked bonus cities on the ruins
  for (int i = 0; i < 6 && bonusCities; i++) if (!cityAlive[i]) { cityAlive[i] = true; bonusCities--; }
  toSpawn = 10 + wave * 3;
  spawnT = 90;
  flyerT = 400 + rnd() % 300;
  state = ST_INTRO; stateT = 0;
  music(nullptr);
}

static void resetGame() {
  score = 0; newHi = false; wave = 1; bonusCities = 0; nextBonus = 10000;
  for (bool& c : cityAlive) c = true;
  crossX = 160; crossY = 110;
  hiscore = loadHi(HI_DEFAULT);
  for (auto& p : popups) p.on = false;
  startWave();
}

// ------------------------------------------------------------ firing
static bool fire(int base, float tx, float ty) {
  // fall back to the nearest base that can still fire
  if (!baseAlive[base] || !ammo[base]) {
    int best = -1; float bd = 1e9;
    for (int i = 0; i < 3; i++) if (baseAlive[i] && ammo[i] && fabsf(BASE_X[i] - tx) < bd) { bd = fabsf(BASE_X[i] - tx); best = i; }
    if (best < 0) { sfx(SFX_BUMP); return false; }
    base = best;
  }
  for (auto& s : shots) {
    if (s.on) continue;
    float x = BASE_X[base], y = LAUNCH_Y - 8;
    float dx = tx - x, dy = ty - y, d = std::max(1.0f, sqrtf(dx * dx + dy * dy));
    s = {true, x, y, tx, ty, dx / d * SHOT_SPEED[base], dy / d * SHOT_SPEED[base], x, y};
    ammo[base]--;
    recoil[base] = 1;
    emit(P_SMOKE, x, y + 4, 6, 0.8f, 0.1f, Color(170, 180, 210), 34);
    emit(P_SPARK, x, y, 5, 1.5f, 0.4f, Color(160, 230, 255), 14);
    sfx(SFX_SHOOT, panOf(x), base == 1 ? 0.62f : 0.5f);
    return true;
  }
  return false;
}

// Attract mode: the original's autopilot, firing straight from the bases. Leads each warhead and fires
// from the nearest base that can.
static void autopilot() {
  for (auto& w : warheads) {
    if (!w.on || w.targeted || w.y < 40) continue;
    int base = 1; float bd = 1e9;
    for (int i = 0; i < 3; i++) if (baseAlive[i] && ammo[i] && fabsf(BASE_X[i] - w.x) < bd) { bd = fabsf(BASE_X[i] - w.x); base = i; }
    float px = w.x, py = w.y;
    for (int it = 0; it < 3; it++) {
      float t = sqrtf((px - BASE_X[base]) * (px - BASE_X[base]) + (py - LAUNCH_Y) * (py - LAUNCH_Y)) / SHOT_SPEED[base] + 8;
      px = w.x + w.vx * t; py = w.y + w.vy * t;
    }
    if (py > 185) continue;
    if (fire(base, px, py)) w.targeted = true;
    return;   // one shot per frame
  }
}

// ------------------------------------------------------------ update
static void destroyWarhead(Warhead& w, bool scored) {
  w.on = false;
  blast(w.x, w.y, 13);
  if (scored) {
    addScore(25 * mult());
    char b[12]; snprintf(b, sizeof(b), "%d", 25 * mult());
    popup(w.x, w.y - 8, b);
  }
}

static void bigBoom(float x, float y, float shakeAmt) {
  emit(P_FIRE, x, y - 4, 30, 2.4f, 0.6f, Color(255, 150, 60), 50);
  emit(P_SMOKE, x, y - 4, 24, 1.0f, 0.45f, Color(90, 80, 100), 90);
  emit(P_DEBRIS, x, y - 6, 22, 3.2f, 2.2f, Color(40, 34, 50), 80);
  emit(P_EMBER, x, y - 6, 20, 2.6f, 1.6f, Color(255, 190, 90), 60);
  shockRing(x, y, 1.8f, Color(255, 160, 80));
  rumble(shakeAmt / 20.0f, 300);
}

static void hitGround(Warhead& w) {
  w.on = false;
  blast(w.x, w.y, 16, true);
  shake = 10;
  for (int i = 0; i < 6; i++)
    if (cityAlive[i] && fabsf(w.x - cityCX(i)) < CITY_W / 2 + 3) {
      cityAlive[i] = false;
      flash = 6; shake = 18;
      sfx(SFX_EXPLODE, panOf(w.x), 0.55f);
      bigBoom(cityCX(i), FLAT_Y - 8, 18);
      return;
    }
  for (int i = 0; i < 3; i++)
    if (baseAlive[i] && fabsf(w.x - BASE_X[i]) < 16) {
      baseAlive[i] = false; ammo[i] = 0;
      flash = 4; shake = 16;
      sfx(SFX_EXPLODE, panOf(w.x), 0.55f);
      bigBoom(BASE_X[i], LAUNCH_Y, 16);
      return;
    }
  sfx(SFX_EXPLODE, panOf(w.x), 0.85f);
  emit(P_SMOKE, w.x, w.y, 8, 0.7f, 0.3f, Color(90, 80, 100), 60);
}

static void updateWorld(bool demo) {
  // spawning
  if (toSpawn > 0 && --spawnT <= 0) {
    int salvo = std::min(toSpawn, 1 + (int)(rnd() % (2 + wave / 2)));
    for (int i = 0; i < salvo; i++) launchWarhead(frange(10, 310), 14, pickTarget(), true);
    toSpawn -= salvo;
    spawnT = frames(std::max(40, 170 - wave * 10)) + rnd() % 60;
  }
  if (wave >= 2 && toSpawn > 0 && --flyerT <= 0) {
    for (auto& f : flyers) if (!f.on) {
      bool left = rnd() & 1;
      f = {true, wave >= 4 && (rnd() & 1), left ? -24.0f : 330.0f, frange(60, 110), (left ? 1 : -1) * 0.7f * speed(), 60, 0};
      break;
    }
    flyerT = 500 + rnd() % 400;
  }
  for (auto& f : flyers) {
    if (!f.on) continue;
    f.x += f.vx; f.frame++;
    if (--f.dropT <= 0 && toSpawn > 0 && f.x > 20 && f.x < 300) {
      launchWarhead(f.x, f.y + 6, pickTarget(), false); toSpawn--;
      f.dropT = frames(110) + rnd() % 60;
    }
    if (f.x < -30 || f.x > 340) f.on = false;
  }
  // warheads
  for (auto& w : warheads) {
    if (!w.on) continue;
    w.x += w.vx; w.y += w.vy;
    if (w.mirv && w.y >= w.splitY) {
      w.mirv = false;
      int n = 2 + (rnd() % 2 == 0);
      for (int i = 0; i < n; i++) launchWarhead(w.x, w.y, pickTarget(), false);
      w.x0 = w.x; w.y0 = w.y;   // the original carries on, with a fresh trail
      emit(P_SPARK, w.x, w.y, 10, 2.0f, 0, Color(255, 120, 120), 20);
      shockRing(w.x, w.y, 0.5f, Color(255, 100, 120));
    }
    if (w.y >= groundAt(w.x) - 1 || (w.target < 6 && w.y >= FLAT_Y - 4) || (w.target >= 6 && w.target < 9 && w.y >= LAUNCH_Y + 2)) hitGround(w);
  }
  // interceptors
  for (auto& s : shots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if ((frameNo & 1) == 0) emit(P_SMOKE, s.x - s.vx * 2, s.y - s.vy * 2, 1, 0.15f, 0.05f, Color(150, 160, 200), 26);
    if ((s.vy <= 0 && s.y <= s.ty) || (s.vy > 0 && s.y >= s.ty)) { s.on = false; blast(s.tx, s.ty, 19); sfx(SFX_EXPLODE, panOf(s.tx), 1.1f); }
  }
  // fireballs
  for (auto& b : blasts) {
    if (!b.on) continue;
    if (++b.t >= 46) { b.on = false; continue; }
    float r = blastR(b);
    for (auto& w : warheads) {
      if (!w.on) continue;
      float dx = w.x - b.x, dy = w.y - b.y;
      if (dx * dx + dy * dy <= r * r) destroyWarhead(w, !demo);
    }
    for (auto& f : flyers) {
      if (!f.on) continue;
      float dx = f.x - b.x, dy = f.y - b.y;
      if (dx * dx + dy * dy <= (r + 8) * (r + 8)) {
        f.on = false;
        blast(f.x, f.y, 20, true);
        emit(P_DEBRIS, f.x, f.y, 16, 2.4f, 1.2f, Color(110, 120, 160), 70);
        emit(P_SPARK, f.x, f.y, 16, 2.6f, 0, Color(220, 230, 255), 26);
        sfx(SFX_EXPLODE, panOf(f.x), 0.6f);
        if (!demo) { addScore(100 * mult()); char t[12]; snprintf(t, sizeof(t), "%d", 100 * mult()); popup(f.x, f.y - 10, t); }
      }
    }
  }
  // burning ruins smoulder
  if ((frameNo & 7) == 0)
    for (int i = 0; i < 6; i++) if (!cityAlive[i]) emit(P_SMOKE, CITY_X[i] + 4 + rnd() % 20, FLAT_Y - 4, 1, 0.2f, 0.35f, Color(70, 64, 84), 80);
  for (auto& p : popups) if (p.on) { p.y -= 0.4f; if (--p.t <= 0) p.on = false; }
  if (shake) shake--;
  if (flash) flash--;
}

static bool waveDone() {
  if (toSpawn > 0) return false;
  for (auto& w : warheads) if (w.on) return false;
  for (auto& b : blasts) if (b.on) return false;
  for (auto& s : shots) if (s.on) return false;
  for (auto& f : flyers) if (f.on) return false;
  return true;
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.rot += p.vr;
    switch (p.kind) {
      case P_SMOKE: p.vx *= 0.97f; p.vy = p.vy * 0.97f - 0.01f; break;
      case P_FIRE: p.vx *= 0.93f; p.vy = p.vy * 0.93f - 0.02f; break;
      case P_SPARK: p.vx *= 0.9f; p.vy = p.vy * 0.9f + 0.03f; break;
      case P_DEBRIS: p.vx *= 0.99f; p.vy += 0.09f; if (p.y > FLAT_Y + 4) { p.y = FLAT_Y + 4; p.vy *= -0.3f; p.vx *= 0.5f; p.vr *= 0.5f; } break;
      case P_EMBER: p.vx *= 0.98f; p.vy += 0.05f; break;
    }
    if (--p.life <= 0) p.on = false;
  }
  for (auto& r : rings) if (r.on && (r.t += 0.045f) >= 1) r.on = false;
  for (auto& s : stars) { s.x += 0.02f * s.z; if (s.x > W) s.x -= W; }
  cloudX += 0.15f;
  float sh = shake * 1.3f;
  if (sh > 0.5f) { shakeX = frange(-sh, sh); shakeY = frange(-sh, sh); } else shakeX = shakeY = 0;
  flashFx = fmaxf(flashFx * 0.85f, flash / 6.0f);
  if (toastT) toastT--;
  // the crosshair glides; the turrets swing towards it
  crossDX = approach(crossDX, crossX, 0.55f); crossDY = approach(crossDY, crossY, 0.55f);
  for (int i = 0; i < 3; i++) {
    float tx = state == ST_TITLE ? 160 : crossX, ty = state == ST_TITLE ? 60 : crossY;
    float a = atan2f(tx - BASE_X[i], LAUNCH_Y - 8 - ty) * 57.29578f;
    barrelA[i] = approach(barrelA[i], clampv(a, -80.0f, 80.0f), 0.2f);
    recoil[i] *= 0.85f;
  }
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { sfx(SFX_START); resetGame(); break; }
      // attract mode: the cities defend themselves
      if (waveDone()) { for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = 99; } for (bool& c : cityAlive) c = true; toSpawn = 8; spawnT = 60; }
      autopilot();
      updateWorld(true);
      break;
    case ST_INTRO:
      if (stateT == 1 || stateT == 25 || stateT == 49) { sfx(SFX_SHOOT, -0.3f, 0.35f); sfx(SFX_SHOOT, 0.3f, 0.3f); }   // the air-raid warning
      updateWorld(false);
      if (stateT > 150 || (stateT > 40 && in.hit(BTN_A | BTN_START))) { state = ST_PLAY; stateT = 0; music(&SONG_PLAY); }
      break;
    case ST_PLAY: {
      if (in.hit(BTN_START)) { nova::pause(); break; }
      float sp = 3.6f;
      crossX = clampv(crossX + in.ax * sp, 4.0f, 316.0f);
      crossY = clampv(crossY + in.ay * sp, 16.0f, 188.0f);
      if (in.hit(BTN_X)) fire(0, crossX, crossY);
      if (in.hit(BTN_A | BTN_Y)) fire(1, crossX, crossY);
      if (in.hit(BTN_B)) fire(2, crossX, crossY);
      updateWorld(false);
      if (stateT > 60 && waveDone()) {
        state = ST_BONUS; stateT = 0; bonusStep = 0;
        bonusAmmo = ammo[0] + ammo[1] + ammo[2]; bonusCity = citiesLeft();
        music(&SONG_CLEAR);
      }
      break;
    }
    case ST_BONUS:
      updateWorld(false);
      // tally: each spare interceptor, then each city
      if (stateT > 60 && stateT % 5 == 0) {
        if (bonusStep < bonusAmmo) { bonusStep++; addScore(5 * mult()); sfx(SFX_MOVE); }
        else if (bonusStep < bonusAmmo + bonusCity && stateT % 15 == 0) { bonusStep++; addScore(100 * mult()); sfx(SFX_COIN); }
      }
      if (bonusStep >= bonusAmmo + bonusCity && stateT > 120 + bonusAmmo * 5 + bonusCity * 15 + 60) {
        if (citiesLeft() == 0 && bonusCities == 0) {
          state = ST_OVER; stateT = 0; music(&SONG_OVER);
          if (newHi) saveHi(hiscore);
        } else { wave++; startWave(); }
      }
      break;
    case ST_OVER:
      updateWorld(true);
      if (stateT % 40 == 0 && stateT < 300) { float x = frange(40, 280), y = frange(60, 180); blast(x, y, 26, true); sfx(SFX_EXPLODE, panOf(x), 0.55f); shake = 8; }
      if (stateT > 150 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; for (bool& c : cityAlive) c = true; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing: the sky
static const uint8_t SKIES[4][3][3] = {
  {{4, 6, 22}, {18, 22, 60}, {60, 40, 90}},       // midnight
  {{14, 4, 30}, {60, 20, 80}, {190, 70, 110}},     // violet dusk
  {{6, 10, 26}, {20, 50, 70}, {40, 140, 120}},     // aurora green
  {{20, 4, 10}, {80, 16, 30}, {230, 110, 60}},     // red dawn
};
static const Color TRAIL[4] = {Color(255, 80, 110), Color(255, 120, 60), Color(255, 90, 200), Color(255, 230, 90)};
static inline int skyIdx() { return (wave - 1) % 4; }
static inline Color skyC(int stop) { const uint8_t* c = SKIES[skyIdx()][stop]; return Color(c[0], c[1], c[2]); }

// a tapered streak from (x0, y0) to (x1, y1) with colours fading along it
static void streak(float x0, float y0, float x1, float y1, float w0, float w1, Color c0, Color c1, Blend b = BLEND_ADD) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  if (l < 0.5f) return;
  float nx = -dy / l, ny = dx / l;
  Vtx v[6] = {{x0 + nx * w0, y0 + ny * w0, c0}, {x1 + nx * w1, y1 + ny * w1, c1}, {x1 - nx * w1, y1 - ny * w1, c1},
              {x0 + nx * w0, y0 + ny * w0, c0}, {x1 - nx * w1, y1 - ny * w1, c1}, {x0 - nx * w0, y0 - ny * w0, c0}};
  tris(v, 6, b);
}

static float ridgeH(float x, int layer) {
  float a = x * 0.0031f + layer * 2.1f;
  return 60 + 38 * sinf(a) + 22 * sinf(a * 2.7f + 1.3f) + 10 * sinf(a * 6.1f + 0.4f) + layer * 30;
}

static void drawSky() {
  float gy = Y(GROUND_Y) + 40;
  Color c0 = skyC(0), c1 = skyC(1), c2 = skyC(2);
  rectGrad(0, 0, W, gy * 0.55f, c0, c1);
  rectGrad(0, gy * 0.55f, W, gy * 0.45f + 1, c1, c2);
  rect(0, gy, W, H - gy, c2);
  // stars, fading out towards the glowing horizon
  for (int i = 0; i < 150; i++) {
    const Star& s = stars[i];
    float tw = 0.55f + 0.45f * sinf(frameNo * 0.05f + i * 1.7f);
    float fade = clampv(1.3f - s.y / 650.0f, 0.0f, 1.0f);
    disc(s.x, s.y, 1.2f + 2.0f * s.z, Color(220, 225, 255).alpha(s.z * tw * fade));
    if (s.z > 0.92f) glow(s.x, s.y, 14, Color(200, 210, 255, (uint8_t)(70 * tw * fade)));
  }
  // aurora curtains on the green-sky waves (and the title)
  if (skyIdx() == 2 || state == ST_TITLE) {
    const int N = 96;
    static Vtx v[N * 12];
    int k = 0;
    for (int i = 0; i < N; i++) {
      float x0 = i * W / (float)N, x1 = (i + 1) * W / (float)N;
      auto top = [](float x) { return Y(26) + Y(12) * sinf(x / K * 0.03f + frameNo * 0.01f) + Y(6) * sinf(x / K * 0.07f - frameNo * 0.017f); };
      auto len = [](float x) { return Y(30) + Y(14) * sinf(x / K * 0.05f + frameNo * 0.013f); };
      auto str = [](float x) { float ph = x / K * 0.03f + frameNo * 0.01f; return clampv(0.5f + 0.5f * sinf(x / K * 0.21f * 0.35f + sinf(ph * 2) * 2), 0.0f, 1.0f); };
      float s0 = str(x0), s1 = str(x1);
      Color a0 = Color(80, 255, 170, (uint8_t)(90 * s0)), a1 = Color(80, 255, 170, (uint8_t)(90 * s1));
      Color b0 = Color(120, 90, 255, 0), b1 = b0;
      Color m0 = Color(120, 200, 255, (uint8_t)(40 * s0)), m1 = Color(120, 200, 255, (uint8_t)(40 * s1));
      float t0 = top(x0), t1 = top(x1), l0 = len(x0), l1 = len(x1);
      // a bright lower edge fading upwards, and a long fade downwards
      v[k++] = {x0, t0 - l0 * 0.8f, b0}; v[k++] = {x1, t1 - l1 * 0.8f, b1}; v[k++] = {x1, t1, a1};
      v[k++] = {x0, t0 - l0 * 0.8f, b0}; v[k++] = {x1, t1, a1}; v[k++] = {x0, t0, a0};
      v[k++] = {x0, t0, m0}; v[k++] = {x1, t1, m1}; v[k++] = {x1, t1 + l1, b1};
      v[k++] = {x0, t0, m0}; v[k++] = {x1, t1 + l1, b1}; v[k++] = {x0, t0 + l0, b0};
    }
    tris(v, k, BLEND_ADD);
  }
  // the moon
  if (state != ST_TITLE) {
    float mx = X(275), my = Y(37);
    glow(mx, my, 360, Color(170, 170, 255, 40));
    glow(mx, my, 150, Color(255, 250, 230, 60));
    draw(csart::IMG_MOON, mx, my);
  }
  // drifting cloud wisps, lit from below by the city
  for (int i = 0; i < 7; i++) {
    float x = fmodf(i * 397.0f + cloudX * (0.6f + i * 0.08f), W + 900) - 450;
    float y = 220 + (i * 131) % 380;
    Fx f; f.sx = 5.5f + (i % 3); f.sy = 0.9f + 0.2f * (i % 2);
    f.tint = mix(c1, c2, 0.6f).alpha(0.45f);
    draw(cart::IMG_PUFF, x, y, f);
    f.tint = mix(c2, Color(255, 200, 160), 0.3f).alpha(0.12f); f.blend = BLEND_ADD; f.sy *= 0.6f;
    draw(cart::IMG_PUFF, x, y + 22, f);
  }
  // searchlights sweeping up from the far city
  if (state != ST_OVER) {
    static const float SX[3] = {360, 980, 1580};
    for (int i = 0; i < 3; i++) {
      float a = (sinf(frameNo * (0.006f + i * 0.0017f) + i * 2.1f) * 0.42f) - 1.5708f;
      float bx = SX[i], by = 960, L = 1000, wdt = 0.07f;
      Vtx v[3] = {{bx, by, Color(200, 220, 255, 60)},
                  {bx + cosf(a - wdt) * L, by + sinf(a - wdt) * L, Color(150, 180, 255, 0)},
                  {bx + cosf(a + wdt) * L, by + sinf(a + wdt) * L, Color(150, 180, 255, 0)}};
      tris(v, 3, BLEND_ADD);
    }
  }
  // two ranges of hills, then the far city
  for (int layer = 0; layer < 2; layer++) {
    const int N = 64;
    Vtx v[N * 6];
    int k = 0;
    Color top = mix(c1, Color(10, 10, 30), layer ? 0.45f : 0.25f), bot = mix(c0, Color(6, 6, 16), 0.5f);
    float base = layer ? 990 : 960;
    for (int i = 0; i < N; i++) {
      float x0 = i * W / (float)N, x1 = (i + 1) * W / (float)N;
      float h0 = base - ridgeH(x0, layer) * (layer ? 1.4f : 2.2f), h1 = base - ridgeH(x1, layer) * (layer ? 1.4f : 2.2f);
      v[k++] = {x0, h0, top}; v[k++] = {x1, h1, top}; v[k++] = {x1, (float)H, bot};
      v[k++] = {x0, h0, top}; v[k++] = {x1, (float)H, bot}; v[k++] = {x0, (float)H, bot};
    }
    tris(v, k);
    if (layer == 0) glow(W / 2, 960, 1100, mix(c2, Color(255, 160, 120), 0.3f).alpha(0.25f));
  }
  Color sk = mix(c0, Color(16, 18, 44), 0.5f);
  drawRect(csart::IMG_SKYLINE, 0, 1000 - 330, W, 330, sk);
  float wk = flashFx > 0.05f ? 0.4f : 0.75f + 0.08f * sinf(frameNo * 0.03f);
  drawRect(csart::IMG_SKYLINE_WIN, 0, 1000 - 330, W, 330, Color(255, 210, 140).alpha(wk), BLEND_ADD);
  rectGrad(0, 880, W, 120, Color(0, 0, 0, 0), mix(c2, Color(255, 170, 120), 0.4f).alpha(0.22f), BLEND_ADD);   // city haze
}

// ------------------------------------------------------------ drawing: the ground, cities and bases
static void drawGround(float ox, float oy) {
  rect(0, H - 40, W, 40, Color(12, 10, 24));
  drawRect(csart::IMG_GROUND, ox - 16, csart::LAYER_TOP + oy, W + 32, 220);
  for (int i = 0; i < 6; i++) {
    float cx = X(cityCX(i)) + ox, by = Y(FLAT_Y) + 6 + oy;
    if (cityAlive[i]) {
      glow(cx, by - 40, 150, Color(255, 190, 120, 40));
      draw(*csart::CITIES[i], cx, by);
      for (int l = 0; l < csart::NLIGHTS; l++) {
        float lx = csart::CITY_LIGHTS[i][l][0], ly = csart::CITY_LIGHTS[i][l][1];
        if (lx < -500) continue;
        bool on = ((frameNo + i * 23 + l * 37) / 40) & 1;
        if (on) { disc(cx + lx, by + ly, 3, Color(255, 70, 80)); glow(cx + lx, by + ly, 18, Color(255, 60, 70, 150)); }
      }
    } else {
      draw(*csart::RUINS[i], cx, by);
      float fl = 0.6f + 0.4f * sinf(frameNo * 0.2f + i * 2);
      glow(cx, by - 10, 110, Color(255, 110, 40, (uint8_t)(70 * fl)));
    }
  }
  for (int i = 0; i < 3; i++) {
    float bx = X(BASE_X[i]) + ox, gy = Y(groundAt(BASE_X[i])) + 10 + oy;
    if (baseAlive[i]) {
      // the barrel turns towards the crosshair and kicks back when it fires
      float a = barrelA[i] * 0.0174533f, kick = recoil[i] * 8;
      Fx f; f.rot = barrelA[i];
      draw(csart::IMG_BARREL, bx - sinf(a) * kick, gy - 46 + cosf(a) * kick, f);
      draw(csart::IMG_BASE, bx, gy);
      if (recoil[i] > 0.3f) glow(bx + sinf(a) * 50, gy - 46 - cosf(a) * 50, 70 * recoil[i], Color(160, 230, 255, 200));
      bool lowAmmo = ammo[i] <= 3 && state == ST_PLAY;
      Color lamp = ammo[i] == 0 ? Color(255, 60, 70) : lowAmmo ? Color(255, 190, 60) : Color(90, 255, 160);
      if (!(lowAmmo && (frameNo & 16))) glow(bx, gy - 18, 26, lamp.alpha(0.8f));
    } else {
      draw(csart::IMG_BASE_RUIN, bx, gy);
      glow(bx, gy - 8, 80, Color(255, 110, 40, (uint8_t)(60 + 20 * sinf(frameNo * 0.25f + i))));
    }
    // the ammo rack below the base
    if (state == ST_TITLE) continue;
    float ay = Y(236) + oy;
    if (baseAlive[i] && ammo[i]) {
      for (int k = 0; k < ammo[i] && k < AMMO_PER_BASE; k++) draw(csart::IMG_AMMO, X(BASE_X[i] - 13.5f + k * 3) + ox, ay);
    } else if (state == ST_PLAY || state == ST_INTRO) {
      TextStyle st = ui::style(34, Color(255, 90, 90)); st.outline = Color(40, 10, 20);
      text("OUT", bx, Y(229) + oy, st);
    }
  }
}

// ------------------------------------------------------------ drawing: the action
static const Color RINGC[6] = {Color(255, 255, 255), Color(255, 230, 90), Color(255, 140, 40), Color(255, 70, 110), Color(170, 90, 255), Color(80, 200, 255)};
static Color cycle(float ph) {
  int k = ((int)floorf(ph)) % 6;
  if (k < 0) k += 6;
  return mix(RINGC[k], RINGC[(k + 1) % 6], ph - floorf(ph));
}

static void drawAction(float ox, float oy) {
  Color tc = TRAIL[skyIdx()];
  // smoke and debris sit behind the bright things
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max, x = X(p.x) + ox, y = Y(p.y) + oy;
    if (p.kind == P_SMOKE) {
      Fx f; f.sx = f.sy = (0.25f + (1 - t) * 0.6f) * p.size; f.tint = p.c.alpha(0.55f * fminf(1, t * 2)); f.rot = p.rot;
      draw(cart::IMG_PUFF, x, y, f);
    } else if (p.kind == P_DEBRIS) {
      float a = p.rot * 0.0174533f, l = 7 * p.size;
      line(x - cosf(a) * l, y - sinf(a) * l, x + cosf(a) * l, y + sinf(a) * l, 5 * p.size, p.c.alpha(fminf(1, t * 3)));
      if (t > 0.6f) glow(x, y, 14, Color(255, 140, 60, (uint8_t)(120 * (t - 0.6f) / 0.4f)));
    }
  }
  // enemy warheads: glowing trails from where they started
  for (auto& w : warheads) {
    if (!w.on) continue;
    float x0 = X(w.x0) + ox, y0 = Y(w.y0) + oy, x1 = X(w.x) + ox, y1 = Y(w.y) + oy;
    streak(x0, y0, x1, y1, 2, 12, tc.alpha(0), tc.alpha(0.35f));
    streak(x0, y0, x1, y1, 1, 3.5f, tc.alpha(0.2f), mix(tc, WHITE, 0.4f));
    bool hot = (frameNo >> 2) & 1;
    glow(x1, y1, 46, (hot ? Color(255, 240, 200) : tc).alpha(0.7f));
    disc(x1, y1, 5.5f, hot ? WHITE : Color(255, 220, 120));
    if (w.mirv) {
      float pu = 0.5f + 0.5f * sinf(frameNo * 0.3f);
      ring(x1, y1, 14 + 4 * pu, 2.5f, Color(255, 90, 90, (uint8_t)(120 + 120 * pu)), BLEND_ADD);
    }
  }
  // bombers and satellites
  for (auto& f : flyers) {
    if (!f.on) continue;
    float x = X(f.x) + ox, y = Y(f.y) + oy + 4 * sinf(f.frame * 0.05f);
    if (f.sat) {
      Fx fx; fx.rot = 8 * sinf(f.frame * 0.03f);
      draw(csart::IMG_SAT, x, y, fx);
      if ((f.frame >> 4) & 1) glow(x, y - 4, 40, Color(255, 80, 80, 200));
      glow(x, y, 120, Color(120, 160, 255, 30));
    } else {
      Fx fx; fx.flip = f.vx < 0;
      draw(csart::IMG_BOMBER, x, y, fx);
      float d = f.vx < 0 ? -1 : 1;
      if ((f.frame >> 3) & 1) glow(x - d * 40, y - 18, 22, Color(255, 60, 60, 220));
      else glow(x + d * 4, y + 30, 22, Color(90, 255, 140, 200));
      glow(x - d * 54, y, 30, Color(255, 170, 90, 120));   // engines
    }
  }
  // interceptors: a fading exhaust trail from the base, the missile, and the target mark
  for (auto& s : shots) {
    if (!s.on) continue;
    float x = X(s.x) + ox, y = Y(s.y) + oy;
    streak(X(s.sx) + ox, Y(s.sy) + oy, x, y, 1, 6, Color(80, 200, 255, 0), Color(120, 220, 255, 150));
    float ang = atan2f(s.vy, s.vx) * 57.29578f;
    glow(x - s.vx * 3, y - s.vy * 3, 34, Color(120, 220, 255, 200));
    Fx f; f.rot = ang;
    draw(csart::IMG_MISSILE, x, y, f);
    if ((frameNo >> 2) & 1) {
      float tx = X(s.tx) + ox, ty = Y(s.ty) + oy;
      line(tx - 12, ty - 12, tx + 12, ty + 12, 3.5f, Color(150, 230, 255), BLEND_ADD);
      line(tx - 12, ty + 12, tx + 12, ty - 12, 3.5f, Color(150, 230, 255), BLEND_ADD);
      glow(tx, ty, 30, Color(120, 220, 255, 90));
    }
  }
  // fireballs: a colour-cycling plasma sphere with a hot core and bloom
  for (auto& b : blasts) {
    if (!b.on) continue;
    float r = blastR(b) * K;
    if (r < 1) continue;
    float x = X(b.x) + ox, y = Y(b.y) + oy;
    float ph = b.t * 0.5f + b.x;
    Color shell = cycle(ph), inner = cycle(ph + (b.big ? 2 : 3));
    float fade = b.t < 30 ? 1.0f : 0.6f + 0.4f * (46 - b.t) / 16.0f;
    glow(x, y, r * 2.6f, shell.alpha(0.45f * fade));
    disc(x, y, r, shell.alpha(0.92f));
    disc(x, y, r * 0.72f, inner.alpha(0.85f), BLEND_ADD);
    glow(x - r * 0.25f, y - r * 0.3f, r * 0.7f, Color(255, 255, 255, 110));
    if (b.t < 30) glow(x, y, r * 0.8f, Color(255, 255, 240, 230));
    ring(x, y, r, 3, Color(255, 255, 255, (uint8_t)(140 * fade)), BLEND_ADD);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = r.size * (0.4f + r.t * 2.4f); f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, X(r.x) + ox, Y(r.y) + oy, f);
  }
  // fire, sparks and embers on top
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max, x = X(p.x) + ox, y = Y(p.y) + oy;
    if (p.kind == P_FIRE) {
      Fx f; f.sx = f.sy = (0.2f + (1 - t) * 0.5f) * p.size; f.rot = p.rot; f.blend = BLEND_ADD;
      f.tint = mix(Color(120, 40, 30), p.c, t).alpha(t);
      draw(cart::IMG_PUFF, x, y, f);
    } else if (p.kind == P_SPARK) {
      Fx f; f.tint = mix(p.c, WHITE, t * 0.4f).alpha(fminf(1, t * 1.6f)); f.blend = BLEND_ADD; f.sx = f.sy = 0.35f * p.size; f.rot = p.rot;
      draw(cart::IMG_SPARK, x, y, f);
    } else if (p.kind == P_EMBER) {
      disc(x, y, 2.5f + 2 * t, p.c.alpha(t));
      glow(x, y, 14, Color(255, 140, 50, (uint8_t)(160 * t)));
    }
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    TextStyle st = ui::style(40, (p.t & 4) ? WHITE : Color(255, 216, 74));
    st.color = st.color.alpha(fminf(1, p.t / 12.0f));
    text(p.txt, X(p.x) + ox, Y(p.y) + oy, st);
  }
}

static void drawCrosshair() {
  float x = X(crossDX), y = Y(crossDY);
  bool any = false;
  for (int i = 0; i < 3; i++) any |= baseAlive[i] && ammo[i] > 0;
  Color c = any ? Color(120, 230, 255) : Color(255, 90, 90);
  float pu = 0.5f + 0.5f * sinf(frameNo * 0.15f);
  glow(x, y, 60, c.alpha(0.25f + 0.1f * pu));
  ring(x, y, 24 + 2 * pu, 3, c, BLEND_ADD);
  float a0 = frameNo * 0.02f;
  for (int k = 0; k < 4; k++) {
    float a = a0 + k * 1.5708f, ca = cosf(a), sa = sinf(a);
    line(x + ca * 14, y + sa * 14, x + ca * 38, y + sa * 38, 3.5f, c);
  }
  disc(x, y, 3.5f, WHITE);
}

static void drawHud() {
  float lx = OX / 2;
  char buf[32];
  ui::box(lx - 105, 40, 210, 260, nullptr);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 64);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 180, Color(255, 220, 110));
  float rx = W - lx;
  ui::box(rx - 105, 40, 210, bonusCities ? 380 : 260, nullptr);
  snprintf(buf, sizeof(buf), "%d", wave);
  ui::stat("WAVE", buf, rx, 64, Color(150, 230, 255));
  snprintf(buf, sizeof(buf), "%dX", mult());
  ui::stat("POINTS", buf, rx, 180, Color(255, 160, 200));
  if (bonusCities) { snprintf(buf, sizeof(buf), "+%d", bonusCities); ui::stat("SPARE CITY", buf, rx, 296, Color(140, 255, 170)); }
}

static void drawOverlays() {
  bool blink = (frameNo >> 4) & 1;
  if (state == ST_INTRO) {
    float k = easeOut(stateT / 20.0f);
    ui::panel(W / 2 - 360, 300, 720, 300 * k + 1, Color(24, 14, 48, 225));
    if (k > 0.9f) {
      textf(W / 2, 330, [] { TextStyle s = ui::style(96); s.shadow = 6; return s; }(), "WAVE %d", wave);
      textf(W / 2, 455, ui::style(48, Color(255, 216, 74)), "%d X POINTS", mult());
      if (blink) text("DEFEND THE CITIES", W / 2, 525, ui::style(40, Color(255, 90, 110)));
    }
  } else if (state == ST_BONUS) {
    ui::panel(W / 2 - 440, 250, 880, 420, Color(18, 14, 44, 230));
    textf(W / 2, 280, [] { TextStyle s = ui::style(84, Color(130, 255, 190)); s.shadow = 5; return s; }(), "WAVE %d CLEAR", wave);
    if (stateT > 60) {
      int a = std::min(bonusStep, bonusAmmo), c = std::max(0, bonusStep - bonusAmmo);
      for (int k = 0; k < a && k < 30; k++) draw(csart::IMG_AMMO, W / 2 - 390 + k * 19, 470);
      textf(W / 2 + 400, 425, ui::style(48, WHITE, RIGHT), "%d", a * 5 * mult());
      for (int k = 0; k < c; k++) { Fx f; f.sx = f.sy = 0.55f; draw(*csart::CITIES[k % 6], W / 2 - 350 + k * 84, 580, f); }
      textf(W / 2 + 400, 525, ui::style(48, WHITE, RIGHT), "%d", c * 100 * mult());
      if (bonusCities) textf(W / 2, 600, ui::style(36, Color(255, 216, 74)), "SPARE CITIES: %d", bonusCities);
    }
  } else if (state == ST_OVER) {
    ui::gameOver(std::max(0, stateT - 60), newHi, "THE END");
    if (stateT > 70) textf(W / 2, 500, ui::style(48, Color(200, 220, 255)), "REACHED WAVE %d", wave);
  }
  if (toastT && state != ST_TITLE) ui::banner("BONUS CITY!", W / 2, 180, toastT, 120, Color(140, 255, 170), 80);
}

static void draw() {
  drawSky();
  float ox = shakeX, oy = shakeY;
  drawGround(ox, oy);
  drawAction(ox, oy);
  if (flashFx > 0.01f) rect(0, 0, W, H, Color(255, 200, 160, (uint8_t)(flashFx * 140)), BLEND_ADD);
  if (state == ST_TITLE) {
    const char* help[2];
    char l1[64], l2[96];
    snprintf(l1, sizeof(l1), "STICK / ARROWS AIM THE CROSSHAIR");
    snprintf(l2, sizeof(l2), "%s LEFT BASE    %s CENTRE (FASTEST)    %s RIGHT BASE", btnName(BTN_X), btnName(BTN_A), btnName(BTN_B));
    help[0] = l1; help[1] = l2;
    ui::titleScreen(cart::IMG_LOGO_CITYSHIELD, "DEFEND THE SIX CITIES", hiscore, help, 2, Color(184, 220, 255));
    return;
  }
  drawHud();
  if (state == ST_PLAY || state == ST_INTRO) drawCrosshair();
  drawOverlays();
}

// ------------------------------------------------------------ bot: the original's autopilot, played through the pad
// It leads each threatening warhead (allowing for the crosshair's travel and the interceptor's flight),
// steers the crosshair there and fires from the base that gets there first.
static int botDeadline[40];
static uint32_t botLastFire = 0;

static float stopY(const Warhead& w) {
  if (w.target < 6) return FLAT_Y - 4;
  if (w.target < 9) return LAUNCH_Y + 2;
  return groundAt(w.x + w.vx * (210 - w.y) / fmaxf(0.05f, w.vy)) - 1;
}
static bool threatening(const Warhead& w) {
  if (w.mirv) return true;
  float t = (stopY(w) - w.y) / fmaxf(0.05f, w.vy), lx = w.x + w.vx * t;
  for (int i = 0; i < 6; i++) if (cityAlive[i] && fabsf(lx - cityCX(i)) < CITY_W / 2 + 6) return true;
  for (int i = 0; i < 3; i++) if (baseAlive[i] && fabsf(lx - BASE_X[i]) < 19) return true;
  return false;
}
// will this warhead fly into a fireball or a shot that is already on its way?
static bool covered(const Warhead& w) {
  for (auto& b : blasts) {
    if (!b.on || b.t >= 34) continue;
    for (int t = 0; t < 30 - b.t + 12 && t < 40; t += 2) {
      int bt = b.t + t;
      float r = bt < 16 ? b.maxR * bt / 16.0f : bt < 30 ? b.maxR : b.maxR * (46 - bt) / 16.0f;
      float dx = w.x + w.vx * t - b.x, dy = w.y + w.vy * t - b.y;
      if (dx * dx + dy * dy < (r - 2) * (r - 2)) return true;
    }
  }
  for (auto& s : shots) {
    if (!s.on) continue;
    float sp = sqrtf(s.vx * s.vx + s.vy * s.vy), d = sqrtf((s.tx - s.x) * (s.tx - s.x) + (s.ty - s.y) * (s.ty - s.y));
    float arrive = d / sp;
    for (int t = 4; t <= 22; t += 3) {
      float dx = w.x + w.vx * (arrive + t) - s.tx, dy = w.y + w.vy * (arrive + t) - s.ty;
      float r = std::min(19.0f, 19.0f * t / 16.0f) - 2;
      if (dx * dx + dy * dy < r * r) return true;
    }
  }
  return false;
}

static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY) return;
  float bestU = 1e9f, aimX = 160, aimY = 100;
  int aimW = -1, aimB = -1;
  for (int i = 0; i < 40; i++) {
    const Warhead& w = warheads[i];
    if (!w.on || (int)(botDeadline[i] - frameNo) > 0 || !threatening(w) || covered(w)) continue;
    float urgency = (stopY(w) - w.y) / fmaxf(0.05f, w.vy);
    for (int b = 0; b < 3; b++) {
      if (!baseAlive[b] || !ammo[b]) continue;
      float px = w.x, py = w.y, t = 0;
      for (int it = 0; it < 4; it++) {
        float travel = fmaxf(fabsf(px - crossX), fabsf(py - crossY)) / 3.6f;
        float fx = px - BASE_X[b], fy = py - (LAUNCH_Y - 8);
        t = travel + sqrtf(fx * fx + fy * fy) / SHOT_SPEED[b] + 7;
        px = w.x + w.vx * t; py = w.y + w.vy * t;
      }
      if (py > 186 || py < 16 || px < 4 || px > 316) continue;
      if (t > urgency) continue;
      float u = urgency + t * 0.3f;     // the most urgent threat, and the quickest base for it
      if (u < bestU) { bestU = u; aimX = px; aimY = py; aimW = i; aimB = b; }
    }
  }
  float dx = aimX - crossX, dy = aimY - crossY;
  p.ax = clampv(dx / 3.6f, -1.0f, 1.0f);
  p.ay = clampv(dy / 3.6f, -1.0f, 1.0f);
  if (aimW >= 0 && fabsf(dx) < 1.5f && fabsf(dy) < 1.5f && frameNo - botLastFire > 1) {
    static const uint32_t BTN[3] = {BTN_X, BTN_A, BTN_B};
    p.held |= BTN[aimB];
    botLastFire = frameNo;
    float fx = aimX - BASE_X[aimB], fy = aimY - (LAUNCH_Y - 8);
    int flight = (int)(sqrtf(fx * fx + fy * fy) / SHOT_SPEED[aimB]);
    botDeadline[aimW] = frameNo + flight + 30;
  }
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d wave %d score %lu cities %d bases %d%d%d ammo %d/%d/%d spare %d", state, wave, (unsigned long)score, citiesLeft(),
           baseAlive[0], baseAlive[1], baseAlive[2], ammo[0], ammo[1], ammo[2], bonusCities);
}

static void init() { loadAtlas(csart::TEX_FILES, csart::TEX, csart::NTEX); }
static void enter() {
  state = ST_TITLE; stateT = 0;
  for (auto& s : stars) { s.x = frand() * W; s.y = frange(0, 760); s.z = frange(0.2f, 1.0f); }
  for (bool& c : cityAlive) c = true;
  for (int i = 0; i < 3; i++) { baseAlive[i] = true; ammo[i] = 99; }
  memset(warheads, 0, sizeof(warheads)); memset(shots, 0, sizeof(shots)); memset(blasts, 0, sizeof(blasts)); memset(flyers, 0, sizeof(flyers));
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  toSpawn = 0; wave = 1;
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state == ST_PLAY || state == ST_INTRO || state == ST_BONUS; }

}  // namespace cs

extern const nova::Game CITY_SHIELD;
const nova::Game CITY_SHIELD = {
  "cityshield", "CITY SHIELD", "DEFEND THE SIX CITIES",
  cs::init, cs::enter, cs::step, cs::draw, cs::canPause, nullptr, cs::bot, cs::debugInfo,
};
