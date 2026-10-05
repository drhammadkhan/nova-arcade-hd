// =====================================================================
//  VOLT RALLY HD  -  paddle tennis against a ladder of CPU rivals (Nova Arcade HD)
//  First to 5 wins the match. Hold A as the ball meets your paddle for a
//  power smash. Every rival returns faster and reads the ball better.
//
//  The original's rules and timing on its 320 x 240 court (drawn 4.5x,
//  centred). New for HD: chrome volt paddles with glowing cores
//  (tools/make_art.py), a plasma ball with a light trail, lightning arcs
//  on smashes and wall hits, scorch marks, speed lines, energy goal
//  curtains, a rival ladder with badges, and a camera that leans with play.
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

namespace vr {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  116, 4, "Am F G E",
  {I_BELL, 0.75f, 0.1f, "A5 - - - - - C6 - B5 - - - G5 - - - | . . . . . . . . . . . . . . . . | A5 - - - - - C6 - B5 - - - G5 - - - | . . . . . . . . . . . . . . . ."},
  {}, I_MARIMBA, "1.3.5.8.5.3.1.3.", 0.25f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.45f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  150, 8, "Am F G E Am F G Am",
  {I_SAWLEAD, 0.55f, 0.05f,
   "A4 - C5 E5 A5 - E5 C5 G5 - E5 - C5 - D5 - | F5 - C5 F5 A5 - F5 C5 E5 - C5 - A4 - C5 - |"
   "G5 - D5 G5 B5 - G5 D5 A5 - G5 - D5 - - - | E5 - Ab5 - B5 - E6 - D6 - B5 - Ab5 - - - |"
   "A4 - C5 E5 A5 - E5 C5 G5 - E5 - C5 - D5 - | F5 - C5 F5 A5 - F5 C5 E5 - C5 - A4 - C5 - |"
   "G5 - D5 G5 B5 - G5 D5 A5 - G5 - D5 - - - | . . . . . . . . . . . . . . . ."},
  {}, I_SQUARE, "1.5.3.5.8.5.3.5.", 0.14f, I_PLUCKBASS, "R.RR..R.R.RR..5.", 0.6f, I_STRINGS, 0.2f,
  "K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.h.K.h.S.h.KKS.|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.hhS.hhK.hhS.hS|K.h.K.h.S.h.KKS.",
  0.85f, 0, true};
static const Song SONG_OVER = {
  96, 2, "Am Am", {I_FLUTE, 0.8f, 0, "E5 - D5 - C5 - B4 - A4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_WIN = {
  140, 2, "A A", {I_MARIMBA, 0.8f, 0, "A5 C6 E6 A6 - - E6 - A6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {I_BELL, 0.4f, 0.3f, ". . . . A6 - - - . . . . E6 - - - | . . . . . . . . . . . . . . . ."},
  I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ the court (original units)
static const int SW_ = 320;
static const int CT = 30, CB = 232;          // court top / bottom (inside the walls)
static const int PW = 6, PH = 34;            // paddle size
static const int PLX = 14, CPX = SW_ - 14 - PW;
static const int BS = 6;                     // ball size
static const int WIN_POINTS = 5;
static const int NRIVALS = 6;
static const char* const RIVALS[NRIVALS] = {"SPARKY", "FLUX", "SURGE", "ARCLIGHT", "DYNAMO", "OVERLOAD"};
static const Color RIVCOL[NRIVALS] = {Color(255, 216, 74), Color(62, 198, 224), Color(79, 214, 107), Color(255, 123, 213), Color(255, 138, 61), Color(235, 60, 80)};
static const Color PCOL(90, 150, 255), HOT(255, 216, 74), VOLT(184, 243, 255);

// camera: the court leans gently towards the ball and punches in on smashes
static const float K = 4.5f;
static float camX = 0, camY = 0, zoom = 1, zoomKick = 0, shake = 0, shakeX = 0, shakeY = 0;
static inline float X(float x) { return W / 2 + (x - 160) * K * zoom + camX + shakeX; }
static inline float Y(float y) { return H / 2 + (y - 120) * K * zoom + camY + shakeY; }
static inline float S(float v) { return v * K * zoom; }

static float py = 0, cy = 0, cpuV = 0, pyV = 0;
static float bx = 0, by = 0, bvx = 0, bvy = 0, bspd = 0;
static bool smash = false;
static int ptsP = 0, ptsC = 0, rival = 0, rally = 0, serveDir = 1, bestRally = 0;
static float cpuErr = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 5000;
static bool newHi = false;
enum State { ST_TITLE, ST_SERVE, ST_PLAY, ST_POINT, ST_WON, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0, lastWinner = 0;
static float flashP = 0, flashC = 0, goalFlash[2] = {0, 0}, charge = 0;
static uint32_t heldNow = 0;

// effects (original units unless noted)
static float trailX[14], trailY[14];
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; bool spark; };
static Part parts[400];
struct Ring { bool on; float x, y, t, size; Color c; };
static Ring rings[12];
struct Zap { bool on; float x, y; int t; Color c; };          // lightning crackle where the ball hits a wall
static Zap zaps[8];
struct Mark { bool on; float x, y; int t; };                  // scorch marks on the walls
static Mark marks[24];
struct Link { int t; float x0, y0; bool player; };            // the arc from the paddle to a smashed ball
static Link arcLink = {0, 0, 0, false};
struct Streak { float x, y, len, a; };                        // speed lines
static Streak streaks[30];
static ui::Stars stars;

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, float dirx = 0, bool spark = true) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.3f, 1.0f) * sp;
        p = {true, x, y, cosf(a) * s + dirx, sinf(a) * s, frange(14, 32), 32, frange(3, 8), col, spark && (i % 3 == 0)};
        break;
      }
}
static void addRing(float x, float y, Color c, float size = 1) {
  for (auto& r : rings) if (!r.on) { r = {true, x, y, 0, size, c}; return; }
}
static void wallHit(float x, float y) {
  for (auto& z : zaps) if (!z.on) { z = {true, x, y, 14, smash ? HOT : Color(200, 140, 255)}; break; }
  Mark* m = &marks[0];
  for (auto& k : marks) { if (!k.on) { m = &k; break; } if (k.t < m->t) m = &k; }
  *m = {true, x, y, 150};
  burst(x, y, smash ? 10 : 5, smash ? HOT : Color(200, 150, 255), 1.4f, 0);
}
static float pan() { return clampv((bx - 160) / 160.0f, -1.0f, 1.0f); }

