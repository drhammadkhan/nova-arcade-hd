// =====================================================================
//  GEM CASCADE HD  -  a match-three puzzle (Nova Arcade HD)
//  Swap neighbouring gems to line up three or more of a kind. Four in a
//  row makes a flame gem (blows up its 3x3 block), an L or T makes a star
//  gem (clears its row and column) and five in a row makes a nova that
//  wipes out every gem of the colour you swap it with. Matches fill the
//  level bar; it drains only while the board is idle. Fill it to level up.
//
//  The original's board, rules, scoring and cascade timing, kept in its
//  own units (a 26 px cell, gems falling at 0.55 px/frame^2) and drawn
//  4.5x. New for HD: faceted vector gems (tools/make_art.py), flame, star
//  and nova gems with animated glows, eased swaps with a little arc,
//  squash-and-bounce landings, shard bursts and light rings, lightning
//  for star and nova blasts, and a soft gradient backdrop with drifting
//  light that changes colour every level.
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

namespace gc {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  96, 4, "F C G Am",
  {I_BELL, 0.7f, 0.1f,
   "F5 - E5 - C5 - - - A4 - C5 - E5 - - - | G5 - E5 - C5 - - - E5 - G5 - C6 - - - |"
   "B5 - - - G5 - D5 - G5 - - - A5 - B5 - | C6 - - - A5 - - - E5 - - - . . . ."},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.2f,
  I_SUBBASS, "R.......5.......", 0.5f,
  I_PAD, 0.45f, "K...............", 0.6f, 0, true};
static const Song SONG_PLAY = {
  118, 8, "Am F C G Am F Dm E",
  {I_MARIMBA, 0.8f, 0.05f,
   "E5 . E5 G5 A5 - G5 E5 D5 - E5 - . . C5 D5 | F5 - E5 D5 C5 - - D5 E5 - - - . . . . |"
   "C5 . C5 E5 G5 - E5 G5 C6 - B5 - A5 - G5 - | G5 - - A5 B5 - A5 G5 D5 - - - . . . . |"
   "E5 . E5 G5 A5 - G5 E5 D5 - E5 - . . C5 D5 | F5 - E5 D5 C5 - - D5 E5 - - - . . . . |"
   "D5 - F5 - A5 - F5 - D6 - C6 - A5 - F5 - | Ab5 - - - B5 - - - E6 - D6 - B5 - Ab5 -"},
  {}, I_BELL, "1...5...3...5...", 0.16f,
  I_PLUCKBASS, "R..R..5.R..R5.O.", 0.55f,
  I_PAD, 0.3f,
  "K...h.h.S...h.h.|K...h.h.S...h.h.|K...h.h.S...h.h.|K...h.h.S...h.h.|K.h.S.h.K.h.S.h.|K.h.S.h.K.h.S.h.|K.hhS.h.K.hhS.hS|K.hhS.h.K.hhS.hS",
  0.75f, 0.04f, true};
static const Song SONG_LEVEL = {
  150, 2, "C C", {I_BELL, 0.8f, 0, "C5 E5 G5 C6 G5 C6 E6 - G6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.6f, 0, false};
static const Song SONG_OVER = {
  90, 2, "Am Am", {I_FLUTE, 0.8f, 0, "A5 - - G5 E5 - - D5 C5 - - - A4 - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ board (original units: a 26 px cell)
static const float K = 4.5f;
static const int N = 8, NTYPES = 7, T_NOVA = 7;
static const float CELL = 26 * K;                                   // 117
static const float BX = (W - N * CELL) / 2, BY = (H - N * CELL) / 2;  // 492, 72
enum Special : uint8_t { SP_NONE, SP_FLAME, SP_STAR, SP_NOVA };
struct Gem {
  int8_t type; uint8_t sp; float yoff, vy; int8_t clearT; uint8_t delay;   // as the original (yoff in its pixels)
  float land;                                                              // cosmetic: landing bounce strength
};
static Gem board[N][N];
static bool mk[N][N], fired[N][N];

enum Phase { PH_IDLE, PH_SWAP, PH_BACK, PH_CLEAR, PH_FALL, PH_LEVELUP, PH_OVER };
static Phase phase = PH_IDLE;
static int phaseT = 0;
static int curR = 3, curC = 3, swR0, swC0, swR1, swC1;
static bool selected = false;
static Repeat repU, repD, repL, repR;

static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 15000;
static bool newHi = false;
static int level = 1, combo = 0, idleT = 0, hintR = -1, hintC = -1, bestCombo = 0, gemsCleared = 0, idleNo = 0;
static float bar = 0.5f, barShown = 0.5f;

enum State { ST_TITLE, ST_PLAY, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

static const Color GEM_COL[8] = {Color(236, 32, 72),  Color(40, 112, 240), Color(30, 190, 96),  Color(255, 204, 40),
                                 Color(168, 72, 236), Color(214, 220, 242), Color(252, 132, 28), Color(255, 255, 255)};

// ------------------------------------------------------------ effects (cosmetic, HD screen pixels)
struct Part { bool on; float x, y, vx, vy, rot, vr, life, max, size; Color col; uint8_t kind; };   // 0 shard 1 spark 2 dot 3 ember
static Part parts[700];
struct RingFx { bool on; float x, y, r0, r1; int t, max; Color col; };
static RingFx rings[40];
struct Popup { bool on; float x, y; int t, t0; char txt[24]; uint8_t big; };
static Popup popups[6];
struct Mote { float x, y, v, s, ph; };
static Mote motes[70];
struct Beam { bool on; int r, c, t; bool row; };     // star-gem lightning across a row or column
static Beam beams[8];
struct Bolt { bool on; float x0, y0, x1, y1; int t; uint32_t seed; };   // nova lightning to each gem it takes
static Bolt bolts[64];
static float shake = 0, shakeX = 0, shakeY = 0, flash = 0;
static float curX = 0, curY = 0;                     // the cursor glides after the selected cell
static int themeFrom = 0, themeTo = 0;
static float themeK = 1;

template <typename T, size_t M>
static T* alloc(T (&arr)[M]) {
  for (auto& a : arr) if (!a.on) { a = T(); a.on = true; return &a; }
  return nullptr;
}
static inline float cellCX(int c) { return BX + c * CELL + CELL / 2; }
static inline float cellCY(int r) { return BY + r * CELL + CELL / 2; }
static inline Color gemColor(int t) { return GEM_COL[t >= 0 && t <= T_NOVA ? t : 0]; }
static float hrand(uint32_t n) {   // a hash for cosmetic jitter (keeps the game's random sequence untouched)
  n = (n ^ 61) ^ (n >> 16); n *= 9; n ^= n >> 4; n *= 0x27d4eb2d; n ^= n >> 15;
  return (n & 0xFFFF) / 65535.0f;
}

static void addShake(float s) { shake = fmaxf(shake, s); }

// the original's burst: n bits flying out at up to `sp` of its pixels per frame
static void burst(float x, float y, int n, Color col, float sp) {
  for (int i = 0; i < n; i++) {
    Part* p = alloc(parts);
    if (!p) return;
    float a = frand() * 6.2832f, v = frange(0.3f, sp) * K;
    p->x = x; p->y = y; p->vx = cosf(a) * v; p->vy = sinf(a) * v - 0.8f * K;
    p->max = p->life = frange(22, 44);
    int k = rnd() % 4;
    p->kind = k == 0 ? 1 : k == 1 ? 2 : 0;
    p->size = p->kind == 1 ? frange(18, 30) : p->kind == 2 ? frange(4, 8) : frange(14, 26);
    p->rot = frand() * 360; p->vr = frange(-12, 12);
    p->col = col;
  }
}
static void ringAt(float x, float y, float r0, float r1, int t, Color c) {
  RingFx* r = alloc(rings);
  if (r) { r->x = x; r->y = y; r->r0 = r0; r->r1 = r1; r->t = 0; r->max = t; r->col = c; }
}
static void popup(float x, float y, const char* s, uint8_t big = 0) {
  if (big) for (auto& p : popups) if (p.big) p.on = false;   // one banner at a time
  for (auto& p : popups)
    if (!p.on) { p.on = true; p.x = x; p.y = y; p.t = p.t0 = big ? 80 : 50; p.big = big; snprintf(p.txt, sizeof(p.txt), "%s", s); return; }
}

// ------------------------------------------------------------ match finding (as the original)
struct Run { int8_t r, c, len; bool horiz; };
static Run runs[40];
static int nruns = 0;

static void findRuns() {
  nruns = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N;) {
      int8_t t = board[r][c].type; int len = 1;
      while (c + len < N && t >= 0 && t < NTYPES && board[r][c + len].type == t && !board[r][c + len].clearT) len++;
      if (t >= 0 && t < NTYPES && len >= 3 && nruns < 40) runs[nruns++] = {(int8_t)r, (int8_t)c, (int8_t)len, true};
      c += len;
    }
  for (int c = 0; c < N; c++)
    for (int r = 0; r < N;) {
      int8_t t = board[r][c].type; int len = 1;
      while (r + len < N && t >= 0 && t < NTYPES && board[r + len][c].type == t && !board[r + len][c].clearT) len++;
      if (t >= 0 && t < NTYPES && len >= 3 && nruns < 40) runs[nruns++] = {(int8_t)r, (int8_t)c, (int8_t)len, false};
      r += len;
    }
}

static bool lineAt(int r, int c) {
  int8_t t = board[r][c].type;
  if (t < 0 || t >= NTYPES) return false;
  int h = 1, v = 1;
  for (int k = c - 1; k >= 0 && board[r][k].type == t; k--) h++;
  for (int k = c + 1; k < N && board[r][k].type == t; k++) h++;
  for (int k = r - 1; k >= 0 && board[k][c].type == t; k--) v++;
  for (int k = r + 1; k < N && board[k][c].type == t; k++) v++;
  return h >= 3 || v >= 3;
}

static void swapCells(int r0, int c0, int r1, int c1) { Gem t = board[r0][c0]; board[r0][c0] = board[r1][c1]; board[r1][c1] = t; }

static bool swapWorks(int r0, int c0, int r1, int c1) {
  if (board[r0][c0].type == T_NOVA || board[r1][c1].type == T_NOVA) return true;
  swapCells(r0, c0, r1, c1);
  bool ok = lineAt(r0, c0) || lineAt(r1, c1);
  swapCells(r0, c0, r1, c1);
  return ok;
}

static bool findMove(int* hr, int* hc) {
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (c + 1 < N && swapWorks(r, c, r, c + 1)) { if (hr) { *hr = r; *hc = c; } return true; }
      if (r + 1 < N && swapWorks(r, c, r + 1, c)) { if (hr) { *hr = r; *hc = c; } return true; }
    }
  return false;
}

// Fill the board with no ready-made lines and at least one move. Gems drop in from above.
static void newBoard() {
  do {
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++) {
        Gem& g = board[r][c];
        g = {0, SP_NONE, 0, 0, 0, 0, 0};
        do { g.type = rnd() % NTYPES; } while (lineAt(r, c));
      }
  } while (!findMove(nullptr, nullptr));
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) { board[r][c].yoff = (N - r) * 26 + 40 + (N - 1 - r) * 10 + c * 6; board[r][c].vy = 0; }
}

static void shuffleBoard() {
  for (int tries = 0; tries < 200; tries++) {
    for (int i = N * N - 1; i > 0; i--) { int j = rnd() % (i + 1); swapCells(i / N, i % N, j / N, j % N); }
    bool lines = false;
    for (int r = 0; r < N && !lines; r++) for (int c = 0; c < N; c++) if (lineAt(r, c)) { lines = true; break; }
    if (!lines && findMove(nullptr, nullptr)) break;
  }
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].yoff = (N - r) * 26 + 30 + c * 5; board[r][c].vy = 0; }
}

