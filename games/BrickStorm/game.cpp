// =====================================================================
//  BRICK STORM HD  -  a brick-breaker (Nova Arcade HD)
//  8 hand-made stages that loop faster; multi-hit silver, unbreakable gold
//  and explosive bricks; power-ups: Wide, Multi-ball, Laser, Slow, Catch
//  and an extra life.
//
//  Gameplay runs in the original's 320x240 units (as floats), so the feel
//  is unchanged; everything is drawn at 1920x1080 through X()/Y(): glossy
//  bricks, a glowing ball with a light trail, lit capsules and lasers.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include <stdio.h>
#include <algorithm>

using namespace nova;
using namespace nova::audio;

namespace bs {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  110, 4, "C Am F G",
  {I_BELL, 0.75f, 0.1f, "C6 - - - B5 - G5 - E5 - - - G5 - - - | . . . . . . . . . . . . . . . . | C6 - - - B5 - G5 - E5 - - - G5 - - - | . . . . . . . . . . . . . . . ."},
  {}, I_MARIMBA, "1.3.5.8.5.3.1.3.", 0.3f, I_SUBBASS, "R.......R...5...", 0.5f, I_PAD, 0.4f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  132, 8, "C Am F G C Am F G",
  {I_SAWLEAD, 0.55f, 0.05f,
   "C5 - E5 G5 C6 - G5 E5 F5 - E5 - D5 - C5 - | E5 - C5 E5 A5 - G5 E5 D5 - C5 - A4 - - - |"
   "F5 - A5 - C6 - A5 F5 G5 - F5 - E5 - D5 - | G5 - B5 - D6 - B5 - G5 - A5 - B5 - - - |"
   "C5 - E5 G5 C6 - G5 E5 F5 - E5 - D5 - C5 - | E5 - C5 E5 A5 - G5 E5 D5 - C5 - A4 - - - |"
   "F5 - A5 - C6 - A5 F5 G5 - F5 - E5 - D5 - | G5 - B5 - D6 - B5 - G5 - A5 - B5 - - -"},
  {}, I_MARIMBA, "1.5.3.5.1.5.3.5.", 0.2f, I_PLUCKBASS, "R.RR..R.R.RR..5.", 0.6f, I_STRINGS, 0.22f,
  "K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.h.S.h.K.hKS.hh|K.S.K.S.K.S.SSSS",
  0.85f, 0, true};
static const Song SONG_CLEAR = {
  140, 2, "C C", {I_MARIMBA, 0.8f, 0, "C5 E5 G5 C6 E6 - - - C6 - E6 - G6 - - - | . . . . . . . . . . . . . . . ."},
  {I_BELL, 0.4f, 0.3f, ". . . . C6 - - - . . . . E6 - - - | . . . . . . . . . . . . . . . ."},
  I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};
