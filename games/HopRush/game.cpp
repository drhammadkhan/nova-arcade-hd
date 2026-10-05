// =====================================================================
//  HOP RUSH HD  -  a road-and-river crossing game (Nova Arcade HD)
//  Guide the little hop-bot across five lanes of traffic and a river of
//  drifting logs and lily pads, then park it in one of the five docks.
//  Fill every dock to clear the level; each level runs a little faster.
//
//  The original's lanes, timing and rules on its 320-wide field (drawn
//  4.5x, centred, with hedges on either side). New for HD: top-down vector
//  vehicles, logs and pads (tools/make_art.py), rippling water, headlights,
//  a squashy hop with a shadow, and splash rings.
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

namespace hr {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  112, 4, "C F G C",
  {I_MARIMBA, 0.8f, 0.1f, "C5 - - E5 - - G5 - - - F5 - E5 - D5 - | . . . . . . . . . . . . . . . . | C5 - - E5 - - G5 - - - F5 - E5 - D5 - | . . . . . . . . . . . . . . . ."},
  {}, I_BELL, "1...5...3...5...", 0.18f, I_SUBBASS, "R.......5.......", 0.5f, I_PAD, 0.4f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  140, 8, "C F G C Am F G C",
  {I_SQUARE, 0.6f, 0.05f,
   "C5 - D5 E5 G5 - E5 - D5 - C5 - D5 - E5 - | F5 - E5 F5 A5 - F5 - E5 - D5 - C5 - - - | G5 - F5 E5 D5 - E5 - F5 - G5 - D5 - - - | E5 - G5 - C6 - B5 - G5 - E5 - C5 - - - |"
   "C5 - D5 E5 G5 - E5 - D5 - C5 - D5 - E5 - | F5 - E5 F5 A5 - F5 - E5 - D5 - C5 - - - | G5 - F5 E5 D5 - E5 - F5 - G5 - D5 - - - | E5 - G5 - C6 - B5 - G5 - E5 - C5 - - -"},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.18f, I_PLUCKBASS, "R..R..5.R..R5.O.", 0.55f, I_NONE, 0,
  "K.h.S.h.K.h.S.hh|K.h.S.h.K.h.S.hh|K.h.S.h.K.h.S.hh|K.hhS.h.KKh.S.h.", 0.8f, 0.06f, true};