// ------------------------------------------------------------ clearing
static int markedCount() { int n = 0; for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) n += mk[r][c]; return n; }

static void fireSpecials() {
  bool again = true;
  while (again) {
    again = false;
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++) {
        if (!mk[r][c] || fired[r][c] || board[r][c].sp == SP_NONE) continue;
        fired[r][c] = true; again = true;
        Gem& g = board[r][c];
        float x = cellCX(c), y = cellCY(r);
        if (g.sp == SP_FLAME) {
          for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++)
            if (r + dr >= 0 && r + dr < N && c + dc >= 0 && c + dc < N) mk[r + dr][c + dc] = true;
          burst(x, y, 30, Color(255, 170, 40), 3.5f);
          ringAt(x, y, 30, CELL * 1.9f, 22, Color(255, 160, 60));
          sfx(SFX_EXPLODE, (x - W / 2) / (W / 2)); addShake(12); rumble(0.4f, 150);
          flash = fmaxf(flash, 0.25f);
        } else if (g.sp == SP_STAR) {
          for (int k = 0; k < N; k++) { mk[r][k] = true; mk[k][c] = true; }
          if (Beam* b = alloc(beams)) { b->r = r; b->c = c; b->t = 18; b->row = true; }
          if (Beam* b = alloc(beams)) { b->r = r; b->c = c; b->t = 18; b->row = false; }
          ringAt(x, y, 20, CELL * 1.5f, 18, Color(150, 240, 255));
          sfx(SFX_SHOOT, 0, 0.55f); sfx(SFX_EXPLODE, 0, 0.75f); addShake(10); rumble(0.5f, 180);
          flash = fmaxf(flash, 0.3f);
        } else if (g.sp == SP_NOVA) {
          int8_t t = rnd() % NTYPES;
          for (int rr = 0; rr < N; rr++)
            for (int cc = 0; cc < N; cc++)
              if (board[rr][cc].type == t) {
                mk[rr][cc] = true;
                if (Bolt* b = alloc(bolts)) { *b = {true, x, y, cellCX(cc), cellCY(rr), 16, rnd()}; }
              }
          ringAt(x, y, 30, CELL * 3, 26, WHITE);
          sfx(SFX_EXPLODE, 0, 0.5f); addShake(14); rumble(0.6f, 220);
          flash = fmaxf(flash, 0.4f);
        }
      }
  }
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}

// Clears everything marked in mk[], scores it and starts the clear animation.
// made[] cells (new specials) survive with their new powers.
struct NewSp { int8_t r, c, type; uint8_t sp; };
static void startClear(NewSp* made, int nmade) {
  fireSpecials();
  for (int i = 0; i < nmade; i++) mk[made[i].r][made[i].c] = false;
  int n = markedCount();
  float sx = 0, sy = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      if (!mk[r][c]) continue;
      Gem& g = board[r][c];
      g.clearT = 16; g.delay = 0;
      float x = cellCX(c), y = cellCY(r);
      sx += x; sy += y;
      int t = g.type < NTYPES ? g.type : rnd() % NTYPES;
      burst(x, y, 9, GEM_COL[t], 2.4f);
      ringAt(x, y, 16, CELL * 0.7f, 16, mix(GEM_COL[t], WHITE, 0.4f));
    }
  for (int i = 0; i < nmade; i++) {
    Gem& g = board[made[i].r][made[i].c];
    g.type = made[i].type; g.sp = made[i].sp;
    float x = cellCX(made[i].c), y = cellCY(made[i].r);
    burst(x, y, 16, WHITE, 2.0f);
    ringAt(x, y, CELL * 0.9f, 10, 20, made[i].sp == SP_FLAME ? Color(255, 170, 60) : made[i].sp == SP_STAR ? Color(150, 240, 255) : WHITE);
    sfx(SFX_POWERUP);
  }
  gemsCleared += n;
  uint32_t pts = (uint32_t)n * 10 * (combo + 1) * (level + 1) / 2 + nmade * 100;
  addScore(pts);
  bar += n * (0.011f / (1 + (level - 1) * 0.22f)) * (1 + combo * 0.25f);
  if (n) {
    char b[24]; snprintf(b, sizeof(b), "%lu", (unsigned long)pts);
    popup(sx / n, sy / n - 30, b);
  }
  if (combo >= 1) {
    char b[24]; snprintf(b, sizeof(b), "CASCADE x%d", std::min(combo + 1, 99));
    popup(BX + N * CELL / 2, BY + N * CELL / 2 - 45, b, 1);
  }
  if (combo + 1 > bestCombo) bestCombo = combo + 1;
  if (n >= 12 && combo == 0) popup(BX + N * CELL / 2, BY + 180, "DAZZLING!", 1);
  // the match chime climbs with the cascade
  sfx(SFX_COIN, n ? ((sx / n) - W / 2) / (W / 2) * 0.6f : 0, powf(2.0f, std::min(combo, 10) * 2 / 12.0f) * 0.85f);
  phase = PH_CLEAR; phaseT = 0;
}