// rival stats scale with the ladder position and the difficulty
static float cpuMaxSpeed() { return (2.0f + 0.35f * std::min(rival, 8)) * speed(); }
static int cpuReactX() { return std::max(90, 200 - rival * 20); }
static float baseBallSpeed() { return (2.8f + 0.15f * std::min(rival, 8)) * speed(); }
static float newErr() { return frange(-1, 1) * std::max(2, 16 - rival * 2); }

static void serve() {
  bx = SW_ / 2 - BS / 2; by = (CT + CB) / 2 - BS / 2;
  bspd = baseBallSpeed();
  float a = frange(-0.5f, 0.5f);
  bvx = cosf(a) * bspd * serveDir; bvy = sinf(a) * bspd;
  smash = false; rally = 0;
  cpuErr = newErr();
  for (int i = 0; i < 14; i++) { trailX[i] = bx; trailY[i] = by; }
  state = ST_PLAY; stateT = 0;
  sfx(SFX_SELECT, 0, 1);
  addRing(bx + BS / 2, by + BS / 2, VOLT, 0.6f);
  burst(bx + BS / 2, by + BS / 2, 12, VOLT, 1.5f);
}

static void startMatch() {
  ptsP = ptsC = 0;
  py = cy = (CT + CB) / 2 - PH / 2;
  serveDir = -1;
  state = ST_SERVE; stateT = 0;
  music(&SONG_GAME);
}

static void resetGame() {
  score = 0; rival = 0; newHi = false; bestRally = 0;
  for (auto& p : parts) p.on = false;
  for (auto& m : marks) m.on = false;
  startMatch();
}

static void pointTo(int who) {   // 1 player, 2 cpu
  lastWinner = who;
  shake = 26;
  zoomKick = 0.03f;
  float gx = who == 1 ? SW_ - 4 : 4;
  Color rc = RIVCOL[rival % NRIVALS];
  burst(gx, by + BS / 2, 50, who == 1 ? PCOL : rc, 3.0f, who == 1 ? -1.5f : 1.5f);
  addRing(gx, by + BS / 2, who == 1 ? PCOL : rc, 1.6f);
  goalFlash[who == 1 ? 1 : 0] = 1;
  if (who == 1) {
    ptsP++;
    addScore(100 * (rival + 1) + rally * 10);
    sfx(SFX_POWERUP);
  } else {
    ptsC++;
    sfx(SFX_EXPLODE, -0.8f, 0.8f);
    rumble(0.6f, 250);
  }
  serveDir = who == 1 ? 1 : -1;
  if (ptsP >= WIN_POINTS) {
    addScore(1000 * (rival + 1));
    state = ST_WON; stateT = 0;
    music(&SONG_WIN);
  } else if (ptsC >= WIN_POINTS) {
    state = ST_OVER; stateT = 0;
    music(&SONG_OVER);
    if (newHi) saveHi(hiscore);
  } else {
    state = ST_POINT; stateT = 0;
  }
}

