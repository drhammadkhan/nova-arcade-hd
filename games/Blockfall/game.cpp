// =====================================================================
//  BLOCKFALL HD  -  a falling-blocks puzzle game (Nova Arcade HD)
//  7-bag randomiser, SRS rotation with wall kicks, hold, ghost piece,
//  3-piece preview, lock delay and auto-repeat, as in the original.
//  New for HD: glossy blocks on a glass well, line clears that burst into
//  light, a hard-drop beam, a stack-height danger glow, and the original
//  tunes re-orchestrated for the new synth.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "ui.h"
#include "common_art.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

using namespace nova;
using namespace nova::audio;

namespace bf {

// ------------------------------------------------------------ music (the original's melodies)
static const char* const GAME_LEAD =
  "D5 - F5 - A5 - G5 F5 E5 - D5 - C5 - D5 - | F5 - - D5 Bb4 - D5 - F5 - G5 - F5 - D5 - |"
  "E5 - G5 - C6 - B5 G5 E5 - G5 - C5 - - - | A5 - - - G5 - E5 - C5 - E5 - A5 - G5 - |"
  "D6 - C6 - A5 - F5 - G5 - A5 - D5 - - - | F5 - - D5 Bb4 - D5 - F5 - G5 - F5 - D5 - |"
  "E5 - G5 - C6 - B5 G5 E5 - G5 - C5 - - - | E5 - - - C#5 - - - A4 - - - . . . .";
static const char* const GAME_DRUMS = "K.h.S.hKK.h.S.h.|K.h.S.hKK.h.S.h.|K.h.S.hKK.h.S.h.|K.h.S.hKK.h.S.h.|"
                                      "K.h.S.hKK.h.S.h.|K.h.S.hKK.h.S.h.|K.h.S.hKK.h.S.h.|K.h.S.h.K.h.SSSS";
static const Song SONG_TITLE = {
  104, 4, "Dm Bb C Am",
  {I_BELL, 0.7f, 0.1f, "D5 - - - A5 - - - G5 - F5 - E5 - - - | F5 - - - E5 - D5 - C5 - - - D5 - - - | . . . . . . . . . . . . . . . . | . . . . . . . . . . . . . . . ."},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.28f,
  I_SUBBASS, "R.......R...5...", 0.5f,
  I_PAD, 0.45f, "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  138, 8, "Dm Bb C Am Dm Bb C A",
  {I_SQUARE, 0.7f, 0.05f, GAME_LEAD},
  {}, I_MARIMBA, "1.5.3.5.1.5.3.5.", 0.22f,
  I_PLUCKBASS, "R.RR..R.R.RR.5O.", 0.6f,
  I_PAD, 0.3f, GAME_DRUMS, 0.85f, 0, true};
static const Song SONG_FAST = {
  168, 8, "Dm Bb C Am Dm Bb C A",
  {I_SAWLEAD, 0.6f, 0.05f, GAME_LEAD},
  {}, I_MARIMBA, "15351535153515351", 0.2f,
  I_PLUCKBASS, "RRRRRRRRRRRRRRRR", 0.55f,
  I_STRINGS, 0.3f, GAME_DRUMS, 0.9f, 0, true};
static const Song SONG_OVER = {
  90, 2, "Dm Dm",
  {I_FLUTE, 0.8f, 0, "A5 - F5 - D5 - Bb4 - A4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f,
  I_PAD, 0.4f, "................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ pieces
static const int COLS = 10, ROWS = 22, HIDDEN = 2, CELL = 48;
static const float BX = (W - COLS * CELL) / 2.0f, BY = 60;   // board origin (first visible row)
enum { P_I, P_O, P_T, P_S, P_Z, P_J, P_L };
static const int8_t SHAPES[7][4][2] = {
  {{0, 1}, {1, 1}, {2, 1}, {3, 1}}, {{1, 0}, {2, 0}, {1, 1}, {2, 1}}, {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
  {{1, 0}, {2, 0}, {0, 1}, {1, 1}}, {{0, 0}, {1, 0}, {1, 1}, {2, 1}}, {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
  {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
};
static int8_t cells[7][4][4][2];   // [piece][rotation][block][x,y]
// SRS kicks (y down). Index: 0 0->R,1 R->0,2 R->2,3 2->R,4 2->L,5 L->2,6 L->0,7 0->L
static const int8_t KICK_JLSTZ[8][5][2] = {
  {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}, {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},
  {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}},   {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}},
  {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}},    {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}},
  {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}};
static const int8_t KICK_I[8][5][2] = {
  {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}}, {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}},
  {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}, {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}},
  {{0, 0}, {2, 0}, {-1, 0}, {2, -1}, {-1, 2}}, {{0, 0}, {-2, 0}, {1, 0}, {-2, 1}, {1, -2}},
  {{0, 0}, {1, 0}, {-2, 0}, {1, 2}, {-2, -1}}, {{0, 0}, {-1, 0}, {2, 0}, {-1, -2}, {2, 1}}};