static const Song SONG_OVER = {
  96, 2, "Am Am", {I_FLUTE, 0.8f, 0, "E5 - D5 - C5 - A4 - G4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ the original's coordinates -> HD
static const float K = 4.5f, OX = (W - 320 * K) / 2;
static inline float X(float x) { return OX + x * K; }
static inline float Y(float y) { return y * K; }

static const int BCOLS = 13, BROWS = 12, BW = 22, BH = 10;
static const int FX = 8, FY = 16, FR = 312;        // playfield walls
static const int GX = 17, GY = 34;                 // brick grid origin
static const char* const STAGES[8][BROWS] = {
  {"", "1111111111111", "2222222222222", "3333333333333", "4444444444444", "5555555555555", "6666666666666"},
  {"", "......S......", ".....S1S.....", "....S222S....", "...S33333S...", "..S4444444S..", ".S555555555S.", "S66666666666S"},
  {"", "1.2.3.4.5.6.1", ".2.3.4.5.6.1.", "G.G.G.G.G.G.G", "3.4.5.6.1.2.3", ".4.5.6.1.2.3.", "5.6.1.2.3.4.5"},
  {"", "......6......", ".....656.....", "....65X56....", "...6554556...", "..655X4X556..", "...6554556...", "....65X56....", ".....656.....", "......6......"},
  {"", "SSSSSSSSSSSSS", "1111111111111", "G22222G22222G", "3333333333333", "4444X444X4444", "SSSSSSSSSSSSS"},
  {"", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "1.2.3.4.5.6.1", "S.S.S.S.S.S.S", "X.X.X.X.X.X.X"},
  {"", "GGGG.....GGGG", "G11G.....G11G", "G11GSSSSSG11G", "G22222222222G", "G33X33333X33G", "G44444444444G", "G55555555555G", "GGGGGG.GGGGGG"},
  {"", "X1X2X3X4X5X6X", "SSSSSSSSSSSSS", "6543216543216", "1234561234561", "SSSSSSSSSSSSS", "X6X5X4X3X2X1X", "G...G...G...G"},
};
static const Color BCOL[10] = {Color(0, 0, 0), Color(245, 70, 90), Color(255, 150, 50), Color(255, 214, 60), Color(90, 225, 105),
                               Color(70, 180, 250), Color(180, 90, 240), Color(200, 205, 220), Color(240, 190, 70), Color(255, 90, 50)};
struct Brick { uint8_t kind, hp, flash; };   // kind: 0 empty, 1-6 colour, 7 silver, 8 gold, 9 explosive
static Brick bricks[BROWS][BCOLS];
static int bricksLeft = 0;

// ------------------------------------------------------------ entities (original units)
struct Ball { bool on; float x, y, vx, vy; bool stuck; float stuckOff; float tx[10], ty[10]; };
static Ball balls[8];
struct Cap { bool on; float x, y; uint8_t type; };
static Cap caps[6];
struct Beam { bool on; float x, y; };
static Beam beams[12];
static float padX = 160, padW = 40;
static const int PAD_Y = 222;
static int lives = 3, stage = 0, loopN = 0;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 15000;
static float ballSpeed = 3.0f;
static int combo = 0;
static int laserT = 0, slowT = 0, wideT = 0, catchT = 0, laserCd = 0;
enum State { ST_TITLE, ST_PLAY, ST_CLEAR, ST_OVER, ST_LOST };
static State state = ST_TITLE;
static int stateT = 0;
static bool newHi = false;
enum { CAP_E, CAP_M, CAP_L, CAP_S, CAP_C, CAP_1, NCAP };
static const char* const CAPCH[NCAP] = {"W", "M", "L", "S", "C", "+"};
static const char* const CAPNAME[NCAP] = {"WIDE", "MULTI", "LASER", "SLOW", "CATCH", "LIFE"};
static const Color CAPCOL[NCAP] = {Color(70, 180, 250), Color(255, 150, 50), Color(245, 70, 90), Color(90, 225, 105), Color(180, 90, 240), Color(250, 250, 255)};

// effects (HD units)
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; bool spark; };
static Part parts[500];
static float shake = 0, shakeX = 0, shakeY = 0, padFlash = 0, flash = 0;
struct Ring { bool on; float x, y, t; Color c; };
static Ring rings[16];

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, bool spark = false) {   // x, y in original units
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.3f, 1.0f) * sp * K;
        p = {true, X(x), Y(y), cosf(a) * s, sinf(a) * s, frange(16, 34), 34, frange(4, 10), col, spark && (i % 3 == 0)};
        break;
      }
}
static void ring(float x, float y, Color c) {
  for (auto& r : rings) if (!r.on) { r = {true, X(x), Y(y), 0, c}; return; }
}

static void loadStage(int n) {
  memset(bricks, 0, sizeof(bricks));
  bricksLeft = 0;
  const char* const* rows = STAGES[n % 8];
  for (int r = 0; r < BROWS; r++) {
    const char* s = rows[r];
    if (!s) break;
    for (int c = 0; c < BCOLS && s[c]; c++) {
      char ch = s[c];
      Brick& b = bricks[r][c];
      if (ch >= '1' && ch <= '6') { b.kind = ch - '0'; b.hp = 1; }
      else if (ch == 'S') { b.kind = 7; b.hp = 2 + loopN; }
      else if (ch == 'G') { b.kind = 8; b.hp = 255; }
      else if (ch == 'X') { b.kind = 9; b.hp = 1; }
      if (b.kind && b.kind != 8) bricksLeft++;
    }
  }
}