static const Song SONG_OVER = {
  100, 2, "C C", {I_FLUTE, 0.8f, 0, "C5 - B4 - A4 - G4 - F4 - E4 - C4 - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_CLEAR = {
  140, 2, "C C", {I_MARIMBA, 0.8f, 0, "C5 E5 G5 C6 - - G5 C6 E6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ layout (original units)
static const float K = 4.5f, OX = (W - 320 * K) / 2;
static inline float X(float x) { return OX + x * K; }
static inline float Y(float y) { return y * K; }
static const int SW_ = 320, T = 16, TOP = 16, NROWS = 13, START_ROW = 12, MEDIAN_ROW = 6, NDOCK = 5;
static inline int rowY(int r) { return TOP + r * T; }
static inline int dockX(int i) { return 24 + i * 64; }   // dock inlets are 32 px wide
static inline bool isRiver(int r) { return r >= 1 && r <= 5; }
static inline bool isRoad(int r) { return r >= 7 && r <= 11; }
enum Kind : uint8_t { K_LOG, K_PAD, K_CAR, K_TRUCK, K_DOZER, K_RACER };
struct Lane { uint8_t row; float speed; uint8_t len, gap; Kind kind; uint8_t col; };
static const Lane LANES[] = {
  {1, 0.45f, 4, 3, K_LOG, 0},   {2, -0.70f, 3, 2, K_PAD, 0},  {3, 0.95f, 6, 4, K_LOG, 0},
  {4, 0.55f, 3, 3, K_LOG, 0},   {5, -0.60f, 2, 2, K_PAD, 0},
  {7, -0.85f, 3, 5, K_TRUCK, 0}, {8, 1.50f, 1, 7, K_RACER, 1}, {9, -0.70f, 1, 3, K_CAR, 2},
  {10, 0.55f, 2, 4, K_DOZER, 3}, {11, -0.80f, 1, 4, K_CAR, 4},
};
static const int NLANES = sizeof(LANES) / sizeof(LANES[0]);
static const Color CARCOL[5] = {Color(245, 70, 90), Color(255, 214, 60), Color(70, 200, 240), Color(255, 150, 50), Color(200, 70, 230)};
static float laneOff[NLANES], laneSpd[NLANES];
// the objects of a lane repeat every `period` px over a loop `span` px wide (wider than the original's
// screen so they keep coming from under the hedges)
static const int VIEW = SW_ + 2 * 54;
static inline int lanePeriod(const Lane& l) { return (l.len + l.gap) * T; }
static inline int laneCount(const Lane& l) { return (VIEW + l.len * T + lanePeriod(l) - 1) / lanePeriod(l) + 1; }
static inline int laneSpan(const Lane& l) { return laneCount(l) * lanePeriod(l); }
static inline float objX(int li, int k) {
  const Lane& l = LANES[li];
  float span = laneSpan(l);
  float x = fmodf(laneOff[li] + k * lanePeriod(l), span);
  if (x < 0) x += span;
  return x - l.len * T - 54;
}

// ------------------------------------------------------------ state
static float px = 0;
static int prow = START_ROW;
static int hopT = 0, hopDir = 0;
static float hopFromX = 0;
static int hopFromRow = 0, bestRow = START_ROW;
static bool docks[NDOCK];
static int gemDock = -1, gemT = 0;
static int lives = 3, level = 1, timeLeft = 0, timeMax = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 3000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_HOME, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0, deathKind = 0;
static int padCycle = 0;
static const int SINK_LANE = 1;   // from level 2 the lily pads in lane 1 sink for a moment every five seconds
static inline bool padSunk(int li) { return level >= 2 && li == SINK_LANE && padCycle >= 220 && padCycle < 290; }
static inline bool padWarn(int li) { return level >= 2 && li == SINK_LANE && padCycle >= 180 && padCycle < 220; }

struct Part { bool on; float x, y, vx, vy, life, max; Color col; bool spark; };
static Part parts[300];
struct Ring { bool on; float x, y, t; Color c; };
static Ring rings[10];
struct Popup { bool on; float x, y; int t; char txt[12]; };
static Popup popups[4];
static float shake = 0, shakeX = 0, shakeY = 0;

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, bool spark = false) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s - 1, frange(14, 30), 30, col, spark && i % 2 == 0}; break; }
}
static void ring(float x, float y, Color c) { for (auto& r : rings) if (!r.on) { r = {true, x, y, 0, c}; return; } }
static void popup(float x, float y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 60, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

static void setupLanes() {
  float mult = (1.0f + 0.14f * std::min(level - 1, 8)) * speed();
  for (int i = 0; i < NLANES; i++) { laneSpd[i] = LANES[i].speed * mult; laneOff[i] = frand() * laneSpan(LANES[i]); }
}
static void startLife() {
  px = SW_ / 2 - 8; prow = START_ROW; hopT = 0; bestRow = START_ROW;
  timeMax = timeLeft = frames(60 * 30);
  state = ST_PLAY; stateT = 0;
}
static void startLevel() {
  for (auto& d : docks) d = false;
  gemDock = -1; gemT = 400;
  setupLanes();
  startLife();
  music(&SONG_GAME);
}
static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false;
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  startLevel();
}
static void die(int kind) {
  deathKind = kind;
  float cx = px + 8, cy = rowY(prow) + 8;
  if (kind) { burst(cx, cy, 24, Color(180, 230, 255), 1.8f); ring(cx, cy, Color(180, 230, 255)); sfx(SFX_SPLASH); }
  else { burst(cx, cy, 30, Color(70, 210, 240), 2.4f, true); sfx(SFX_HURT); sfx(SFX_DIE); shake = 14; }
  rumble(0.6f, 350);
  lives--;
  state = ST_DEAD; stateT = 0;
}
static int platformUnder(float cx, int row) {
  for (int li = 0; li < NLANES; li++) {
    if (LANES[li].row != row) continue;
    if (padSunk(li)) return -1;
    for (int k = 0; k < laneCount(LANES[li]); k++) {
      float x = objX(li, k);
      if (cx >= x + 2 && cx <= x + LANES[li].len * T - 2) return li;
    }
    return -1;
  }
  return -1;
}
static bool hitByTraffic(float x0, int row) {
  for (int li = 0; li < NLANES; li++) {
    if (LANES[li].row != row) continue;
    for (int k = 0; k < laneCount(LANES[li]); k++) {
      float x = objX(li, k);
      if (x0 + 12 > x + 1 && x0 + 4 < x + LANES[li].len * T - 1) return true;
    }
  }
  return false;
}
static void reachHome() {
  float cx = px + 8;
  for (int i = 0; i < NDOCK; i++) {
    if (cx >= dockX(i) + 2 && cx <= dockX(i) + 30) {
      if (docks[i]) break;
      docks[i] = true;
      uint32_t v = 50 + 10 * (timeLeft * 30 / std::max(1, timeMax));
      if (gemDock == i) { v += 200; gemDock = -1; sfx(SFX_POWERUP); }
      else sfx(SFX_CHECKPOINT);
      addScore(v);
      popup(dockX(i) + 4, rowY(0) + 20, v);
      burst(dockX(i) + 16, rowY(0) + 8, 20, Color(255, 220, 90), 1.6f, true);
      int n = 0;
      for (bool d : docks) n += d;
      if (n == NDOCK) { addScore(1000 * level); state = ST_CLEAR; music(&SONG_CLEAR); }
      else state = ST_HOME;
      stateT = 0;
      return;
    }
  }
  die(0);   // bumped into the bank or an occupied dock
}
static void landed() {
  if (prow < bestRow) { bestRow = prow; addScore(10); }
  if (prow == 0) { reachHome(); return; }
  if (isRiver(prow) && platformUnder(px + 8, prow) < 0) die(1);
}
static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  if (--timeLeft <= 0) { die(0); return; }
  if (hopT == 0) {
    int dx = 0, dy = 0;
    if (in.hit(BTN_UP)) dy = -1;
    else if (in.hit(BTN_DOWN)) dy = 1;
    else if (in.hit(BTN_LEFT)) dx = -1;
    else if (in.hit(BTN_RIGHT)) dx = 1;
    if (dy == 1 && prow == START_ROW) dy = 0;
    if ((dx < 0 && px < 4) || (dx > 0 && px > SW_ - 20)) dx = 0;
    if (dx || dy) {
      hopFromX = px; hopFromRow = prow; hopDir = dx ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
      prow += dy;
      px += dx * T;
      hopT = 7;
      sfx(SFX_JUMP, 0, 1.3f);
    }
  }
  if (hopT) { if (--hopT == 0) { landed(); if (state != ST_PLAY) return; } }
  if (isRiver(prow) && hopT == 0) {   // ride the river
    int li = platformUnder(px + 8, prow);
    if (li < 0) { die(1); return; }
    px += laneSpd[li];
    if (px < -6 || px > SW_ - 10) { die(1); return; }
  }
  int hitRow = hopT > 3 ? hopFromRow : prow;   // mid-hop she still counts in the row she's leaving
  float hitX = hopT > 3 ? hopFromX : px;
  if (isRoad(hitRow) && hitByTraffic(hitX, hitRow)) { die(0); return; }
}
static void updateWorld() {
  for (int i = 0; i < NLANES; i++) laneOff[i] += laneSpd[i];
  padCycle = (padCycle + 1) % 300;
  if (gemDock < 0) {
    if (--gemT <= 0) {
      int d = rnd() % NDOCK;
      if (!docks[d]) { gemDock = d; gemT = frames(360); } else gemT = 60;
    }
  } else if (--gemT <= 0) { gemDock = -1; gemT = 500; }
}
static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vy += 0.08f; if (--p.life <= 0) p.on = false; }
  for (auto& r : rings) if (r.on && (r.t += 0.04f) >= 1) r.on = false;
  for (auto& p : popups) if (p.on) { p.y -= 0.25f; if (--p.t <= 0) p.on = false; }
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
}
static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      updateWorld();
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_PLAY: updateWorld(); updatePlay(in); break;
    case ST_DEAD:
      updateWorld();
      if (stateT > 80) {
        if (lives <= 0) { state = ST_OVER; stateT = 0; music(&SONG_OVER); if (newHi) saveHi(hiscore); }
        else startLife();
      }
      break;
    case ST_HOME: updateWorld(); if (stateT > 40) startLife(); break;
    case ST_CLEAR: if (stateT > 150) { level++; startLevel(); } break;
    case ST_OVER:
      updateWorld();
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); setupLanes(); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static void drawGround(float ox, float oy) {
  rect(0, 0, W, H, Color(10, 8, 25));
  float x0 = 0, w = W;
  for (int r = 0; r < NROWS; r++) {
    float y = Y(rowY(r)) + oy, h = T * K;
    if (r == 0) {   // the bank, with five water inlets
      rectGrad(x0, y, w, h, Color(60, 150, 80), Color(30, 100, 60));
      for (int i = 0; i < 12; i++) disc(X(i * 30 + 6) + ox, y + 14 + (i % 2) * 20, 14, Color(90, 190, 100));
      for (int i = 0; i < NDOCK; i++) {
        float dx = X(dockX(i)) + ox;
        roundRect(dx, y + 14, 32 * K, h - 14, 14, Color(30, 80, 160));
        rectGrad(dx + 6, y + 20, 32 * K - 12, h - 20, Color(50, 120, 200), Color(30, 70, 150));
      }
    } else if (isRiver(r)) {
      rectGrad(x0, y, w, h, Color(36, 90, 180), Color(26, 64, 140));
      float t = frameNo * (r & 1 ? 1.2f : -1.2f);
      for (int i = 0; i < 14; i++) {
        float wx = fmodf(i * 160 + t + r * 37, (float)W + 200);
        if (wx < 0) wx += W + 200;
        wx -= 100;
        line(wx, y + 20 + (i % 3) * 16, wx + 50, y + 20 + (i % 3) * 16, 4, Color(120, 190, 255, 90));
      }
    } else if (r == MEDIAN_ROW || r == START_ROW) {
      rectGrad(x0, y, w, h, Color(110, 60, 170), Color(70, 32, 110));
      for (float bx = fmodf(ox, 72.0f) - 72; bx < W; bx += 72) rect(bx, y, 3, h, Color(140, 90, 200, 120));
      rect(x0, y, w, 4, Color(180, 130, 240));
    } else {   // the road
      rect(x0, y, w, h, Color(36, 36, 52));
      if (r > 7)
        for (float dx = -60; dx < W; dx += 72) rect(dx + 20, y - 3, 36, 6, Color(200, 200, 140, 140));
    }
  }
  float yb = Y(rowY(NROWS)) + oy;
  rect(0, yb, W, H - yb, Color(16, 12, 36));
}