static const Color PCOL[10] = {Color(0, 0, 0), Color(60, 220, 240), Color(255, 214, 60), Color(180, 90, 240), Color(90, 225, 105),
                               Color(245, 70, 90), Color(70, 125, 250), Color(255, 150, 50), Color(110, 110, 140), Color(255, 255, 255)};

static void buildShapes() {
  for (int p = 0; p < 7; p++)
    for (int rot = 0; rot < 4; rot++)
      for (int k = 0; k < 4; k++) {
        int x = SHAPES[p][k][0], y = SHAPES[p][k][1];
        int n = p == P_I ? 4 : 3;
        for (int i = 0; i < rot && p != P_O; i++) { int nx = n - 1 - y; y = x; x = nx; }
        cells[p][rot][k][0] = x; cells[p][rot][k][1] = y;
      }
}

// ------------------------------------------------------------ state
static uint8_t board[ROWS][COLS];
struct Piece { int type, rot, x, y; };
static Piece cur;
static int pieceNo = 0;   // counts spawns (the bot watches it)
static int hold = -1;
static bool holdUsed = false;
static uint8_t bag[7], bagN = 0, queue[3];
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 10000;
static int level = 1, lines = 0, combo = -1;
static int gravT = 0, lockT = 0, lockResets = 0;
static Repeat repL, repR;
enum State { ST_TITLE, ST_PLAY, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int clearRows[4], nClear = 0, clearT = 0, overT = 0;
static bool newHi = false;
static int bannerT = 0, bannerT0 = 1;
static char banner[24];
static bool dangerSong = false;

// effects
struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; bool spark; };
static Part parts[600];
static float shake = 0, shakeX = 0, shakeY = 0;
static float beamT = 0, beamX0 = 0, beamX1 = 0, beamY0 = 0, beamY1 = 0;
static Color beamCol;
static float lockFlash[ROWS][COLS];
static float danger = 0;
struct Demo { float x, y, v, rot, vr; int type; float s; };
static Demo demo[14];
static ui::Stars stars;

static void spawnPart(float x, float y, float vx, float vy, float life, float size, Color col, bool spark) {
  for (auto& p : parts) if (!p.on) { p = {true, x, y, vx, vy, life, life, size, col, spark}; return; }
}

static uint8_t nextFromBag() {
  if (bagN == 0) {
    for (int i = 0; i < 7; i++) bag[i] = i;
    for (int i = 6; i > 0; i--) { int j = rnd() % (i + 1); uint8_t t = bag[i]; bag[i] = bag[j]; bag[j] = t; }
    bagN = 7;
  }
  return bag[--bagN];
}

static bool fits(int type, int rot, int x, int y) {
  for (int k = 0; k < 4; k++) {
    int cx = x + cells[type][rot][k][0], cy = y + cells[type][rot][k][1];
    if (cx < 0 || cx >= COLS || cy >= ROWS) return false;
    if (cy >= 0 && board[cy][cx]) return false;
  }
  return true;
}

static void setBanner(const char* s, int t) { snprintf(banner, sizeof(banner), "%s", s); bannerT = bannerT0 = t; }

static void gameOver() {
  state = ST_OVER; overT = 0;
  music(&SONG_OVER);
  sfx(SFX_DIE);
  rumble(0.7f, 400);
  if (newHi) saveHi(hiscore);
}

static void spawn(int type) {
  cur.type = type; cur.rot = 0; cur.x = 3; cur.y = HIDDEN - 1 - (type == P_I ? 1 : 0);
  gravT = 0; lockT = 0; lockResets = 0;
  pieceNo++;
  if (!fits(cur.type, cur.rot, cur.x, cur.y)) gameOver();
}

static void nextPiece() {
  int t = queue[0];
  queue[0] = queue[1]; queue[1] = queue[2]; queue[2] = nextFromBag();
  spawn(t);
  holdUsed = false;
}

