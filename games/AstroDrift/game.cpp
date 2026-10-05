// =====================================================================
//  ASTRO DRIFT HD  -  a space-rock shooter (Nova Arcade HD)
//  Turn, thrust and fire in a wrap-around field. Big rocks split into
//  smaller ones; from wave 2 a hunter saucer drops in to take shots at you.
//
//  The original's rules and handling, simulated in its units (scaled 4.5x)
//  on a field widened to 16:9. New for HD: shaded tumbling asteroids with
//  glowing rims, a neon ship with a flame, saucer lights, debris, shock
//  rings, and a nebula with parallax stars.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include <stdio.h>
#include <algorithm>

using namespace nova;
using namespace nova::audio;

namespace ad {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  92, 4, "Am F G E",
  {I_FLUTE, 0.8f, 0.1f, "A4 - - - C5 - E5 - A5 - - - G5 - E5 - | F5 - - - E5 - C5 - A4 - - - C5 - - - | D5 - - - G5 - - - B5 - A5 - G5 - - - | Ab5 - - - E5 - - - B4 - - - . . . ."},
  {}, I_BELL, "1...5...3...8...", 0.22f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.5f,
  "K.......K.......", 0.6f, 0, true};
static const Song SONG_PLAY = {   // the original's play music is a pulsing drive with no tune; HD adds a slow pad and arp
  84, 4, "Am Am F E",
  {}, {}, I_BELL, "1.......5.......|3.......8.......|1.......5.......|3.......7.......", 0.16f,
  I_PLUCKBASS, "R.RRR.RRR.RRR.RR", 0.55f, I_CHOIR, 0.3f,
  "K...h...K...h.h.", 0.75f, 0, true};