static void hitPaddle(bool player, float padY) {
  float off = ((by + BS / 2) - (padY + PH / 2)) / (PH / 2);   // -1..1
  off = clampv(off, -1.0f, 1.0f);
  rally++;
  bestRally = std::max(bestRally, rally);
  bspd = std::min(bspd + 0.18f * speed(), 7.5f * speed());
  float spd = bspd;
  smash = false;
  if (player && (heldNow & (BTN_A | BTN_R))) { smash = true; spd *= 1.45f; addScore(20); }
  float a = off * 1.0f;
  bvx = cosf(a) * spd * (player ? 1 : -1);
  bvy = sinf(a) * spd;
  if (player) { addScore(10); flashP = 1; } else { flashC = 1; cpuErr = newErr(); }
  float hx = player ? PLX + PW : CPX;
  Color c = smash ? HOT : player ? PCOL : RIVCOL[rival % NRIVALS];
  burst(hx, by + BS / 2, smash ? 30 : 12, smash ? HOT : VOLT, smash ? 2.6f : 1.4f, player ? 1.0f : -1.0f);
  addRing(hx, by + BS / 2, c, smash ? 1.2f : 0.5f);
  sfx(SFX_KNOCK, pan(), 0.85f + 0.03f * std::min(rally, 16));
  if (smash) {
    sfx(SFX_SHOOT, pan(), 0.55f);
    sfx(SFX_EXPLODE, pan(), 1.8f);
    shake = 14; zoomKick = 0.025f;
    arcLink = {14, hx, padY + PH / 2, true};
    rumble(0.4f, 90);
  }
}

static void updateCpu() {
  float target = (CT + CB) / 2 - PH / 2;
  if (bvx > 0 && bx > cpuReactX()) {
    // predict where the ball reaches the CPU paddle, bouncing off the walls
    float t = (CPX - (bx + BS)) / bvx, yy = by + bvy * t;
    float span = CB - CT - BS;
    float rel = fmodf(yy - CT, 2 * span);
    if (rel < 0) rel += 2 * span;
    yy = CT + (rel > span ? 2 * span - rel : rel);
    target = yy + BS / 2 - PH / 2 + cpuErr;
  }
  float d = target - cy, mx = cpuMaxSpeed();
  cpuV = clampv(d * 0.25f, -mx, mx);
  cy = clampv(cy + cpuV, (float)CT, (float)(CB - PH));
}

static void movePlayer(const Pad& in) {
  float ny = clampv(py + in.ay * 4.2f, (float)CT, (float)(CB - PH));
  pyV = ny - py;
  py = ny;
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  movePlayer(in);
  updateCpu();
  for (int i = 13; i > 0; i--) { trailX[i] = trailX[i - 1]; trailY[i] = trailY[i - 1]; }
  trailX[0] = bx; trailY[0] = by;
  int n = (int)ceilf(fabsf(bvx) / 2.0f) + 1;
  for (int k = 0; k < n && state == ST_PLAY; k++) {
    bx += bvx / n; by += bvy / n;
    if (by < CT) { by = CT; bvy = fabsf(bvy); sfx(SFX_BUMP, pan(), 1.5f); wallHit(bx + BS / 2, CT); }
    if (by > CB - BS) { by = CB - BS; bvy = -fabsf(bvy); sfx(SFX_BUMP, pan(), 1.5f); wallHit(bx + BS / 2, CB); }
    if (bvx < 0 && bx <= PLX + PW && bx + BS >= PLX && by + BS >= py && by <= py + PH) { bx = PLX + PW; hitPaddle(true, py); }
    else if (bvx > 0 && bx + BS >= CPX && bx <= CPX + PW && by + BS >= cy && by <= cy + PH) { bx = CPX - BS; hitPaddle(false, cy); }
    if (bx < -BS) pointTo(2);
    else if (bx > SW_) pointTo(1);
  }
  if (smash && (frameNo & 1)) burst(bx + BS / 2, by + BS / 2, 1, HOT, 0.4f, 0, false);
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vx *= 0.94f; p.vy *= 0.94f;
    if (--p.life <= 0) p.on = false;
  }
  for (auto& r : rings) if (r.on && (r.t += 0.045f) >= 1) r.on = false;
  for (auto& z : zaps) if (z.on && --z.t <= 0) z.on = false;
  for (auto& m : marks) if (m.on && --m.t <= 0) m.on = false;
  if (arcLink.t > 0) arcLink.t--;
  stars.step(-0.25f, 0.05f);
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  flashP *= 0.86f; flashC *= 0.86f;
  goalFlash[0] *= 0.95f; goalFlash[1] *= 0.95f;
  charge = approach(charge, (state == ST_PLAY && (heldNow & (BTN_A | BTN_R))) ? 1.0f : 0.0f, 0.2f);
  // the camera leans towards the ball during play
  bool live = state == ST_PLAY;
  camX = approach(camX, live ? -(bx - 160) * K * 0.02f : 0, 0.05f);
  camY = approach(camY, live ? -(by - 131) * K * 0.03f : 0, 0.05f);
  zoomKick *= 0.88f;
  zoom = 1 + zoomKick;
  // speed lines stream along the ball's path, more of them the faster it goes
  float spd = sqrtf(bvx * bvx + bvy * bvy);
  float dx = live ? bvx : 2.0f, dy = live ? bvy : 0.0f;
  float k = live ? 2.2f : 1;
  for (auto& s : streaks) {
    s.x += dx * k; s.y += dy * k;
    if (s.x < -30 || s.x > SW_ + 30 || s.y < CT || s.y > CB) {
      s.x = dx >= 0 ? frange(-30, 40) : frange(SW_ - 40, SW_ + 30);
      if (frand() < 0.5f) s.x = frange(0, SW_);
      s.y = frange(CT + 2, CB - 2); s.len = frange(10, 30);
    }
    s.a = live ? clampv((spd - 3.6f) / 4.0f, 0.0f, 1.0f) + (smash ? 0.4f : 0) : 0;
  }
}