static void drawHedges() {   // the side margins: hedges and trees that the lanes run under
  for (int side = 0; side < 2; side++) {
    float x0 = side ? X(SW_) : 0, w = side ? W - X(SW_) : OX;
    rectGrad(x0, Y(TOP), w, Y(rowY(NROWS)) - Y(TOP), Color(30, 90, 60), Color(16, 50, 40));
    for (int i = 0; i < 9; i++) {
      float cx = x0 + w * (side ? 0.15f : 0.85f) + ((i * 37) % 60) - 30, cy = Y(TOP) + 70 + i * 104;
      disc(cx, cy, 60, Color(40, 120, 70));
      disc(cx - 14, cy - 16, 34, Color(80, 170, 100));
    }
    rect(side ? x0 : x0 + w - 6, Y(TOP), 6, Y(rowY(NROWS)) - Y(TOP), Color(20, 50, 30));
  }
}

static void drawLane(int li, float ox, float oy) {
  const Lane& l = LANES[li];
  float y = Y(rowY(l.row)) + oy;
  bool right = laneSpd[li] > 0;
  for (int k = 0; k < laneCount(l); k++) {
    float x = X(objX(li, k)) + ox, w = l.len * T * K;
    if (x > W || x + w < 0) continue;
    switch (l.kind) {
      case K_LOG:
        for (int s = 0; s < l.len; s++)
          draw(s == 0 ? hrart::IMG_LOG_L : s == l.len - 1 ? hrart::IMG_LOG_R : hrart::IMG_LOG_M, x + s * T * K, y);
        break;
      case K_PAD: {
        bool sunk = padSunk(li), warn = padWarn(li);
        for (int s = 0; s < l.len; s++) {
          float cx = x + (s + 0.5f) * T * K, cy = y + T * K / 2;
          if (sunk) { Fx r; r.tint = Color(160, 210, 255, 150); r.sx = r.sy = 0.4f; r.blend = BLEND_ADD; draw(cart::IMG_RING, cx, cy, r); continue; }
          Fx f; f.rot = sinf(frameNo * 0.03f + s + li) * 8 + s * 70; f.tint = warn && (frameNo & 4) ? Color(140, 160, 140) : WHITE;
          f.sx = f.sy = warn ? 0.85f : 1;
          draw(hrart::IMG_PAD, cx, cy, f);
        }
        break;
      }
      default: {
        Fx f; f.flip = !right;
        float cx = x + (right ? 0 : w);
        if (l.kind == K_TRUCK) draw(hrart::IMG_TRUCK, cx, y, f);
        else if (l.kind == K_DOZER) draw(hrart::IMG_DOZER, cx, y, f);
        else {
          Fx b = f; b.tint = CARCOL[l.col];
          draw(l.kind == K_RACER ? hrart::IMG_RACER_BODY : hrart::IMG_CAR_BODY, cx, y, b);
          draw(l.kind == K_RACER ? hrart::IMG_RACER_TOP : hrart::IMG_CAR_TOP, cx, y, f);
        }
        float hx = right ? x + w : x;   // headlight glow
        glow(hx + (right ? 30 : -30), y + T * K / 2, 70, Color(255, 250, 200, 60));
        break;
      }
    }
  }
}