static void resetGame() {
  memset(board, 0, sizeof(board));
  for (auto& p : parts) p.on = false;
  memset(lockFlash, 0, sizeof(lockFlash));
  bagN = 0;
  for (auto& q : queue) q = nextFromBag();
  hold = -1; score = 0; level = 1; lines = 0; combo = -1; newHi = false; nClear = 0;
  dangerSong = false;
  state = ST_PLAY;
  setBanner("LEVEL 1", 90);
  nextPiece();
}

static int gravityFrames() {
  static const uint8_t G[] = {48, 43, 38, 33, 28, 23, 18, 13, 9, 7, 6, 5, 5, 4, 4, 4, 3, 3, 3, 2};
  return frames(level <= 20 ? G[level - 1] : 1);
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}

static int ghostY() {
  int y = cur.y;
  while (fits(cur.type, cur.rot, cur.x, y + 1)) y++;
  return y;
}

static float cellX(int c) { return BX + c * CELL; }
static float cellY(int r) { return BY + (r - HIDDEN) * CELL; }

static void lockPiece() {
  for (int k = 0; k < 4; k++) {
    int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = cur.y + cells[cur.type][cur.rot][k][1];
    if (cy >= 0) { board[cy][cx] = cur.type + 1; lockFlash[cy][cx] = 1; }
  }
  bool visible = false;
  for (int k = 0; k < 4; k++) if (cur.y + cells[cur.type][cur.rot][k][1] >= HIDDEN) visible = true;
  if (!visible) { gameOver(); return; }
  nClear = 0;
  for (int r = 0; r < ROWS; r++) {
    bool full = true;
    for (int c = 0; c < COLS; c++) if (!board[r][c]) { full = false; break; }
    if (full) clearRows[nClear++] = r;
  }
  if (nClear) {
    static const int PTS[5] = {0, 100, 300, 500, 800};
    combo++;
    addScore(PTS[nClear] * level + (combo > 0 ? 50 * combo * level : 0));
    static const char* NAMES[5] = {"", "", "DOUBLE!", "TRIPLE!", "QUAD!"};
    if (nClear >= 2) setBanner(NAMES[nClear], 70);
    else if (combo > 0) { char b[24]; snprintf(b, sizeof(b), "COMBO x%d", combo); setBanner(b, 60); }
    sfx(nClear == 4 ? SFX_POWERUP : SFX_SCROLL, 0, 0.9f + 0.1f * nClear);
    if (nClear == 4) { shake = 16; rumble(0.6f, 250); }
    else shake = fmaxf(shake, 4.0f + 2 * nClear);
    for (int i = 0; i < nClear; i++)
      for (int c = 0; c < COLS; c++) {
        Color col = PCOL[board[clearRows[i]][c]];
        for (int k = 0; k < 5; k++)
          spawnPart(cellX(c) + frange(0, CELL), cellY(clearRows[i]) + frange(0, CELL), frange(-9, 9), frange(-12, 3), frange(30, 55),
                    frange(5, 12), col, k < 2);
      }
    state = ST_CLEAR; clearT = 0;
  } else {
    combo = -1;
    sfx(SFX_LAND);
    nextPiece();
  }
}

static void collapseRows() {
  for (int i = 0; i < nClear; i++) {
    int r = clearRows[i];
    for (int y = r; y > 0; y--) { memcpy(board[y], board[y - 1], COLS); memcpy(lockFlash[y], lockFlash[y - 1], sizeof(lockFlash[0])); }
    memset(board[0], 0, COLS);
  }
  int before = lines / 10;
  lines += nClear;
  if (lines / 10 > before) {
    level++;
    char b[24]; snprintf(b, sizeof(b), "LEVEL %d", level);
    setBanner(b, 90);
    sfx(SFX_ONEUP);
  }
  nClear = 0;
  state = ST_PLAY;
  nextPiece();
}

static bool tryMove(int dx, int dy) {
  if (!fits(cur.type, cur.rot, cur.x + dx, cur.y + dy)) return false;
  cur.x += dx; cur.y += dy;
  return true;
}

static void touchedGround() {   // reset lock delay after a successful move while grounded
  if (!fits(cur.type, cur.rot, cur.x, cur.y + 1) && lockResets < 15) { lockT = 0; lockResets++; }
}