// title demo rally
static float dbx = 100, dby = 150, dvx = 3.0f, dvy = 1.7f;

static void step(const Pad& in) {
  heldNow = in.held;
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      dbx += dvx; dby += dvy;
      if (dbx < 30 || dbx > SW_ - 36) { dvx = -dvx; sfx(SFX_MOVE, dbx < 160 ? -0.7f : 0.7f); }
      if (dby < 104 || dby > 226) dvy = -dvy;
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_SERVE:
    case ST_POINT:
      if (in.hit(BTN_START)) { nova::pause(); break; }
      movePlayer(in);
      if (stateT > (state == ST_SERVE ? 90 : 70)) serve();
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_WON:
      if (stateT > 180) { rival++; startMatch(); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing helpers
static uint32_t dseed = 1;   // drawing has its own random numbers, so the game's stay untouched
static float drnd() { dseed ^= dseed << 13; dseed ^= dseed >> 17; dseed ^= dseed << 5; return (dseed & 0xFFFF) / 65535.0f; }

// a jagged lightning arc between two screen points
static void bolt(float x0, float y0, float x1, float y1, Color c, float amp, int n, float thick = 4) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy) + 0.001f, nx = -dy / len, ny = dx / len;
  float px = x0, py_ = y0;
  for (int i = 1; i <= n; i++) {
    float t = (float)i / n, o = i == n ? 0 : (drnd() * 2 - 1) * amp;
    float qx = x0 + dx * t + nx * o, qy = y0 + dy * t + ny * o;
    line(px, py_, qx, qy, thick * 3.5f, c.alpha(0.25f), BLEND_ADD);
    line(px, py_, qx, qy, thick, mix(c, WHITE, 0.65f), BLEND_ADD);
    px = qx; py_ = qy;
  }
}

static void drawBackdrop() {
  rectGrad(0, 0, W, H, Color(8, 6, 28), Color(22, 8, 46));
  stars.draw(Color(170, 190, 255), 2.5f);
  Color rc = RIVCOL[rival % NRIVALS];
  glow(0, H / 2, 600, PCOL.alpha(0.18f + 0.3f * goalFlash[0]));
  glow(W, H / 2, 600, rc.alpha(0.18f + 0.3f * goalFlash[1]));
}