static void resetBall() {
  for (auto& b : balls) b.on = false;
  Ball& b = balls[0];
  b = Ball();
  b.on = true; b.x = padX; b.y = PAD_Y - 4; b.stuck = true;
  for (int i = 0; i < 10; i++) { b.tx[i] = b.x; b.ty[i] = b.y; }
  for (auto& c : caps) c.on = false;
  for (auto& bm : beams) bm.on = false;
  laserT = slowT = wideT = catchT = 0;
  padW = 40;
  combo = 0;
}

static void startStage() {
  loadStage(stage);
  ballSpeed = (3.0f + 0.35f * loopN + 0.05f * (stage % 8)) * speed();
  resetBall();
  state = ST_PLAY; stateT = 0;
}

static void resetGame() {
  score = 0; lives = 3; stage = 0; loopN = 0; newHi = false; padX = 160;
  for (auto& p : parts) p.on = false;
  startStage();
}

static void spawnCap(float x, float y) {
  if (rnd() % 100 >= 14) return;
  for (auto& c : caps)
    if (!c.on) {
      int r = rnd() % 100;
      c = {true, x - 8, y, (uint8_t)(r < 22 ? CAP_E : r < 42 ? CAP_M : r < 60 ? CAP_L : r < 75 ? CAP_S : r < 92 ? CAP_C : CAP_1)};
      return;
    }
}

static void hitBrick(int r, int c, bool byBeam = false);
static void explodeAt(int r, int c) {
  addScore(50);
  rumble(0.5f, 120);
  shake = fmaxf(shake, 14.0f);
  flash = fmaxf(flash, 0.25f);
  sfx(SFX_EXPLODE, 0, 0.75f);
  float cx = GX + c * BW + BW / 2.0f, cy = GY + r * BH + BH / 2.0f;
  burst(cx, cy, 26, Color(255, 190, 70), 2.5f, true);
  ring(cx, cy, Color(255, 160, 60));
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      int rr = r + dr, cc = c + dc;
      if ((dr || dc) && rr >= 0 && rr < BROWS && cc >= 0 && cc < BCOLS && bricks[rr][cc].kind && bricks[rr][cc].kind != 8) {
        bricks[rr][cc].hp = 1;
        hitBrick(rr, cc, true);
      }
    }
}

static void hitBrick(int r, int c, bool byBeam) {
  Brick& b = bricks[r][c];
  if (!b.kind) return;
  b.flash = 8;
  if (b.kind == 8) { sfx(SFX_BUMP, 0, 1.4f); return; }
  if (--b.hp > 0) { sfx(SFX_BUMP, 0, 1.1f); addScore(20); return; }
  uint8_t k = b.kind;
  b.kind = 0;
  bricksLeft--;
  combo++;
  addScore(k == 7 ? 100 : 60 + std::min(combo, 10) * 10);
  sfx(SFX_COIN, 0, 0.75f + 0.04f * std::min(combo, 20));
  float cx = GX + c * BW + BW / 2.0f, cy = GY + r * BH + BH / 2.0f;
  burst(cx, cy, 12, BCOL[k], 1.8f, true);
  if (k == 9) explodeAt(r, c);
  else if (!byBeam) spawnCap(cx, cy);
}

static bool brickAt(float x, float y, int& r, int& c) {
  if (x < GX || y < GY) return false;
  c = (int)((x - GX) / BW); r = (int)((y - GY) / BH);
  if (c < 0 || c >= BCOLS || r < 0 || r >= BROWS) return false;
  return bricks[r][c].kind != 0;
}

static void launch(Ball& b) {
  float off = clampv(b.stuckOff / (padW / 2), -1.0f, 1.0f);
  float ang = -1.5708f + off * 0.9f;
  if (fabsf(off) < 0.05f) ang = -1.5708f + 0.25f;   // never perfectly vertical
  b.vx = cosf(ang) * ballSpeed; b.vy = sinf(ang) * ballSpeed; b.stuck = false;
  sfx(SFX_JUMP, 0, 1.3f);
}