static void rotate(int dir) {
  if (cur.type == P_O) return;
  int to = (cur.rot + (dir > 0 ? 1 : 3)) & 3;
  static const int CW_IDX[4] = {0, 2, 4, 6}, CCW_IDX[4] = {7, 1, 3, 5};
  int idx = dir > 0 ? CW_IDX[cur.rot] : CCW_IDX[cur.rot];
  const int8_t (*kicks)[2] = cur.type == P_I ? KICK_I[idx] : KICK_JLSTZ[idx];
  for (int k = 0; k < 5; k++) {
    int nx = cur.x + kicks[k][0], ny = cur.y + kicks[k][1];
    if (fits(cur.type, to, nx, ny)) {
      cur.x = nx; cur.y = ny; cur.rot = to;
      sfx(SFX_MOVE, 0, 1.4f);
      touchedGround();
      return;
    }
  }
}

static int stackHeight() {
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) if (board[r][c]) return ROWS - r;
  return 0;
}

// ------------------------------------------------------------ update
static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += 0.45f; p.vx *= 0.98f;
    if (--p.life <= 0 || p.y > H + 20) p.on = false;
  }
  for (auto& row : lockFlash) for (auto& f : row) f *= 0.88f;
  stars.step(0, 0.4f);
  if (bannerT) bannerT--;
  if (beamT > 0) beamT -= 0.07f;
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  float want = state == ST_PLAY ? clampv((stackHeight() - 12) / 6.0f, 0.0f, 1.0f) : 0;
  danger = approach(danger, want, 0.05f);
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  if (in.hit(BTN_Y | BTN_L | BTN_R) && !holdUsed) {
    int t = cur.type;
    if (hold < 0) { hold = t; nextPiece(); }
    else { int h = hold; hold = t; spawn(h); }
    holdUsed = true;
    sfx(SFX_SELECT);
    return;
  }
  if (in.hit(BTN_A | BTN_UP)) rotate(+1);   // Up rotates too: a nudge on the stick must never slam the piece down
  if (in.hit(BTN_B)) rotate(-1);
  if (repL(in.down(BTN_LEFT), 10, 2) && tryMove(-1, 0)) { sfx(SFX_MOVE); touchedGround(); }
  if (repR(in.down(BTN_RIGHT), 10, 2) && tryMove(1, 0)) { sfx(SFX_MOVE); touchedGround(); }
  if (in.hit(BTN_X)) {   // hard drop
    int d = 0, y0 = cur.y;
    while (tryMove(0, 1)) d++;
    addScore(d * 2);
    int minx = 9, maxx = -9;
    for (int k = 0; k < 4; k++) { minx = std::min(minx, (int)cells[cur.type][cur.rot][k][0]); maxx = std::max(maxx, (int)cells[cur.type][cur.rot][k][0]); }
    beamT = 1; beamCol = PCOL[cur.type + 1];
    beamX0 = cellX(cur.x + minx); beamX1 = cellX(cur.x + maxx + 1);
    beamY0 = cellY(y0); beamY1 = cellY(cur.y + 2);
    for (int k = 0; k < 4; k++) {
      float cx = cellX(cur.x + cells[cur.type][cur.rot][k][0]), cy = cellY(cur.y + cells[cur.type][cur.rot][k][1]) + CELL;
      for (int j = 0; j < 3; j++) spawnPart(cx + frange(0, CELL), cy, frange(-3, 3), frange(-8, -2), 22, frange(4, 8), WHITE, true);
    }
    shake = fmaxf(shake, 6.0f);
    lockPiece();
    return;
  }
  bool soft = in.down(BTN_DOWN);
  int g = soft ? std::min(2, gravityFrames()) : gravityFrames();
  if (++gravT >= g) {
    gravT = 0;
    if (tryMove(0, 1)) { if (soft) addScore(1); lockT = 0; }
  }
  if (!fits(cur.type, cur.rot, cur.x, cur.y + 1)) {
    if (++lockT >= 30) lockPiece();
  }
  bool want = stackHeight() > 14;
  if (want != dangerSong) { dangerSong = want; music(want ? &SONG_FAST : &SONG_GAME); }
}