// Resolve the runs found by findRuns(). The pivot cells (the two swapped
// gems) are where new specials appear; in a cascade the middle of the run is used.
static void resolveRuns(int pr0, int pc0, int pr1, int pc1) {
  memset(mk, 0, sizeof(mk)); memset(fired, 0, sizeof(fired));
  static bool inH[N][N], inV[N][N];
  memset(inH, 0, sizeof(inH)); memset(inV, 0, sizeof(inV));
  NewSp made[16]; int nmade = 0;
  auto isMade = [&](int r, int c) { for (int i = 0; i < nmade; i++) if (made[i].r == r && made[i].c == c) return true; return false; };
  for (int i = 0; i < nruns; i++) {
    Run& u = runs[i];
    int pr = -1, pc = -1;
    for (int k = 0; k < u.len; k++) {
      int r = u.r + (u.horiz ? 0 : k), c = u.c + (u.horiz ? k : 0);
      mk[r][c] = true;
      (u.horiz ? inH : inV)[r][c] = true;
      if ((r == pr0 && c == pc0) || (r == pr1 && c == pc1)) { pr = r; pc = c; }
    }
    if (pr < 0) { pr = u.r + (u.horiz ? 0 : u.len / 2); pc = u.c + (u.horiz ? u.len / 2 : 0); }
    if (u.len >= 4 && nmade < 16 && !isMade(pr, pc) && board[pr][pc].sp == SP_NONE)
      made[nmade++] = {(int8_t)pr, (int8_t)pc, u.len >= 5 ? (int8_t)T_NOVA : board[pr][pc].type, u.len >= 5 ? (uint8_t)SP_NOVA : (uint8_t)SP_FLAME};
  }
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (inH[r][c] && inV[r][c] && !isMade(r, c) && board[r][c].sp == SP_NONE && nmade < 16)
        made[nmade++] = {(int8_t)r, (int8_t)c, board[r][c].type, SP_STAR};
  startClear(made, nmade);
}

// A nova swapped with a gem: every gem of that colour goes (two novas clear the board)
static void novaBlast(int nr, int nc, int orr, int oc) {
  memset(mk, 0, sizeof(mk)); memset(fired, 0, sizeof(fired));
  int8_t t = board[orr][oc].type;
  mk[nr][nc] = true; fired[nr][nc] = true;
  float x = cellCX(nc), y = cellCY(nr);
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      if (t == T_NOVA || board[r][c].type == t) {
        mk[r][c] = true;
        if (board[r][c].type == T_NOVA) fired[r][c] = true;
        if (Bolt* b = alloc(bolts)) { *b = {true, x, y, cellCX(c), cellCY(r), 16 + (int)(rnd() % 6), rnd()}; }
      }
  sfx(SFX_EXPLODE, 0, 0.5f); sfx(SFX_SHOOT, 0, 0.5f);
  addShake(16); rumble(0.8f, 300);
  flash = 0.5f;
  ringAt(x, y, 30, CELL * 4, 30, WHITE);
  popup(BX + N * CELL / 2, BY + 270, t == T_NOVA ? "SUPERNOVA!" : "NOVA!", 1);
  startClear(nullptr, 0);
}

static void collapse() {
  for (int c = 0; c < N; c++) {
    int dst = N - 1;
    for (int r = N - 1; r >= 0; r--) {
      if (board[r][c].type < 0) continue;
      if (dst != r) { board[dst][c] = board[r][c]; board[dst][c].yoff += (dst - r) * 26; board[r][c].type = -1; }
      dst--;
    }
    int k = 0;
    for (int r = dst; r >= 0; r--, k++) {
      Gem& g = board[r][c];
      g = {(int8_t)(rnd() % NTYPES), SP_NONE, (float)((dst + 1) * 26 + k * 6 + 8), 0, 0, 0, 0};
    }
  }
  phase = PH_FALL; phaseT = 0;
}

static bool updateFall() {
  bool moving = false;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      Gem& g = board[r][c];
      if (g.yoff <= 0) continue;
      g.vy = std::min(g.vy + 0.55f, 11.0f);
      g.yoff -= g.vy;
      if (g.yoff <= 0) { g.land = std::min(1.0f, g.vy / 9.0f); g.yoff = 0; g.vy = 0; }
      moving = true;
    }
  return moving;
}

// ------------------------------------------------------------ game flow
static void startGame() {
  score = 0; newHi = false; level = 1; combo = 0; bar = barShown = 0.5f; bestCombo = 0; gemsCleared = 0;
  curR = curC = 3; selected = false; idleT = 0; hintR = -1;
  curX = cellCX(curC); curY = cellCY(curR);
  hiscore = loadHi(HI_DEFAULT);
  themeFrom = themeTo = 0; themeK = 1;
  newBoard();
  phase = PH_FALL; phaseT = 0;
  state = ST_PLAY; stateT = 0;
  music(&SONG_PLAY);
}

static void gameOver() {
  state = ST_OVER; stateT = 0; phase = PH_OVER; phaseT = 0;
  if (newHi) saveHi(hiscore);
  music(&SONG_OVER);
  sfx(SFX_DIE);
  rumble(0.7f, 400);
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].delay = (uint8_t)((N - 1 - r) * 6 + rnd() % 8); board[r][c].vy = -frange(1, 3); }
}

static void startLevelUp() {
  phase = PH_LEVELUP; phaseT = 0;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { board[r][c].clearT = 16; board[r][c].delay = (uint8_t)((r + c) * 3); }
  music(&SONG_LEVEL);
  addScore(1000 * level);
}

static void trySwap(int dr, int dc) {
  int r1 = curR + dr, c1 = curC + dc;
  selected = false;
  if (r1 < 0 || r1 >= N || c1 < 0 || c1 >= N) return;
  swR0 = curR; swC0 = curC; swR1 = r1; swC1 = c1;
  phase = PH_SWAP; phaseT = 0;
  curR = r1; curC = c1;
  hintR = -1; idleT = 0;
  sfx(SFX_ROLL, (cellCX(c1) - W / 2) / W);
}

static void enterIdle() { phase = PH_IDLE; combo = 0; idleT = 0; idleNo++; }