static void updateBall(Ball& b) {
  for (int i = 9; i > 0; i--) { b.tx[i] = b.tx[i - 1]; b.ty[i] = b.ty[i - 1]; }
  b.tx[0] = b.x; b.ty[0] = b.y;
  if (b.stuck) { b.x = padX + b.stuckOff; b.y = PAD_Y - 4; return; }
  float spd = slowT ? ballSpeed * 0.65f : ballSpeed;
  float len = sqrtf(b.vx * b.vx + b.vy * b.vy);
  if (len > 0.01f) { b.vx = b.vx / len * spd; b.vy = b.vy / len * spd; }
  const int sub = 3;
  for (int s = 0; s < sub; s++) {
    float nx = b.x + b.vx / sub;
    int r, c;
    if (brickAt(nx + (b.vx > 0 ? 3 : -3), b.y, r, c)) { b.vx = -b.vx; hitBrick(r, c); }
    else b.x = nx;
    float ny = b.y + b.vy / sub;
    if (brickAt(b.x, ny + (b.vy > 0 ? 3 : -3), r, c)) { b.vy = -b.vy; hitBrick(r, c); }
    else b.y = ny;
    if (b.x < FX + 3) { b.x = FX + 3; b.vx = fabsf(b.vx); sfx(SFX_BUMP, -0.8f, 1.6f); }
    if (b.x > FR - 3) { b.x = FR - 3; b.vx = -fabsf(b.vx); sfx(SFX_BUMP, 0.8f, 1.6f); }
    if (b.y < FY + 3) { b.y = FY + 3; b.vy = fabsf(b.vy); sfx(SFX_BUMP, 0, 1.7f); }
    if (b.vy > 0 && b.y + 3 >= PAD_Y && b.y + 3 <= PAD_Y + 7 && b.x > padX - padW / 2 - 3 && b.x < padX + padW / 2 + 3) {
      float off = clampv((b.x - padX) / (padW / 2), -1.0f, 1.0f);
      float ang = -1.5708f + off * 1.05f;
      b.vx = cosf(ang) * spd; b.vy = sinf(ang) * spd;
      b.y = PAD_Y - 3;
      combo = 0;
      padFlash = 1;
      ballSpeed = std::min(ballSpeed + 0.03f * speed(), (5.6f + 0.3f * loopN) * speed());
      sfx(SFX_STOMP, 0, 1.2f);
      burst(b.x, PAD_Y, 5, Color(160, 230, 255), 1.2f);
      if (catchT) { b.stuck = true; b.stuckOff = b.x - padX; }
    }
  }
  if (fabsf(b.vy) < 0.6f) b.vy = b.vy < 0 ? -0.6f : 0.6f;   // stop near-horizontal loops
  if (b.y > 240 + 4) b.on = false;
}