static void step(const Pad& in) {
  updateFx();
  switch (state) {
    case ST_TITLE:
      for (auto& d : demo) {
        d.y += d.v; d.rot += d.vr;
        if (d.y > H + 200) { d.y = -200 - frand() * 300; d.x = frange(80, W - 80); d.type = rnd() % 7; }
      }
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); music(&SONG_GAME); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_CLEAR:
      if (++clearT >= 20) collapseRows();
      break;
    case ST_OVER:
      overT++;
      if (overT < ROWS * 3 && overT % 3 == 0) {   // grey out, bottom to top
        int r = ROWS - 1 - overT / 3;
        for (int c = 0; c < COLS; c++) if (board[r][c]) board[r][c] = 8;
      }
      if (overT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static void drawBlock(float x, float y, float s, Color c, float alpha = 1, float glowAmt = 0) {
  if (glowAmt > 0) glow(x + s / 2, y + s / 2, s * 1.2f, c.alpha(0.5f * glowAmt));
  drawRect(cart::IMG_BLOCK, x, y, s, s, c.alpha(alpha));
}

static void drawBackground() {
  rectGrad(0, 0, W, H, Color(10, 10, 34), Color(34, 14, 58));
  for (int i = 0; i < 4; i++) {   // slow aurora
    float ph = frameNo * 0.003f + i * 1.7f;
    glow(W * (0.2f + 0.2f * i) + 200 * sinf(ph), 300 + 160 * cosf(ph * 1.3f), 600, Color(70 + 30 * i, 80, 220 - 30 * i, 40));
  }
  stars.draw(Color(170, 170, 255));
}

static void pieceBounds(int type, int& minx, int& maxx, int& miny, int& maxy) {
  minx = miny = 9; maxx = maxy = -9;
  for (int k = 0; k < 4; k++) {
    minx = std::min(minx, (int)cells[type][0][k][0]); maxx = std::max(maxx, (int)cells[type][0][k][0]);
    miny = std::min(miny, (int)cells[type][0][k][1]); maxy = std::max(maxy, (int)cells[type][0][k][1]);
  }
}

static void drawPieceAt(int type, float cx, float cy, float s, float alpha = 1) {
  int minx, maxx, miny, maxy;
  pieceBounds(type, minx, maxx, miny, maxy);
  float w = (maxx - minx + 1) * s, h = (maxy - miny + 1) * s;
  for (int k = 0; k < 4; k++)
    drawBlock(cx - w / 2 + (cells[type][0][k][0] - minx) * s, cy - h / 2 + (cells[type][0][k][1] - miny) * s, s, PCOL[type + 1], alpha);
}

static void drawWell(float ox, float oy) {
  float w = COLS * CELL, h = 20 * CELL;
  float pulse = 0.5f + 0.5f * sinf(frameNo * 0.03f);
  Color edge = mix(mix(Color(70, 200, 240), Color(180, 90, 240), pulse), Color(255, 60, 70), danger);
  glow(ox + w / 2, oy + h / 2, h * 0.75f, edge.alpha(0.18f + 0.25f * danger));
  roundRect(ox - 14, oy - 14, w + 28, h + 28, 20, edge.alpha(0.9f));
  roundRect(ox - 8, oy - 8, w + 16, h + 16, 14, Color(14, 12, 36));
  rectGrad(ox, oy, w, h, Color(20, 18, 52, 255), Color(30, 16, 60, 255));
  for (int r = 1; r < 20; r++) rect(ox, oy + r * CELL - 1, w, 2, Color(255, 255, 255, 10));
  for (int c = 1; c < COLS; c++) rect(ox + c * CELL - 1, oy, 2, h, Color(255, 255, 255, 10));
}

static void drawBoard() {
  float ox = BX + shakeX, oy = BY + shakeY;
  drawWell(ox, oy);
  for (int r = HIDDEN; r < ROWS; r++) {
    float y = oy + (r - HIDDEN) * CELL;
    bool clearing = false;
    if (state == ST_CLEAR) for (int i = 0; i < nClear; i++) if (clearRows[i] == r) clearing = true;
    for (int c = 0; c < COLS; c++) {
      uint8_t v = board[r][c];
      if (!v) continue;
      float x = ox + c * CELL;
      if (clearing) {   // flash white, then shrink into the light
        float k = clearT / 20.0f;
        float s = CELL * (1 - easeInOut(k));
        drawBlock(x + (CELL - s) / 2, y + (CELL - s) / 2, s, mix(WHITE, PCOL[v], k * 0.3f), 1, 1);
      } else {
        drawBlock(x, y, CELL, PCOL[v]);
        if (lockFlash[r][c] > 0.02f) rect(x + 3, y + 3, CELL - 6, CELL - 6, Color(255, 255, 255, (uint8_t)(160 * lockFlash[r][c])), BLEND_ADD);
      }
    }
    if (clearing) {
      float k = clearT / 20.0f;
      glow(ox + COLS * CELL / 2, y + CELL / 2, COLS * CELL * (0.4f + k), Color(255, 255, 255, (uint8_t)(120 * (1 - k))));
    }
  }
  if (beamT > 0) {
    Color c = beamCol.alpha(0.5f * beamT);
    rectGrad(beamX0 + shakeX, fmaxf(oy, beamY0 + shakeY), beamX1 - beamX0, beamY1 - fmaxf(oy, beamY0), Color(c.r, c.g, c.b, 0), c, BLEND_ADD);
  }
  if (state == ST_PLAY) {
    int gy = ghostY();
    Color gc = PCOL[cur.type + 1];
    for (int k = 0; k < 4; k++) {
      int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = gy + cells[cur.type][cur.rot][k][1];
      if (cy < HIDDEN) continue;
      float x = ox + cx * CELL, y = oy + (cy - HIDDEN) * CELL;
      roundRect(x + 4, y + 4, CELL - 8, CELL - 8, 8, gc.alpha(0.22f));
      roundRect(x + 10, y + 10, CELL - 20, CELL - 20, 5, Color(14, 12, 36, 200));
    }
    float lockK = !fits(cur.type, cur.rot, cur.x, cur.y + 1) ? lockT / 30.0f : 0;
    for (int k = 0; k < 4; k++) {
      int cx = cur.x + cells[cur.type][cur.rot][k][0], cy = cur.y + cells[cur.type][cur.rot][k][1];
      if (cy < HIDDEN) continue;
      float x = ox + cx * CELL, y = oy + (cy - HIDDEN) * CELL;
      drawBlock(x, y, CELL, mix(PCOL[cur.type + 1], WHITE, 0.25f * lockK * (0.5f + 0.5f * sinf(frameNo * 0.6f))), 1, 0.6f);
    }
  }
}

static void drawParticles() {
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    Color c = mix(p.col, WHITE, t * 0.6f).alpha(fminf(1, t * 2));
    if (p.spark) {
      Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 16; f.rot = p.life * 9;
      draw(cart::IMG_SPARK, p.x, p.y, f);
    } else {
      disc(p.x, p.y, p.size * (0.5f + 0.5f * t), c);
    }
  }
}

static void drawPanels() {
  char buf[32];
  ui::box(270, 60, 380, 270, "HOLD");
  if (hold >= 0) drawPieceAt(hold, 460, 210, 46, holdUsed ? 0.4f : 1);
  ui::box(270, 360, 380, 660, nullptr);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, 460, 400);
  snprintf(buf, sizeof(buf), "%d", level);
  ui::stat("LEVEL", buf, 460, 530, Color(150, 230, 255));
  snprintf(buf, sizeof(buf), "%d", lines);
  ui::stat("LINES", buf, 460, 660, Color(150, 230, 255));
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, 460, 820, Color(255, 220, 110));
  ui::box(1270, 60, 380, 560, "NEXT");
  drawPieceAt(queue[0], 1460, 210, 52);
  drawPieceAt(queue[1], 1460, 380, 40, 0.85f);
  drawPieceAt(queue[2], 1460, 520, 40, 0.7f);
  ui::box(1270, 650, 380, 370, "CONTROLS");
  TextStyle k = ui::style(28, Color(200, 205, 240)); k.outline = CLEAR;
  textf(1460, 730, k, "%s / UP  ROTATE", btnName(BTN_A));
  textf(1460, 780, k, "%s  ROTATE BACK", btnName(BTN_B));
  textf(1460, 830, k, "%s  HARD DROP", btnName(BTN_X));
  text("DOWN  SOFT DROP", 1460, 880, k);
  textf(1460, 930, k, "%s  HOLD", btnName(BTN_Y));
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    for (auto& d : demo) {
      int minx, maxx, miny, maxy;
      pieceBounds(d.type, minx, maxx, miny, maxy);
      float s = 56 * d.s;
      for (int k = 0; k < 4; k++) {
        float lx = (cells[d.type][0][k][0] - (minx + maxx + 1) / 2.0f) * s, ly = (cells[d.type][0][k][1] - (miny + maxy + 1) / 2.0f) * s;
        float a = d.rot * 0.0174533f, rx = lx * cosf(a) - ly * sinf(a), ry = lx * sinf(a) + ly * cosf(a);
        Fx f; f.rot = d.rot; f.sx = f.sy = s / 64.0f; f.tint = PCOL[d.type + 1].alpha(0.55f * d.s);
        draw(cart::IMG_BLOCK, d.x + rx, d.y + ry, f);
      }
    }
    static const char* HELP[] = {"LEFT / RIGHT MOVE    DOWN SOFT DROP", "A / UP ROTATE    B ROTATE BACK    X HARD DROP    Y HOLD"};
    ui::titleScreen(cart::IMG_LOGO_BLOCKFALL, "A FALLING BLOCKS PUZZLER", hiscore, HELP, 2);
    return;
  }
  drawPanels();
  drawBoard();
  drawParticles();
  if (state != ST_OVER) ui::banner(banner, BX + COLS * CELL / 2, 420, bannerT, bannerT0);
  if (state == ST_OVER && overT > 40) ui::gameOver(overT - 40, newHi);
}