static void drawCourt(bool title) {
  float l = X(0), r = X(SW_), t = Y(CT), b = Y(CB);
  // floor
  rectGrad(l, t, r - l, b - t, Color(16, 14, 50, 235), Color(26, 12, 60, 235));
  rectGrad(l, t, r - l, (b - t) * 0.3f, Color(0, 0, 10, 90), Color(0, 0, 10, 0));
  rectGrad(l, b - (b - t) * 0.3f, r - l, (b - t) * 0.3f, Color(0, 0, 10, 0), Color(0, 0, 10, 90));
  // the grid (the original's 20 px squares)
  for (int x = 20; x < SW_; x += 20) line(X(x), t, X(x), b, 2, Color(60, 56, 140, 110));
  for (int y = CT + 20; y < CB; y += 20) line(l, Y(y), r, Y(y), 2, Color(60, 56, 140, 110));
  // floor light from the ball and the paddles
  Color rc = RIVCOL[rival % NRIVALS];
  if (!title) {
    glow(X(PLX + PW / 2.0f), Y(py + PH / 2.0f), S(34), PCOL.alpha(0.22f + 0.35f * flashP));
    glow(X(CPX + PW / 2.0f), Y(cy + PH / 2.0f), S(34), rc.alpha(0.22f + 0.35f * flashC));
    if (state == ST_PLAY) glow(X(bx + BS / 2.0f), Y(by + BS / 2.0f), S(smash ? 40 : 28), (smash ? HOT : VOLT).alpha(0.3f));
  }
  // centre line and circle with a faint bolt crest
  for (int y = CT + 2; y < CB; y += 12) roundRect(X(SW_ / 2.0f) - S(1), Y(y), S(2), S(7), S(1), Color(110, 100, 190, 170));
  ring(X(SW_ / 2.0f), Y((CT + CB) / 2.0f), S(26), 4, Color(90, 80, 170, 160));
  glow(X(SW_ / 2.0f), Y((CT + CB) / 2.0f), S(30), Color(120, 90, 255, 26));
  Fx cf; cf.sx = cf.sy = zoom * 2.2f; cf.tint = Color(120, 110, 220, 40);
  draw(vrart::IMG_G0, X(SW_ / 2.0f), Y((CT + CB) / 2.0f), cf);
  // the goal curtains at each end
  for (int s = 0; s < 2; s++) {
    float gx = s == 0 ? X(-1) : X(SW_ + 1);
    Color c = s == 0 ? PCOL : rc;
    float a = 0.35f + 0.15f * sinf(frameNo * 0.1f + s * 2) + 0.5f * goalFlash[s];
    float fx = s == 0 ? gx + S(12) : gx - S(12);
    Vtx v[6] = {{gx, t, c.alpha(a * 0.45f)}, {fx, t, c.alpha(0)}, {fx, b, c.alpha(0)},
                {gx, t, c.alpha(a * 0.45f)}, {fx, b, c.alpha(0)}, {gx, b, c.alpha(a * 0.45f)}};
    tris(v, 6, BLEND_ADD);
    rect(gx - 2, t, 4, b - t, c.alpha(a));
    if (goalFlash[s] > 0.2f) {
      dseed = frameNo * 7919u + s * 104729u + 1;
      bolt(gx, t, gx, b, c, S(4) * goalFlash[s], 18, 3);
    }
  }
  // scorch marks left by the ball on the walls
  for (auto& m : marks) {
    if (!m.on) continue;
    float k = m.t / 150.0f;
    float my = m.y == CT ? Y(CT) - S(2) : Y(CB) + S(2);
    disc(X(m.x), my, S(5), Color(10, 6, 20, (uint8_t)(140 * k)));
    glow(X(m.x), my, S(6), Color(255, 140, 220, (uint8_t)(90 * k * k)));
  }
  // walls: neon tubes with a light pulse running along them
  for (int s = 0; s < 2; s++) {
    float wy = s == 0 ? Y(CT - 4) : Y(CB), h = S(4);
    rect(l - S(6), wy - 6, r - l + S(12), h + 12, Color(20, 12, 44));
    roundRect(l - S(4), wy, r - l + S(8), h, h / 2, Color(122, 61, 184));
    roundRect(l - S(4), wy + h * 0.2f, r - l + S(8), h * 0.3f, h * 0.15f, Color(194, 120, 240));
    roundRect(l, wy + h * 0.25f, r - l, 3, 1.5f, Color(255, 230, 255, 180));
    line(l, wy + h / 2, r, wy + h / 2, h * 2.2f, Color(150, 80, 255, 26), BLEND_ADD);
    float pulse = fmodf(frameNo * 6.0f + s * 700, (r - l) + 600) - 300;
    glow(l + pulse, wy + h / 2, 90, Color(220, 160, 255, 90));
    glow(r - pulse, wy + h / 2, 70, Color(160, 220, 255, 60));
  }
  // wall-hit lightning
  for (int i = 0; i < 8; i++) {
    const Zap& z = zaps[i];
    if (!z.on) continue;
    float k = z.t / 14.0f;
    float zy = z.y == CT ? Y(CT) - S(2) : Y(CB) + S(2);
    dseed = (frameNo / 2) * 2654435761u + i * 40503u + 7;
    glow(X(z.x), zy, S(14) * (0.6f + k), z.c.alpha(0.6f * k));
    for (int j = 0; j < 2; j++) {
      float len = S(18) * (0.5f + k);
      bolt(X(z.x), zy, X(z.x) + (j ? len : -len), zy + (drnd() - 0.5f) * 20, z.c.alpha(k), 10, 6, 3);
    }
    float out = z.y == CT ? 1.0f : -1.0f;
    bolt(X(z.x), zy, X(z.x) + (drnd() - 0.5f) * 60, zy + out * S(12) * k, z.c.alpha(k), 8, 4, 2.5f);
  }
}

static void drawStreaks() {
  float ang = atan2f(bvy, bvx) * 57.2958f;
  for (auto& s : streaks) {
    if (s.a <= 0.01f) continue;
    Fx f; f.rot = ang; f.sx = S(s.len) / 40; f.sy = 0.6f; f.blend = BLEND_ADD;
    f.tint = (smash ? HOT : VOLT).alpha(0.16f * s.a);
    draw(cart::IMG_STREAK, X(s.x), Y(s.y), f);
  }
}