static void stepBoard(const Pad& in) {
  phaseT++;
  switch (phase) {
    case PH_IDLE: {
      idleT++;
      bar -= (0.00011f + (level - 1) * 0.00004f) * speed();
      if (bar <= 0) { bar = 0; gameOver(); return; }
      if (idleT == frames(420)) findMove(&hintR, &hintC);
      bool aHeld = in.down(BTN_A);
      int dr = 0, dc = 0;
      if (repU(in.down(BTN_UP), 14, 5)) dr = -1;
      else if (repD(in.down(BTN_DOWN), 14, 5)) dr = 1;
      else if (repL(in.down(BTN_LEFT), 14, 5)) dc = -1;
      else if (repR(in.down(BTN_RIGHT), 14, 5)) dc = 1;
      if (dr || dc) {
        if (selected || (aHeld && !in.hit(BTN_A))) { trySwap(dr, dc); break; }
        curR = clampv(curR + dr, 0, N - 1); curC = clampv(curC + dc, 0, N - 1);
        sfx(SFX_MOVE);
      }
      if (in.hit(BTN_A)) { selected = !selected; sfx(selected ? SFX_SELECT : SFX_MOVE); }
      if (in.hit(BTN_B)) selected = false;
      break;
    }
    case PH_SWAP:
      if (phaseT >= 9) {
        swapCells(swR0, swC0, swR1, swC1);
        combo = 0;
        Gem &a = board[swR1][swC1], &b = board[swR0][swC0];   // a = the gem the player moved
        if (a.type == T_NOVA) { novaBlast(swR1, swC1, swR0, swC0); break; }
        if (b.type == T_NOVA) { novaBlast(swR0, swC0, swR1, swC1); break; }
        findRuns();
        if (nruns) resolveRuns(swR1, swC1, swR0, swC0);
        else { swapCells(swR0, swC0, swR1, swC1); phase = PH_BACK; phaseT = 0; sfx(SFX_BUMP); curR = swR0; curC = swC0; }
      }
      break;
    case PH_BACK:
      if (phaseT >= 9) phase = PH_IDLE;
      break;
    case PH_CLEAR: {
      bool any = false;
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (!g.clearT) continue;
          if (--g.clearT == 0) { g.type = -1; g.sp = SP_NONE; }
          else any = true;
        }
      if (!any) collapse();
      break;
    }
    case PH_FALL:
      if (!updateFall()) {
        findRuns();
        if (nruns) { combo++; resolveRuns(-1, -1, -1, -1); }
        else if (bar >= 1.0f) startLevelUp();
        else {
          enterIdle();
          if (!findMove(nullptr, nullptr)) {
            popup(BX + N * CELL / 2, BY + 400, "NO MOVES - SHUFFLE", 1);
            shuffleBoard(); phase = PH_FALL; sfx(SFX_CHECKPOINT);
          }
        }
      }
      break;
    case PH_LEVELUP: {
      bool any = false;
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (!g.clearT) continue;
          any = true;
          if (g.delay) { g.delay--; continue; }
          if (g.clearT == 16) {
            int t = g.type < NTYPES ? g.type : 0;
            burst(cellCX(c), cellCY(r), 7, GEM_COL[t], 3.0f);
            ringAt(cellCX(c), cellCY(r), 12, CELL * 0.6f, 14, mix(GEM_COL[t], WHITE, 0.5f));
            if (((r + c) & 3) == 0) sfx(SFX_KNOCK, (cellCX(c) - W / 2) / W, 0.8f + (r + c) * 0.04f);
          }
          if (--g.clearT == 0) g.type = -1;
        }
      if (!any && phaseT > 150) {
        level++; bar = barShown = 0.5f;
        themeFrom = themeTo; themeTo = (level - 1) % 6; themeK = 0;
        newBoard();
        phase = PH_FALL; phaseT = 0;
        music(&SONG_PLAY);
      }
      break;
    }
    case PH_OVER:
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          Gem& g = board[r][c];
          if (g.delay) { g.delay--; continue; }
          g.vy += 0.4f; g.yoff -= g.vy;
        }
      break;
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.rot += p.vr;
    if (p.kind == 3) { p.vy -= 0.04f; p.vx *= 0.96f; }
    else { p.vy += 0.12f * K; p.vx *= 0.97f; }
    if (--p.life <= 0) p.on = false;
  }
  for (auto& r : rings) if (r.on && ++r.t >= r.max) r.on = false;
  for (auto& p : popups) if (p.on) { p.y -= (p.big ? 0.25f : 0.6f) * K; if (--p.t <= 0) p.on = false; }
  for (auto& m : motes) { m.y -= m.v; m.x += 0.3f * sinf(frameNo * 0.01f + m.ph); if (m.y < -10) { m.y = H + 10; m.x = frand() * W; } }
  for (auto& b : beams) if (b.on && --b.t <= 0) b.on = false;
  for (auto& b : bolts) if (b.on && --b.t <= 0) b.on = false;
  barShown += (clampv(bar, 0.0f, 1.0f) - barShown) * 0.12f;
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake) * 0.6f; shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  flash *= 0.88f;
  if (themeK < 1) themeK = fminf(1, themeK + 1 / 90.0f);
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      Gem& g = board[r][c];
      if (g.land > 0.01f) g.land *= 0.9f; else g.land = 0;
      // embers rise off flame gems
      if (g.sp == SP_FLAME && g.type >= 0 && !g.clearT && hrand(frameNo * 64 + r * 8 + c) < 0.12f) {
        if (Part* p = alloc(parts)) {
          p->x = cellCX(c) + frange(-30, 30); p->y = cellCY(r) - g.yoff * K + frange(-10, 20);
          p->vx = frange(-0.6f, 0.6f); p->vy = frange(-2.2f, -1.0f);
          p->max = p->life = frange(20, 36); p->kind = 3; p->size = frange(4, 8);
          p->col = Color(255, (uint8_t)(140 + rnd() % 100), 60);
        }
      }
    }
  curX = approach(curX, cellCX(curC), 0.35f);
  curY = approach(curY, cellCY(curR), 0.35f);
}