static void drawBot(float x, float y, float lift, int dirn, bool squash) {
  float cx = X(x + 8), cy = Y(y + 8);
  disc(cx, cy + 22, 26 - lift * 0.2f, Color(0, 0, 0, 70));
  Fx f; f.flip = dirn == 3; f.rot = dirn == 1 ? 8.0f : dirn == 3 ? 8.0f : 0;
  if (squash) { draw(hrart::IMG_BOT_FLAT, cx, cy, f); return; }
  draw(lift > 2 ? hrart::IMG_BOT_HOP : hrart::IMG_BOT, cx, cy - lift, f);
}

static void drawHud() {
  textf(OX, 14, ui::style(44, WHITE, LEFT), "%07lu", (unsigned long)score);
  textf(W / 2, 14, ui::style(44, Color(150, 230, 255)), "LEVEL %d", level);
  textf(X(SW_), 22, ui::style(32, Color(255, 220, 110), RIGHT), "HI %07lu", (unsigned long)hiscore);
  float y = Y(rowY(NROWS)) + 14;
  for (int i = 0; i < std::min(lives - 1, 6); i++) { Fx f; f.sx = f.sy = 0.55f; draw(hrart::IMG_BOT, OX + 30 + i * 52, y + 22, f); }
  text("TIME", X(220), y + 4, ui::style(36, Color(150, 230, 255), RIGHT));
  float tw = 90 * K * 0.95f;
  roundRect(X(226), y + 6, tw, 30, 15, Color(255, 255, 255, 30));
  Color tc = timeLeft < timeMax / 4 ? ((frameNo & 8) ? Color(245, 70, 90) : Color(255, 220, 90)) : Color(100, 225, 110);
  if (timeMax) roundRect(X(226), y + 6, tw * timeLeft / timeMax, 30, 15, tc);
}