static void drawPaddle(float ux, float uy, float vel, Color c, float flash, float crackle, bool isPlayer) {
  float x = X(ux + PW / 2.0f), y = Y(uy + PH / 2.0f);
  float tilt = clampv(vel * 1.4f, -7.0f, 7.0f) * (isPlayer ? 1 : -1);
  // afterimages when moving fast
  if (fabsf(vel) > 1.5f)
    for (int i = 1; i <= 3; i++) {
      Fx g; g.sx = g.sy = zoom; g.rot = tilt; g.tint = c.alpha(0.12f * (4 - i) * std::min(1.0f, fabsf(vel) / 4)); g.blend = BLEND_ADD;
      draw(vrart::IMG_CORE, x, y - vel * S(1.2f) * i, g);
    }
  glow(x, y, S(28), c.alpha(0.3f + 0.4f * flash + 0.3f * crackle));
  Fx f; f.sx = f.sy = zoom; f.rot = tilt;
  f.tint = mix(c, WHITE, flash * 0.8f);
  draw(vrart::IMG_CORE, x, y, f);
  Fx a = f; a.blend = BLEND_ADD; a.tint = c.alpha(0.15f + 0.08f * sinf(frameNo * 0.2f) + 0.4f * crackle);
  draw(vrart::IMG_CORE, x, y, a);
  Fx s; s.sx = s.sy = zoom; s.rot = tilt;
  draw(vrart::IMG_SHELL, x, y, s);
  if (crackle > 0.05f) {   // smash armed: arcs crawl along the core
    dseed = (frameNo / 2) * 2246822519u + (isPlayer ? 3 : 5);
    float h = S(PH / 2.0f - 3);
    float sn = sinf(tilt / 57.2958f), cs = cosf(tilt / 57.2958f);
    bolt(x + sn * h, y - cs * h, x - sn * h, y + cs * h, HOT.alpha(crackle), 9, 10, 2.5f);
    glow(x, y, S(20), HOT.alpha(0.3f * crackle));
  }
}

static void drawBall(float ux, float uy, bool hot) {
  float x = X(ux + BS / 2.0f), y = Y(uy + BS / 2.0f);
  Color c = hot ? HOT : VOLT;
  glow(x, y, S(hot ? 18 : 13), c.alpha(0.55f));
  Fx f; f.sx = f.sy = zoom * 0.62f; f.rot = frameNo * (hot ? 9.0f : 4.0f);
  draw(*(hot ? vrart::HOT : vrart::BALL)[(frameNo / 3) & 1], x, y, f);
  Fx g; g.sx = g.sy = zoom * 0.62f; g.blend = BLEND_ADD; g.tint = WHITE.alpha(0.2f + 0.15f * sinf(frameNo * 0.5f));
  draw(*(hot ? vrart::HOT : vrart::BALL)[((frameNo / 3) + 1) & 1], x, y, g);
}

static void drawTrail() {
  Color c = smash ? HOT : VOLT;
  for (int i = 13; i >= 1; i--) {
    float k = 1 - i / 14.0f;
    disc(X(trailX[i] + BS / 2.0f), Y(trailY[i] + BS / 2.0f), S(BS / 2.0f) * (0.35f + 0.65f * k), c.alpha(0.5f * k), BLEND_ADD);
  }
  if (smash) {   // a crackling arc along the trail
    dseed = (frameNo / 2) * 97u + 13;
    for (int i = 0; i < 10; i += 3)
      bolt(X(trailX[i] + 3), Y(trailY[i] + 3), X(trailX[i + 3] + 3), Y(trailY[i + 3] + 3), HOT.alpha(0.8f - i * 0.07f), 10, 3, 2.5f);
  }
}

static void drawParticles() {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    Color c = mix(p.col, WHITE, t * 0.5f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 12 * zoom; f.rot = p.life * 8; draw(cart::IMG_SPARK, X(p.x), Y(p.y), f); }
    else disc(X(p.x), Y(p.y), p.size * (0.4f + 0.6f * t) * zoom, c, BLEND_ADD);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = (0.3f + r.t * 1.8f) * r.size * zoom; f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, X(r.x), Y(r.y), f);
  }
}

static void drawBadge(int idx, float x, float y, float scale, bool lit, bool beaten) {
  Color c = idx < 0 ? PCOL : RIVCOL[idx % NRIVALS];
  if (lit) glow(x, y, 110 * scale * 1.4f, c.alpha(0.35f + 0.15f * sinf(frameNo * 0.08f)));
  Fx f; f.sx = f.sy = scale;
  f.tint = lit || beaten ? c : mix(c, Color(60, 60, 90), 0.75f);
  draw(vrart::IMG_BADGE, x, y, f);
  Fx g; g.sx = g.sy = scale * 0.85f; g.tint = lit || beaten ? Color(24, 18, 50) : Color(40, 40, 70, 200);
  draw(idx < 0 ? vrart::IMG_GYOU : *vrart::GLYPH[idx % NRIVALS], x, y, g);
}