static void step(const Pad& in) {
  stateT++;
  updateFx();
  switch (state) {
    case ST_TITLE:
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { sfx(SFX_START); startGame(); }
      // demo: gems rain behind the title
      if (!updateFall()) {
        if (stateT % 200 == 0) newBoard();
      }
      break;
    case ST_PLAY:
      if (in.hit(BTN_START)) { nova::pause(); break; }
      stepBoard(in);
      break;
    case ST_OVER:
      stepBoard(in);
      if (stateT > 120 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; stateT = 0; newBoard(); music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
// per level: deep, middle and light colours (the original's six themes)
static const Color THEMES[6][3] = {
  {Color(18, 8, 44), Color(70, 20, 90), Color(150, 90, 220)},    // amethyst dusk
  {Color(4, 18, 40), Color(10, 70, 110), Color(80, 200, 240)},   // sapphire deep
  {Color(4, 30, 24), Color(16, 90, 70), Color(110, 230, 150)},   // emerald cave
  {Color(40, 10, 16), Color(120, 30, 50), Color(250, 120, 140)}, // ruby glow
  {Color(30, 18, 6), Color(110, 64, 20), Color(255, 200, 90)},   // amber hall
  {Color(10, 10, 30), Color(40, 44, 90), Color(200, 210, 255)},  // pearl night
};
static Color theme(int i) { return mix(THEMES[themeFrom][i], THEMES[themeTo][i], easeInOut(themeK)); }

static void drawBackground() {
  Color deep = theme(0), midc = theme(1), lite = theme(2);
  rectGrad(0, 0, W, H * 0.6f, deep, midc);
  rectGrad(0, H * 0.6f - 1, W, H * 0.4f + 1, midc, deep);
  // drifting pools of light
  for (int i = 0; i < 5; i++) {
    float ph = frameNo * 0.0025f + i * 1.9f;
    float x = W * (0.1f + 0.2f * i) + 260 * sinf(ph * (1 + i * 0.13f));
    float y = H * 0.5f + 330 * cosf(ph * 0.8f + i);
    glow(x, y, 520 + 80 * sinf(ph * 2), lite.alpha(0.13f));
  }
  // a slow diagonal crystal lattice
  float sh = fmodf(frameNo * 0.5f * K * 0.5f, 288.0f);
  Color lc = lite.alpha(0.07f);
  for (float d = -H - 288 + sh; d < W + 288; d += 288) {
    line(d, 0, d + H, H, 2, lc);
    line(d + H - sh * 0.5f, 0, d - sh * 0.5f, H, 2, lc);
  }
  for (auto& m : motes) {
    float tw = 0.5f + 0.5f * sinf(frameNo * 0.06f + m.ph * 5);
    glow(m.x, m.y, m.s * 4, lite.alpha(0.25f * tw));
    disc(m.x, m.y, m.s * 0.6f, mix(lite, WHITE, 0.6f).alpha(0.5f + 0.4f * tw));
  }
}

// Draw one gem centred at (x, y). flashK > 0 whitens it; tint darkens or fades it.
static void drawGem(int type, uint8_t sp, float x, float y, float sx, float sy, float flashK = 0, Color tint = WHITE, float rot = 0) {
  if (type < 0) return;
  float a = tint.a / 255.0f;
  float pulse = 0.5f + 0.5f * sinf(frameNo * 0.12f + x * 0.01f);
  if (sp == SP_FLAME) {
    glow(x, y, 130 * sx, Color(255, 120, 30, (uint8_t)(170 * a * (0.7f + 0.3f * pulse))));
    Fx f; f.blend = BLEND_ADD; f.rot = frameNo * 2.0f; f.sx = f.sy = sx * (0.86f + 0.06f * sinf(frameNo * 0.5f));
    f.tint = Color(255, 210, 160, (uint8_t)(255 * a));
    draw(gcart::IMG_FLAME, x, y, f);
    f.rot = -frameNo * 3.1f; f.sx = f.sy = sx * (0.66f + 0.05f * sinf(frameNo * 0.7f + 1));
    draw(gcart::IMG_FLAME, x, y, f);
  } else if (sp == SP_STAR) {
    glow(x, y, 110 * sx, Color(90, 220, 255, (uint8_t)(110 * a * (0.6f + 0.4f * pulse))));
  } else if (type == T_NOVA) {
    float h = frameNo * 0.05f;
    Color hc(128 + 127 * sinf(h), 128 + 127 * sinf(h + 2.1f), 128 + 127 * sinf(h + 4.2f));
    glow(x, y, 130 * sx, hc.alpha(0.6f * a));
  }
  Fx f; f.sx = sx; f.sy = sy; f.tint = tint; f.rot = rot;
  if (type == T_NOVA) {
    f.rot = rot + frameNo * 1.2f;
    draw(gcart::IMG_NOVA, x, y, f);
    f.rot = rot;
    draw(gcart::IMG_NOVA_SHINE, x, y, f);
    for (int i = 0; i < 3; i++) {   // orbiting sparkles
      float an = frameNo * 0.07f + i * 2.094f;
      Fx s; s.blend = BLEND_ADD; s.sx = s.sy = sx * (0.28f + 0.1f * sinf(frameNo * 0.2f + i)); s.rot = frameNo * 4; s.tint = WHITE.alpha(a);
      draw(cart::IMG_SPARK, x + cosf(an) * 52 * sx, y + sinf(an) * 30 * sy, s);
    }
  } else {
    draw(*gcart::GEM[type], x, y, f);
  }
  if (flashK > 0) {
    Fx w = f; w.blend = BLEND_ADD; w.tint = WHITE.alpha(flashK * a);
    draw(type == T_NOVA ? gcart::IMG_NOVA : *gcart::GEM[type], x, y, w);
    draw(type == T_NOVA ? gcart::IMG_NOVA : *gcart::GEM[type], x, y, w);
  }
  if (sp == SP_STAR) {
    Fx m; m.sx = m.sy = sx * (0.9f + 0.12f * pulse); m.tint = tint;
    draw(gcart::IMG_STARMARK, x, y + 2, m);
    Fx g; g.blend = BLEND_ADD; g.rot = frameNo * 0.8f; g.sx = g.sy = sx * (0.42f + 0.1f * pulse); g.tint = WHITE.alpha(a * (0.5f + 0.5f * pulse));
    draw(gcart::IMG_GLINT, x, y, g);
    g.rot = 45 - frameNo * 0.5f; g.sx = g.sy = sx * 0.25f;
    draw(gcart::IMG_GLINT, x, y, g);
  } else if (sp == SP_FLAME) {
    Fx g; g.blend = BLEND_ADD; g.sx = g.sy = sx; g.tint = Color(255, 130, 40, (uint8_t)(70 * a * pulse));
    draw(*gcart::GEM[type < NTYPES ? type : 0], x, y, g);
  }
}

static void drawWell(float ox, float oy) {
  float w = N * CELL;
  Color lite = theme(2);
  glow(ox + w / 2, oy + w / 2, w * 0.8f, lite.alpha(0.16f));
  roundRect(ox - 34, oy - 26, w + 68, w + 68, 40, Color(0, 0, 0, 70));
  roundRect(ox - 30, oy - 30, w + 60, w + 60, 38, mix(Color(60, 52, 110), lite, 0.25f));
  roundRect(ox - 24, oy - 24, w + 48, w + 48, 33, mix(Color(150, 140, 210), lite, 0.3f));
  roundRect(ox - 20, oy - 20, w + 40, w + 40, 30, Color(40, 34, 80));
  roundRect(ox - 12, oy - 12, w + 24, w + 24, 22, Color(12, 8, 30, 235));
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      roundRect(ox + c * CELL + 4, oy + r * CELL + 4, CELL - 8, CELL - 8, 16, ((r + c) & 1) ? Color(255, 255, 255, 12) : Color(255, 255, 255, 22));
  rectGrad(ox - 12, oy - 12, w + 24, 70, Color(0, 0, 0, 120), Color(0, 0, 0, 0));
  // rivets
  for (int i = 0; i < 4; i++) {
    float rx = (i & 1) ? ox + w + 13 : ox - 13, ry = (i & 2) ? oy + w + 13 : oy - 13;
    disc(rx, ry, 9, Color(30, 24, 60));
    disc(rx, ry, 6, mix(Color(200, 196, 240), lite, 0.3f));
    disc(rx - 2, ry - 2, 2.5f, WHITE);
  }
}

static void drawBolt(const Bolt& b) {
  float k = b.t / 16.0f;
  int segs = 9;
  float dx = b.x1 - b.x0, dy = b.y1 - b.y0, len = sqrtf(dx * dx + dy * dy) + 1;
  float nx = -dy / len, ny = dx / len;
  float px = b.x0, py = b.y0;
  for (int i = 1; i <= segs; i++) {
    float t = (float)i / segs;
    float j = i == segs ? 0 : (hrand(b.seed + i * 31 + (frameNo >> 1) * 977) - 0.5f) * 46;
    float qx = b.x0 + dx * t + nx * j, qy = b.y0 + dy * t + ny * j;
    line(px, py, qx, qy, 14, Color(180, 140, 255, (uint8_t)(90 * k)), BLEND_ADD);
    line(px, py, qx, qy, 4, Color(255, 255, 255, (uint8_t)(230 * k)), BLEND_ADD);
    px = qx; py = qy;
  }
  glow(b.x1, b.y1, 60, Color(220, 190, 255, (uint8_t)(160 * k)));
}

static void drawBeam(const Beam& b, float ox, float oy) {
  float k = b.t / 18.0f, w = N * CELL;
  float x0, y0, x1, y1;
  if (b.row) { x0 = ox; x1 = ox + w; y0 = y1 = oy + b.r * CELL + CELL / 2; }
  else { y0 = oy; y1 = oy + w; x0 = x1 = ox + b.c * CELL + CELL / 2; }
  line(x0, y0, x1, y1, 70 * k + 10, Color(90, 200, 255, (uint8_t)(70 * k)), BLEND_ADD);
  line(x0, y0, x1, y1, 26 * k + 6, Color(150, 240, 255, (uint8_t)(160 * k)), BLEND_ADD);
  line(x0, y0, x1, y1, 6, Color(255, 255, 255, (uint8_t)(255 * k)), BLEND_ADD);
  // crackling lightning along the beam
  float px = x0, py = y0;
  for (int i = 1; i <= 14; i++) {
    float t = i / 14.0f;
    float j = i == 14 ? 0 : (hrand(i * 13 + (frameNo >> 1) * 101 + b.r * 7 + b.c) - 0.5f) * 40 * k;
    float qx = x0 + (x1 - x0) * t + (b.row ? 0 : j), qy = y0 + (y1 - y0) * t + (b.row ? j : 0);
    line(px, py, qx, qy, 3, Color(220, 250, 255, (uint8_t)(220 * k)), BLEND_ADD);
    px = qx; py = qy;
  }
}

static void drawBoard(float dim = 1) {
  float ox = BX + shakeX, oy = BY + shakeY;
  if (state != ST_TITLE) drawWell(ox, oy);
  bool swapping = phase == PH_SWAP || phase == PH_BACK;
  float st = 0;
  if (swapping) {
    st = clampv(phaseT / 9.0f, 0.0f, 1.0f);
    if (phase == PH_BACK) st = 1 - st;
    st = st * st * (3 - 2 * st);
  }
  Color dark = Color(90, 90, 120);
  auto gemAt = [&](int r, int c, bool front) {
    Gem& g = board[r][c];
    if (g.type < 0) return;
    float x = ox + c * CELL + CELL / 2, y = oy + r * CELL + CELL / 2 - g.yoff * K;
    float sx = 1, sy = 1, rot = 0;
    if (swapping && ((r == swR0 && c == swC0) || (r == swR1 && c == swC1))) {
      bool mover = r == swR0 && c == swC0;
      int tr = mover ? swR1 : swR0, tc = mover ? swC1 : swC0;
      float dx = (tc - c) * CELL, dy = (tr - r) * CELL;
      float arc = sinf(st * 3.14159f) * CELL * 0.16f * (mover ? 1 : -1);
      x += dx * st + (dy != 0 ? arc : 0);
      y += dy * st + (dx != 0 ? -arc : 0);
      float s = 1 + sinf(st * 3.14159f) * (mover ? 0.14f : -0.1f);
      sx = sy = s;
      if (phase == PH_BACK) rot = sinf(phaseT * 1.3f) * 10 * (1 - phaseT / 9.0f);
    }
    if (front != (swapping && ((r == swR0 && c == swC0) || (r == swR1 && c == swC1)))) return;
    if (y < oy - CELL * 0.6f && state != ST_OVER) return;   // still out of sight above the well
    float alpha = dim;
    if (state == ST_OVER) {
      if (y > H + 100) return;
      drawGem(g.type < NTYPES ? g.type : T_NOVA, SP_NONE, x, y, 1, 1, 0, dark.alpha(alpha), g.vy * 2 * ((c & 1) ? 1 : -1));
      return;
    }
    if (g.land > 0.01f) {   // squash on landing, then a little bounce
      float b = g.land * cosf((1 - g.land) * 14);
      sy = 1 - 0.16f * b; sx = 1 + 0.12f * b;
      y += (1 - sy) * CELL * 0.4f;
    }
    if (g.clearT) {
      if (g.delay) { drawGem(g.type, g.sp, x, y, sx, sy, 0, WHITE.alpha(alpha)); return; }
      if (g.clearT > 11) {   // flash white and swell
        float k = (16 - g.clearT) / 5.0f;
        drawGem(g.type, g.sp, x, y, 1 + 0.15f * k, 1 + 0.15f * k, 0.5f + 0.5f * k, WHITE.alpha(alpha));
      } else {               // shrink into the light, with a twist
        float k = g.clearT / 11.0f;
        float s = easeOut(k) * 1.15f;
        glow(x, y, 90 * (1.4f - k), mix(gemColor(g.type), WHITE, 0.5f).alpha(0.8f * k * alpha));
        drawGem(g.type, g.sp, x, y, s, s, 1, WHITE.alpha(alpha * (0.4f + 0.6f * k)), (1 - k) * 90);
      }
      return;
    }
    bool sel = selected && r == curR && c == curC && phase == PH_IDLE && state == ST_PLAY;
    if (sel) { y -= 6 + 6 * sinf(frameNo * 0.25f); sx *= 1.06f; sy *= 1.06f; }
    drawGem(g.type, g.sp, x, y, sx, sy, 0, WHITE.alpha(alpha), rot);
  };
  if (state != ST_OVER) clip(0, oy - 14, W, H);   // new gems slide in from behind the top of the well
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) gemAt(r, c, false);
  if (swapping) {   // the gem being moved passes in front
    int br = swR1, bc = swC1;
    gemAt(br, bc, true);
    gemAt(swR0, swC0, true);
  }
  noClip();
  for (auto& b : beams) if (b.on) drawBeam(b, ox, oy);
  for (auto& b : bolts) if (b.on) drawBolt(b);
  // hint sparkle
  if (hintR >= 0 && phase == PH_IDLE && state == ST_PLAY) {
    float x = ox + hintC * CELL + CELL / 2, y = oy + hintR * CELL + CELL / 2;
    float p = 0.5f + 0.5f * sinf(frameNo * 0.15f);
    glow(x, y, 90, Color(255, 255, 220, (uint8_t)(70 * p)));
    for (int i = 0; i < 2; i++) {
      Fx s; s.blend = BLEND_ADD; s.rot = frameNo * 3 + i * 45; s.sx = s.sy = 0.5f + 0.25f * sinf(frameNo * 0.2f + i * 2);
      draw(cart::IMG_SPARK, x + (i ? 30 : -32), y + (i ? 28 : -30), s);
    }
  }
  // cursor
  if (state == ST_PLAY && (phase == PH_IDLE || phase == PH_FALL || phase == PH_CLEAR)) {
    float cx = curX + shakeX, cy = curY + shakeY;
    if (selected) {
      Fx f; f.tint = mix(Color(255, 230, 120), Color(255, 160, 40), 0.5f + 0.5f * sinf(frameNo * 0.4f));
      f.sx = f.sy = 0.98f;
      glow(cx, cy, 110, Color(255, 200, 80, 70));
      draw(gcart::IMG_SELECT, cx, cy, f);
    } else {
      Fx f; f.sx = f.sy = 1 + 0.04f * sinf(frameNo * 0.1f); f.tint = Color(230, 245, 255);
      draw(gcart::IMG_CURSOR, cx, cy, f);
    }
  }
}

static void drawParticles() {
  for (auto& r : rings) {
    if (!r.on) continue;
    float k = (float)r.t / r.max;
    float rad = lerp(r.r0, r.r1, easeOut(k));
    Fx f; f.blend = BLEND_ADD; f.sx = f.sy = rad / 54; f.tint = r.col.alpha(1 - k);
    draw(cart::IMG_RING, r.x + shakeX, r.y + shakeY, f);
  }
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    if (p.kind == 0) {
      Fx f; f.rot = p.rot; f.sx = f.sy = p.size / 15 * (0.6f + 0.4f * t); f.tint = mix(p.col, WHITE, 0.35f).alpha(fminf(1, t * 2.5f));
      draw(gcart::IMG_SHARD, p.x, p.y, f);
    } else if (p.kind == 1) {
      Fx f; f.blend = BLEND_ADD; f.rot = p.rot; f.sx = f.sy = p.size / 32 * t; f.tint = mix(p.col, WHITE, 0.6f);
      draw(cart::IMG_SPARK, p.x, p.y, f);
    } else if (p.kind == 2) {
      disc(p.x, p.y, p.size * (0.4f + 0.6f * t), mix(p.col, WHITE, t * 0.5f).alpha(fminf(1, t * 2)));
    } else {
      glow(p.x, p.y, p.size * 3, p.col.alpha(0.6f * t));
      disc(p.x, p.y, p.size * 0.5f * t + 1, Color(255, 240, 200, (uint8_t)(220 * t)));
    }
  }
}

static void drawPopups() {
  for (auto& p : popups) {
    if (!p.on) continue;
    if (p.big) {
      float age = (p.t0 - p.t) / 12.0f;
      float k = 1 + 0.45f * (1 - easeOut(age));
      float a = clampv(p.t / 15.0f, 0.0f, 1.0f);
      float w = N * CELL - 40;
      roundRect(BX + 20, p.y - 22, w, 120, 40, Color(10, 6, 30, (uint8_t)(150 * a)));
      glow(p.x, p.y + 38, 300, Color(255, 140, 220, (uint8_t)(60 * a)));
      Color c = mix(Color(255, 230, 120), Color(255, 140, 220), 0.5f + 0.5f * sinf(frameNo * 0.35f));
      TextStyle st = ui::style(78 * k, c.alpha(a)); st.outline = Color(40, 14, 60, (uint8_t)(255 * a)); st.shadow = 5;
      text(p.txt, p.x, p.y - 39 * (k - 1), st);
    } else {
      float a = clampv(p.t / 12.0f, 0.0f, 1.0f);
      float k = 1 + 0.3f * (1 - easeOut((p.t0 - p.t) / 8.0f));
      TextStyle st = ui::style(48 * k, ((p.t >> 2) & 1 ? WHITE : Color(255, 216, 74)).alpha(a));
      st.outline = Color(30, 14, 50, (uint8_t)(255 * a));
      text(p.txt, p.x, p.y, st);
    }
  }
}

static void drawLevelBar() {
  float x = 1482, y = BY - 20, w = 52, h = N * CELL + 40;
  roundRect(x - 8, y - 8, w + 16, h + 16, 34, Color(0, 0, 0, 80));
  roundRect(x - 6, y - 6, w + 12, h + 12, 32, mix(Color(150, 140, 210), theme(2), 0.3f));
  roundRect(x, y, w, h, 26, Color(14, 8, 32));
  float ih = h - 16, fh = barShown * ih, fy = y + 8 + ih - fh;
  bool low = bar < 0.18f && state == ST_PLAY;
  if (fh > 1) {
    Color top = Color(255, 120, 220), bot = Color(80, 200, 255);
    float t0 = (fy - y) / h;
    Color ct = mix(top, bot, t0);
    if (low) { float b = 0.5f + 0.5f * sinf(frameNo * 0.4f); ct = mix(ct, Color(255, 60, 70), b); bot = mix(bot, Color(255, 60, 70), b); }
    glow(x + w / 2, fy + fh / 2, fmaxf(80, fh * 0.6f), ct.alpha(0.18f));
    roundRect(x + 8, fy, w - 16, fh, 16, bot);
    rectGrad(x + 8, fy + 8, w - 16, fmaxf(0, fh - 16), ct, bot);
    // light running up the bar
    for (int i = 0; i < 6; i++) {
      float sy = fmodf(i * 150 - frameNo * 2.5f, ih);
      if (sy < 0) sy += ih;
      float yy = y + 8 + sy;
      if (yy > fy + 6 && yy < fy + fh - 10) rect(x + 10, yy, w - 20, 4, Color(255, 255, 255, 60), BLEND_ADD);
    }
    rect(x + 13, fy + 10, 6, fmaxf(0, fh - 20), Color(255, 255, 255, 70));
    glow(x + w / 2, fy, 50, Color(255, 255, 255, 120));
    roundRect(x + 10, fy, w - 20, 5, 2.5f, Color(255, 255, 255, 220));
  }
  Fx f; f.blend = BLEND_ADD; f.rot = frameNo * 1.5f; f.sx = f.sy = 0.7f + 0.1f * sinf(frameNo * 0.1f);
  draw(cart::IMG_SPARK, x + w / 2, y - 2, f);
}

static void drawPanels() {
  char buf[32];
  ui::box(60, 52, 370, 976, nullptr);
  TextStyle l = ui::style(30, Color(150, 170, 220)); l.outline = CLEAR;
  text("LEVEL", 245, 90, l);
  TextStyle big = ui::style(120, WHITE); big.shadow = 5;
  textf(245, 132, big, "%d", level);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, 245, 300, Color(255, 216, 74));
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, 245, 420);
  snprintf(buf, sizeof(buf), "x%d", bestCombo);
  ui::stat("BEST CASCADE", buf, 245, 540, Color(150, 240, 255));
  snprintf(buf, sizeof(buf), "%d", gemsCleared);
  ui::stat("GEMS", buf, 245, 660, Color(255, 170, 230));
  static const Color DC[3] = {Color(120, 230, 140), Color(255, 214, 90), Color(255, 110, 110)};
  Difficulty d = difficulty();
  roundRect(245 - 110, 790, 220, 56, 28, DC[d].alpha(0.25f));
  TextStyle ds = ui::style(32, DC[d]); ds.outline = CLEAR;
  text(difficultyName(d), 245, 803, ds);
  drawGem(T_NOVA, SP_NONE, 245, 935, 0.95f, 0.95f);

  ui::box(1574, 52, 300, 976, "SPECIALS");
  TextStyle k = ui::style(26, Color(200, 205, 240)); k.outline = CLEAR;
  struct { int type; uint8_t sp; const char* a; const char* b; } SPX[3] = {
    {0, SP_FLAME, "4 IN A ROW", "BLASTS 3x3"}, {1, SP_STAR, "L OR T SHAPE", "ROW + COLUMN"}, {T_NOVA, SP_NONE, "5 IN A ROW", "CLEARS A COLOUR"}};
  for (int i = 0; i < 3; i++) {
    float y = 170 + i * 205;
    drawGem(SPX[i].type, SPX[i].sp, 1724, y, 0.85f, 0.85f);
    text(SPX[i].a, 1724, y + 58, ui::style(28, Color(255, 230, 150)));
    text(SPX[i].b, 1724, y + 94, k);
  }
  TextStyle h = ui::style(30, Color(150, 220, 255)); h.outline = CLEAR;
  text("CONTROLS", 1724, 790, h);
  textf(1724, 840, k, "%s  PICK A GEM", btnName(BTN_A));
  text("D-PAD  SWAP IT", 1724, 880, k);
  textf(1724, 920, k, "%s  CANCEL", btnName(BTN_B));
  textf(1724, 960, k, "HOLD %s + D-PAD", btnName(BTN_A));
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    drawBoard(0.32f);
    drawParticles();
    static const char* HELP[] = {"PICK A GEM, THEN SWAP IT WITH THE D-PAD    LINE UP THREE OR MORE",
                                 "4 IN A ROW = FLAME    L OR T = STAR    5 IN A ROW = NOVA"};
    ui::titleScreen(cart::IMG_LOGO_GEMCASCADE, "MATCH THREE - CHAIN THE CASCADES", hiscore, HELP, 2, Color(240, 200, 255));
    for (int i = 0; i < 7; i++) {
      float bob = sinf(frameNo * 0.08f + i * 0.9f) * 12;
      drawGem(i, SP_NONE, W / 2 + (i - 3) * 130, 920 + bob, 0.8f, 0.8f);
    }
    return;
  }
  drawPanels();
  drawLevelBar();
  drawBoard();
  drawParticles();
  drawPopups();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 250, 240, (uint8_t)(90 * flash)), BLEND_ADD);
  if (phase == PH_LEVELUP && phaseT > 40) {
    float k = easeOut((phaseT - 40) / 20.0f);
    float cx = BX + N * CELL / 2, cy = BY + N * CELL / 2;
    ui::panel(cx - 380, cy - 170, 760, 330, Color(24, 16, 60, (uint8_t)(225 * k)));
    glow(cx, cy - 60, 400 * k, Color(255, 220, 120, 60));
    TextStyle t = ui::style(120 * (0.8f + 0.2f * k), Color(255, 230, 120).alpha(k)); t.shadow = 6;
    textf(cx, cy - 140, t, "LEVEL %d", level + 1);
    textf(cx, cy + 10, ui::style(52, WHITE.alpha(k)), "BONUS %d", 1000 * level);
    text("NEW GEMS INCOMING", cx, cy + 80, ui::style(40, Color(150, 240, 255).alpha(k)));
  }
  if (state == ST_OVER && stateT > 40) {
    ui::gameOver(stateT - 40, newHi);
    if (stateT > 70) {
      textf(W / 2, 760, ui::style(52), "SCORE %07lu", (unsigned long)score);
      textf(W / 2, 830, ui::style(36, Color(150, 240, 255)), "LEVEL %d    BEST CASCADE x%d", level, bestCombo);
    }
  }
}