static const Song SONG_OVER = {
  90, 2, "Am Am", {I_FLUTE, 0.8f, 0, "E5 - C5 - A4 - Ab4 - A4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_CLEAR = {
  130, 2, "A A", {I_MARIMBA, 0.8f, 0, "A5 C#6 E6 A6 - - G6 - A6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ world (original units, field widened to 16:9)
static const float K = 4.5f;
static const float FW = W / K, FY = 14, FH = 240 - 14;   // playfield wraps both ways
static inline float X(float x) { return x * K; }
static inline float Y(float y) { return y * K; }
static const int NV = 12;
static const float ROCK_R[3] = {21, 12, 6};
static const int ROCK_PTS[3] = {20, 50, 100};
struct Rock { bool on; float x, y, vx, vy, a, va; uint8_t size; int8_t shape[NV]; uint8_t flash; uint8_t tone; };
static Rock rocks[56];
struct Shot { bool on; float x, y, vx, vy; int16_t life; };
static Shot pshots[6], eshots[6];
struct Ship { float x, y, vx, vy, a; int invuln, fireCd; bool alive, thrust; };
static Ship ship;
struct Saucer { bool on; float x, y, vx, baseY, t; int fireT; bool small; };
static Saucer ufo;
static int ufoTimer = 900;
static int lives = 3, wave = 1;
static uint32_t score = 0, hiscore = 0, nextExtra = 10000;
static const uint32_t HI_DEFAULT = 8000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

// effects (original units for positions)
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; uint8_t kind; float rot, vr; };   // kind 0 glow dot, 1 spark, 2 debris
static Part parts[600];
struct Ring { bool on; float x, y, t, size; Color c; };
static Ring rings[16];
struct Popup { bool on; float x, y; int t; char txt[12]; };
static Popup popups[6];
static float shake = 0, shakeX = 0, shakeY = 0, flash = 0;
struct Star { float x, y, z; };
static Star stars[220];

static inline void wrap(float& x, float& y) {
  if (x < 0) x += FW; else if (x >= FW) x -= FW;
  if (y < FY) y += FH; else if (y >= FY + FH) y -= FH;
}
static inline float wdx(float a, float b) { float d = b - a; if (d > FW / 2) d -= FW; if (d < -FW / 2) d += FW; return d; }
static inline float wdy(float a, float b) { float d = b - a; if (d > FH / 2) d -= FH; if (d < -FH / 2) d += FH; return d; }
static inline bool hitCircle(float ax, float ay, float bx, float by, float r) {
  float dx = wdx(ax, bx), dy = wdy(ay, by);
  return dx * dx + dy * dy < r * r;
}

static void addScore(uint32_t v) {
  score += v;
  if (score >= nextExtra) { nextExtra += 10000; lives = std::min(lives + 1, 9); sfx(SFX_ONEUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, uint8_t kind = 0) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.2f, 1) * sp;
        p = {true, x, y, cosf(a) * s, sinf(a) * s, frange(16, 40), 40, frange(3, 8), col, kind, frand() * 360, frange(-9, 9)};
        break;
      }
}
static void ring(float x, float y, float size, Color c) {
  for (auto& r : rings) if (!r.on) { r = {true, x, y, 0, size, c}; return; }
}
static void popup(float x, float y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 50, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

static void spawnRock(float x, float y, uint8_t size, float dirA = -1) {
  for (auto& r : rocks) {
    if (r.on) continue;
    float a = dirA < 0 ? frand() * 6.283f : dirA;
    float sp = frange(0.45f, 1.0f) * (1.0f + size * 0.45f) * (1.0f + std::min(wave - 1, 8) * 0.07f) * speed();
    r = {true, x, y, cosf(a) * sp, sinf(a) * sp, frand() * 6.283f, frange(-0.03f, 0.03f), size, {0}, 0, (uint8_t)(rnd() % 3)};
    for (auto& s : r.shape) s = (int8_t)frange(-26, 12);   // % of radius
    return;
  }
}

static void startWave() {
  for (auto& r : rocks) r.on = false;
  int n = std::min(4 + wave, 12);   // one more than the original: the field is wider
  for (int i = 0; i < n; i++) {
    float x, y;
    do { x = frand() * FW; y = FY + frand() * FH; } while (hitCircle(x, y, ship.x, ship.y, 80));
    spawnRock(x, y, 0);
  }
  ufo.on = false;
  ufoTimer = frames(900 + rnd() % 600);
  state = ST_PLAY; stateT = 0;
}
static void resetShip() {
  ship = Ship();
  ship.x = FW / 2; ship.y = FY + FH / 2; ship.a = -1.5708f;
  ship.alive = true; ship.invuln = 150;
}
static void resetGame() {
  score = 0; lives = 3; wave = 1; nextExtra = 10000; newHi = false;
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  for (auto& s : pshots) s.on = false;
  for (auto& s : eshots) s.on = false;
  resetShip();
  startWave();
  music(&SONG_PLAY);
}
static void gameOver() {
  state = ST_OVER; stateT = 0;
  music(&SONG_OVER);
  if (newHi) saveHi(hiscore);
}
static void killShip() {
  ship.alive = false;
  burst(ship.x, ship.y, 50, Color(255, 200, 120), 3.0f, 1);
  burst(ship.x, ship.y, 24, Color(100, 220, 255), 2.0f);
  ring(ship.x, ship.y, 1.4f, Color(140, 220, 255));
  shake = 22; flash = 0.35f;
  sfx(SFX_DIE);
  rumble(1.0f, 500);
  lives--;
  if (lives <= 0) gameOver(); else { state = ST_DEAD; stateT = 0; }
}
static const Color ROCK_RIM[3] = {Color(230, 190, 150), Color(255, 170, 110), Color(255, 220, 90)};
static void breakRock(Rock& r) {
  r.on = false;
  uint32_t v = ROCK_PTS[r.size];
  addScore(v);
  burst(r.x, r.y, 8 + (2 - r.size) * 8, ROCK_RIM[r.size], 1.5f + (2 - r.size) * 0.4f, 1);
  burst(r.x, r.y, 4 + (2 - r.size) * 5, Color(120, 100, 100), 1.2f, 2);
  ring(r.x, r.y, 0.4f + (2 - r.size) * 0.4f, ROCK_RIM[r.size]);
  sfx(r.size == 0 ? SFX_KNOCK : SFX_STOMP, clampv((r.x - FW / 2) / (FW / 2), -1.0f, 1.0f) * 0.6f, r.size == 0 ? 0.6f : 0.8f + 0.3f * r.size);
  if (r.size == 0) { shake = fmaxf(shake, 9.0f); rumble(0.4f, 120); }
  if (r.size < 2) {
    float x = r.x, y = r.y, base = atan2f(r.vy, r.vx);   // copy first: the first spawn can reuse r's own slot
    uint8_t size = r.size + 1;
    spawnRock(x, y, size, base + frange(0.4f, 1.0f));
    spawnRock(x, y, size, base - frange(0.4f, 1.0f));
  }
}
static void fireShot(Shot* pool, int n, float x, float y, float a, float sp, float bvx, float bvy, int life) {
  for (int i = 0; i < n; i++)
    if (!pool[i].on) { pool[i] = {true, x, y, cosf(a) * sp + bvx, sinf(a) * sp + bvy, (int16_t)life}; return; }
}

static void updateShip(const Pad& in) {
  if (!ship.alive) return;
  float turn = in.ax;
  if (in.down(BTN_LEFT)) turn = -1;
  if (in.down(BTN_RIGHT)) turn = 1;
  ship.a += turn * 0.085f;
  ship.thrust = in.down(BTN_UP | BTN_B | BTN_L);
  if (ship.thrust) {
    ship.vx += cosf(ship.a) * 0.09f;
    ship.vy += sinf(ship.a) * 0.09f;
    for (auto& p : parts)
      if (!p.on) {
        float a = ship.a + 3.14159f + frange(-0.3f, 0.3f);
        p = {true, ship.x - cosf(ship.a) * 6, ship.y - sinf(ship.a) * 6, cosf(a) * 1.6f + ship.vx, sinf(a) * 1.6f + ship.vy, 16, 16, 6,
             Color(255, 150, 60), 0, 0, 0};
        break;
      }
  }
  float v = sqrtf(ship.vx * ship.vx + ship.vy * ship.vy);
  if (v > 4.0f) { ship.vx *= 4.0f / v; ship.vy *= 4.0f / v; }
  ship.vx *= 0.992f; ship.vy *= 0.992f;
  ship.x += ship.vx; ship.y += ship.vy;
  wrap(ship.x, ship.y);
  if (ship.invuln) ship.invuln--;
  if (ship.fireCd) ship.fireCd--;
  if (in.hit(BTN_A | BTN_R) && ship.fireCd == 0) {
    fireShot(pshots, 6, ship.x + cosf(ship.a) * 8, ship.y + sinf(ship.a) * 8, ship.a, 5.0f, ship.vx, ship.vy, 48);
    ship.fireCd = 5;
    sfx(SFX_BUMP, 0, 2.4f);
  }
  if (in.hit(BTN_X | BTN_Y)) {   // hyperspace: a risky jump to a random spot
    burst(ship.x, ship.y, 18, Color(180, 240, 255), 1.5f, 1);
    ring(ship.x, ship.y, 0.6f, Color(180, 240, 255));
    ship.x = frand() * FW; ship.y = FY + frand() * FH; ship.vx = ship.vy = 0;
    burst(ship.x, ship.y, 18, Color(180, 240, 255), 1.5f, 1);
    ring(ship.x, ship.y, 0.6f, Color(180, 240, 255));
    sfx(SFX_ROLL);
  }
}

static void updateUfo() {
  if (!ufo.on) {
    if (wave >= 2 && --ufoTimer <= 0) {
      ufo.on = true;
      ufo.small = wave >= 4 && (rnd() % 2);
      int dirn = rnd() % 2 ? 1 : -1;
      ufo.x = dirn > 0 ? -16 : FW + 16;
      ufo.baseY = FY + 30 + frand() * (FH - 60);
      ufo.vx = dirn * (ufo.small ? 1.3f : 0.9f) * speed();
      ufo.t = 0; ufo.fireT = frames(60);
    }
    return;
  }
  ufo.t += 1;
  ufo.x += ufo.vx;
  ufo.y = ufo.baseY + 24 * sinf(ufo.t * 0.03f);
  if (--ufo.fireT <= 0 && ship.alive) {
    ufo.fireT = frames(ufo.small ? 55 : 80);
    float a = ufo.small ? atan2f(wdy(ufo.y, ship.y), wdx(ufo.x, ship.x)) + frange(-0.15f, 0.15f) : frand() * 6.283f;
    fireShot(eshots, 6, ufo.x, ufo.y, a, 2.6f * speed(), 0, 0, 90);
    sfx(SFX_BUMP, 0, 1.6f);
  }
  if (ufo.x < -24 || ufo.x > FW + 24) { ufo.on = false; ufoTimer = frames(800 + rnd() % 700); }
}
static void killUfo() {
  uint32_t v = ufo.small ? 1000 : 200;
  addScore(v);
  popup(ufo.x, ufo.y - 12, v);
  burst(ufo.x, ufo.y, 40, Color(255, 120, 220), 2.5f, 1);
  ring(ufo.x, ufo.y, 1.2f, Color(255, 120, 220));
  sfx(SFX_KNOCK, 0, 0.5f);
  ufo.on = false; ufoTimer = frames(900 + rnd() % 700);
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  updateShip(in);
  updateUfo();
  int alive = 0;
  for (auto& r : rocks) {
    if (!r.on) continue;
    alive++;
    r.x += r.vx; r.y += r.vy; r.a += r.va;
    wrap(r.x, r.y);
    if (r.flash) r.flash--;
    if (ship.alive && !ship.invuln && hitCircle(r.x, r.y, ship.x, ship.y, ROCK_R[r.size] * 0.9f + 5)) { breakRock(r); killShip(); }
  }
  for (auto& s : pshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy; wrap(s.x, s.y);
    if (--s.life <= 0) { s.on = false; continue; }
    for (auto& r : rocks)
      if (r.on && hitCircle(r.x, r.y, s.x, s.y, ROCK_R[r.size] + 1)) { s.on = false; breakRock(r); break; }
    if (s.on && ufo.on && hitCircle(ufo.x, ufo.y, s.x, s.y, ufo.small ? 7 : 11)) { s.on = false; killUfo(); }
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy; wrap(s.x, s.y);
    if (--s.life <= 0) { s.on = false; continue; }
    if (ship.alive && !ship.invuln && hitCircle(ship.x, ship.y, s.x, s.y, 6)) { s.on = false; killShip(); }
  }
  if (ufo.on && ship.alive && !ship.invuln && hitCircle(ufo.x, ufo.y, ship.x, ship.y, 14)) { killUfo(); killShip(); }
  if (state == ST_PLAY && alive == 0 && !ufo.on) {
    state = ST_CLEAR; stateT = 0;
    addScore(250 * wave);
    music(&SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vx *= 0.96f; p.vy *= 0.96f; p.rot += p.vr;
    if (--p.life <= 0) p.on = false;
  }
  for (auto& r : rings) if (r.on && (r.t += 0.035f) >= 1) r.on = false;
  for (auto& p : popups) if (p.on) { p.y -= 0.3f; if (--p.t <= 0) p.on = false; }
  float vx = ship.alive ? -ship.vx : 0, vy = ship.alive ? -ship.vy : 0;
  for (auto& s : stars) {
    s.x += vx * s.z * 0.5f * K + 0.15f; s.y += vy * s.z * 0.5f * K;
    if (s.x < 0) s.x += W;
    if (s.x >= W) s.x -= W;
    if (s.y < 0) s.y += H;
    if (s.y >= H) s.y -= H;
  }
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.86f; }
  else { shake = 0; shakeX = shakeY = 0; }
  flash *= 0.9f;
}

static void driftRocks() { for (auto& r : rocks) if (r.on) { r.x += r.vx; r.y += r.vy; r.a += r.va; wrap(r.x, r.y); } }

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      driftRocks();
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DEAD: {   // keep the world moving; respawn once the centre is clear
      updatePlay(in);
      bool clear = true;
      for (auto& r : rocks) if (r.on && hitCircle(r.x, r.y, FW / 2, FY + FH / 2, ROCK_R[r.size] + 40)) clear = false;
      if (state == ST_DEAD && stateT > 90 && (clear || stateT > 400)) { resetShip(); state = ST_PLAY; }
      break;
    }
    case ST_CLEAR:
      updateShip(in);
      for (auto& s : pshots) if (s.on) { s.x += s.vx; s.y += s.vy; wrap(s.x, s.y); if (--s.life <= 0) s.on = false; }
      if (stateT > 150) { wave++; startWave(); music(&SONG_PLAY); }
      break;
    case ST_OVER:
      driftRocks();
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static void drawBackground() {
  rectGrad(0, 0, W, H, Color(4, 4, 16), Color(10, 8, 28));
  glow(W * 0.25f, H * 0.35f, 800, Color(90, 40, 160, 50));
  glow(W * 0.7f, H * 0.7f, 700, Color(30, 90, 160, 45));
  glow(W * 0.85f, H * 0.2f, 400, Color(200, 60, 120, 30));
  for (auto& s : stars) {
    float tw = 0.65f + 0.35f * sinf(frameNo * 0.05f + s.x);
    disc(s.x, s.y, 1 + 2 * s.z, Color(200, 210, 255, (uint8_t)(255 * s.z * tw)));
  }
}

// draw at (x, y) and at the wrapped copies when near an edge
template <typename F>
static void drawWrapped(float x, float y, float r, F fn) {
  float xs[2] = {0, 0}, ys[2] = {0, 0};
  int nx = 1, ny = 1;
  if (x - r < 0) xs[nx++] = FW; else if (x + r >= FW) xs[nx++] = -FW;
  if (y - r < FY) ys[ny++] = FH; else if (y + r >= FY + FH) ys[ny++] = -FH;
  for (int i = 0; i < nx; i++)
    for (int j = 0; j < ny; j++) fn(X(x + xs[i]) + shakeX, Y(y + ys[j]) + shakeY);
}

static const Color ROCK_BODY[3] = {Color(96, 76, 82), Color(110, 84, 86), Color(82, 74, 96)};
static void drawRock(const Rock& r) {
  float R = ROCK_R[r.size] * K;
  drawWrapped(r.x, r.y, ROCK_R[r.size] + 2, [&](float cx, float cy) {
    float px[NV], py[NV];
    for (int i = 0; i < NV; i++) {
      float a = r.a + i * 6.2832f / NV, rr = R * (1.0f + r.shape[i] / 100.0f);
      px[i] = cx + cosf(a) * rr; py[i] = cy + sinf(a) * rr;
    }
    Color body = ROCK_BODY[r.tone], lit = mix(body, Color(255, 230, 210), 0.35f), dark = mix(body, Color(10, 6, 20), 0.55f);
    Vtx v[NV * 3];
    int k = 0;
    for (int i = 0; i < NV; i++) {   // fan, lit from the top left
      int j = (i + 1) % NV;
      auto shadeAt = [&](float x, float y) { float d = ((x - cx) * -0.7f + (y - cy) * -0.7f) / R; return d > 0 ? mix(body, lit, d) : mix(body, dark, -d); };
      v[k++] = {cx - R * 0.2f, cy - R * 0.2f, lit};
      v[k++] = {px[i], py[i], shadeAt(px[i], py[i])};
      v[k++] = {px[j], py[j], shadeAt(px[j], py[j])};
    }
    tris(v, k);
    for (int c = 0; c < 2 + (2 - r.size); c++) {   // craters
      float a = r.a * 1.0f + c * 2.1f + r.tone, d = R * (0.25f + 0.2f * c);
      disc(cx + cosf(a) * d, cy + sinf(a) * d, R * (0.16f - 0.03f * c), dark.alpha(0.8f));
    }
    Color rim = r.flash ? WHITE : ROCK_RIM[r.size];
    for (int i = 0; i < NV; i++) line(px[i], py[i], px[(i + 1) % NV], py[(i + 1) % NV], 4, rim);
    glow(cx, cy, R * 1.5f, rim.alpha(0.08f));
  });
}

static void drawShip() {
  if (!ship.alive || (ship.invuln && (ship.invuln & 4))) return;
  float ca = cosf(ship.a), sa = sinf(ship.a);
  drawWrapped(ship.x, ship.y, 12, [&](float cx, float cy) {
    auto P = [&](float fx, float fy, float& ox, float& oy) { ox = cx + (fx * ca - fy * sa) * K; oy = cy + (fx * sa + fy * ca) * K; };
    float nx, ny, lx, ly, rx, ry, bx, by, wx, wy;
    P(9, 0, nx, ny); P(-6, -6, lx, ly); P(-6, 6, rx, ry); P(-3, 0, bx, by); P(2, 0, wx, wy);
    if (ship.thrust) {
      float fl = 0.8f + 0.4f * frand();
      float fx, fy, f1x, f1y, f2x, f2y;
      P(-10 - 5 * fl, 0, fx, fy); P(-5, -3, f1x, f1y); P(-5, 3, f2x, f2y);
      Vtx fv[3] = {{f1x, f1y, Color(255, 220, 90, 230)}, {f2x, f2y, Color(255, 220, 90, 230)}, {fx, fy, Color(255, 80, 40, 0)}};
      tris(fv, 3, BLEND_ADD);
      glow(f1x / 2 + f2x / 2, f1y / 2 + f2y / 2, 50, Color(255, 140, 50, 120));
    }
    glow(cx, cy, 70, Color(80, 200, 255, 50));
    Vtx hull[6] = {{nx, ny, Color(200, 240, 255)}, {lx, ly, Color(70, 110, 170)}, {bx, by, Color(110, 160, 220)},
                   {nx, ny, Color(200, 240, 255)}, {bx, by, Color(110, 160, 220)}, {rx, ry, Color(50, 80, 140)}};
    tris(hull, 6);
    Color edge(120, 230, 255);
    line(nx, ny, lx, ly, 4, edge); line(nx, ny, rx, ry, 4, edge);
    line(lx, ly, bx, by, 3, edge); line(rx, ry, bx, by, 3, edge);
    disc(wx, wy, 6, Color(255, 120, 220));
    disc(wx - 1, wy - 2, 2.5f, WHITE);
  });
}

static void drawUfo() {
  if (!ufo.on) return;
  float w = (ufo.small ? 9 : 14) * K, x = X(ufo.x) + shakeX, y = Y(ufo.y) + shakeY;
  Color body = ufo.small ? Color(255, 120, 220) : Color(190, 70, 230);
  glow(x, y, w * 1.6f, body.alpha(0.3f));
  disc(x, y - w * 0.2f, w * 0.45f, Color(180, 240, 255, 220));
  disc(x - w * 0.12f, y - w * 0.32f, w * 0.15f, Color(255, 255, 255, 200));
  roundRect(x - w, y - w * 0.12f, 2 * w, w * 0.4f, w * 0.2f, body);
  roundRect(x - w * 0.8f, y + w * 0.2f, 1.6f * w, w * 0.2f, w * 0.1f, mix(body, BLACK, 0.5f));
  for (int i = -2; i <= 2; i++) {
    bool on = ((frameNo >> 3) + i) & 1;
    disc(x + i * w * 0.38f, y + w * 0.08f, w * 0.07f, on ? Color(255, 230, 100) : WHITE);
    if (on) glow(x + i * w * 0.38f, y + w * 0.08f, w * 0.25f, Color(255, 220, 90, 110));
  }
}

static void drawShots() {
  for (auto& s : pshots) {
    if (!s.on) continue;
    Fx f; f.rot = atan2f(s.vy, s.vx) * 57.3f; f.tint = Color(160, 240, 255); f.blend = BLEND_ADD; f.sx = 1.3f;
    draw(cart::IMG_STREAK, X(s.x) + shakeX, Y(s.y) + shakeY, f);
    glow(X(s.x) + shakeX, Y(s.y) + shakeY, 24, Color(140, 220, 255, 140));
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    Color c = (frameNo & 4) ? Color(255, 120, 220) : Color(255, 220, 90);
    glow(X(s.x) + shakeX, Y(s.y) + shakeY, 30, c.alpha(0.6f));
    disc(X(s.x) + shakeX, Y(s.y) + shakeY, 7, c);
  }
}

static void drawParticles() {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max, x = X(p.x) + shakeX, y = Y(p.y) + shakeY;
    Color c = mix(p.col, WHITE, t * 0.4f).alpha(fminf(1, t * 1.6f));
    if (p.kind == 1) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 12; f.rot = p.rot; draw(cart::IMG_SPARK, x, y, f); }
    else if (p.kind == 2) {   // a tumbling shard of rock
      float a = p.rot * 0.0174f, s = p.size * 1.4f;
      Vtx v[3] = {{x + cosf(a) * s, y + sinf(a) * s, c}, {x + cosf(a + 2.3f) * s, y + sinf(a + 2.3f) * s, c}, {x + cosf(a + 4.2f) * s * 0.7f, y + sinf(a + 4.2f) * s * 0.7f, c}};
      tris(v, 3);
    } else glow(x, y, p.size * 3, c);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = r.size * (0.3f + r.t * 2.4f); f.tint = r.c.alpha((1 - r.t) * 0.9f); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, X(r.x) + shakeX, Y(r.y) + shakeY, f);
  }
  for (auto& p : popups)
    if (p.on) text(p.txt, X(p.x), Y(p.y), ui::style(40, Color(255, 220, 110).alpha(clampv(p.t / 15.0f, 0.0f, 1.0f))));
}