static void applyCap(uint8_t t) {
  sfx(SFX_POWERUP);
  addScore(100);
  switch (t) {
    case CAP_E: wideT = 900; break;
    case CAP_M: {
      Ball* src = nullptr;
      for (auto& b : balls) if (b.on) { src = &b; break; }
      if (!src) break;
      if (src->stuck) launch(*src);
      for (int k = 0; k < 2; k++)
        for (auto& b : balls)
          if (!b.on) {
            float ang = atan2f(src->vy, src->vx) + (k ? 0.5f : -0.5f);
            b = *src;
            b.vx = cosf(ang) * ballSpeed; b.vy = sinf(ang) * ballSpeed; b.stuck = false;
            break;
          }
      break;
    }
    case CAP_L: laserT = 720; break;
    case CAP_S: slowT = 600; break;
    case CAP_C: catchT = 900; break;
    case CAP_1: lives = std::min(lives + 1, 9); sfx(SFX_ONEUP); break;
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  float target = wideT ? 64 : 40;
  padW += (target - padW) * 0.2f;
  padX += in.ax * 5.0f;
  padX = clampv(padX, FX + padW / 2, FR - padW / 2);
  if (wideT) wideT--;
  if (slowT) slowT--;
  if (catchT) catchT--;
  if (laserT) laserT--;
  bool fire = in.hit(BTN_A | BTN_R | BTN_X | BTN_B);
  for (auto& b : balls) if (b.on && b.stuck && fire) { launch(b); fire = false; }
  if (laserT && in.down(BTN_A | BTN_R | BTN_X | BTN_B) && --laserCd <= 0) {
    laserCd = 12;
    for (int s = -1; s <= 1; s += 2)
      for (auto& bm : beams) if (!bm.on) { bm = {true, padX + s * (padW / 2 - 4), (float)PAD_Y - 4}; break; }
    sfx(SFX_SHOOT, 0, 1.3f);
  }
  for (auto& bm : beams) {
    if (!bm.on) continue;
    bm.y -= 7;
    int r, c;
    if (brickAt(bm.x, bm.y, r, c)) { hitBrick(r, c, true); bm.on = false; burst(bm.x, bm.y, 4, Color(255, 140, 140), 1); }
    else if (bm.y < FY) bm.on = false;
  }
  int alive = 0;
  for (auto& b : balls) if (b.on) { updateBall(b); if (b.on) alive++; }
  for (auto& c : caps) {
    if (!c.on) continue;
    c.y += 1.3f * speed();
    if (c.y + 8 >= PAD_Y && c.y <= PAD_Y + 6 && c.x + 16 > padX - padW / 2 && c.x < padX + padW / 2) {
      c.on = false; applyCap(c.type); ring(c.x + 8, c.y + 4, CAPCOL[c.type]);
    } else if (c.y > 240) c.on = false;
  }
  if (!alive) {
    lives--;
    sfx(SFX_DIE);
    rumble(0.6f, 300);
    burst(padX, PAD_Y, 40, Color(100, 220, 255), 2.5f, true);
    shake = 12;
    state = lives > 0 ? ST_LOST : ST_OVER; stateT = 0;
    if (state == ST_OVER) { music(&SONG_OVER); if (newHi) saveHi(hiscore); }
  }
  if (bricksLeft <= 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(1000 * (loopN + 1));
    music(&SONG_CLEAR);
    flash = 0.4f;
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += 0.27f; p.vx *= 0.99f;
    if (--p.life <= 0) p.on = false;
  }
  for (auto& r : rings) if (r.on && (r.t += 0.04f) >= 1) r.on = false;
  for (auto& row : bricks) for (auto& b : row) if (b.flash) b.flash--;
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  padFlash *= 0.85f;
  flash *= 0.9f;
}

static float dbx = 60, dby = 150, dvx = 2.2f, dvy = -1.7f;   // the title screen's bouncing ball

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      dbx += dvx; dby += dvy;
      if (dbx < 10 || dbx > 310) dvx = -dvx;
      if (dby < 20 || dby > 230) dvy = -dvy;
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); music(&SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_LOST:
      if (stateT > 70) { resetBall(); state = ST_PLAY; }
      break;
    case ST_CLEAR:
      if (stateT > 150) {
        stage++;
        if (stage % 8 == 0) loopN++;
        startStage();
        music(&SONG_GAME);
      }
      break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static const Color STAGE_BG[4][2] = {{Color(12, 14, 46), Color(46, 12, 66)}, {Color(36, 10, 46), Color(80, 22, 46)},
                                     {Color(8, 30, 48), Color(22, 56, 80)}, {Color(36, 24, 10), Color(80, 36, 46)}};

static void drawBackground() {
  const Color* c = STAGE_BG[stage & 3];
  rectGrad(0, 0, W, H, c[0], c[1]);
  // drifting diagonal light bands
  float off = fmodf(frameNo * 0.6f, 240.0f);
  for (int i = -2; i < 14; i++) {
    float x = i * 240 + off - 400;
    Vtx v[6] = {{x, 0, Color(255, 255, 255, 8)}, {x + 120, 0, Color(255, 255, 255, 8)}, {x + 120 + 600, (float)H, Color(255, 255, 255, 0)},
                {x, 0, Color(255, 255, 255, 8)}, {x + 120 + 600, (float)H, Color(255, 255, 255, 0)}, {x + 600, (float)H, Color(255, 255, 255, 0)}};
    tris(v, 6);
  }
  glow(W / 2, Y(120), 900, c[1].alpha(0.5f));
}

static void drawWalls() {
  float l = X(FX), r = X(FR), t = Y(FY);
  Color edge(140, 120, 255);
  rect(l - 22, t - 22, 22, H, Color(40, 32, 90));
  rect(r, t - 22, 22, H, Color(40, 32, 90));
  rect(l - 22, t - 22, r - l + 44, 22, Color(40, 32, 90));
  rect(l - 4, t - 4, 4, H, edge); rect(r, t - 4, 4, H, edge); rect(l - 4, t - 4, r - l + 8, 4, edge);
  glow(l, H / 2, 160, edge.alpha(0.12f)); glow(r, H / 2, 160, edge.alpha(0.12f));
}

static void drawBrick(int r, int c) {
  const Brick& b = bricks[r][c];
  float x = X(GX + c * BW) + shakeX, y = Y(GY + r * BH) + shakeY;
  float w = BW * K - 4, h = BH * K - 4;
  Color col = BCOL[b.kind];
  if (b.kind == 7) col = mix(col, Color(90, 96, 120), (2 + loopN - b.hp) / (float)(2 + loopN));
  if (b.flash) col = mix(col, WHITE, b.flash / 8.0f);
  if (b.kind == 9) glow(x + w / 2, y + h / 2, 70, Color(255, 100, 40, (uint8_t)(60 + 50 * sinf(frameNo * 0.2f + c))));
  drawRect(cart::IMG_BRICK, x, y, w, h, col);
  if (b.kind == 8) {   // gold: a glint runs across
    float g = fmodf(frameNo * 0.02f + c * 0.07f + r * 0.11f, 2.0f);
    if (g < 1) line(x + g * w - 10, y + h - 6, x + g * w + 10, y + 6, 8, Color(255, 255, 220, (uint8_t)(150 * sinf(g * 3.14f))), BLEND_ADD);
  }
  if (b.kind == 9) {   // explosive: a pulsing core
    float p = 0.5f + 0.5f * sinf(frameNo * 0.25f);
    disc(x + w / 2, y + h / 2, 9 + 3 * p, Color(255, 240, 120, (uint8_t)(160 + 90 * p)));
  }
  if (b.kind == 7 && b.hp < 2 + loopN) {   // cracks on damaged silver
    line(x + w * 0.3f, y + 6, x + w * 0.45f, y + h * 0.6f, 3, Color(60, 60, 80, 200));
    line(x + w * 0.45f, y + h * 0.6f, x + w * 0.38f, y + h - 6, 3, Color(60, 60, 80, 200));
  }
}

static void drawPaddle() {
  if (state == ST_LOST || state == ST_OVER) return;
  float w = padW * K, x = X(padX) - w / 2 + shakeX, y = Y(PAD_Y) + shakeY, h = 7 * K;
  Color cap = laserT ? Color(245, 70, 90) : catchT ? Color(180, 90, 240) : Color(70, 200, 250);
  glow(x + w / 2, y + h / 2, w * 0.8f, cap.alpha(0.25f + 0.3f * padFlash));
  roundRect(x, y, w, h, h / 2, Color(30, 30, 60));
  roundRect(x + 3, y + 3, w - 6, h - 6, h / 2 - 3, mix(Color(200, 210, 235), WHITE, padFlash));
  roundRect(x + 10, y + 6, w - 20, 6, 3, Color(255, 255, 255, 200));
  roundRect(x + 3, y + 3, 36, h - 6, (h - 6) / 2, cap);
  roundRect(x + w - 39, y + 3, 36, h - 6, (h - 6) / 2, cap);
  if (laserT) { rect(x + 14, y - 14, 8, 14, cap); rect(x + w - 22, y - 14, 8, 14, cap); }
}

static void drawBall(const Ball& b) {
  float x = X(b.x) + shakeX, y = Y(b.y) + shakeY;
  if (!b.stuck)
    for (int i = 9; i >= 0; i--) {
      float k = 1 - i / 10.0f;
      disc(X(b.tx[i]) + shakeX, Y(b.ty[i]) + shakeY, 12 * k, Color(140, 210, 255, (uint8_t)(90 * k)), BLEND_ADD);
    }
  glow(x, y, 60, Color(160, 220, 255, 140));
  Fx f; f.sx = f.sy = 15 / 32.0f;
  draw(cart::IMG_ORB, x, y, f);
}

static void drawCaps() {
  for (auto& c : caps) {
    if (!c.on) continue;
    float x = X(c.x) + shakeX, y = Y(c.y) + shakeY, w = 16 * K, h = 8 * K;
    Color cc = CAPCOL[c.type];
    glow(x + w / 2, y + h / 2, 70, cc.alpha(0.35f));
    roundRect(x, y, w, h, h / 2, Color(20, 20, 40));
    roundRect(x + 3, y + 3, w - 6, h - 6, (h - 6) / 2, cc);
    roundRect(x + 10, y + 6, w - 20, 7, 3, Color(255, 255, 255, 170));
    TextStyle st = ui::style(30, Color(20, 16, 40)); st.outline = CLEAR;
    text(CAPCH[c.type], x + w / 2, y + 7, st);
  }
}

static void drawParticles() {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    Color c = mix(p.col, WHITE, t * 0.5f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 14; f.rot = p.life * 8; draw(cart::IMG_SPARK, p.x + shakeX, p.y + shakeY, f); }
    else disc(p.x + shakeX, p.y + shakeY, p.size * (0.4f + 0.6f * t), c);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = 0.3f + r.t * 2.2f; f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, r.x, r.y, f);
  }
}