// ------------------------------------------------------------ the bot (tests and demo play)
// Tries every swap on a copy of the board: plays out its matches, specials and the cascades that
// follow (without the unknown refill), and picks the swap that clears the most.
struct SimBoard { int8_t t[N][N]; uint8_t sp[N][N]; };

static float simCascade(SimBoard& b, int pr0, int pc0, int pr1, int pc1) {
  float total = 0;
  for (int chain = 0; chain < 20; chain++) {
    bool m[N][N] = {}, inH[N][N] = {}, inV[N][N] = {}, keep[N][N] = {};
    float bonus = 0;
    bool any = false;
    for (int dir = 0; dir < 2; dir++)
      for (int a = 0; a < N; a++)
        for (int s = 0; s < N;) {
          int r = dir ? s : a, c = dir ? a : s;
          int8_t t = b.t[r][c]; int len = 1;
          while (s + len < N && t >= 0 && t < NTYPES && (dir ? b.t[s + len][c] : b.t[r][s + len]) == t) len++;
          if (t >= 0 && t < NTYPES && len >= 3) {
            any = true;
            int kr = -1, kc = -1;
            for (int k = 0; k < len; k++) {
              int rr = dir ? s + k : r, cc = dir ? c : s + k;
              m[rr][cc] = true;
              (dir ? inV : inH)[rr][cc] = true;
              if ((rr == pr0 && cc == pc0) || (rr == pr1 && cc == pc1)) { kr = rr; kc = cc; }
            }
            if (len >= 4) {
              if (kr < 0) { kr = dir ? s + len / 2 : r; kc = dir ? c : s + len / 2; }
              if (!b.sp[kr][kc]) { keep[kr][kc] = true; bonus += len >= 5 ? 14 : 5; }
            }
          }
          s += len;
        }
    if (!any) break;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) if (inH[r][c] && inV[r][c] && !keep[r][c] && !b.sp[r][c]) { keep[r][c] = true; bonus += 7; }
    // set off any specials caught in the matches
    bool again = true, done[N][N] = {};
    while (again) {
      again = false;
      for (int r = 0; r < N; r++)
        for (int c = 0; c < N; c++) {
          if (!m[r][c] || done[r][c] || !b.sp[r][c]) continue;
          done[r][c] = again = true;
          if (b.sp[r][c] == SP_FLAME) { for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) if (r + dr >= 0 && r + dr < N && c + dc >= 0 && c + dc < N) m[r + dr][c + dc] = true; }
          else if (b.sp[r][c] == SP_STAR) { for (int k = 0; k < N; k++) m[r][k] = m[k][c] = true; }
          else bonus += 8;
        }
    }
    int n = 0;
    for (int r = 0; r < N; r++)
      for (int c = 0; c < N; c++)
        if (m[r][c] && !keep[r][c]) { n++; b.t[r][c] = -1; b.sp[r][c] = 0; }
        else if (keep[r][c]) b.sp[r][c] = SP_FLAME;   // (good enough for the lookahead)
    total += n * (1 + chain * 0.6f) + bonus;
    for (int c = 0; c < N; c++) {   // gravity, no refill
      int dst = N - 1;
      for (int r = N - 1; r >= 0; r--) {
        if (b.t[r][c] < 0) continue;
        b.t[dst][c] = b.t[r][c]; b.sp[dst][c] = b.sp[r][c];
        if (dst != r) { b.t[r][c] = -1; b.sp[r][c] = 0; }
        dst--;
      }
    }
    pr0 = pc0 = pr1 = pc1 = -1;
  }
  return total;
}