static void drawHud() {
  rectGrad(0, 0, W, 90, Color(4, 4, 16, 220), Color(4, 4, 16, 0));
  TextStyle st = ui::style(44, WHITE, LEFT);
  textf(40, 20, st, "%07lu", (unsigned long)score);
  TextStyle wv = ui::style(44, Color(150, 230, 255));
  textf(W / 2, 20, wv, "WAVE %d", wave);
  TextStyle hi = ui::style(32, Color(255, 220, 110), RIGHT);
  textf(W - 40, 28, hi, "HI %07lu", (unsigned long)hiscore);
  for (int i = 0; i < std::min(lives - 1, 6); i++) {   // little ships for spare lives
    float x = 440 + i * 46, y = 44;
    Vtx v[3] = {{x, y - 18, Color(180, 240, 255)}, {x - 14, y + 16, Color(70, 140, 220)}, {x + 14, y + 16, Color(70, 140, 220)}};
    tris(v, 3);
  }
}

static void draw() {
  drawBackground();
  for (auto& r : rocks) if (r.on) drawRock(r);
  if (state == ST_TITLE) {
    static const char* HELP[] = {"LEFT / RIGHT TURN    UP OR B THRUST", "A FIRE    X HYPERSPACE"};
    ui::titleScreen(cart::IMG_LOGO_ASTRODRIFT, "SPLIT THE ROCKS, DODGE THE HUNTERS", hiscore, HELP, 2, Color(180, 210, 255));
    return;
  }
  drawUfo();
  drawShip();
  drawShots();
  drawParticles();
  drawHud();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 240, 220, (uint8_t)(flash * 255)), BLEND_ADD);
  if (state == ST_PLAY && stateT < 90) { char s[24]; snprintf(s, sizeof(s), "WAVE %d", wave); ui::banner(s, W / 2, 420, 90 - stateT, 90, WHITE, 100); }
  if (state == ST_CLEAR) {
    ui::banner("WAVE CLEARED!", W / 2, 420, 150 - stateT + 30, 150, Color(180, 255, 120), 110);
    if (stateT > 20) textf(W / 2, 560, ui::style(52), "BONUS %d", 250 * wave);
  }
  if (state == ST_OVER) ui::gameOver(stateT, newHi);
}