static void draw() {
  float ox = shakeX, oy = shakeY;
  drawGround(ox, oy);
  for (int li = 0; li < NLANES; li++) drawLane(li, ox, oy);
  for (int i = 0; i < NDOCK; i++) {   // parked bots and the bonus gem
    float cx = X(dockX(i) + 16) + ox, cy = Y(rowY(0) + 9) + oy;
    if (docks[i]) { Fx f; f.sx = f.sy = 0.9f; draw(hrart::IMG_BOT, cx, cy, f); }
    else if (gemDock == i) {
      glow(cx, cy, 70, Color(255, 120, 210, 120));
      Fx f; f.sx = f.sy = 1 + 0.1f * sinf(frameNo * 0.2f);
      draw(hrart::IMG_GEM, cx, cy, f);
    }
  }
  drawHedges();
  if (state == ST_TITLE) {
    static const char* HELP[] = {"HOP WITH THE ARROWS / D-PAD    FILL ALL FIVE DOCKS"};
    ui::titleScreen(cart::IMG_LOGO_HOPRUSH, "CROSS THE ROAD, RIDE THE RIVER", hiscore, HELP, 1, Color(190, 255, 170));
    return;
  }
  if (state == ST_PLAY || state == ST_HOME) {
    float x = px, y = rowY(prow), lift = 0;
    if (hopT) {
      float t = 1.0f - hopT / 7.0f;
      x = hopFromX + (px - hopFromX) * t;
      y = rowY(hopFromRow) + (rowY(prow) - rowY(hopFromRow)) * t;
      lift = sinf(t * 3.14159f) * 26;
    }
    if (state == ST_PLAY) drawBot(x + ox / K, y + oy / K, lift, hopDir, false);
  } else if (state == ST_DEAD && stateT < 60 && deathKind == 0) drawBot(px, rowY(prow), 0, hopDir, true);
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max, x = X(p.x), y = Y(p.y);
    Color c = mix(p.col, WHITE, t * 0.4f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = 0.4f; f.rot = p.life * 10; draw(cart::IMG_SPARK, x, y, f); }
    else disc(x, y, 7 * t + 2, c);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = 0.3f + r.t * 1.2f; f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, X(r.x), Y(r.y), f);
  }
  for (auto& p : popups) if (p.on) text(p.txt, X(p.x), Y(p.y), ui::style(44, (p.t & 8) ? Color(255, 220, 110) : WHITE, LEFT));
  drawHud();
  if (state == ST_PLAY && stateT < 60 && prow == START_ROW) { char s[24]; snprintf(s, sizeof(s), "LEVEL %d", level); ui::banner(s, W / 2, 470, 60 - stateT, 60, WHITE, 100); }
  if (state == ST_CLEAR) {
    ui::panel(W / 2 - 420, 400, 840, 260);
    ui::banner("LEVEL CLEAR!", W / 2, 440, 150 - stateT + 30, 150, Color(180, 255, 120), 100);
    if (stateT > 20) textf(W / 2, 570, ui::style(52), "BONUS %d", 1000 * level);
  }
  if (state == ST_OVER) ui::gameOver(stateT, newHi);
}

