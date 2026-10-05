// =====================================================================
//  MAZE MUNCH HD  -  a maze chase (Nova Arcade HD)
//  Gobble every glow-dot while four wisps hunt you through the maze.
//  Power crystals turn the tables for a few seconds. Each wisp has its
//  own way of hunting: Blaze chases, Pinkie cuts you off, Frost flanks
//  and Ember wanders off when it gets close.
//
//  The original's maze, rules, speeds, timings and wisp AI, unchanged, on
//  its 280 x 220 maze (drawn 4.5x, centred, with the HUD at the sides).
//  New for HD: a glowing neon-tube maze built from the layout by
//  tools/make_art.py, a glossy munch-bot with a smooth chomp and a
//  wide-open death, flickering flame wisps whose eyes look where they're
//  heading, pulsing crystals, sub-pixel smooth motion, sparks, rings and
//  score popups.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include "art.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

using namespace nova;
using namespace nova::audio;

namespace mm {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  108, 4, "C Am F G",
  {I_BELL, 0.7f, 0.1f, "C5 - E5 G5 - - C6 - B5 - G5 - E5 - - - | . . . . . . . . . . . . . . . . | C5 - E5 G5 - - C6 - B5 - G5 - E5 - - - | . . . . . . . . . . . . . . . ."},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.25f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.4f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  132, 4, "C Am F G",
  {I_SQUARE, 0.42f, 0.05f,
   "C5 - G5 - E5 - G5 - C6 - G5 - E5 - D5 - | A4 - E5 - C5 - E5 - A5 - E5 - C5 - B4 - |"
   "F4 - C5 - A4 - C5 - F5 - C5 - A4 - G4 - | G4 - D5 - B4 - D5 - G5 - F5 - D5 - B4 -"},
  {}, I_NONE, nullptr, 0, I_PLUCKBASS, "R...R.5.R...R.5.", 0.6f, I_PAD, 0.2f,
  "K.h.S.h.K.h.S.h.|K.h.S.h.K.h.S.h.|K.h.S.h.K.h.S.h.|K.h.S.h.K.hhS.hS", 0.8f, 0, true};
static const Song SONG_FRIGHT = {   // the original is drums only; HD adds a nervous arpeggio
  170, 2, "Em Em", {}, {}, I_SQUARE, "1538153815381538", 0.22f, I_SUBBASS, "R.R.R.R.R.R.R.R.", 0.5f, I_NONE, 0,
  "K.h.S.h.K.hhS.hS|K.h.S.h.K.hhS.hS", 0.8f, 0, true};