static void drawHud() {
  char buf[32];
  float lx = OX / 2;
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 120);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 260, Color(255, 220, 110));
  snprintf(buf, sizeof(buf), "%d", stage + 1 + loopN * 8);
  ui::stat("STAGE", buf, W - lx, 120, Color(150, 230, 255));
  TextStyle l = ui::style(26, Color(150, 170, 220)); l.outline = CLEAR;
  text("LIVES", W - lx, 260, l);
  for (int i = 0; i < std::min(lives - 1, 6); i++) {
    float x = W - lx - 75 + (i % 3) * 52, y = 310 + (i / 3) * 34;
    roundRect(x - 22, y, 44, 16, 8, Color(200, 210, 235));
    roundRect(x - 22, y, 12, 16, 6, Color(70, 200, 250)); roundRect(x + 10, y, 12, 16, 6, Color(70, 200, 250));
  }
  // active power-ups with time bars
  struct { int t, max; int cap; } act[4] = {{wideT, 900, CAP_E}, {laserT, 720, CAP_L}, {slowT, 600, CAP_S}, {catchT, 900, CAP_C}};
  float y = 460;
  for (auto& a : act) {
    if (!a.t) continue;
    Color cc = CAPCOL[a.cap];
    TextStyle s = ui::style(30, cc); s.outline = CLEAR;
    text(CAPNAME[a.cap], lx, y, s);
    roundRect(lx - 90, y + 42, 180, 10, 5, Color(255, 255, 255, 30));
    roundRect(lx - 90, y + 42, 180 * a.t / (float)a.max, 10, 5, cc);
    y += 90;
  }
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    float x = X(dbx), y = Y(dby);
    glow(x, y, 120, Color(160, 220, 255, 120));
    Fx f; f.sx = f.sy = 0.6f;
    draw(cart::IMG_ORB, x, y, f);
    static const char* HELP[] = {"LEFT / RIGHT MOVE THE PADDLE    A LAUNCH / FIRE LASERS"};
    ui::titleScreen(cart::IMG_LOGO_BRICKSTORM, "SMASH EVERY BRICK", hiscore, HELP, 1, Color(255, 210, 160));
    for (int i = 0; i < NCAP; i++) {   // power-up legend
      float cx = W / 2 + (i - 2.5f) * 170, cy = 880;
      roundRect(cx - 40, cy, 80, 40, 20, CAPCOL[i]);
      TextStyle st = ui::style(30, Color(20, 16, 40)); st.outline = CLEAR;
      text(CAPCH[i], cx, cy + 6, st);
      TextStyle n = ui::style(24, Color(200, 205, 240)); n.outline = CLEAR;
      text(CAPNAME[i], cx, cy + 52, n);
    }
    return;
  }
  drawWalls();
  for (int r = 0; r < BROWS; r++)
    for (int c = 0; c < BCOLS; c++) if (bricks[r][c].kind) drawBrick(r, c);
  for (auto& bm : beams)
    if (bm.on) {
      Fx f; f.rot = 90; f.tint = Color(255, 120, 130); f.blend = BLEND_ADD; f.sx = 1.2f; f.sy = 1.4f;
      draw(cart::IMG_STREAK, X(bm.x), Y(bm.y), f);
    }
  drawCaps();
  drawPaddle();
  for (auto& b : balls) if (b.on) drawBall(b);
  drawParticles();
  drawHud();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 250, 230, (uint8_t)(flash * 255)), BLEND_ADD);
  bool blink = (frameNo >> 5) & 1;
  if (state == ST_PLAY) {
    for (auto& b : balls)
      if (b.on && b.stuck && stateT < 400 && blink) { textf(W / 2, 820, ui::style(44), "PRESS %s TO LAUNCH", btnName(BTN_A)); break; }
    if (stateT < 90) { char s[24]; snprintf(s, sizeof(s), "STAGE %d", stage + 1 + loopN * 8); ui::banner(s, W / 2, 640, 90 - stateT, 90, WHITE, 96); }
  } else if (state == ST_CLEAR) {
    ui::banner("STAGE CLEAR!", W / 2, 500, 150 - stateT + 30, 150, Color(255, 220, 110), 110);
    if (stateT > 20) textf(W / 2, 660, ui::style(52), "BONUS %d", 1000 * (loopN + 1));
  } else if (state == ST_OVER) {
    ui::gameOver(stateT, newHi);
  }
}