static float evalSwap(int r0, int c0, int r1, int c1) {
  int8_t a = board[r0][c0].type, b = board[r1][c1].type;
  if (a == T_NOVA || b == T_NOVA) {
    if (a == T_NOVA && b == T_NOVA) return 200;
    int8_t t = a == T_NOVA ? b : a;
    int n = 0;
    for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) n += board[r][c].type == t;
    return 12 + n * 1.5f;
  }
  SimBoard s;
  for (int r = 0; r < N; r++) for (int c = 0; c < N; c++) { s.t[r][c] = board[r][c].type; s.sp[r][c] = board[r][c].sp; }
  std::swap(s.t[r0][c0], s.t[r1][c1]); std::swap(s.sp[r0][c0], s.sp[r1][c1]);
  float v = simCascade(s, r1, c1, r0, c0);
  return v > 0 ? v + (r0 + r1) * 0.08f : 0;   // moves low on the board stir up more cascades
}

static int botIdle = -1, botR = 0, botC = 0, botDR = 0, botDC = 0, botWait = 0;
static void botPlan() {
  float best = 0;
  botR = -1;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      for (int d = 0; d < 2; d++) {
        int r1 = r + (d ? 1 : 0), c1 = c + (d ? 0 : 1);
        if (r1 >= N || c1 >= N) continue;
        float v = evalSwap(r, c, r1, c1);
        if (v <= 0) continue;
        v += frand() * 0.5f;   // break ties differently each time
        if (v > best) {
          best = v;
          // pick the end nearer the cursor to start from
          bool fromFirst = abs(r - curR) + abs(c - curC) <= abs(r1 - curR) + abs(c1 - curC);
          botR = fromFirst ? r : r1; botC = fromFirst ? c : c1;
          botDR = fromFirst ? r1 - r : r - r1; botDC = fromFirst ? c1 - c : c - c1;
        }
      }
}