static const Song SONG_OVER = {
  100, 2, "C C", {I_SQUARE, 0.5f, 0, "G5 - F#5 - F5 - E5 - Eb5 - D5 - C5 - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.35f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_CLEAR = {
  140, 2, "C C", {I_MARIMBA, 0.8f, 0, "C5 E5 G5 C6 - - E6 - G6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ maze (the original's)
static const int MCOLS = 28, MROWS = 22, TL = 10;
static const int TUNNEL_ROW = 10;
static const char* const MAZE[MROWS] = {
  "############################",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#o####.#####.##.#####.####o#",
  "#..........................#",
  "#.####.##.########.##.####.#",
  "#......##....##....##......#",
  "######.##### ## #####.######",
  "     #.##          ##.#     ",
  "######.## ###--### ##.######",
  "      .   #      #   .      ",
  "######.## ######## ##.######",
  "     #.##          ##.#     ",
  "######.## ######## ##.######",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#o..##.......  .......##..o#",
  "###.##.##.########.##.##.###",
  "#......##....##....##......#",
  "#.##########.##.##########.#",
  "#..........................#",
  "############################",
};

// screen placement: one maze pixel is 4.5 screen pixels, the maze centred with the HUD either side
static const float K = 4.5f, MOX = (W - MCOLS * TL * K) / 2, MOY = 45, GM = 40;
static inline float X(float x) { return MOX + x * K; }
static inline float Y(float y) { return MOY + y * K; }

static uint8_t dots[MROWS][MCOLS];     // 0 none, 1 dot, 2 power crystal
static int dotsLeft = 0, dotsEaten = 0, dotsTotal = 0;

static inline bool wallAt(int c, int r, bool allowDoor) {
  if (r == TUNNEL_ROW && (c < 0 || c >= MCOLS)) return false;
  if (c < 0 || c >= MCOLS || r < 0 || r >= MROWS) return true;
  char ch = MAZE[r][c];
  return ch == '#' || (ch == '-' && !allowDoor);
}

// ------------------------------------------------------------ actors (maze pixels; tile centre = t*10+5)
static const int8_t DX[4] = {0, -1, 0, 1}, DY[4] = {-1, 0, 1, 0};   // up, left, down, right
static const int HOME_X = 135, HOME_Y = 85;    // tile just above the door
static const int PEN_Y = 105;

struct Actor { int x, y, dir; float acc; };
static Actor pl;
static int want = 1, mouthT = 0;
enum GMode : uint8_t { G_PEN, G_LEAVE, G_ACTIVE, G_EYES, G_ENTER };
struct Wisp { Actor a; GMode mode; int penT; bool scared; };
static Wisp wisps[4];
static const char* const WISP_NAMES[4] = {"BLAZE", "PINKIE", "FROST", "EMBER"};
static const Color WISP_COL[4] = {Color(235, 60, 80), Color(255, 123, 213), Color(62, 198, 224), Color(255, 138, 61)};
static const int SCATTER[4][2] = {{25, -2}, {2, -2}, {27, 23}, {0, 23}};

static int level = 1, lives = 3;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 10000;
static bool newHi = false, extraGiven = false;
static int frightT = 0, frightMax = 1, eatChain = 0, modeT = 0, modeIdx = 0;
static int gemT = 0, freezeT = 0;
enum State { ST_TITLE, ST_READY, ST_PLAY, ST_DYING, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

// ------------------------------------------------------------ cosmetics (screen pixels)
struct Popup { bool on; float x, y; int t; char txt[12]; Color c; };
static Popup popups[6];
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; bool spark; };
static Part parts[500];
struct RingFx { bool on; float x, y, t, size; Color c; };
static RingFx rings[12];
static float lookX[4], lookY[4];               // where each wisp's eyes are looking (eases towards its heading)
static float shake = 0, shakeX = 0, shakeY = 0, munchPulse = 0;
static ui::Stars stars;
static bool fxOn = true;   // off while the bot plays out simulated futures

static void addScore(uint32_t v) {
  score += v;
  if (!extraGiven && score >= 10000) { extraGiven = true; lives++; sfx(SFX_ONEUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void popup(float mx, float my, uint32_t v, Color c = Color(120, 230, 255)) {
  if (!fxOn) return;
  for (auto& p : popups) if (!p.on) { p = {true, X(mx), Y(my), 70, {0}, c}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}
static void burst(float mx, float my, int n, Color col, float sp, bool spark = true) {
  if (!fxOn) return;
  float x = X(mx), y = Y(my);
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.3f, 1) * sp * K;
        p = {true, x, y, cosf(a) * s, sinf(a) * s, frange(16, 34), 34, frange(4, 9), col, spark && i % 2 == 0};
        break;
      }
}
static void ringFx(float mx, float my, float size, Color c) { if (!fxOn) return; for (auto& r : rings) if (!r.on) { r = {true, X(mx), Y(my), 0, size, c}; return; } }

static inline int tileOf(int v) { return (int)floorf(v / (float)TL); }
static inline bool atCentre(const Actor& a) { return ((a.x % TL) + TL) % TL == 5 && ((a.y % TL) + TL) % TL == 5; }
static inline bool canGo(const Actor& a, int d, bool door) { return !wallAt(tileOf(a.x) + DX[d], tileOf(a.y) + DY[d], door); }

// speeds in maze pixels per frame
static float playerSpeed() { return std::min(1.25f + 0.04f * (level - 1), 1.5f); }
static float wispSpeed(const Wisp& w) {
  if (w.mode == G_EYES || w.mode == G_ENTER) return 2.4f;
  if (w.mode != G_ACTIVE) return 0.8f;
  float s = std::min(1.1f + 0.06f * (level - 1), 1.45f) * speed();
  if (w.scared) s *= 0.55f;
  if (tileOf(w.a.y) == TUNNEL_ROW && (w.a.x < 60 || w.a.x > 220)) s *= 0.55f;
  return s;
}
static int frightFrames() { return frames(std::max(90, 420 - (level - 1) * 45)); }

static void buildDots() {
  dotsLeft = 0; dotsEaten = 0;
  for (int r = 0; r < MROWS; r++)
    for (int c = 0; c < MCOLS; c++) {
      char ch = MAZE[r][c];
      dots[r][c] = ch == '.' ? 1 : ch == 'o' ? 2 : 0;
      if (dots[r][c]) dotsLeft++;
    }
  dotsTotal = dotsLeft;
}

static void placeActors() {
  pl = {135, 165, 1, 0}; want = 1;
  for (int i = 0; i < 4; i++) {
    Wisp& w = wisps[i];
    w.scared = false;
    if (i == 0) { w.a = {HOME_X, HOME_Y, 1, 0}; w.mode = G_ACTIVE; w.penT = 0; }
    else { w.a = {115 + i * 10, PEN_Y, 0, 0}; w.mode = G_PEN; w.penT = frames(i * 180); }
    lookX[i] = i == 0 ? -1.0f : 0.0f; lookY[i] = i == 0 ? 0.0f : -1.0f;
  }
  frightT = 0; modeT = 0; modeIdx = 0; freezeT = 0;
}

static void startLevel() {
  buildDots();
  placeActors();
  gemT = 0;
  state = ST_READY; stateT = 0;
  music(nullptr);
}

static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false; extraGiven = false;
  for (auto& p : popups) p.on = false;
  for (auto& p : parts) p.on = false;
  for (auto& r : rings) r.on = false;
  startLevel();
}

// odd schedule slots chase, even ones scatter; after the last slot they chase for good
static const int SCHED[] = {7 * 60, 20 * 60, 7 * 60, 20 * 60, 5 * 60, 20 * 60, 5 * 60};
static bool chasing() { return modeIdx >= 7 || (modeIdx & 1); }

static void wispTarget(int i, int& tx, int& ty) {
  Wisp& w = wisps[i];
  int pc = tileOf(pl.x), pr = tileOf(pl.y);
  if (w.mode == G_EYES) { tx = tileOf(HOME_X); ty = tileOf(HOME_Y); return; }
  if (!chasing()) { tx = SCATTER[i][0]; ty = SCATTER[i][1]; return; }
  switch (i) {
    case 0: tx = pc; ty = pr; break;                                              // straight at you
    case 1: tx = pc + DX[pl.dir] * 4; ty = pr + DY[pl.dir] * 4; break;            // four tiles ahead
    case 2: {                                                                    // mirror Blaze through a point ahead of you
      int ax = pc + DX[pl.dir] * 2, ay = pr + DY[pl.dir] * 2;
      tx = 2 * ax - tileOf(wisps[0].a.x); ty = 2 * ay - tileOf(wisps[0].a.y);
      break;
    }
    default: {                                                                   // chases from afar, retreats up close
      int dx = tileOf(w.a.x) - pc, dy = tileOf(w.a.y) - pr;
      if (dx * dx + dy * dy > 64) { tx = pc; ty = pr; } else { tx = SCATTER[3][0]; ty = SCATTER[3][1]; }
      break;
    }
  }
}

static void chooseDir(int i) {
  Wisp& w = wisps[i];
  int tx, ty; wispTarget(i, tx, ty);
  int best = -1; long bestD = 1L << 30;
  int opts[4], n = 0;
  for (int d = 0; d < 4; d++) {
    if (d == (w.a.dir + 2) % 4) continue;               // no reversing
    if (!canGo(w.a, d, w.mode == G_EYES)) continue;
    opts[n++] = d;
    int nx = tileOf(w.a.x) + DX[d], ny = tileOf(w.a.y) + DY[d];
    long dd = (long)(nx - tx) * (nx - tx) + (long)(ny - ty) * (ny - ty);
    if (dd < bestD) { bestD = dd; best = d; }
  }
  if (n == 0) { w.a.dir = (w.a.dir + 2) % 4; return; }
  w.a.dir = (w.scared && w.mode == G_ACTIVE) ? opts[rnd() % n] : best;
}

static void wrapTunnel(Actor& a) {
  // wrap by a whole number of tiles so tile centres stay aligned
  if (a.x < -10) a.x += MCOLS * TL + 2 * TL;
  else if (a.x > MCOLS * TL + TL) a.x -= MCOLS * TL + 2 * TL;
}

static void moveWisp(int i) {
  Wisp& w = wisps[i];
  w.a.acc += wispSpeed(w);
  while (w.a.acc >= 1) {
    w.a.acc -= 1;
    switch (w.mode) {
      case G_PEN:     // bob up and down until released
        w.a.y += DY[w.a.dir];
        if (w.a.y <= PEN_Y - 3) w.a.dir = 2; else if (w.a.y >= PEN_Y + 3) w.a.dir = 0;
        break;
      case G_LEAVE:   // centre on the door, then float up through it
        if (w.a.x != HOME_X) w.a.x += w.a.x < HOME_X ? 1 : -1;
        else if (w.a.y > HOME_Y) w.a.y--;
        else { w.mode = G_ACTIVE; w.a.dir = 1; }
        break;
      case G_ENTER:   // eyes drop back into the pen, then head out again
        if (w.a.y < PEN_Y) w.a.y++;
        else { w.mode = G_LEAVE; w.scared = false; }
        break;
      default:
        if (atCentre(w.a)) {
          if (w.mode == G_EYES && w.a.x == HOME_X && w.a.y == HOME_Y) { w.mode = G_ENTER; break; }
          chooseDir(i);
        }
        w.a.x += DX[w.a.dir]; w.a.y += DY[w.a.dir];
        wrapTunnel(w.a);
        break;
    }
  }
  if (w.mode == G_PEN && --w.penT <= 0 && state == ST_PLAY) w.mode = G_LEAVE;
}

static void eatAt(int c, int r) {
  if (c < 0 || c >= MCOLS || r < 0 || r >= MROWS || !dots[r][c]) return;
  uint8_t d = dots[r][c];
  dots[r][c] = 0; dotsLeft--; dotsEaten++;
  float pan = (c - 14) / 14.0f * 0.5f;
  if (d == 2) {
    addScore(50);
    frightT = frightMax = frightFrames(); eatChain = 0;
    for (auto& w : wisps) if (w.mode == G_ACTIVE || w.mode == G_PEN || w.mode == G_LEAVE) { w.scared = true; if (w.mode == G_ACTIVE) w.a.dir = (w.a.dir + 2) % 4; }
    sfx(SFX_POWERUP, pan);
    music(&SONG_FRIGHT);
    if (fxOn) rumble(0.35f, 80);
    burst(c * TL + 5, r * TL + 5, 24, Color(182, 255, 110), 1.8f);
    ringFx(c * TL + 5, r * TL + 5, 1.6f, Color(182, 255, 110));
    if (fxOn) shake = 6;
  } else {
    addScore(10);
    sfx(SFX_MOVE, pan);
    if (fxOn) munchPulse = 1;
  }
  if (dotsEaten == 70 || dotsEaten == 170) gemT = 540;
}

static void movePlayer(const Pad& in) {
  if (in.down(BTN_UP)) want = 0; else if (in.down(BTN_DOWN)) want = 2;
  else if (in.down(BTN_LEFT)) want = 1; else if (in.down(BTN_RIGHT)) want = 3;
  if (want == (pl.dir + 2) % 4) pl.dir = want;          // reverse any time
  pl.acc += playerSpeed();
  bool moved = false;
  while (pl.acc >= 1) {
    pl.acc -= 1;
    if (atCentre(pl)) {
      eatAt(tileOf(pl.x), tileOf(pl.y));
      if (canGo(pl, want, false)) pl.dir = want;
      if (!canGo(pl, pl.dir, false)) { pl.acc = 0; break; }
    }
    pl.x += DX[pl.dir]; pl.y += DY[pl.dir];
    wrapTunnel(pl);
    moved = true;
  }
  if (moved) mouthT++;
}

static void loseLife() {
  state = ST_DYING; stateT = 0;
  music(nullptr);
  sfx(SFX_DIE);
  if (fxOn) rumble(0.8f, 500);
}

static void checkCollisions() {
  for (int i = 0; i < 4; i++) {
    Wisp& w = wisps[i];
    if (w.mode != G_ACTIVE) continue;
    if (abs(w.a.x - pl.x) < 7 && abs(w.a.y - pl.y) < 7) {
      if (w.scared) {
        uint32_t v = 200u << std::min(eatChain, 3);
        eatChain++;
        addScore(v);
        popup(w.a.x, w.a.y - 9, v);
        burst(w.a.x, w.a.y, 26, WISP_COL[i], 2.0f);
        ringFx(w.a.x, w.a.y, 1.0f, Color(120, 160, 255));
        w.mode = G_EYES; w.scared = false;
        freezeT = 30;
        sfx(SFX_EXPLODE, (w.a.x - 140) / 280.0f, 1.7f);
        if (fxOn) { rumble(0.4f, 120); shake = 8; }
      } else {
        loseLife();
        return;
      }
    }
  }
  if (gemT && abs(pl.x - 135) < 7 && abs(pl.y - 165) < 7) {
    uint32_t v = 500 * level;
    addScore(v); popup(135, 156, v, Color(255, 150, 230)); gemT = 0;
    burst(135, 165, 30, Color(255, 150, 230), 2.0f);
    ringFx(135, 165, 1.2f, Color(255, 220, 110));
    sfx(SFX_COIN);
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  if (freezeT) { freezeT--; return; }                  // brief pause after eating a wisp
  if (!frightT && modeIdx < 7 && ++modeT >= SCHED[modeIdx]) {
    modeT = 0; modeIdx++;
    for (auto& w : wisps) if (w.mode == G_ACTIVE) w.a.dir = (w.a.dir + 2) % 4;
  }
  if (frightT && --frightT == 0) {
    for (auto& w : wisps) w.scared = false;
    music(&SONG_GAME);
  }
  if (gemT) gemT--;
  movePlayer(in);
  checkCollisions();
  if (state != ST_PLAY) return;
  for (int i = 0; i < 4; i++) moveWisp(i);
  checkCollisions();
  if (state == ST_PLAY && dotsLeft == 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(1000 * level);
    music(&SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.94f; p.vy *= 0.94f; if (--p.life <= 0) p.on = false; }
  for (auto& r : rings) if (r.on && (r.t += 0.035f) >= 1) r.on = false;
  for (auto& p : popups) if (p.on) { p.y -= 0.6f; if (--p.t <= 0) p.on = false; }
  for (int i = 0; i < 4; i++) {
    lookX[i] = approach(lookX[i], DX[wisps[i].a.dir], 0.2f);
    lookY[i] = approach(lookY[i], DY[wisps[i].a.dir], 0.2f);
  }
  stars.step(0.03f, 0.01f);
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  munchPulse *= 0.85f;
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_READY:
      if (in.hit(BTN_START) && stateT > 10) { nova::pause(); break; }
      for (auto& w : wisps) if (w.mode == G_PEN) { w.a.acc += 0.5f; while (w.a.acc >= 1) { w.a.acc -= 1; w.a.y += DY[w.a.dir]; if (w.a.y <= PEN_Y - 3) w.a.dir = 2; else if (w.a.y >= PEN_Y + 3) w.a.dir = 0; } }
      if (stateT > 120) { state = ST_PLAY; stateT = 0; music(&SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DYING:
      if (stateT == 60) {
        burst(pl.x, pl.y, 40, Color(182, 255, 110), 2.4f);
        ringFx(pl.x, pl.y, 1.3f, Color(182, 255, 110));
        shake = 14;
      }
      if (stateT > 130) {
        lives--;
        if (lives <= 0) { state = ST_OVER; stateT = 0; music(&SONG_OVER); if (newHi) saveHi(hiscore); }
        else { placeActors(); state = ST_READY; stateT = 60; }
      }
      break;
    case ST_CLEAR:
      if (stateT > 150) { level++; startLevel(); }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static const Color BG_TOP(7, 5, 24), BG_BOT(16, 8, 36);
static Color bgAt(float y) { return mix(BG_TOP, BG_BOT, clampv(y / H, 0.0f, 1.0f)); }
static const Color TUBES[4] = {Color(130, 105, 255), Color(70, 190, 255), Color(240, 90, 220), Color(60, 225, 170)};
static Color tubeColor() { return TUBES[(level - 1) % 4]; }

static void drawBackground() {
  rectGrad(0, 0, W, H, BG_TOP, BG_BOT);
  glow(W * 0.5f, H * 0.5f, 900, tubeColor().alpha(0.10f));
  stars.draw(Color(170, 180, 255), 2.2f);
}

static void drawMaze(float ox, float oy, float dim = 1) {
  Color tc = tubeColor();
  float white = 0;
  if (state == ST_CLEAR) white = ((stateT >> 4) & 1) ? 1.0f : 0.0f;   // the original flashes white on a clear
  if (frightT > 0 && state == ST_PLAY) tc = mix(tc, Color(70, 80, 255), 0.35f);
  tc = mix(tc, WHITE, white * 0.8f);
  float x = MOX - GM + ox, y = MOY - GM + oy;
  Fx f;
  f.tint = Color(255, 255, 255, (uint8_t)(255 * dim));
  draw(mmart::IMG_MAZE_FILL, x, y, f);
  float pulse = 0.85f + 0.15f * sinf(frameNo * 0.05f);
  f.blend = BLEND_ADD; f.tint = Color(tc.r, tc.g, tc.b, (uint8_t)(150 * pulse * dim + 80 * white));
  draw(mmart::IMG_MAZE_GLOW, x, y, f);
  f.blend = BLEND_ALPHA; f.tint = Color(tc.r, tc.g, tc.b, (uint8_t)(255 * dim));
  draw(mmart::IMG_MAZE_TUBE, x, y, f);
  f.blend = BLEND_ADD; f.tint = Color(100, 100, 100, (uint8_t)(255 * dim));
  draw(mmart::IMG_MAZE_TUBE, x, y, f);
  // the pen door
  float dx0 = X(130) + ox, dy0 = Y(94.5f) + oy;
  glow(dx0 + 45, dy0, 70, Color(255, 123, 213, (uint8_t)(90 * dim)));
  roundRect(dx0 + 2, dy0 - 5, 86, 10, 5, Color(255, 123, 213, (uint8_t)(255 * dim)));
  roundRect(dx0 + 8, dy0 - 2, 74, 3, 1.5f, Color(255, 220, 245, (uint8_t)(220 * dim)));
}

static void drawDots(float ox, float oy) {
  float pulse = 0.5f + 0.5f * sinf(frameNo * 0.12f);
  for (int r = 0; r < MROWS; r++)
    for (int c = 0; c < MCOLS; c++) {
      if (!dots[r][c]) continue;
      float x = X(c * TL + 5) + ox, y = Y(r * TL + 5) + oy;
      if (dots[r][c] == 1) {
        glow(x, y, 20, Color(255, 200, 150, 70));
        disc(x, y, 5.5f, Color(255, 216, 170));
      } else {
        glow(x, y, 60 + 14 * pulse, Color(150, 255, 90, (uint8_t)(90 + 60 * pulse)));
        Fx f; f.sx = f.sy = 0.85f + 0.15f * pulse; f.rot = 8 * sinf(frameNo * 0.05f + c);
        draw(mmart::IMG_CRYSTAL, x, y, f);
        Fx s; s.blend = BLEND_ADD; s.tint = Color(255, 255, 255, (uint8_t)(200 * pulse)); s.sx = s.sy = 0.3f + 0.2f * pulse; s.rot = frameNo * 3.0f;
        draw(cart::IMG_SPARK, x - 6, y - 10, s);
      }
    }
}

// where an actor is drawn: its pixel plus the fraction of the next one it's part way to, so motion is smooth
static void actorPos(const Actor& a, bool moving, float& x, float& y) {
  float fx = a.x, fy = a.y;
  if (moving) { fx += DX[a.dir] * a.acc; fy += DY[a.dir] * a.acc; }
  x = X(fx); y = Y(fy);
}

static void drawMuncherAt(float x, float y, int dir, int frame, float scale = 1, float alpha = 1) {
  Fx f; f.sx = f.sy = scale; f.tint = Color(255, 255, 255, (uint8_t)(255 * alpha));
  if (dir == 1) f.flip = true;
  else if (dir == 0) f.rot = -90;
  else if (dir == 2) f.rot = 90;
  draw(*mmart::MUNCH[clampv(frame, 0, 15)], x, y, f);
}

static void drawPlayer(float ox, float oy) {
  float x, y;
  bool moving = state == ST_PLAY && !freezeT && canGo(pl, pl.dir, false);
  actorPos(pl, moving || !atCentre(pl), x, y);
  x += ox; y += oy;
  if (state == ST_DYING) {
    if (stateT >= 60) return;
    float t = clampv((stateT - 15) / 45.0f, 0.0f, 1.0f);   // the mouth opens right round, then it pops
    glow(x, y, 90, Color(182, 255, 110, (uint8_t)(80 * (1 - t))));
    drawMuncherAt(x, y, 0, 3 + (int)(t * 12.5f), 1 + 0.15f * t);
    return;
  }
  if (state == ST_PLAY && freezeT) return;   // hidden while the eaten wisp's score shows
  glow(x, y, 80, Color(182, 255, 110, 60));
  float open = (state == ST_PLAY || state == ST_CLEAR) ? fabsf(sinf(mouthT * 0.2f)) : 0.35f;
  if (state == ST_CLEAR) open = 0;
  drawMuncherAt(x, y, pl.dir, (int)(open * 4 + 0.5f), 1 + 0.05f * munchPulse);
}

static void drawWispAt(int i, float x, float y, GMode mode, bool scared, float lx, float ly, float alpha = 1) {
  bool eyesOnly = mode == G_EYES || mode == G_ENTER;
  float bob = sinf(frameNo * 0.15f + i * 1.7f) * 2.5f;
  y += bob;
  if (!eyesOnly) {
    Color col = WISP_COL[i];
    bool blink = false;
    if (scared) {
      blink = frightT < 120 && ((frightT >> 3) & 1);
      col = blink ? Color(240, 240, 255) : Color(60, 56, 190);
    }
    glow(x, y + 4, 75, col.alpha(0.32f * alpha));
    Fx f; f.tint = col.alpha(alpha);
    draw(*mmart::WISP[(frameNo / 6 + i) & 3], x, y, f);
    if (scared) {
      Fx ff; ff.tint = (blink ? Color(235, 60, 80) : Color(255, 216, 170)).alpha(alpha);
      draw(mmart::IMG_FRIGHT, x, y + 2, ff);
      return;
    }
  } else {
    glow(x, y, 40, Color(150, 180, 255, 60));
  }
  for (int s = -1; s <= 1; s += 2) {
    float ex = x + s * 10 + lx * 3, ey = y - 5 + ly * 3;
    disc(ex, ey, 8.5f, Color(255, 255, 255, (uint8_t)(255 * alpha)));
    disc(ex + lx * 4, ey + ly * 4, 4.6f, Color(20, 30, 120, (uint8_t)(255 * alpha)));
  }
}

static void drawWisps(float ox, float oy) {
  for (int i = 3; i >= 0; i--) {
    const Wisp& w = wisps[i];
    float x, y;
    bool moving = state == ST_PLAY && !freezeT && (w.mode == G_ACTIVE || w.mode == G_EYES);
    actorPos(w.a, moving, x, y);
    drawWispAt(i, x + ox, y + oy, w.mode, w.scared, lookX[i], lookY[i]);
  }
}

// actors fade into darkness as they go through the side tunnel
static void drawTunnelMouths(float ox, float oy) {
  float y0 = Y(TUNNEL_ROW * TL) - 6 + oy, y1 = Y(TUNNEL_ROW * TL + TL) + 6 + oy;
  Color c = bgAt((y0 + y1) / 2), cc = c.alpha(0);
  float l0 = MOX - 70 + ox, l1 = MOX - 4 + ox, r0 = W - MOX + 4 + ox, r1 = W - MOX + 70 + ox;
  rect(0, y0, l0, y1 - y0, c);
  rect(r1, y0, W - r1, y1 - y0, c);
  Vtx v[12] = {{l0, y0, c}, {l1, y0, cc}, {l1, y1, cc}, {l0, y0, c}, {l1, y1, cc}, {l0, y1, c},
               {r0, y0, cc}, {r1, y0, c}, {r1, y1, c}, {r0, y0, cc}, {r1, y1, c}, {r0, y1, cc}};
  tris(v, 12);
}

static void drawFx(float ox, float oy) {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    Color c = mix(p.col, WHITE, t * 0.5f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 14; f.rot = p.life * 8; draw(cart::IMG_SPARK, p.x + ox, p.y + oy, f); }
    else disc(p.x + ox, p.y + oy, p.size * t, c);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = r.size * (0.3f + r.t * 2.2f); f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, r.x + ox, r.y + oy, f);
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    float k = easeOut((70 - p.t) / 10.0f);
    TextStyle st = ui::style(30 + 14 * k, p.c.alpha(clampv(p.t / 15.0f, 0.0f, 1.0f)));
    st.outline = Color(20, 10, 50, (uint8_t)(255 * clampv(p.t / 15.0f, 0.0f, 1.0f)));
    text(p.txt, p.x + ox, p.y + oy - 22, st);
  }
}

static void drawHud() {
  char buf[32];
  float lx = MOX / 2;
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 110);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 250, Color(255, 220, 110));
  TextStyle l = ui::style(26, Color(150, 170, 220)); l.outline = CLEAR;
  // dots eaten, as a meter
  text("DOTS", lx, 640, l);
  float bw = 220, frac = dotsTotal ? (float)(dotsTotal - dotsLeft) / dotsTotal : 0;
  roundRect(lx - bw / 2, 684, bw, 22, 11, Color(30, 26, 70));
  if (frac > 0.01f) roundRect(lx - bw / 2, 684, std::max(22.0f, bw * frac), 22, 11, Color(255, 216, 170));
  snprintf(buf, sizeof(buf), "%d LEFT", dotsLeft);
  text(buf, lx, 720, ui::style(32));

  float rx = W - MOX / 2;
  snprintf(buf, sizeof(buf), "%d", level);
  ui::stat("LEVEL", buf, rx, 110, Color(150, 230, 255));
  text("LIVES", rx, 250, l);
  for (int i = 0; i < std::min(lives - 1, 6); i++) {
    Fx f; f.sx = f.sy = 0.8f;
    draw(*mmart::MUNCH[3], rx - 56 + (i % 3) * 56, 312 + (i / 3) * 56, f);
  }
  if (frightT > 0 && state == ST_PLAY) {   // how long the wisps stay scared
    text("POWER", rx, 640, ui::style(26, Color(182, 255, 110)));
    float k = (float)frightT / frightMax;
    roundRect(rx - bw / 2, 684, bw, 22, 11, Color(30, 26, 70));
    roundRect(rx - bw / 2, 684, std::max(22.0f, bw * k), 22, 11, frightT < 120 && ((frightT >> 3) & 1) ? WHITE : Color(182, 255, 110));
  }
  if (gemT) {
    Fx f; f.tint = (frameNo & 8) ? Color(255, 140, 220) : Color(255, 220, 110);
    draw(mmart::IMG_GEM, rx, 860, f);
    text("BONUS GEM!", rx, 910, ui::style(28, Color(255, 200, 240)));
  }
}

static void drawGem(float ox, float oy) {
  if (!gemT) return;
  float x = X(135) + ox, y = Y(165) + oy + sinf(frameNo * 0.08f) * 4;
  Color gc = (frameNo & 8) ? Color(255, 123, 213) : Color(255, 216, 74);   // pink and gold by turns, like the original
  glow(x, y, 80, gc.alpha(0.5f));
  Fx f; f.tint = gc; f.sx = f.sy = gemT < 90 && (gemT & 4) ? 0.0f : 0.9f;
  if (f.sx > 0) draw(mmart::IMG_GEM, x, y, f);
  Fx s; s.blend = BLEND_ADD; s.sx = s.sy = 0.4f + 0.2f * sinf(frameNo * 0.2f); s.rot = frameNo * 2.0f;
  draw(cart::IMG_SPARK, x - 12, y - 10, s);
}

static void drawTitle() {
  drawMaze(0, 0, 0.5f);
  drawDots(0, 0);
  static const char* HELP[] = {"STEER WITH THE ARROWS / D-PAD    CLEAR EVERY DOT", "POWER CRYSTALS LET YOU EAT THE WISPS"};
  ui::titleScreen(cart::IMG_LOGO_MAZEMUNCH, "CLEAR THE MAZE, OUTRUN THE WISPS", hiscore, HELP, 2, Color(184, 243, 255));
  // the cast
  for (int i = 0; i < 4; i++) {
    float x = W / 2 + (i - 1.5f) * 250 + 60, y = 912;
    drawWispAt(i, x, y, G_ACTIVE, false, sinf(frameNo * 0.03f + i), 0.3f);
    TextStyle st = ui::style(26, WISP_COL[i]);
    text(WISP_NAMES[i], x, y + 44, st);
  }
  drawMuncherAt(W / 2 - 1.5f * 250 - 140, 912, 3, (int)(fabsf(sinf(frameNo * 0.2f)) * 4 + 0.5f));
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) { stars.draw(Color(170, 180, 255)); drawTitle(); return; }
  float ox = shakeX, oy = shakeY;
  drawMaze(ox, oy);
  drawDots(ox, oy);
  drawGem(ox, oy);
  bool showWisps = state != ST_CLEAR && !(state == ST_DYING && stateT > 40);
  if (showWisps) drawWisps(ox, oy);
  drawPlayer(ox, oy);
  drawTunnelMouths(ox, oy);
  drawFx(ox, oy);
  drawHud();
  if (state == ST_READY) {
    TextStyle st = ui::style(56, Color(255, 216, 74)); st.shadow = 4;
    text("READY!", X(140), Y(116) - 6, st);
    if (level > 1 && stateT < 60) textf(X(140), Y(56), ui::style(56), "LEVEL %d", level);
  }
  if (state == ST_CLEAR && stateT > 40) {
    ui::banner("MAZE CLEAR!", W / 2, 420, 150 - stateT + 40, 110, Color(182, 255, 110), 110);
    if (stateT > 50) textf(W / 2, 560, ui::style(52), "BONUS %d", 1000 * level);
  }
  if (state == ST_OVER) ui::gameOver(stateT, newHi);
}

// ------------------------------------------------------------ bot
// Breadth-first searches over the tile graph: how soon each wisp could reach every tile, how soon we
// can, and then the nearest dot we can reach with time to spare (a crystal when a wisp closes in, a
// scared wisp when there's time to catch it). With nothing safe, run to where we're furthest ahead.
static const int GC = MCOLS + 2;                       // tile columns -1..28 (the tunnel ends)
static const int NODES = GC * MROWS;
static inline int nodeOf(int c, int r) { return r * GC + c + 1; }
static inline bool openTile(int c, int r) { return c >= -1 && c <= MCOLS && r >= 0 && r < MROWS && !wallAt(c, r, false); }
static inline bool step1(int c, int r, int d, int& nc, int& nr) {
  nc = c + DX[d]; nr = r + DY[d];
  if (r == TUNNEL_ROW) { if (nc < -1) nc = MCOLS; else if (nc > MCOLS) nc = -1; }
  return openTile(nc, nr);
}
static inline int clampCol(int c) { return clampv(c, -1, MCOLS); }

// tiles a wisp can reach, in frames: it can't turn back, so the tile behind it costs extra
static void wispReach(float delay, int c, int r, int dir, float fpt, float* best) {
  static int dist[NODES];
  static int q[NODES * 4];
  for (int& d : dist) d = 1 << 20;
  int qh = 0, qt = 0;
  c = clampCol(c);
  if (!openTile(c, r)) return;
  dist[nodeOf(c, r)] = 0;
  // first steps (anything but straight back), then the way back counted as two tiles
  int back = -1;
  for (int d = 0; d < 4; d++) {
    int nc, nr;
    if (!step1(c, r, d, nc, nr)) continue;
    if (dir >= 0 && d == (dir + 2) % 4) { back = nodeOf(nc, nr); continue; }
    dist[nodeOf(nc, nr)] = 1; q[qt++] = nodeOf(nc, nr);
  }
  bool backAdded = back < 0;
  while (qh < qt || !backAdded) {
    if (!backAdded && (qh >= qt || dist[q[qh]] >= 2)) {
      if (dist[back] > 2) { dist[back] = 2; q[qt++] = back; }
      backAdded = true;
      continue;
    }
    int v = q[qh++];
    int vc = v % GC - 1, vr = v / GC;
    for (int d = 0; d < 4; d++) {
      int nc, nr;
      if (!step1(vc, vr, d, nc, nr)) continue;
      int n = nodeOf(nc, nr);
      if (dist[n] <= dist[v] + 1) continue;
      dist[n] = dist[v] + 1; q[qt++] = n;
    }
  }
  for (int i = 0; i < NODES; i++)
    if (dist[i] < (1 << 20)) best[i] = std::min(best[i], delay + dist[i] * fpt);
}

static int botDir() {
  static float danger[NODES];
  for (float& d : danger) d = 1e9f;
  for (int i = 0; i < 4; i++) {
    const Wisp& w = wisps[i];
    int gc = tileOf(w.a.x), gr = tileOf(w.a.y);
    float fpt = TL / std::max(0.3f, wispSpeed(w));
    if (w.mode == G_ACTIVE) {
      if (w.scared && frightT > 70) continue;
      float delay = w.scared ? (float)frightT : 0;
      if (w.scared) fpt = TL / std::min(1.1f + 0.06f * (level - 1), 1.45f) / speed();
      wispReach(delay, gc, gr, w.a.dir, fpt, danger);
    } else if (w.mode == G_LEAVE || (w.mode == G_PEN && w.penT < 150)) {
      float leave = (abs(w.a.x - HOME_X) + abs(w.a.y - HOME_Y)) / 0.8f + (w.mode == G_PEN ? w.penT : 0);
      if (w.scared && frightT > leave + 70) continue;
      wispReach(leave, tileOf(HOME_X), tileOf(HOME_Y), -1, TL / std::min(1.1f + 0.06f * (level - 1), 1.45f) / speed(), danger);
    }
  }
  // our own search, starting where we can next choose a direction
  static int dist[NODES], first[NODES], order[NODES];
  static bool safe[NODES];
  for (int i = 0; i < NODES; i++) { dist[i] = 1 << 20; first[i] = -1; }
  float fpp = TL / playerSpeed();
  int pc = clampCol(tileOf(pl.x)), pr = tileOf(pl.y);
  int off = DX[pl.dir] ? (pl.x - (tileOf(pl.x) * TL + 5)) * DX[pl.dir] : (pl.y - (pr * TL + 5)) * DY[pl.dir];
  int qt = 0;
  int start = nodeOf(pc, pr);
  if (!openTile(pc, pr)) return pl.dir;
  if (off > 0) {   // past the middle of this tile: next stop is the tile ahead, or we turn back
    int nc, nr;
    if (step1(pc, pr, pl.dir, nc, nr)) { int n = nodeOf(nc, nr); dist[n] = 1; first[n] = pl.dir; order[qt++] = n; }
    dist[start] = 1; first[start] = (pl.dir + 2) % 4; order[qt++] = start;
  } else {
    dist[start] = 0; order[qt++] = start;
  }
  const float MARGIN = 16;
  // tiles count as safe when we'd get there well ahead of any wisp; the search only goes through safe ones
  auto isSafe = [&](int n, int d, float margin) { return d * fpp + margin < danger[n]; };
  for (int i = 0; i < NODES; i++) safe[i] = false;
  int nOrder = 0;
  {
    int qq[NODES]; int h = 0, t = 0;
    for (int i = 0; i < qt; i++) { qq[t++] = order[i]; safe[order[i]] = isSafe(order[i], dist[order[i]], MARGIN) || dist[order[i]] == 0; }
    while (h < t) {
      int v = qq[h++];
      order[nOrder++] = v;
      if (!safe[v]) continue;
      int vc = v % GC - 1, vr = v / GC;
      for (int d = 0; d < 4; d++) {
        int nc, nr;
        if (!step1(vc, vr, d, nc, nr)) continue;
        int n = nodeOf(nc, nr);
        if (dist[n] < (1 << 20)) continue;
        dist[n] = dist[v] + 1;
        first[n] = first[v] >= 0 ? first[v] : d;
        safe[n] = isSafe(n, dist[n], MARGIN);
        qq[t++] = n;
      }
    }
  }
  bool threatened = danger[start] < 70 + dist[start] * fpp;
  int nDots1 = 0;
  for (int r = 0; r < MROWS; r++) for (int c = 0; c < MCOLS; c++) nDots1 += dots[r][c] == 1;
  int goal = -1, prio = 99;
  for (int i = 0; i < nOrder; i++) {
    int v = order[i];
    if (!safe[v] || first[v] < 0) continue;
    int vc = v % GC - 1, vr = v / GC;
    float tp = dist[v] * fpp;
    int p = 99;
    for (int k = 0; k < 4 && p > 0; k++) {   // a scared wisp we can catch
      const Wisp& w = wisps[k];
      if (w.mode == G_ACTIVE && w.scared && clampCol(tileOf(w.a.x)) == vc && tileOf(w.a.y) == vr && tp + 20 < frightT && dist[v] <= 16) p = 0;
    }
    if (p > 1 && vc >= 0 && vc < MCOLS) {
      uint8_t d = dots[vr][vc];
      if (d == 2 && (threatened || nDots1 == 0) && dist[v] <= 12) p = 1;
      else if (gemT && vc == 13 && vr == 16 && tp + 30 < gemT && dist[v] <= 12) p = 2;
      else if (d == 1) p = 3;
      else if (d == 2) p = 4;
    }
    if (p < prio) { prio = p; goal = v; if (p == 0) break; }
  }
  if (goal >= 0 && prio <= 3) return first[goal];
  if (goal >= 0 && !threatened) return first[goal];
  // nowhere safe to eat: head for the tile where we'd be furthest ahead of the wisps
  float bestGap = -1e9f; int bestDir = pl.dir;
  for (int i = 0; i < nOrder; i++) {
    int v = order[i];
    if (first[v] < 0) continue;
    float gap = std::min(danger[v], 400.0f) - dist[v] * fpp + dist[v] * 0.5f;
    if (gap > bestGap) { bestGap = gap; bestDir = first[v]; }
  }
  if (goal >= 0 && bestGap < 40) return first[goal];
  return bestDir;
}

// The searches can't see wisps turning round or boxing us in, so each choice is checked by playing the
// game forward a second: if the plan gets us caught, try each direction held for a moment instead and
// take the one that survives longest (then eats most).
struct Snap {
  Actor pl; int want, mouthT; Wisp wisps[4];
  int level, lives; uint32_t score, hiscore; bool newHi, extraGiven;
  int frightT, frightMax, eatChain, modeT, modeIdx, gemT, freezeT, stateT;
  State state; uint8_t dots[MROWS][MCOLS]; int dotsLeft, dotsEaten;
};
static void save(Snap& s) {
  s.pl = pl; s.want = want; s.mouthT = mouthT; memcpy(s.wisps, wisps, sizeof(wisps));
  s.level = level; s.lives = lives; s.score = score; s.hiscore = hiscore; s.newHi = newHi; s.extraGiven = extraGiven;
  s.frightT = frightT; s.frightMax = frightMax; s.eatChain = eatChain; s.modeT = modeT; s.modeIdx = modeIdx;
  s.gemT = gemT; s.freezeT = freezeT; s.stateT = stateT; s.state = state;
  memcpy(s.dots, dots, sizeof(dots)); s.dotsLeft = dotsLeft; s.dotsEaten = dotsEaten;
}
static void load(const Snap& s) {
  pl = s.pl; want = s.want; mouthT = s.mouthT; memcpy(wisps, s.wisps, sizeof(wisps));
  level = s.level; lives = s.lives; score = s.score; hiscore = s.hiscore; newHi = s.newHi; extraGiven = s.extraGiven;
  frightT = s.frightT; frightMax = s.frightMax; eatChain = s.eatChain; modeT = s.modeT; modeIdx = s.modeIdx;
  gemT = s.gemT; freezeT = s.freezeT; stateT = s.stateT; state = s.state;
  memcpy(dots, s.dots, sizeof(dots)); dotsLeft = s.dotsLeft; dotsEaten = s.dotsEaten;
}
static const uint32_t DIRBTN[4] = {BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT};

// play forward holding d0 for `hold` frames, then following the searches; returns frames survived
static int rollout(int d0, int hold, int total, uint32_t& gained) {
  uint32_t s0 = score;
  int f = 0;
  for (; f < total; f++) {
    Pad p; p.held = DIRBTN[f < hold ? d0 : botDir()];
    updatePlay(p);
    if (state == ST_DYING) break;
    if (state == ST_CLEAR) { f = total + 1; break; }
  }
  gained = score - s0;
  return f;
}

static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY) return;
  int d = botDir();
  if (!freezeT) {
    static Snap snap;
    save(snap);
    fxOn = false; audio::mute++;
    const int LOOK = 70;
    uint32_t g;
    int best = rollout(d, 0, LOOK, g);
    load(snap);
    if (best < LOOK) {
      uint32_t bestG = g;
      for (int k = 0; k < 4; k++) {
        if (k == d) continue;
        int t = rollout(k, 16, LOOK, g);
        load(snap);
        if (t > best || (t == best && g > bestG)) { best = t; bestG = g; d = k; }
      }
    }
    fxOn = true; audio::mute--;
  }
  p.held = DIRBTN[d];
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d level %d score %lu lives %d dots %d/%d", state, level, (unsigned long)score, lives, dotsLeft, dotsTotal);
}

static void init() { loadAtlas(mmart::TEX_FILES, mmart::TEX, mmart::NTEX); }
static void enter() {
  state = ST_TITLE; stateT = 0;
  stars.init(90);
  level = 1;
  buildDots();
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace mm

extern const nova::Game MAZE_MUNCH;
const nova::Game MAZE_MUNCH = {
  "mazemunch", "MAZE MUNCH", "CLEAR THE MAZE, OUTRUN THE WISPS",
  mm::init, mm::enter, mm::step, mm::draw, mm::canPause, nullptr, mm::bot, mm::debugInfo,
};