// ------------------------------------------------------------ bot: follow the lowest falling ball, aim with the paddle edge
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY) return;
  const Ball* t = nullptr;
  for (auto& b : balls)
    if (b.on && (!t || (b.vy > 0 && b.y > t->y) || (t->vy <= 0 && b.vy > 0))) t = &b;
  float target = padX;
  if (t) {
    float x = t->x;
    if (t->vy > 0) {   // predict where it crosses the paddle line, bouncing off the walls
      float steps = (PAD_Y - t->y) / t->vy;
      x = t->x + t->vx * steps;
      float span = FR - FX - 6;
      float u = fmodf(x - FX - 3, 2 * span);
      if (u < 0) u += 2 * span;
      x = FX + 3 + (u < span ? u : 2 * span - u);
    }
    target = x + ((frameNo / 240) % 2 ? 6.0f : -6.0f);   // hit off-centre to steer the ball
  }
  float d = target - padX;
  p.ax = clampv(d / 5.0f, -1.0f, 1.0f);
  if (fabsf(d) > 1) p.held |= d > 0 ? BTN_RIGHT : BTN_LEFT;
  if ((frameNo & 3) == 0) p.held |= BTN_A;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d stage %d score %lu lives %d bricks %d", state, stage + 1, (unsigned long)score, lives, bricksLeft);
}

static void enter() {
  state = ST_TITLE; stateT = 0;
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace bs

extern const nova::Game BRICK_STORM;
const nova::Game BRICK_STORM = {
  "brickstorm", "BRICK STORM", "SMASH EVERY BRICK",
  nullptr, bs::enter, bs::step, bs::draw, bs::canPause, nullptr, bs::bot, bs::debugInfo,
};