static void drawHud() {
  Color rc = RIVCOL[rival % NRIVALS];
  // scoreboard across the top
  ui::panel(W / 2 - 600, 10, 1200, 100, Color(18, 14, 44, 220), 30);
  textf(W / 2, 22, ui::style(68), "%d  -  %d", ptsP, ptsC);
  for (int i = 0; i < WIN_POINTS; i++) {
    float lx = W / 2 - 150 - i * 50, rx = W / 2 + 150 + i * 50, yy = 60;
    bool on = i < ptsP, onC = i < ptsC;
    disc(lx, yy, 15, Color(40, 36, 96));
    if (on) { glow(lx, yy, 40, PCOL.alpha(0.5f)); disc(lx, yy, 12, PCOL); disc(lx - 3, yy - 4, 4, Color(255, 255, 255, 200)); }
    disc(rx, yy, 15, Color(40, 36, 96));
    if (onC) { glow(rx, yy, 40, rc.alpha(0.5f)); disc(rx, yy, 12, rc); disc(rx - 3, yy - 4, 4, Color(255, 255, 255, 200)); }
  }
  TextStyle ls = ui::style(40, PCOL, LEFT); text("YOU", W / 2 - 570, 38, ls);
  TextStyle rs = ui::style(40, rc, RIGHT); text(RIVALS[rival % NRIVALS], W / 2 + 570, 38, rs);
  // left: the player's numbers
  char buf[32];
  float lx = 120;
  drawBadge(-1, lx, 210, 0.75f, true, false);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 330);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 460, Color(255, 220, 110));
  snprintf(buf, sizeof(buf), "%d", rally);
  ui::stat("RALLY", buf, lx, 590, rally >= 10 ? HOT : WHITE);
  snprintf(buf, sizeof(buf), "%d", bestRally);
  ui::stat("BEST RALLY", buf, lx, 720, Color(180, 200, 255));
  TextStyle h = ui::style(24, Color(170, 180, 220)); h.outline = CLEAR;
  textf(lx, 900, h, "HOLD %s", btnName(BTN_A));
  textf(lx, 932, h, "AS IT HITS:");
  TextStyle sm = ui::style(34, HOT.alpha(0.6f + 0.4f * charge)); text("SMASH", lx, 966, sm);
  // right: the rival ladder
  float rx = W - 120;
  TextStyle lt = ui::style(26, Color(150, 170, 220)); lt.outline = CLEAR;
  if (rival >= NRIVALS) textf(rx, 130, lt, "ROUND %d", rival / NRIVALS + 1);
  else text("RIVALS", rx, 130, lt);
  for (int i = 0; i < NRIVALS; i++) {
    float y = 220 + i * 140;
    int cur = rival % NRIVALS;
    bool lit = i == cur, beaten = i < cur;
    drawBadge(i, rx, y, lit ? 0.62f : 0.48f, lit, beaten);
    TextStyle n = ui::style(lit ? 28 : 22, lit ? RIVCOL[i] : Color(120, 120, 160)); if (!lit) n.outline = CLEAR;
    text(RIVALS[i], rx, y + (lit ? 44 : 36), n);
    if (beaten) {   // a tick for each rival already beaten
      disc(rx + 34, y - 24, 15, Color(20, 40, 30));
      disc(rx + 34, y - 24, 12, Color(90, 230, 120));
      line(rx + 28, y - 24, rx + 33, y - 18, 4, Color(10, 40, 20));
      line(rx + 33, y - 18, rx + 41, y - 30, 4, Color(10, 40, 20));
    }
  }
}