// ------------------------------------------------------------ the bot (tests and demo play)
// Tries every rotation and column, scores the result (Dellacherie-style), then steers there.
static int botPiece = -1, botRot = 0, botX = 0, botT = 0;

static float evaluate(const uint8_t (*b)[COLS], int cleared) {
  int heights[COLS], holes = 0, agg = 0, bump = 0;
  for (int c = 0; c < COLS; c++) {
    heights[c] = 0;
    bool seen = false;
    for (int r = 0; r < ROWS; r++) {
      if (b[r][c]) { if (!seen) heights[c] = ROWS - r; seen = true; }
      else if (seen) holes++;
    }
    agg += heights[c];
  }
  for (int c = 0; c + 1 < COLS; c++) bump += abs(heights[c] - heights[c + 1]);
  return -0.51f * agg + 0.76f * cleared - 0.36f * holes - 0.18f * bump;
}

static void botPlan() {
  float best = -1e9f;
  botRot = 0; botX = cur.x;
  static uint8_t tmp[ROWS][COLS];
  for (int rot = 0; rot < 4; rot++)
    for (int x = -3; x < COLS; x++) {
      if (!fits(cur.type, rot, x, cur.y)) continue;
      int y = cur.y;
      while (fits(cur.type, rot, x, y + 1)) y++;
      memcpy(tmp, board, sizeof(tmp));
      for (int k = 0; k < 4; k++) {
        int cy = y + cells[cur.type][rot][k][1];
        if (cy >= 0) tmp[cy][x + cells[cur.type][rot][k][0]] = 1;
      }
      int cleared = 0;
      for (int r = 0; r < ROWS; r++) {
        bool full = true;
        for (int c = 0; c < COLS; c++) if (!tmp[r][c]) { full = false; break; }
        if (full) { cleared++; for (int yy = r; yy > 0; yy--) memcpy(tmp[yy], tmp[yy - 1], COLS); memset(tmp[0], 0, COLS); }
      }
      float s = evaluate(tmp, cleared);
      if (s > best) { best = s; botRot = rot; botX = x; }
    }
}