// ------------------------------------------------------------ bot: turn toward the nearest threat and shoot; thrust away if close
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (!ship.alive) return;
  float best = 1e9f, tx = 0, ty = 0;
  for (auto& r : rocks) {
    if (!r.on) continue;
    float dx = wdx(ship.x, r.x + r.vx * 8), dy = wdy(ship.y, r.y + r.vy * 8), d = dx * dx + dy * dy;
    if (d < best) { best = d; tx = dx; ty = dy; }
  }
  if (ufo.on) {
    float dx = wdx(ship.x, ufo.x), dy = wdy(ship.y, ufo.y), d = dx * dx + dy * dy;
    if (d < best * 1.5f) { best = d; tx = dx; ty = dy; }
  }
  if (best > 1e8f) return;
  float want = atan2f(ty, tx), diff = want - ship.a;
  while (diff > 3.14159f) diff -= 6.28318f;
  while (diff < -3.14159f) diff += 6.28318f;
  if (diff > 0.08f) p.held |= BTN_RIGHT; else if (diff < -0.08f) p.held |= BTN_LEFT;
  if (fabsf(diff) < 0.25f && (frameNo & 3) == 0) p.held |= BTN_A;
  float spd = sqrtf(ship.vx * ship.vx + ship.vy * ship.vy);
  if (best > 70 * 70 && spd < 0.6f && fabsf(diff) < 0.4f && (frameNo % 90) < 12) p.held |= BTN_UP;   // drift a little closer
  if (best < 26 * 26 && fabsf(diff) > 2.5f) p.held |= BTN_UP;   // something behind: run
}

static void debugInfo(char* buf, int n) {
  int rocksLeft = 0;
  for (auto& r : rocks) rocksLeft += r.on;
  snprintf(buf, n, "state %d wave %d score %lu lives %d rocks %d", state, wave, (unsigned long)score, lives, rocksLeft);
}

static void enter() {
  state = ST_TITLE; stateT = 0;
  hiscore = loadHi(HI_DEFAULT);
  for (auto& s : stars) s = {frand() * W, frand() * H, frange(0.15f, 1.0f)};
  for (auto& r : rocks) r.on = false;
  ship.x = -100; ship.y = -100; ship.alive = false;
  for (int i = 0; i < 7; i++) spawnRock(frand() * FW, FY + frand() * FH, i % 3);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace ad

extern const nova::Game ASTRO_DRIFT;
const nova::Game ASTRO_DRIFT = {
  "astrodrift", "ASTRO DRIFT", "SPLIT THE ROCKS, DODGE THE HUNTERS",
  nullptr, ad::enter, ad::step, ad::draw, ad::canPause, nullptr, ad::bot, ad::debugInfo,
};