static void draw() {
  drawBackdrop();
  if (state == ST_TITLE) {
    // a demo rally behind the title
    drawCourt(true);
    float lp = clampv(dby - 17, 104.0f, 196.0f), rp = clampv(dby - 17 + 10 * sinf(frameNo * 0.05f), 104.0f, 196.0f);
    drawPaddle(PLX, lp, dvy * (dby - 17 > 104 && dby - 17 < 196 ? 1 : 0), RIVCOL[1], 0, 0, true);
    drawPaddle(CPX, rp, 0, RIVCOL[5], 0, 0, false);
    glow(X(dbx + 3), Y(dby + 3), S(18), VOLT.alpha(0.5f));
    drawBall(dbx, dby, false);
    char h0[64], h1[64];
    snprintf(h0, sizeof(h0), "UP / DOWN MOVE    HOLD %s AS THE BALL HITS: SMASH", btnName(BTN_A));
    snprintf(h1, sizeof(h1), "FIRST TO 5 BEATS EACH RIVAL");
    const char* help[] = {h0, h1};
    ui::titleScreen(cart::IMG_LOGO_VOLTRALLY, "PADDLE TENNIS WITH A SPARK", hiscore, help, 2, Color(184, 243, 255));
    for (int i = 0; i < NRIVALS; i++) drawBadge(i, W / 2 + (i - 2.5f) * 150, 945, 0.42f, false, true);
    return;
  }
  drawCourt(false);
  if (state == ST_PLAY) drawStreaks();
  Color rc = RIVCOL[rival % NRIVALS];
  drawPaddle(PLX, py, pyV, PCOL, flashP, charge, true);
  drawPaddle(CPX, cy, cpuV, rc, flashC, 0, false);
  if (state == ST_PLAY) {
    drawTrail();
    if (arcLink.t > 0) {   // lightning from the paddle to the smashed ball
      dseed = frameNo * 31u + 5;
      float k = arcLink.t / 14.0f;
      bolt(X(arcLink.x0), Y(arcLink.y0), X(bx + BS / 2.0f), Y(by + BS / 2.0f), HOT.alpha(k), 18, 12, 3.5f);
    }
    drawBall(bx, by, smash);
  }
  drawParticles();
  drawHud();
  if (state == ST_SERVE) {
    float a = easeOut(stateT / 20.0f);
    ui::panel(W / 2 - 330, 330, 660, 330, Color(18, 14, 44, (uint8_t)(225 * a)), 36);
    drawBadge(rival % NRIVALS, W / 2, 420, 0.8f * a, true, false);
    textf(W / 2, 498, ui::style(44, rc.alpha(a)), rival >= NRIVALS ? "ROUND %d  RIVAL %d" : "RIVAL %d", rival >= NRIVALS ? rival / NRIVALS + 1 : rival % NRIVALS + 1, rival % NRIVALS + 1);
    TextStyle n = ui::style(84, WHITE.alpha(a)); n.shadow = 4;
    text(RIVALS[rival % NRIVALS], W / 2, 552, n);
  } else if (state == ST_POINT) {
    ui::banner(lastWinner == 1 ? "POINT!" : "MISSED", W / 2, 470, 70 - stateT + 15, 70, lastWinner == 1 ? PCOL : Color(235, 60, 80), 120);
  } else if (state == ST_WON) {
    float a = easeOut(stateT / 20.0f);
    ui::panel(W / 2 - 360, 340, 720, 320, Color(18, 14, 44, (uint8_t)(225 * a)), 36);
    TextStyle t = ui::style(96 * (0.8f + 0.2f * a), Color(182, 255, 110).alpha(a)); t.shadow = 6;
    text("MATCH WON!", W / 2, 380, t);
    if (stateT > 20) textf(W / 2, 510, ui::style(52), "BONUS %d", 1000 * (rival + 1));
    if (stateT > 50) text("NEXT RIVAL...", W / 2, 590, ui::style(40, VOLT));
  } else if (state == ST_OVER) {
    ui::gameOver(stateT, newHi);
  }
}

// ------------------------------------------------------------ bot: read the ball, angle it away from the rival, smash
static float botAim = 0;
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  float target = (CT + CB) / 2.0f - PH / 2.0f;
  if (state == ST_PLAY && bvx < 0) {
    float t = (bx - (PLX + PW)) / -bvx, yy = by + bvy * t;
    float span = CB - CT - BS;
    float rel = fmodf(yy - CT, 2 * span);
    if (rel < 0) rel += 2 * span;
    yy = CT + (rel > span ? 2 * span - rel : rel);
    // pick an angle once per approach: send it to the side away from the rival
    if (t > 30 || botAim == 0) botAim = (cy + PH / 2.0f < (CT + CB) / 2.0f) ? 1 : -1;
    float off = 0.55f * botAim;   // where on the paddle the ball should land (-1..1)
    target = yy + BS / 2.0f - PH / 2.0f - off * PH / 2.0f;
    if (t < 26) p.held |= BTN_A;   // smash as it arrives
  } else botAim = 0;
  float d = target - py;
  p.ay = clampv(d / 4.2f, -1.0f, 1.0f);
  if (fabsf(d) > 1) p.held |= d > 0 ? BTN_DOWN : BTN_UP;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d rival %d (%s) points %d-%d score %lu best rally %d", state, rival + 1, RIVALS[rival % NRIVALS], ptsP, ptsC,
           (unsigned long)score, bestRally);
}

static void init() { loadAtlas(vrart::TEX_FILES, vrart::TEX, vrart::NTEX); }
static void enter() {
  state = ST_TITLE; stateT = 0;
  rival = 0; ptsP = ptsC = 0;
  stars.init(110);
  for (auto& s : streaks) { s.x = frange(0, SW_); s.y = frange(CT, CB); s.len = frange(10, 30); s.a = 0; }
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace vr

extern const nova::Game VOLT_RALLY;
const nova::Game VOLT_RALLY = {
  "voltrally", "VOLT RALLY", "PADDLE TENNIS WITH A SPARK",
  vr::init, vr::enter, vr::step, vr::draw, vr::canPause, nullptr, vr::bot, vr::debugInfo,
};