static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY) return;
  if (botPiece != pieceNo) { botPiece = pieceNo; botPlan(); botT = 0; }
  if (++botT & 1) return;   // release every other frame so presses register
  if (cur.rot != botRot) p.held = BTN_A;
  else if (cur.x < botX) p.held = BTN_RIGHT;
  else if (cur.x > botX) p.held = BTN_LEFT;
  else p.held = BTN_X;
  if (botT > 60) p.held = BTN_X;   // can't get there: drop anyway
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d score %lu level %d lines %d stack %d", state, (unsigned long)score, level, lines, stackHeight());
}

// ------------------------------------------------------------ hooks
static void init() {
  buildShapes();
}

static void enter() {
  state = ST_TITLE;
  stars.init(120);
  for (auto& d : demo) { d = {frange(80, W - 80), -frand() * H, frange(1.0f, 3.0f), frand() * 360, frange(-0.6f, 0.6f), (int)(rnd() % 7), frange(0.5f, 1.2f)}; }
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}

static bool canPause() { return state == ST_PLAY || state == ST_CLEAR; }

}  // namespace bf

extern const nova::Game BLOCKFALL;
const nova::Game BLOCKFALL = {
  "blockfall", "BLOCKFALL", "A FALLING BLOCKS PUZZLER",
  bf::init, bf::enter, bf::step, bf::draw, bf::canPause, nullptr, bf::bot, bf::debugInfo,
};