// ------------------------------------------------------------ bot: look one hop ahead in each direction, take the safest that makes progress
static bool safeAt(float x, int row, int lookFrames) {
  if (row == 0) {
    float cx = x + 8;
    for (int i = 0; i < NDOCK; i++) if (!docks[i] && cx >= dockX(i) + 4 && cx <= dockX(i) + 28) return true;
    return false;
  }
  for (int f = 0; f <= lookFrames; f += 2) {   // shift the lanes forward f frames
    for (int li = 0; li < NLANES; li++) laneOff[li] += laneSpd[li] * f;
    bool ok = true;
    if (isRoad(row)) ok = !hitByTraffic(x, row);
    else if (isRiver(row)) ok = platformUnder(x + 8, row) >= 0 && platformUnder(x + 4, row) >= 0 && platformUnder(x + 12, row) >= 0;
    for (int li = 0; li < NLANES; li++) laneOff[li] -= laneSpd[li] * f;
    if (!ok) return false;
  }
  return true;
}
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY || hopT || (frameNo & 1)) return;
  // the nearest open dock
  float target = px;
  float bd = 1e9f;
  for (int i = 0; i < NDOCK; i++) if (!docks[i] && fabsf(dockX(i) + 8 - px) < bd) { bd = fabsf(dockX(i) + 8 - px); target = dockX(i) + 8; }
  float rideX = px;   // on the river she drifts: judge sideways moves where she'll be
  struct Opt { int dx, dy; uint32_t b; } opts[4] = {{0, -1, BTN_UP}, {1, 0, BTN_RIGHT}, {-1, 0, BTN_LEFT}, {0, 1, BTN_DOWN}};
  bool stayOk = safeAt(rideX, prow, 14) || prow == START_ROW || prow == MEDIAN_ROW;
  float best = stayOk ? 0 : -1000;
  uint32_t pick = 0;
  for (auto& o : opts) {
    int r = prow + o.dy;
    float x = px + o.dx * T;
    if (r < 0 || r > START_ROW || x < 4 || x > SW_ - 20) continue;
    if (r == 0 && fabsf(x - target) > 6) continue;
    if (!safeAt(x, r, 16)) continue;
    float s = -o.dy * 10 - fabsf(x - target) * 0.05f + (o.dx ? (((x - target) * (px - target) < 0 || fabsf(x - target) < fabsf(px - target)) ? 3 : -4) : 0);
    if (r == 0) s += 50;
    if (o.dy > 0) s -= 12;
    if (s > best) { best = s; pick = o.b; }
  }
  p.held = pick;
}

static void debugInfo(char* buf, int n) {
  int d = 0;
  for (bool b : docks) d += b;
  snprintf(buf, n, "state %d level %d score %lu lives %d docks %d row %d", state, level, (unsigned long)score, lives, d, prow);
}

static void init() { loadAtlas(hrart::TEX_FILES, hrart::TEX, hrart::NTEX); }
static void enter() {
  state = ST_TITLE; stateT = 0;
  setupLanes();
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace hr

extern const nova::Game HOP_RUSH;
const nova::Game HOP_RUSH = {
  "hoprush", "HOP RUSH", "CROSS THE ROAD, RIDE THE RIVER",
  hr::init, hr::enter, hr::step, hr::draw, hr::canPause, nullptr, hr::bot, hr::debugInfo,
};