static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY || phase != PH_IDLE) return;
  if (botIdle != idleNo) { botIdle = idleNo; botPlan(); botWait = 14; }
  if (botWait > 0) { botWait--; return; }
  if (frameNo & 1) return;   // release every other frame so each press registers
  if (botR < 0) return;
  if (curR != botR || curC != botC) {
    if (selected) { p.held = BTN_B; return; }
    p.held = curR < botR ? BTN_DOWN : curR > botR ? BTN_UP : curC < botC ? BTN_RIGHT : BTN_LEFT;
    return;
  }
  if (!selected) { p.held = BTN_A; return; }
  p.held = botDR > 0 ? BTN_DOWN : botDR < 0 ? BTN_UP : botDC > 0 ? BTN_RIGHT : BTN_LEFT;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d phase %d score %lu level %d bar %.2f best x%d gems %d", state, phase, (unsigned long)score, level, bar,
           bestCombo, gemsCleared);
}

// ------------------------------------------------------------ hooks
static void init() { loadAtlas(gcart::TEX_FILES, gcart::TEX, gcart::NTEX); }

static void enter() {
  state = ST_TITLE; stateT = 0; phase = PH_IDLE;
  for (auto& m : motes) { m.x = frand() * W; m.y = frand() * H; m.v = frange(0.1f, 0.5f) * K * 0.6f; m.s = frange(2, 6); m.ph = frand() * 6.28f; }
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  themeFrom = themeTo = 0; themeK = 1; level = 1;
  newBoard();
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}

static bool canPause() { return state == ST_PLAY; }

}  // namespace gc

extern const nova::Game GEM_CASCADE;
const nova::Game GEM_CASCADE = {
  "gemcascade", "GEM CASCADE", "MATCH THREE - CHAIN THE CASCADES",
  gc::init, gc::enter, gc::step, gc::draw, gc::canPause, nullptr, gc::bot, gc::debugInfo,
};
