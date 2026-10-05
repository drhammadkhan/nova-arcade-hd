// =====================================================================
//  PIXEL PEAKS HD  -  a platformer (Nova Arcade HD)
//  A little judoka runs, jumps and rolls her way over three worlds of
//  mountain trails, two levels each: Blossom Hills and Petal River, Bamboo
//  Grove and Firefly Marsh, Misty Peaks and Summit Shrine. Stomp the mochi
//  blobs, roll through spiky chestnuts, find each level's three secret
//  scrolls, and bow at the torii gate. Her belt changes colour with every
//  level she clears.
//
//  Gameplay keeps the original's feel: the same rules and physics, scaled
//  by 4.5 to 72 px tiles. Everything you see is new: a posed character rig,
//  vector art (tools/make_art.py -> art.h), painted parallax, particles and
//  lighting. Levels: tools/make_levels.py -> levels.h.
//
//  The gameplay state lives in one struct (G) so a bot can copy it and play
//  ahead (tests/test_pixelpeaks.cpp). Particles, popups and other effects
//  are cosmetic and stay outside it.
// =====================================================================
#include "nova.h"
#include "audio.h"
#include "art.h"
#include "levels.h"
#include "pixelpeaks.h"
#include <stdio.h>

using namespace nova;
using namespace nova::audio;
using namespace ppart;

namespace pp {

// ------------------------------------------------------------ music (original)
static const Song SONG_TITLE_DEF = {
  88, 8, "Am F G Em Am F G Am",
  {I_FLUTE, 0.9f, 0.1f,
   "A4 - - C5 E5 - D5 - C5 - A4 - - - G4 - | A4 - - - C5 - D5 - E5 - - - G5 - E5 - | D5 - - - B4 - D5 - G5 - - - E5 - D5 - | E5 - - - - - - - . . . . . . . . |"
   "A5 - - G5 E5 - D5 - E5 - - - C5 - A4 - | C5 - - - D5 - E5 - G5 - - - A5 - G5 - | E5 - D5 - B4 - G4 - A4 - B4 - D5 - - - | A4 - - - - - - - - - - - . . . ."},
  {},
  I_KOTO, "1.2.3.5.1.2.3.5.", 0.32f,
  I_SUBBASS, "R.......5.......", 0.55f,
  I_PAD, 0.45f,
  "T...............|T.......t.......|T...............|T.......t...w.w.", 0.7f,
  0, true};

static const Song SONG_W1 = {
  126, 8, "C Am F G C Am Dm G",
  {I_KOTO, 0.75f, 0.15f,
   "E5 - G5 - A5 - G5 E5 D5 - C5 - D5 - E5 - | C5 - - - A4 - C5 - D5 - E5 - - - . . | A5 - G5 - E5 - G5 - A5 - C6 - A5 - G5 - | G5 - - - D5 - E5 - G5 - - - . . . . |"
   "E5 - G5 - A5 - G5 E5 D5 - C5 - D5 - E5 - | A5 - G5 - E5 - D5 - C5 - - - A4 - C5 - | D5 - E5 - G5 - A5 - G5 - E5 - D5 - C5 - | D5 - - - - - - - G4 - A4 - B4 - D5 -"},
  {I_FLUTE, 0.35f, -0.25f,
   ". . . . . . . . . . . . . . . . | . . . . . . . . . . . . . . . . | C5 - - - - - - - E5 - - - - - - - | D5 - - - - - - - B4 - - - - - - - |"
   "G4 - - - - - - - C5 - - - - - - - | E5 - - - - - - - C5 - - - - - - - | F5 - - - - - - - A5 - - - - - - - | G5 - - - - - - - - - - - - - - -"},
  I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.3f,
  I_PLUCKBASS, "R..R..5.R..R5.O.", 0.6f,
  I_NONE, 0,
  "K.h.C.h.K.hKC.hs|K.h.C.h.K.hKC.ws", 0.8f,
  0.08f, true};

static const Song SONG_W2 = {
  104, 8, "Am F Dm E Am F Dm E",
  {I_FLUTE, 0.85f, 0.1f,
   "E5 - - - F5 E5 - - C5 - - - B4 - - - | A4 - - - - - C5 - B4 - A4 - F4 - - - | D5 - - - F5 - E5 - D5 - - - A4 - - - | B4 - - - C5 B4 - - E4 - - - - - - - |"
   "A5 - - - B5 A5 - - E5 - - - F5 - E5 - | C5 - - - - - E5 - D5 - C5 - A4 - - - | D5 - F5 - A5 - - - G5 - F5 - E5 - D5 - | E5 - - - - - - - G#4 - - - B4 - - -"},
  {},
  I_KOTO, "1.2.3.5.3.2.5.3.", 0.34f,
  I_SUBBASS, "R.......R...5...", 0.6f,
  I_CHOIR, 0.35f,
  "T.....t.T...t.w.|T.....t.T..tt.ww", 0.85f,
  0, true};

static const Song SONG_W3 = {
  138, 8, "Em C D Bm Em C D Em",
  {I_SAWLEAD, 0.55f, 0.05f,
   "E5 - - - B4 - E5 - G5 - - - F#5 - E5 - | E5 - - - D5 - C5 - D5 - - - G4 - - - | F#5 - - - A5 - - - F#5 - D5 - A4 - D5 - | F#5 - - - - - - - B4 - D5 - F#5 - - - |"
   "G5 - - - F#5 - E5 - B5 - - - A5 - G5 - | E5 - - - G5 - - - C6 - B5 - G5 - E5 - | F#5 - A5 - D6 - - - C6 - B5 - A5 - F#5 - | E5 - - - - - - - . . . . B4 - D5 -"},
  {I_FLUTE, 0.3f, -0.3f,
   "B4 - - - - - - - - - - - - - - - | G4 - - - - - - - - - - - - - - - | A4 - - - - - - - - - - - - - - - | B4 - - - - - - - D5 - - - - - - - |"
   "E5 - - - - - - - - - - - - - - - | C5 - - - - - - - E5 - - - - - - - | D5 - - - - - - - F#5 - - - - - - - | G5 - - - - - - - - - - - - - - -"},
  I_KOTO, "1.5.3.5.1.5.3.5.", 0.3f,
  I_PLUCKBASS, "R.RR..R.R.RR..5.", 0.62f,
  I_STRINGS, 0.4f,
  "K.h.S.hKK.h.S.hS|K.h.S.hKK.h.SThT", 0.85f,
  0, true};

static const Song SONG_CLEAR = {
  132, 3, "C C,G C",
  {I_MARIMBA, 0.8f, 0, "C5 E5 G5 C6 - - G5 - C6 - - - D6 - E6 - | - - - - D6 - C6 - B5 - G5 - D6 - - - | C6 - - - - - - - . . . . . . . ."},
  {I_KOTO, 0.6f, -0.3f, "G4 - - - C5 - - - E5 - - - G5 - - - | F5 - - - E5 - - - D5 - - - B4 - - - | C5 - - - - - - - . . . . . . . ."},
  I_NONE, nullptr, 0,
  I_SUBBASS, "R.......R...5...|R.......5.......|R...............", 0.5f,
  I_STRINGS, 0.4f,
  "T...t.t.T...t...|T...t.t.T.t.TttT|T...............", 0.8f,
  0, false};

static const Song SONG_OVER = {
  84, 2, "Am F,E",
  {I_FLUTE, 0.8f, 0, "E5 - D5 - C5 - A4 - G4 - - - A4 - - - | E4 - - - - - - - . . . . . . . ."},
  {}, I_NONE, nullptr, 0,
  I_SUBBASS, "R...............|R.......R.......", 0.5f,
  I_PAD, 0.4f,
  "T...............|t.......T.......", 0.7f,
  0, false};

static const Song* const WORLD_SONGS[3] = {&SONG_W1, &SONG_W2, &SONG_W3};
extern const Song* const SONGS[];
const Song* const SONGS[] = {&SONG_TITLE_DEF, &SONG_W1, &SONG_W2, &SONG_W3, &SONG_CLEAR, &SONG_OVER};
extern const char* const SONG_NAMES[];
const char* const SONG_NAMES[] = {"title", "world1", "world2", "world3", "clear", "gameover"};
extern const int NSONGS;
const int NSONGS = 6;

// ------------------------------------------------------------ the map
static const int TS = 72, ROWS = 15, MAXW = 240;
static const float K = 4.5f;   // the original game's pixels -> ours
enum Cell : uint8_t { C_AIR, C_GROUND, C_BLOCK, C_PLANK, C_SPIKE, C_WATER, C_BOX, C_BOXHEART, C_BOXUSED, C_COIN, C_SCROLL };
enum EType : uint8_t { E_MOCHI, E_CROW, E_CHESTNUT };
enum DType : uint8_t { D_TREE, D_TORO, D_BUSH, D_FLOWERS, D_ROCK, D_CHECK, D_GOAL };
enum State : uint8_t { ST_TITLE, ST_INTRO, ST_PLAY, ST_DEAD, ST_CLEAR, ST_WIN, ST_OVER };

struct Enemy { bool on, awake; uint8_t type, state; float x, y, vx, vy, hx, hy; int t; };   // state 0 alive, 1 squashed, 2 knocked away
struct Mover { bool on; float x, x0, y, v, dx; };
struct Deco { uint8_t type; float x, y; };   // x = left, y = bottom (ground line)
struct Hero {
  float x, y, vx, vy;          // hitbox top-left
  bool onGround, facingLeft, alive;
  int coyote, jumpBuf, rollT, rollCd, invuln, hurtT, anim, deadT;
  int riding;                  // index of the moving plank under her, or -1
};
static const float HW = 10 * K, HH = 22 * K, RH = 14 * K;
static const uint32_t HI_DEFAULT = 20000;

// everything the game needs to carry on from a moment (copied by the bot)
struct Game {
  uint8_t cells[ROWS][MAXW];
  uint8_t tileGfx[ROWS][MAXW];
  int mapW, world, levelIdx, loopN;
  Enemy enemies[48];
  Mover movers[8];
  Deco decos[80];
  int ndecos;
  Hero hero;
  int hearts, lives, coins, levelCoins, scrollsGot;
  uint32_t score, hiscore;
  bool newHi;
  float checkX, checkY, camX;
  State state;
  int stateT, clearBonus;
};
static Game G;

// ------------------------------------------------------------ effects (cosmetic: not part of G)
struct Part { bool on; float x, y, vx, vy, life, max, size, rot, vr, grav; Color col; const Img* img; bool add; };
static Part parts[400];
struct Popup { bool on; float x, y; int t; char txt[16]; Color col; };
static Popup popups[10];
struct Weather { float x, y, v, ph, z; };
static Weather weather[90];
static float shake = 0, shakeX = 0, shakeY = 0;
static float tailAng = 0, tailVel = 0, bandAng = 0, bandVel = 0, beltAng = 0, beltVel = 0;
static float squash = 1;   // landing squash for the heroine
static float flash = 0;    // white flash on big moments
static bool fxOn() { return mute == 0; }

static Color hsv(float h, float s, float v) {
  h = fmodf(h, 1) * 6;
  int i = (int)h;
  float f = h - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
  float r, g, b;
  switch (i) { case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
  return Color((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
}

static Part* spawn() {
  for (auto& p : parts) if (!p.on) { p = {}; p.on = true; return &p; }
  return nullptr;
}
static void burst(float x, float y, int n, Color col, float sp, bool add = false, const Img* img = nullptr) {
  if (!fxOn()) return;
  for (int i = 0; i < n; i++) {
    Part* p = spawn();
    if (!p) return;
    float a = frand() * 6.283f, s = frange(0.3f, 1) * sp * K;
    p->x = x; p->y = y; p->vx = cosf(a) * s; p->vy = sinf(a) * s - 0.6f * K;
    p->life = p->max = frange(18, 36); p->size = frange(4, 9); p->col = col; p->grav = 0.12f * K;
    p->img = img; p->rot = frand() * 360; p->vr = frange(-12, 12); p->add = add;
  }
}
static void dust(float x, float y, int n) {
  if (!fxOn()) return;
  for (int i = 0; i < n; i++) {
    Part* p = spawn();
    if (!p) return;
    p->x = x + frange(-18, 18); p->y = y - frange(0, 6);
    p->vx = frange(-3.5f, 3.5f); p->vy = frange(-2.6f, -0.4f);
    p->life = p->max = frange(16, 28); p->size = frange(9, 16); p->col = Color(240, 232, 220, 200); p->grav = -0.02f;
  }
}
static void sparkle(float x, float y, int n, Color col) {
  if (!fxOn()) return;
  for (int i = 0; i < n; i++) {
    Part* p = spawn();
    if (!p) return;
    float a = frand() * 6.283f, s = frange(1, 6);
    p->x = x; p->y = y; p->vx = cosf(a) * s; p->vy = sinf(a) * s - 2;
    p->life = p->max = frange(20, 40); p->size = frange(10, 22); p->col = col; p->img = &IMG_SPARK; p->add = true;
    p->rot = frand() * 90; p->vr = frange(-6, 6); p->grav = 0.08f;
  }
}
static void popup(float x, float y, const char* s, Color col = Color(255, 255, 255)) {
  if (!fxOn()) return;
  for (auto& p : popups)
    if (!p.on) { p = {true, x, y, 60, {0}, col}; snprintf(p.txt, sizeof(p.txt), "%s", s); return; }
}
static void popupNum(float x, float y, uint32_t v) { char b[16]; snprintf(b, sizeof(b), "%lu", (unsigned long)v); popup(x, y, b, Color(255, 226, 120)); }
static void sfxAt(Sfx s, float x, float pitch = 1) {
  float pan = clampv((x - G.camX - W / 2) / (W / 2), -1.0f, 1.0f) * 0.6f;
  sfx(s, pan, pitch);
}
static void addShake(float s) { if (fxOn()) shake = fmaxf(shake, s); }

static void addScore(uint32_t v) {
  G.score += v;
  if (G.score > G.hiscore) { G.hiscore = G.score; G.newHi = true; }
}

// ------------------------------------------------------------ the level
static inline uint8_t cellAt(int tx, int ty) {
  if (ty < 0 || ty >= ROWS) return C_AIR;
  if (tx < 0 || tx >= G.mapW) return C_BLOCK;   // the level edges are walls
  return G.cells[ty][tx];
}
static inline bool solidCell(uint8_t c) { return c == C_GROUND || c == C_BLOCK || c == C_BOX || c == C_BOXHEART || c == C_BOXUSED; }

// belt colours earned level by level: yellow, orange, green, blue, purple, brown, then black
static const Color BELTS[7] = {Color(250, 206, 60), Color(250, 140, 50), Color(80, 190, 100), Color(70, 130, 230),
                               Color(160, 90, 210), Color(150, 96, 60), Color(48, 44, 58)};
static Color beltColor() { return BELTS[clampv(G.levelIdx + G.loopN * NLEVELS, 0, 6)]; }

static void autotile() {
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < G.mapW; x++) {
      if (G.cells[y][x] != C_GROUND) continue;
      auto open = [&](int xx, int yy) {
        if (yy >= ROWS || xx < 0 || xx >= G.mapW) return false;
        if (yy < 0) return true;
        return G.cells[yy][xx] != C_GROUND;
      };
      int m = (open(x, y - 1) ? 1 : 0) | (open(x - 1, y) ? 2 : 0) | (open(x + 1, y) ? 4 : 0) | (open(x, y + 1) ? 8 : 0);
      int t = m;
      if (m == 0) { uint32_t h = (uint32_t)(x * 73856093u ^ y * 19349663u) % 11; if (h == 1) t = 16; else if (h == 6) t = 17; }
      G.tileGfx[y][x] = t;
    }
}

static void resetHero(float x, float y) {
  G.hero = {};
  G.hero.x = x; G.hero.y = y; G.hero.alive = true; G.hero.invuln = 60; G.hero.riding = -1;
  G.camX = clampv(x - W * 0.4f, 0.0f, (float)(G.mapW * TS - W));
}

static void initWeather() {
  for (auto& w : weather) { w.x = frand() * W; w.y = frand() * H; w.v = frange(0.4f, 1.0f); w.ph = frand() * 6.28f; w.z = frange(0.5f, 1.4f); }
}

static void loadLevel(int idx) {
  const LevelDef& L = LEVELS[idx];
  G.levelIdx = idx; G.world = L.world; G.mapW = L.w < MAXW ? L.w : MAXW;
  memset(G.cells, 0, sizeof(G.cells)); memset(G.enemies, 0, sizeof(G.enemies)); memset(G.movers, 0, sizeof(G.movers));
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  G.ndecos = 0; G.scrollsGot = 0; G.levelCoins = 0;
  float sx = 32 * K, sy = 150 * K;
  auto deco = [&](uint8_t type, float x, float y) { if (G.ndecos < 80) G.decos[G.ndecos++] = {type, x, y}; };
  auto enemy = [&](uint8_t type, float x, float y) {
    for (auto& e : G.enemies)
      if (!e.on) { e = {}; e.on = true; e.type = type; e.x = e.hx = x; e.y = e.hy = y; e.vx = -1; e.t = (int)(rnd() % 200); return; }
  };
  for (int y = 0; y < ROWS; y++)
    for (int x = 0; x < G.mapW; x++) {
      char ch = L.rows[y][x];
      uint8_t c = C_AIR;
      float px = x * TS, py = y * TS;
      switch (ch) {
        case '#': c = C_GROUND; break;
        case 'X': c = C_BLOCK; break;
        case '=': c = C_PLANK; break;
        case '^': c = C_SPIKE; break;
        case '~': c = C_WATER; break;
        case '?': c = C_BOX; break;
        case '!': c = C_BOXHEART; break;
        case 'o': c = C_COIN; break;
        case 'S': c = C_SCROLL; break;
        case 'P': sx = px + 3 * K; sy = py + TS - HH; break;
        case 'b': enemy(E_MOCHI, px, py + TS - 13 * K); break;
        case 'k': enemy(E_CHESTNUT, px, py); break;
        case 'c': enemy(E_CROW, px, py); break;
        case 'M': for (auto& m : G.movers) if (!m.on) { m = {true, px, px, py, 0.55f * K, 0}; break; } break;
        case 'C': deco(D_CHECK, px + 2 * K, py + TS); break;
        case 'G': deco(D_GOAL, px + TS / 2, py + TS); break;
        case 't': deco(D_TREE, px + TS / 2, py + TS); break;
        case 'l': deco(D_TORO, px + TS / 2, py + TS); break;
        case 'u': deco(D_BUSH, px + TS / 2, py + TS); break;
        case 'v': deco(D_FLOWERS, px + TS / 2, py + TS); break;
        case 'r': deco(D_ROCK, px + TS / 2, py + TS); break;
      }
      G.cells[y][x] = c;
    }
  autotile();
  G.checkX = sx; G.checkY = sy;
  resetHero(sx, sy);
  initWeather();
}

static void startLevel(int idx) {
  loadLevel(idx);
  G.hearts = 3;
  G.state = ST_INTRO; G.stateT = 0;
  music(nullptr);
}

static void resetGame() {
  G.score = 0; G.lives = 3; G.coins = 0; G.loopN = 0; G.newHi = false;
  startLevel(0);
}

// ------------------------------------------------------------ collisions
static bool boxSolid(float x, float y, float w, float h) {
  int tx0 = (int)floorf(x / TS), tx1 = (int)floorf((x + w - 0.01f) / TS);
  int ty0 = (int)floorf(y / TS), ty1 = (int)floorf((y + h - 0.01f) / TS);
  for (int ty = ty0; ty <= ty1; ty++)
    for (int tx = tx0; tx <= tx1; tx++)
      if (solidCell(cellAt(tx, ty))) return true;
  return false;
}
static float heroH() { return G.hero.rollT ? RH : HH; }

static void hurtHero(bool fatal);

static void hitBox(int tx, int ty) {
  if (tx < 0 || tx >= G.mapW || ty < 0 || ty >= ROWS) { sfxAt(SFX_BUMP, tx * TS); return; }
  uint8_t c = G.cells[ty][tx];
  if (c != C_BOX && c != C_BOXHEART) { sfxAt(SFX_BUMP, tx * TS); return; }
  G.cells[ty][tx] = C_BOXUSED;
  float px = tx * TS + TS / 2, py = ty * TS;
  if (c == C_BOXHEART) {
    if (G.hearts < 3) { G.hearts++; popup(px, py - 30, "HEART", Color(255, 150, 170)); } else { addScore(1000); popupNum(px, py - 30, 1000); }
    sfxAt(SFX_POWERUP, px);
    burst(px, py - 10, 16, Color(236, 60, 86), 1.6f, false, &IMG_HEART);
  } else {
    G.coins++; G.levelCoins++; addScore(100);
    sfxAt(SFX_COIN, px);
    burst(px, py - 20, 10, Color(255, 214, 74), 1.5f, true);
    popupNum(px, py - 40, 100);
  }
  sfxAt(SFX_BUMP, px);
  addShake(6);
}

static void killHero() {
  Hero& h = G.hero;
  if (!h.alive) return;
  h.alive = false; h.deadT = 0;
  h.vy = -5.5f * K; h.vx = 0; h.rollT = 0;
  sfx(SFX_DIE);
  if (fxOn()) rumble(0.8f, 400);
  addShake(18);
  G.state = ST_DEAD; G.stateT = 0;
}

bool invincible = false;   // tests: foes and spikes can't hurt her (pits and water still do)

static void hurtHero(bool fatal) {
  Hero& h = G.hero;
  if (!h.alive) return;
  if (invincible && !fatal) return;
  if (fatal) { G.hearts = 0; killHero(); return; }
  if (h.invuln || h.rollT) return;
  G.hearts--;
  if (G.hearts <= 0) { killHero(); return; }
  h.invuln = 100; h.hurtT = 24;
  h.vx = h.facingLeft ? 2.2f * K : -2.2f * K; h.vy = -3.5f * K;
  sfxAt(SFX_HURT, h.x);
  if (fxOn()) rumble(0.5f, 180);
  addShake(9);
}

static void collectAt(int tx, int ty) {
  uint8_t c = cellAt(tx, ty);
  float cx = tx * TS + TS / 2, cy = ty * TS + TS / 2;
  if (c == C_COIN) {
    G.cells[ty][tx] = C_AIR;
    G.coins++; G.levelCoins++; addScore(50);
    sfxAt(SFX_COIN, cx, 1 + (G.levelCoins % 5) * 0.02f);
    sparkle(cx, cy, 5, Color(255, 230, 140));
    if (G.coins % 100 == 0) { G.lives++; popup(cx, cy - 40, "1UP", Color(150, 255, 150)); sfx(SFX_ONEUP); }
  } else if (c == C_SCROLL) {
    G.cells[ty][tx] = C_AIR;
    G.scrollsGot++; addScore(2000);
    popup(cx, cy - 40, "SCROLL!", Color(255, 236, 170));
    sfx(SFX_SCROLL);
    sparkle(cx, cy, 24, Color(255, 240, 200));
    if (fxOn()) flash = 0.35f;
  }
}

// ------------------------------------------------------------ the heroine
static void updateHero(const Pad& in) {
  Hero& h = G.hero;
  if (h.invuln) h.invuln--;
  if (h.hurtT) h.hurtT--;
  if (h.rollCd) h.rollCd--;
  float ax = in.ax;
  if (in.down(BTN_LEFT)) ax = -1;
  if (in.down(BTN_RIGHT)) ax = 1;
  if (h.hurtT) ax = 0;
  const float MAXV = 2.3f * K;
  if (h.rollT) {
    h.rollT--;
    h.vx = (h.facingLeft ? -1 : 1) * 3.4f * K;
    if (!h.rollT) {   // stand up only if there's headroom, otherwise keep rolling
      if (boxSolid(h.x, h.y - (HH - RH), HW, HH)) h.rollT = 4;
      else { h.y -= HH - RH; h.rollCd = 14; }
    }
  } else {
    float acc = (h.onGround ? 0.28f : 0.18f) * K;
    if (ax > 0.2f) { h.vx = fminf(h.vx + acc * ax, MAXV * ax); h.facingLeft = false; }
    else if (ax < -0.2f) { h.vx = fmaxf(h.vx + acc * ax, MAXV * ax); h.facingLeft = true; }
    else { float f = (h.onGround ? 0.32f : 0.06f) * K; if (h.vx > f) h.vx -= f; else if (h.vx < -f) h.vx += f; else h.vx = 0; }
  }
  if (in.hit(BTN_A)) h.jumpBuf = 7; else if (h.jumpBuf) h.jumpBuf--;
  if (in.hit(BTN_B | BTN_X | BTN_R) && h.onGround && !h.rollT && !h.rollCd && !h.hurtT) {   // judo roll
    h.rollT = 26; h.y += HH - RH;
    sfxAt(SFX_ROLL, h.x);
    dust(h.x + HW / 2, h.y + RH, 6);
  }
  if (h.jumpBuf && (h.onGround || h.coyote)) {
    if (h.rollT && boxSolid(h.x, h.y - (HH - RH), HW, HH)) { /* no room to jump out of a roll here */ }
    else {
      if (h.rollT) { h.y -= HH - RH; h.rollT = 0; h.rollCd = 14; }
      h.vy = -7.0f * K; h.onGround = false;   // rises ~4.3 tiles: 3-tile climbs with room to spare
      h.coyote = 0; h.jumpBuf = 0; h.riding = -1;
      sfxAt(SFX_JUMP, h.x);
      dust(h.x + HW / 2, h.y + HH, 5);
      if (fxOn()) squash = 1.18f;
    }
  }
  if (!in.down(BTN_A) && h.vy < -2.2f * K) h.vy = -2.2f * K;   // let go early for a short hop
  h.vy = fminf(h.vy + (h.vy < 0 ? 0.34f : 0.4f) * K, 6.5f * K);
  if (h.riding >= 0) h.x += G.movers[h.riding].dx;
  float hh = heroH();
  h.x += h.vx;
  if (boxSolid(h.x, h.y, HW, hh)) {
    if (h.vx > 0) h.x = floorf((h.x + HW) / TS) * TS - HW; else if (h.vx < 0) h.x = floorf(h.x / TS) * TS + TS;
    else h.x = roundf(h.x);
    if (boxSolid(h.x, h.y, HW, hh)) h.x -= h.vx;
    h.vx = 0;
  }
  float oldBottom = h.y + hh;
  h.y += h.vy;
  bool wasGround = h.onGround;
  float landV = h.vy;
  h.onGround = false; h.riding = -1;
  if (h.vy >= 0) {
    float bottom = h.y + hh;
    int ty = (int)floorf(bottom / TS);
    bool land = false;
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 0.01f) / TS); tx++) {
      uint8_t c = cellAt(tx, ty);
      if (solidCell(c) || (c == C_PLANK && oldBottom <= ty * TS + 0.5f * K)) land = true;
    }
    if (land) { h.y = ty * TS - hh; h.vy = 0; h.onGround = true; }
    for (int i = 0; i < 8; i++) {
      Mover& m = G.movers[i];
      if (!m.on) continue;
      if (h.x + HW > m.x && h.x < m.x + 3 * TS && oldBottom <= m.y + 0.5f * K && bottom >= m.y) {
        h.y = m.y - hh; h.vy = 0; h.onGround = true; h.riding = i;
      }
    }
    if (h.onGround && !wasGround && h.vy == 0) {
      sfxAt(SFX_LAND, h.x);
      dust(h.x + HW / 2, h.y + hh, landV > 5 * K ? 7 : 3);
      if (fxOn()) squash = landV > 5 * K ? 0.78f : 0.88f;
    }
  } else {
    int ty = (int)floorf(h.y / TS);
    int cx = (int)floorf((h.x + HW / 2) / TS);
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 0.01f) / TS); tx++) {
      if (solidCell(cellAt(tx, ty))) {
        h.y = (ty + 1) * TS; h.vy = 0.5f * K;
        int bx = (cellAt(cx, ty) == C_BOX || cellAt(cx, ty) == C_BOXHEART) ? cx : tx;
        hitBox(bx, ty);
        break;
      }
    }
  }
  if (h.onGround) h.coyote = 6; else if (h.coyote) h.coyote--;
  // pickups and hazards
  for (int ty = (int)floorf(h.y / TS); ty <= (int)floorf((h.y + hh - 1) / TS); ty++)
    for (int tx = (int)floorf(h.x / TS); tx <= (int)floorf((h.x + HW - 1) / TS); tx++) {
      uint8_t c = cellAt(tx, ty);
      if (c == C_COIN || c == C_SCROLL) collectAt(tx, ty);
      if (c == C_SPIKE && h.y + hh > ty * TS + 8 * K) hurtHero(false);
      if (c == C_WATER && h.y + hh > ty * TS + 6 * K) {
        burst(h.x + HW / 2, ty * TS + 20, 22, Color(200, 236, 255), 1.8f);
        sfxAt(SFX_SPLASH, h.x);
        hurtHero(true);
      }
    }
  if (h.y > H + 8 * K) hurtHero(true);
  // checkpoints and the goal
  for (int i = 0; i < G.ndecos; i++) {
    Deco& d = G.decos[i];
    if (d.type == D_CHECK && h.x + HW > d.x && h.x < d.x + 20 * K && G.checkX < d.x) {
      G.checkX = d.x; G.checkY = d.y - HH - 1;
      sfx(SFX_CHECKPOINT);
      popup(d.x + 50, d.y - 230, "CHECKPOINT", Color(255, 220, 140));
      sparkle(d.x + 50, d.y - 110, 18, Color(255, 200, 120));
    }
    if (d.type == D_GOAL && h.x + HW / 2 > d.x - 4 * K && G.state == ST_PLAY) {
      G.state = ST_CLEAR; G.stateT = 0;
      h.vx = 0; h.rollT = 0;
      G.clearBonus = G.levelCoins * 10 + G.scrollsGot * 3000 + G.hearts * 500;
      addScore(G.clearBonus);
      music(&SONG_CLEAR);
      sparkle(d.x, d.y - 300, 40, Color(255, 220, 160));
    }
  }
  if (h.rollT) h.anim++;
  else if (h.onGround && fabsf(h.vx) > 0.3f * K) h.anim++;
  else if (h.onGround) h.anim = 0;
}

// ------------------------------------------------------------ foes and moving planks
static void updateEnemies() {
  float sp = speed() * (1.0f + 0.12f * G.loopN);
  Hero& hero = G.hero;
  for (auto& e : G.enemies) {
    if (!e.on) continue;
    if (!e.awake) { if (e.x < G.camX + W + 32 * K) e.awake = true; else continue; }
    e.t++;
    if (e.state == 2) {                              // knocked away: tumble off screen
      e.x += e.vx; e.y += e.vy; e.vy += 0.35f * K;
      if (e.y > H + 20 * K) e.on = false;
      continue;
    }
    if (e.state == 1) { if (e.t > 40) e.on = false; continue; }
    float w = (e.type == E_CROW ? 18 : 16) * K, hgt = (e.type == E_MOCHI ? 13 : e.type == E_CROW ? 11 : 16) * K;
    if (e.type == E_CROW) {
      // patrols in a lazy figure of eight, and dips towards her when she's close below
      float tt = e.t * 0.02f * sp;
      e.x = e.hx + 56 * K * sinf(tt);
      float dx = fabsf(hero.x - e.x);
      float dip = (dx < 70 * K && hero.y > e.y) ? 26 * K * (1 - dx / (70 * K)) : 0;
      e.y = e.hy + 10 * K * sinf(tt * 2) + dip;
      e.vx = cosf(tt);
    } else {
      float v = (e.type == E_MOCHI ? 0.55f : 0.42f) * K * sp;
      e.vx = e.vx < 0 ? -v : v;
      float nx = e.x + e.vx;
      int ahead = (int)floorf((e.vx > 0 ? nx + w : nx) / TS);
      int footRow = (int)floorf((e.y + hgt + 1) / TS);
      bool wall = solidCell(cellAt(ahead, (int)floorf((e.y + hgt - 2 * K) / TS)));
      bool ledge = !solidCell(cellAt(ahead, footRow)) && cellAt(ahead, footRow) != C_PLANK;
      if (wall || ledge) e.vx = -e.vx; else e.x = nx;
      e.vy = fminf(e.vy + 0.4f * K, 6.0f * K);
      e.y += e.vy;
      int by = (int)floorf((e.y + hgt) / TS);
      int bx = (int)floorf((e.x + w / 2) / TS);
      if (solidCell(cellAt(bx, by)) || cellAt(bx, by) == C_PLANK) { e.y = by * TS - hgt; e.vy = 0; }
      if (e.y > H + 20 * K) e.on = false;
    }
    if (!hero.alive) continue;
    float hh = heroH();
    if (!overlap(hero.x, hero.y, HW, hh, e.x + K, e.y + K, w - 2 * K, hgt - 2 * K)) continue;
    float cx = e.x + w / 2, cy = e.y + hgt / 2;
    if (hero.rollT) {                                 // a judo roll bowls anything over
      e.state = 2; e.vy = -4 * K; e.vx = hero.facingLeft ? -2 * K : 2 * K; e.t = 0;
      addScore(200); popupNum(cx, e.y - 40, 200);
      sfxAt(SFX_KNOCK, cx); burst(cx, cy, 12, Color(255, 255, 255), 1.6f, true);
      addShake(6);
    } else if (hero.vy > 0.5f * K && hero.y + hh - e.y < 10 * K && e.type != E_CHESTNUT) {   // stomp
      if (e.type == E_MOCHI) { e.state = 1; e.t = 0; } else { e.state = 2; e.vy = -2 * K; e.vx = 0; e.t = 0; }
      hero.vy = pad.down(BTN_A) ? -7.2f * K : -4.8f * K;
      addScore(100); popupNum(cx, e.y - 40, 100);
      sfxAt(SFX_STOMP, cx); burst(cx, e.y, 10, Color(255, 246, 240), 1.4f);
      if (fxOn()) squash = 1.15f;
    } else {
      hurtHero(false);
    }
  }
  for (auto& m : G.movers) {
    if (!m.on) continue;
    float px = m.x;
    m.x += m.v * sp;
    if (m.x > m.x0 + 4 * TS || m.x < m.x0) m.v = -m.v;
    m.dx = m.x - px;
  }
}

static void updateCamera() {
  Hero& h = G.hero;
  float target = h.x + HW / 2 - W * 0.42f + (h.facingLeft ? -26 * K : 26 * K);
  G.camX += (target - G.camX) * 0.1f;
  G.camX = clampv(G.camX, 0.0f, (float)(G.mapW * TS - W));
}

// the whole game's logic for one tick (no cosmetics) - the bot calls this directly
void logicStep(const Pad& in) {
  G.stateT++;
  switch (G.state) {
    case ST_TITLE:
      G.camX += 1.6f;
      if (G.camX > G.mapW * TS - W) G.camX = 0;
      if (titleInput(in)) G.hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_INTRO:
      if (G.stateT > 130 || (G.stateT > 30 && in.hit(BTN_A | BTN_START))) {
        G.state = ST_PLAY; G.stateT = 0;
        music(WORLD_SONGS[G.world]);
      }
      break;
    case ST_PLAY:
      if (in.hit(BTN_START)) { nova::pause(); break; }
      updateHero(in);
      if (G.state == ST_PLAY || G.state == ST_CLEAR) updateEnemies();
      updateCamera();
      break;
    case ST_DEAD:
      G.hero.y += G.hero.vy; G.hero.vy += 0.3f * K; G.hero.deadT++;
      updateEnemies();
      if (G.stateT > 110) {
        G.lives--;
        if (G.lives <= 0) { G.state = ST_OVER; G.stateT = 0; music(&SONG_OVER); if (G.newHi) saveHi(G.hiscore); }
        else { G.hearts = 3; resetHero(G.checkX, G.checkY); G.state = ST_PLAY; G.stateT = 0; }
      }
      break;
    case ST_CLEAR:
      if (!G.hero.onGround) { Pad none; updateHero(none); }
      G.hero.vx = 0;
      updateCamera();
      if (G.stateT > 220) {
        if (G.levelIdx + 1 < NLEVELS) startLevel(G.levelIdx + 1);
        else { G.state = ST_WIN; G.stateT = 0; music(&SONG_CLEAR); if (G.newHi) saveHi(G.hiscore); }
      }
      break;
    case ST_WIN:
      if (G.stateT > 240 && in.hit(BTN_A | BTN_START)) { G.loopN++; startLevel(0); }
      break;
    case ST_OVER:
      if (G.stateT > 90 && in.hit(BTN_START | BTN_A)) { G.state = ST_TITLE; G.stateT = 0; loadLevel(0); music(&SONG_TITLE_DEF); }
      break;
  }
}

static void spring(float& a, float& v, float target, float k, float damp) {
  v += (target - a) * k;
  v *= damp;
  a += v;
}

static void cosmeticStep() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vy += p.grav; p.rot += p.vr;
    if (!p.img && p.grav < 0) { p.vx *= 0.92f; p.vy *= 0.92f; }
    if (--p.life <= 0) p.on = false;
  }
  for (auto& p : popups) if (p.on) { p.y -= 1.2f; if (--p.t <= 0) p.on = false; }
  int w = G.world;
  for (auto& wt : weather) {
    wt.ph += 0.03f;
    if (w == 0) { wt.x += wt.v * 2.2f * wt.z + sinf(wt.ph) * 1.2f; wt.y += wt.v * 1.8f * wt.z; }       // petals drift down-right
    else if (w == 1) { wt.x += sinf(wt.ph) * 1.0f; wt.y += cosf(wt.ph * 0.7f) * 0.7f; }                // fireflies wander
    else { wt.x += sinf(wt.ph) * 1.4f - 0.8f; wt.y += wt.v * 2.6f * wt.z; }                             // snow falls
    if (wt.y > H) wt.y -= H;
    if (wt.y < 0) wt.y += H;
    if (wt.x > W) wt.x -= W;
    if (wt.x < 0) wt.x += W;
  }
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  // hair, headband and belt tails lag behind her movement
  Hero& h = G.hero;
  float dir = h.facingLeft ? -1.0f : 1.0f;
  float wind = sinf(frameNo * 0.07f) * 6;
  float sway = clampv(h.vx * dir * 3.0f + (h.onGround ? 0 : -h.vy * 1.4f), -10.0f, 80.0f);
  spring(tailAng, tailVel, 12 + sway + wind * 0.6f, 0.08f, 0.82f);
  spring(bandAng, bandVel, 30 + sway * 1.2f + wind, 0.1f, 0.8f);
  spring(beltAng, beltVel, 6 + sway * 0.5f, 0.12f, 0.78f);
  squash += (1 - squash) * 0.2f;
  flash *= 0.9f;
}

static void step(const Pad& in) {
  logicStep(in);
  cosmeticStep();
}

// ------------------------------------------------------------ drawing: the heroine
struct Pose {
  float torso, head;                  // torso lean (clockwise = forward), head tilt relative to the torso
  float armF, elbF, armB, elbB;       // shoulder and elbow
  float legF, kneeF, legB, kneeB;     // hip and knee
  float bob;                          // hip height offset
  const Img* face;
};

static void rot(float ang, float x, float y, float& ox, float& oy) {
  float a = ang * 0.0174533f, c = cosf(a), s = sinf(a);
  ox = x * c - y * s; oy = x * s + y * c;
}

static void heroPose(Pose& p) {
  Hero& h = G.hero;
  float t = frameNo;
  p = {};
  p.face = (frameNo % 200) < 7 ? &IMG_HEAD_BLINK : &IMG_HEAD_N;
  if (!h.alive || h.hurtT) {
    p.torso = -14; p.head = -10;
    p.armF = -150 + 20 * sinf(t * 0.5f); p.elbF = -30; p.armB = 140; p.elbB = 30;
    p.legF = -30; p.kneeF = 40; p.legB = 20; p.kneeB = 30;
    p.face = &IMG_HEAD_HURT;
    return;
  }
  if (G.state == ST_CLEAR && G.stateT > 20) {   // a bow at the gate
    float k = easeInOut((G.stateT - 20) / 25.0f) * (G.stateT < 150 ? 1 : 1 - easeInOut((G.stateT - 150) / 25.0f));
    p.torso = 42 * k; p.head = 10 * k;
    p.armF = -6 - 34 * k; p.elbF = -6; p.armB = 4 - 30 * k; p.elbB = -6;
    p.legF = -6; p.legB = 6; p.kneeF = p.kneeB = 4;
    p.face = k > 0.3f ? &IMG_HEAD_HAPPY : p.face;
    return;
  }
  if (!h.onGround) {
    if (h.vy < 0) {   // rising: one knee up, arm reaching
      p.torso = 6; p.head = -4;
      p.armF = 55; p.elbF = -50; p.armB = -150; p.elbB = -25;
      p.legF = -70; p.kneeF = 80; p.legB = 18; p.kneeB = 40;
      p.face = &IMG_HEAD_FOCUS;
    } else {          // falling: legs reaching down, arms out for balance
      p.torso = 2; p.head = 2;
      p.armF = -95; p.elbF = -20; p.armB = 95; p.elbB = 25;
      p.legF = -25; p.kneeF = 30; p.legB = 14; p.kneeB = 20;
    }
    return;
  }
  float speedK = fabsf(h.vx) / (2.3f * K);
  if (speedK > 0.12f) {   // the run cycle
    float ph = h.anim * 0.24f;
    float s = sinf(ph), c = cosf(ph);
    p.torso = 10 * speedK; p.head = -6 * speedK;
    p.legF = -s * 40; p.legB = s * 40;
    p.kneeF = 12 + 55 * fmaxf(0, -cosf(ph - 0.6f)) ; p.kneeB = 12 + 55 * fmaxf(0, cosf(ph - 0.6f));
    p.armF = s * 50; p.elbF = -70; p.armB = -s * 50; p.elbB = -70;
    p.bob = -fabsf(c) * 6 + 3;
    p.face = &IMG_HEAD_FOCUS;
    return;
  }
  // idle: breathing
  float b = sinf(t * 0.05f);
  p.torso = 2 + b * 1.2f; p.head = -2 - b * 1.5f;
  p.armF = -8 + b * 3; p.elbF = -18; p.armB = 10 - b * 3; p.elbB = -14;
  p.legF = -7; p.kneeF = 4; p.legB = 7; p.kneeB = 4;
  p.bob = b * 1.2f;
}

// draws the rigged heroine with her feet at (fx, fy). scale lets the title screen draw her bigger.
static void drawHeroAt(float fx, float fy, bool left, float scale, float alpha, const Pose& p, Color belt) {
  float dir = left ? -1.0f : 1.0f;
  float sq = squash;
  // hip point: legs are 17 + 13 + 9 long
  float hipX = 0, hipY = -39 + p.bob;
  // squash and stretch about her feet: wider when squashed, thinner when stretched
  float kx = scale / sqrtf(sq), ky = scale * sq;
  auto place = [&](const Img& img, float lx, float ly, float ang, Color tint) {
    Fx f;
    f.sx = kx; f.sy = ky;
    f.rot = ang * dir; f.flip = left; f.tint = tint.alpha(alpha);
    draw(img, fx + lx * dir * kx, fy + ly * ky, f);
  };
  Color back(196, 200, 226), front = WHITE;
  float ox, oy;
  // legs from the hip
  auto leg = [&](float hipAng, float knee, float sideX, Color tint) {
    float kx, ky;
    rot(hipAng, 0, 17, kx, ky);
    place(IMG_LEG_UP, hipX + sideX, hipY, hipAng, tint);
    place(IMG_LEG_LO, hipX + sideX + kx, hipY + ky, hipAng + knee, tint);
  };
  float nx, ny, shx, shy;
  rot(p.torso, 0, -32, nx, ny);
  rot(p.torso, 1, -27, shx, shy);
  auto arm = [&](float sAng, float elb, Color tint) {
    float a = p.torso + sAng, ex, ey;
    rot(a, 0, 15, ex, ey);
    place(IMG_ARM_UP, hipX + shx, hipY + shy, a, tint);
    place(IMG_ARM_LO, hipX + shx + ex, hipY + shy + ey, a + elb, tint);
  };
  float headAng = p.torso + p.head;
  // hair and headband tails behind everything
  float px, py;
  rot(headAng, -15, -40, px, py);
  place(IMG_PONYTAIL, hipX + nx + px, hipY + ny + py, tailAng, front);
  rot(headAng, -21, -35, px, py);
  place(IMG_BAND_TAIL, hipX + nx + px, hipY + ny + py, bandAng, front);
  arm(p.armB, p.elbB, back);
  leg(p.legB, p.kneeB, -2, back);
  leg(p.legF, p.kneeF, 2, front);
  place(IMG_TORSO, hipX, hipY, p.torso, front);
  rot(p.torso, 0, -5, ox, oy);
  place(IMG_BELT, hipX + ox, hipY + oy, p.torso, belt);
  rot(p.torso, 11, -3, ox, oy);
  place(IMG_BELT_TAIL, hipX + ox, hipY + oy, beltAng, belt);
  place(*p.face, hipX + nx, hipY + ny, headAng, front);
  arm(p.armF, p.elbF, front);
}

static const float HERO_SCALE = 1.15f;   // drawn a little bigger than the original's proportions

static void drawHero(float cx, float cy) {
  Hero& h = G.hero;
  if (!h.alive && G.state != ST_DEAD) return;
  if (h.invuln && ((h.invuln >> 2) & 1) && h.alive) return;
  float hh = heroH();
  float fx = h.x + HW / 2 - cx, fy = h.y + hh + cy;
  // a soft shadow on the ground under her
  if (h.alive) {
    int ty = (int)floorf((h.y + hh + 2) / TS);
    for (int k = 0; k < 4 && ty + k < ROWS; k++) {
      int tx = (int)floorf((h.x + HW / 2) / TS);
      uint8_t c = cellAt(tx, ty + k);
      if (solidCell(c) || c == C_PLANK) {
        float gy = (ty + k) * TS + cy;
        float d = (gy - fy) / (TS * 4);
        disc(fx, gy + 2, 26 * (1 - d * 0.6f), Color(20, 10, 40, (uint8_t)(70 * (1 - d))));
        break;
      }
    }
  }
  if (h.rollT) {
    Fx f; f.rot = (h.facingLeft ? -1 : 1) * h.anim * 22.0f; f.sx = f.sy = HERO_SCALE;
    draw(IMG_BALL, fx, fy - RH / 2, f);
    f.tint = beltColor();
    draw(IMG_BALL_BELT, fx, fy - RH / 2, f);
    return;
  }
  Pose p;
  heroPose(p);
  drawHeroAt(fx, fy, h.facingLeft, HERO_SCALE, 1, p, beltColor());
}

// ------------------------------------------------------------ drawing: the world
struct Sky { Color top, bot, sun; bool moon; };
static const Sky SKY[3] = {
  {Color(110, 178, 244), Color(255, 218, 200), Color(255, 240, 200), false},
  {Color(56, 46, 120), Color(255, 150, 120), Color(255, 200, 150), false},
  {Color(118, 150, 200), Color(232, 240, 252), Color(255, 255, 255), true},
};

static void drawSky(float cx) {
  const Sky& s = SKY[G.world];
  rectGrad(0, 0, W, 760, s.top, s.bot);
  rect(0, 760, W, H - 760, s.bot);
  float sx = 1460 - cx * 0.02f, sy = G.world == 1 ? 560 : 200;
  glow(sx, sy, 420, s.sun.alpha(0.35f));
  glow(sx, sy, 200, s.sun.alpha(0.6f));
  disc(sx, sy, 78, s.sun);
  if (G.world == 1) {   // a few early stars at dusk
    for (int i = 0; i < 40; i++) {
      uint32_t hsh = i * 2654435761u;
      float x = (float)(hsh % 1920), y = (float)((hsh >> 12) % 300);
      disc(x, y, 1.5f + (hsh % 2), Color(255, 255, 255, (uint8_t)(90 + 80 * sinf(frameNo * 0.04f + i))));
    }
  }
  for (int i = 0; i < 4; i++) {
    float speed = 0.05f + i * 0.02f;
    float x = fmodf(i * 610 - cx * speed - frameNo * (0.15f + i * 0.05f), 2400.0f);
    if (x < -300) x += 2400;
    float y = 120 + i * 70 + (i % 2) * 30;
    Fx f; f.tint = Color(255, 255, 255, G.world == 1 ? 150 : 230); f.sx = f.sy = 0.8f + 0.15f * i;
    if (G.world == 1) f.tint = Color(255, 190, 200, 170);
    draw(*CLOUDS[i % 3], x, y, f);
  }
}

static void drawBackdrop(float cx) {
  static const Img* FAR[3] = {&IMG_BG_FAR0, &IMG_BG_FAR1, &IMG_BG_FAR2};
  static const Img* MID[3] = {&IMG_BG_MID0, &IMG_BG_MID1, &IMG_BG_MID2};
  drawStrip(*FAR[G.world], -cx * 0.12f, 200);
  // mist between the layers
  const Sky& s = SKY[G.world];
  rectGrad(0, 520, W, 260, s.bot.alpha(0), s.bot.alpha(G.world == 2 ? 0.75f : 0.45f));
  drawStrip(*MID[G.world], -cx * 0.35f, 600);
  rectGrad(0, 860, W, 220, Color(0, 0, 0, 0), Color(20, 10, 40, 40));
}

static const Img& groundImg(int t) {
  static const Img* const* TBL[3] = {GROUND0, GROUND1, GROUND2};
  return *TBL[G.world][t];
}

static void drawDecos(float cx, float cy, bool front) {
  for (int i = 0; i < G.ndecos; i++) {
    const Deco& d = G.decos[i];
    const Img* img = nullptr;
    switch (d.type) {
      case D_TREE: img = G.world == 0 ? &IMG_SAKURA : G.world == 1 ? &IMG_BAMBOO : &IMG_PINE; break;
      case D_TORO: img = &IMG_TORO; break;
      case D_BUSH: img = &IMG_BUSH; break;
      case D_FLOWERS: img = &IMG_FLOWERS; break;
      case D_ROCK: img = &IMG_ROCK; break;
      case D_CHECK: img = G.checkX >= d.x ? &IMG_LANTERN1 : &IMG_LANTERN0; break;
      case D_GOAL: img = &IMG_TORII; break;
    }
    if (!img || front != (d.type == D_FLOWERS || d.type == D_BUSH)) continue;
    float x = d.x - cx, y = d.y + cy;
    if (d.type == D_CHECK) x += 30;
    if (x < -300 || x > W + 300) continue;
    Fx f;
    if (d.type == D_TREE || d.type == D_FLOWERS || d.type == D_BUSH) {   // a gentle sway
      f.rot = sinf(frameNo * 0.02f + d.x * 0.01f) * (d.type == D_TREE ? 0.8f : 2.0f);
    }
    if (d.type == D_GOAL) glow(x, y - 200, 300, Color(255, 200, 140, 60));
    draw(*img, x, y, f);
    if (d.type == D_CHECK && G.checkX >= d.x) {   // a warm, flickering glow round a lit lantern
      float fl = 0.85f + 0.15f * sinf(frameNo * 0.3f + d.x);
      glow(x + 24, y - 104, 120 * fl, Color(255, 170, 80, 150));
      glow(x + 24, y - 104, 50, Color(255, 230, 160, 160));
    }
  }
}

static void drawTiles(float cx, float cy, bool waterPass) {
  int tx0 = clampv((int)floorf(cx / TS) - 1, 0, G.mapW - 1), tx1 = clampv((int)((cx + W) / TS) + 1, 0, G.mapW - 1);
  for (int ty = 0; ty < ROWS; ty++)
    for (int tx = tx0; tx <= tx1; tx++) {
      uint8_t c = G.cells[ty][tx];
      float x = tx * TS - cx, y = ty * TS + cy;
      if (waterPass) {
        if (c != C_WATER) continue;
        bool top = cellAt(tx, ty - 1) != C_WATER;
        const int N = 6;
        Color deep(40, 90, 170, 220), shallow(120, 200, 240, 170);
        if (G.world == 1) { deep = Color(40, 50, 110, 225); shallow = Color(240, 140, 150, 160); }
        if (G.world == 2) { deep = Color(70, 110, 170, 220); shallow = Color(200, 230, 250, 180); }
        Vtx v[N * 6];
        int k = 0;
        for (int i = 0; i < N; i++) {
          float xa = x + TS * i / (float)N, xb = x + TS * (i + 1) / (float)N;
          auto surf = [&](float xx) {
            float wx = xx + cx;
            return top ? y + 14 + 5 * sinf(wx * 0.03f + frameNo * 0.08f) + 3 * sinf(wx * 0.071f - frameNo * 0.05f) : y;
          };
          float ya = surf(xa), yb = surf(xb), y2 = y + TS + 0.5f;
          Color ct = top ? shallow : mix(shallow, deep, 0.6f);
          Color cb = top ? mix(shallow, deep, 0.6f) : deep;
          v[k++] = {xa, ya, ct}; v[k++] = {xb, yb, ct}; v[k++] = {xb, y2, cb};
          v[k++] = {xa, ya, ct}; v[k++] = {xb, y2, cb}; v[k++] = {xa, y2, cb};
          if (top) line(xa, ya, xb, yb, 4, Color(255, 255, 255, 200));
        }
        tris(v, k);
        if (top && ((tx * 7 + frameNo / 20) % 5 == 0)) glow(x + 36 + 20 * sinf(frameNo * 0.05f + tx), y + 26, 18, Color(255, 255, 255, 80));
        continue;
      }
      switch (c) {
        case C_GROUND: draw(groundImg(G.tileGfx[ty][tx]), x, y); break;
        case C_BLOCK: draw(IMG_BLOCK, x, y); break;
        case C_PLANK: {
          bool l = cellAt(tx - 1, ty) == C_PLANK, r = cellAt(tx + 1, ty) == C_PLANK;
          draw(!l && !r ? IMG_PLANK_ONE : !l ? IMG_PLANK_L : !r ? IMG_PLANK_R : IMG_PLANK_M, x, y);
          break;
        }
        case C_SPIKE: draw(IMG_SPIKES, x, y); break;
        case C_BOX: case C_BOXHEART: {
          draw(c == C_BOX ? IMG_BOX : IMG_BOXHEART, x, y);
          float sh = fmodf(frameNo * 0.02f + tx * 0.13f, 3.0f);   // a glint sweeping across now and then
          if (sh < 1) {
            float gx = x + 8 + sh * (TS - 16);
            line(gx - 10, y + TS - 8, gx + 10, y + 8, 6, Color(255, 255, 255, (uint8_t)(120 * sinf(sh * 3.14f))), BLEND_ADD);
          }
          break;
        }
        case C_BOXUSED: draw(IMG_USED, x, y); break;
        case C_COIN: {
          float ph = frameNo * 0.08f + tx * 0.4f;
          Fx f; f.sx = fmaxf(0.12f, fabsf(cosf(ph)));
          f.tint = cosf(ph) < 0 ? Color(230, 200, 150) : WHITE;
          float bob = 3 * sinf(frameNo * 0.06f + tx);
          glow(x + TS / 2, y + TS / 2 + bob, 34, Color(255, 220, 120, 50));
          draw(IMG_COIN, x + TS / 2, y + TS / 2 + bob, f);
          break;
        }
        case C_SCROLL: {
          float bob = 8 * sinf(frameNo * 0.06f);
          float pulse = 0.7f + 0.3f * sinf(frameNo * 0.1f);
          glow(x + TS / 2, y + TS / 2 + bob, 90 * pulse, Color(255, 236, 170, 120));
          Fx f; f.rot = 6 * sinf(frameNo * 0.04f);
          draw(IMG_SCROLL, x + TS / 2, y + TS / 2 + bob, f);
          if ((frameNo / 8) % 3 == 0) {
            Fx s; s.blend = BLEND_ADD; s.sx = s.sy = 0.6f; s.rot = frameNo * 3.0f;
            draw(IMG_SPARK, x + 10 + (frameNo * 13) % 50, y + 6 + bob + (frameNo * 7) % 40, s);
          }
          break;
        }
        default: break;
      }
    }
}

static void drawMovers(float cx, float cy) {
  for (auto& m : G.movers) {
    if (!m.on) continue;
    float x = m.x - cx, y = m.y + cy;
    // ropes up out of the top of the screen
    line(x + 14, y + 8, x + 14, 0, 3, Color(120, 90, 70, 200));
    line(x + 3 * TS - 14, y + 8, x + 3 * TS - 14, 0, 3, Color(120, 90, 70, 200));
    draw(IMG_PLANK_L, x, y); draw(IMG_PLANK_M, x + TS, y); draw(IMG_PLANK_R, x + 2 * TS, y);
  }
}

static void drawEnemies(float cx, float cy) {
  for (auto& e : G.enemies) {
    if (!e.on || !e.awake) continue;
    float x = e.x - cx, y = e.y + cy;
    if (x < -150 || x > W + 150) continue;
    Fx f;
    if (e.type == E_MOCHI) {
      float w = 16 * K, h = 13 * K;
      float bx = x + w / 2, by = y + h;
      const Img* img;
      static const Img* MO[3][2] = {{&IMG_MOCHI0_0, &IMG_MOCHI0_1}, {&IMG_MOCHI1_0, &IMG_MOCHI1_1}, {&IMG_MOCHI2_0, &IMG_MOCHI2_1}};
      static const Img* FLAT[3] = {&IMG_MOCHI0_FLAT, &IMG_MOCHI1_FLAT, &IMG_MOCHI2_FLAT};
      if (e.state == 1) { img = FLAT[G.world]; f.tint.a = (uint8_t)(255 * clampv(1 - (e.t - 25) / 15.0f, 0.0f, 1.0f)); }
      else {
        img = MO[G.world][(e.t % 150) < 6 ? 1 : 0];
        float b = sinf(e.t * 0.16f);
        f.sy = 1 + 0.07f * b; f.sx = 1 - 0.06f * b;
        f.flip = e.vx > 0;
      }
      if (e.state == 2) { f.sy = -1; f.rot = e.t * 9.0f; by = y + h / 2; }
      disc(bx, by + 2, 30, Color(20, 10, 40, 50));
      draw(*img, bx, by, f);
    } else if (e.type == E_CROW) {
      float w = 18 * K, h = 11 * K;
      float bx = x + w / 2, by = y + h / 2;
      bool left = e.vx < 0;
      f.flip = left;
      if (e.state == 2) { f.sy = -1; f.rot = e.t * 8.0f; }
      float flap = sinf(e.t * 0.32f) * 40;
      Fx wf = f; wf.rot = (left ? 1 : -1) * flap + f.rot;
      draw(IMG_CROW, bx, by, f);
      draw(IMG_CROW_WING, bx + (left ? 4 : -4), by - 10, wf);
    } else {
      float w = 16 * K;
      float bx = x + w / 2, by = y + w / 2;
      f.rot = e.x * 1.3f;
      if (e.state == 2) f.rot = e.t * 15.0f;
      disc(bx, y + w + 2, 30, Color(20, 10, 40, 50));
      draw(IMG_CHESTNUT, bx, by, f);
      if (e.state != 2) {
        Fx ff; ff.flip = e.vx < 0;
        bool angry = fabsf(G.hero.x - e.x) < 300;
        draw(angry ? IMG_CHESTNUT_FACE1 : IMG_CHESTNUT_FACE0, bx + (e.vx < 0 ? -6 : 6), by + 2, ff);
      }
    }
  }
}

static void drawParticles(float cx, float cy) {
  for (auto& p : parts) {
    if (!p.on) continue;
    float a = clampv(p.life / (p.max * 0.5f), 0.0f, 1.0f);
    float x = p.x - cx, y = p.y + cy;
    if (p.img) {
      Fx f; f.rot = p.rot; f.tint = p.col.alpha(a); f.blend = p.add ? BLEND_ADD : BLEND_ALPHA;
      f.sx = f.sy = p.size / 20.0f;
      draw(*p.img, x, y, f);
    } else if (p.add) {
      glow(x, y, p.size * 2.5f, p.col.alpha(a));
    } else {
      float grow = p.grav < 0 ? 1 + (1 - p.life / p.max) * 1.2f : 1;
      disc(x, y, p.size * grow, p.col.alpha(a * p.col.a / 255.0f));
    }
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    TextStyle st; st.size = 40; st.align = CENTER; st.color = p.col.alpha(clampv(p.t / 15.0f, 0.0f, 1.0f));
    st.outline = Color(40, 20, 60, st.color.a);
    text(p.txt, p.x - cx, p.y + cy, st);
  }
}

static void drawWeather() {
  for (auto& w : weather) {
    if (G.world == 0) {
      Fx f; f.rot = w.ph * 60; f.sx = f.sy = w.z; f.tint = Color(255, 255, 255, 220);
      draw(IMG_PETAL, w.x, w.y, f);
    } else if (G.world == 1) {
      float b = sinf(w.ph * 3);
      if (b > 0) {
        glow(w.x, w.y, 26 * w.z, Color(255, 236, 120, (uint8_t)(140 * b)));
        disc(w.x, w.y, 3 * w.z, Color(255, 255, 210, (uint8_t)(255 * b)));
      }
    } else {
      disc(w.x, w.y, 3.5f * w.z, Color(255, 255, 255, (uint8_t)(160 + 60 * (w.z - 0.5f))));
    }
  }
}

static void drawHud() {
  roundRect(20, 18, W - 40, 84, 42, Color(20, 14, 50, 150));
  for (int i = 0; i < 3; i++) {
    Fx f; f.sx = f.sy = 1.15f;
    if (i == G.hearts - 1 && G.hearts == 1) f.sx = f.sy = 1.15f + 0.1f * sinf(frameNo * 0.3f);   // the last heart beats
    draw(i < G.hearts ? IMG_HEART : IMG_HEART_EMPTY, 74 + i * 56, 60, f);
  }
  TextStyle st; st.size = 46; st.color = WHITE;
  draw(IMG_COIN, 290, 60);
  textf(326, 38, st, "x %02d", G.coins % 100);
  for (int i = 0; i < 3; i++) {
    Fx f; f.sx = f.sy = 0.75f;
    if (i >= G.scrollsGot) f.tint = Color(60, 50, 90, 200);
    draw(IMG_SCROLL, 520 + i * 74, 60, f);
  }
  TextStyle sc = st; sc.color = Color(255, 220, 110); sc.align = CENTER;
  textf(W / 2 + 40, 38, sc, "%07lu", (unsigned long)G.score);
  Fx hf; hf.sx = hf.sy = 0.85f;
  draw(IMG_HEAD_N, 1330, 90, hf);
  textf(1366, 38, st, "x %d", G.lives);
  TextStyle lv = st; lv.align = RIGHT; lv.color = Color(170, 236, 255);
  textf(W - 60, 38, lv, "%d-%d", G.world + 1 + G.loopN * 3, G.levelIdx % 2 + 1);
}

static void drawWorld() {
  float cx = G.camX - shakeX, cy = shakeY;
  drawSky(cx);
  drawBackdrop(cx);
  drawDecos(cx, cy, false);
  drawTiles(cx, cy, false);
  drawMovers(cx, cy);
  drawEnemies(cx, cy);
  if (G.state != ST_TITLE) drawHero(cx, cy);
  drawTiles(cx, cy, true);
  drawDecos(cx, cy, true);
  drawParticles(cx, cy);
  drawWeather();
  if (G.world == 1) rectGrad(0, 0, W, H, Color(60, 20, 80, 0), Color(60, 20, 80, 40));   // dusk tint
}

static void panel(float x, float y, float w, float h) {
  roundRect(x + 10, y + 14, w, h, 40, Color(0, 0, 0, 80));
  roundRect(x, y, w, h, 40, Color(28, 22, 64, 242));
  roundRect(x + 8, y + 8, w - 16, 70, 32, Color(255, 255, 255, 16));
}

static void draw() {
  drawWorld();
  bool blink = (frameNo >> 5) & 1;
  if (G.state == ST_TITLE) {
    rectGrad(0, 0, W, H, Color(20, 10, 50, 60), Color(20, 10, 50, 170));
    roundRect(W / 2 - 600, 340, 1200, 600, 48, Color(20, 14, 50, 170));
    Fx lf; lf.sx = lf.sy = 1 + 0.015f * sinf(frameNo * 0.04f);
    draw(IMG_LOGO, W / 2, 210, lf);
    TextStyle sub; sub.size = 40; sub.align = CENTER; sub.color = Color(200, 240, 255); sub.outline = Color(40, 20, 70);
    text("A LITTLE JUDOKA'S MOUNTAIN QUEST", W / 2, 370, sub);
    // the heroine, big, cycling idle -> bow
    Pose p;
    int f = (frameNo / 150) % 3;
    p = {};
    float b = sinf(frameNo * 0.05f);
    p.torso = 2 + b; p.head = -2 - b; p.armF = -8 + b * 3; p.elbF = -18; p.armB = 10; p.elbB = -14;
    p.legF = -7; p.kneeF = 4; p.legB = 7; p.kneeB = 4;
    p.face = (frameNo % 200) < 7 ? &IMG_HEAD_BLINK : &IMG_HEAD_N;
    if (f == 2) {
      float t = (frameNo % 150) / 150.0f;
      float k = t < 0.2f ? easeInOut(t / 0.2f) : t > 0.8f ? 1 - easeInOut((t - 0.8f) / 0.2f) : 1;
      p.torso = 40 * k; p.head = 10 * k; p.armF = -6 - 30 * k; p.armB = 4 - 28 * k; p.elbF = p.elbB = -6;
      if (k > 0.3f) p.face = &IMG_HEAD_HAPPY;
    }
    float sq = squash; squash = 1;
    drawHeroAt(330, 900, false, 2.6f, 1, p, beltColor());
    squash = sq;
    Fx mf; mf.sx = mf.sy = 1.6f; mf.sy *= 1 + 0.07f * sinf(frameNo * 0.16f);
    draw(IMG_MOCHI0_0, 1560, 880, mf);
    Fx cf; cf.sx = cf.sy = 1.5f; cf.rot = frameNo * 1.5f;
    draw(IMG_CHESTNUT, 1720, 830, cf);
    Fx ff; ff.sx = ff.sy = 1.5f;
    draw(IMG_CHESTNUT_FACE1, 1720, 834, ff);
    TextStyle hi; hi.size = 40; hi.align = CENTER; hi.color = Color(255, 220, 110);
    textf(W / 2, 450, hi, "HI-SCORE %07lu", (unsigned long)G.hiscore);
    TextStyle go; go.size = 64; go.align = CENTER; go.color = WHITE; go.shadow = 4;
    if (blink) textf(W / 2, 530, go, "PRESS %s", btnName(BTN_A));
    drawTitleOptions(640);
    TextStyle help; help.size = 30; help.align = CENTER; help.color = Color(220, 228, 250);
    textf(W / 2, 830, help, "%s JUMP (HOLD FOR HIGHER)     %s JUDO ROLL: BOWLS FOES OVER", btnName(BTN_A), btnName(BTN_B));
    text("STOMP THE BLOBS, ROLL THROUGH CHESTNUTS, FIND 3 SCROLLS IN EVERY LEVEL", W / 2, 880, help);
    TextStyle back = help; back.size = 24; back.color = Color(170, 180, 220);
    textf(W / 2, 1000, back, "%s: PAUSE / BACK TO THE MENU", btnName(BTN_HOME));
    return;
  }
  drawHud();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 250, 230, (uint8_t)(flash * 255)), BLEND_ADD);
  if (G.state == ST_INTRO) {
    float t = easeOut(G.stateT / 25.0f);
    rect(0, 0, W, H, Color(10, 6, 30, (uint8_t)(110 * t)));
    float y = 330 + (1 - t) * 60;
    panel(W / 2 - 520, y, 1040, 420);
    TextStyle a; a.size = 40; a.align = CENTER; a.color = Color(170, 236, 255);
    textf(W / 2, y + 40, a, "WORLD %d-%d", G.world + 1 + G.loopN * 3, G.levelIdx % 2 + 1);
    TextStyle n = a; n.size = 96; n.color = WHITE; n.shadow = 5;
    text(LEVELS[G.levelIdx].name, W / 2, y + 110, n);
    // the belt she wears into this level
    Color bc = beltColor();
    roundRect(W / 2 - 220, y + 270, 440, 30, 12, bc);
    roundRect(W / 2 - 220, y + 288, 440, 12, 6, mix(bc, BLACK, 0.25f));
    roundRect(W / 2 - 26, y + 256, 52, 58, 14, mix(bc, BLACK, 0.15f));
    TextStyle l = a; l.size = 40; l.color = Color(255, 220, 110);
    textf(W / 2, y + 340, l, "LIVES %d", G.lives);
  } else if (G.state == ST_CLEAR && G.stateT > 30) {
    float t = easeOut((G.stateT - 30) / 20.0f);
    float y = 240 + (1 - t) * 50;
    panel(W / 2 - 480, y, 960, 500);
    TextStyle a; a.size = 84; a.align = CENTER; a.color = Color(180, 255, 120); a.shadow = 5;
    text("REI!  STAGE CLEAR", W / 2, y + 40, a);
    TextStyle r; r.size = 44; r.color = WHITE; r.outline = CLEAR;
    const char* labels[3] = {"COINS", "SCROLLS", "HEARTS"};
    char vals[3][32];
    snprintf(vals[0], 32, "%d x 10", G.levelCoins);
    snprintf(vals[1], 32, "%d/3 x 3000", G.scrollsGot);
    snprintf(vals[2], 32, "%d x 500", G.hearts);
    for (int i = 0; i < 3; i++) {
      if (G.stateT < 50 + i * 15) break;
      r.align = LEFT; text(labels[i], W / 2 - 380, y + 160 + i * 70, r);
      r.align = RIGHT; text(vals[i], W / 2 + 380, y + 160 + i * 70, r);
    }
    if (G.stateT > 100) {
      TextStyle b; b.size = 56; b.align = CENTER; b.color = Color(255, 220, 110);
      textf(W / 2, y + 390, b, "BONUS %d", G.clearBonus);
    }
  } else if (G.state == ST_WIN) {
    rect(0, 0, W, H, Color(10, 6, 30, 120));
    panel(W / 2 - 560, 180, 1120, 640);
    TextStyle a; a.size = 90; a.align = CENTER; a.color = Color(180, 255, 120); a.shadow = 5;
    text("ALL PEAKS CLEARED!", W / 2, 220, a);
    TextStyle b; b.size = 44; b.align = CENTER; b.color = WHITE;
    text("A NEW BELT AND A HARDER CLIMB", W / 2, 350, b);
    text("AWAIT ON THE NEXT LAP", W / 2, 410, b);
    Pose p = {};
    float k = 0.5f + 0.5f * sinf(frameNo * 0.05f);
    p.torso = 38 * k; p.head = 10 * k; p.armF = -30 * k; p.armB = -26 * k; p.face = &IMG_HEAD_HAPPY;
    p.legF = -6; p.legB = 6;
    drawHeroAt(W / 2, 740, false, 1.8f, 1, p, BELTS[clampv((G.loopN + 1) * NLEVELS, 0, 6)]);
    if (G.stateT > 240 && blink) { TextStyle c = b; c.color = Color(255, 220, 110); textf(W / 2, 760, c, "PRESS %s", btnName(BTN_A)); }
  } else if (G.state == ST_OVER) {
    rect(0, 0, W, H, Color(10, 6, 30, (uint8_t)(140 * easeOut(G.stateT / 30.0f))));
    TextStyle a; a.size = 150; a.align = CENTER; a.color = Color(240, 70, 90); a.shadow = 8;
    text("GAME OVER", W / 2, 360, a);
    TextStyle b; b.size = 48; b.align = CENTER; b.color = Color(255, 220, 110);
    if (G.newHi && blink) text("NEW HI-SCORE!", W / 2, 560, b);
    b.color = WHITE;
    if (G.stateT > 90) textf(W / 2, 650, b, "PRESS %s", btnName(BTN_A));
  }
}

// ------------------------------------------------------------ hooks for the launcher and the tests
static void init() {
  loadAtlas(TEX_FILES, TEX, NTEX);
}

static void enter() {
  G = Game();
  loadLevel(0);
  G.state = ST_TITLE; G.stateT = 0;
  G.hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE_DEF);
}

static bool canPause() { return G.state != ST_TITLE; }

void debugState(DebugInfo& d) {
  d.state = G.state; d.level = G.levelIdx; d.lives = G.lives; d.hearts = G.hearts;
  d.x = G.hero.x; d.y = G.hero.y; d.alive = G.hero.alive; d.onGround = G.hero.onGround;
  d.mapW = G.mapW * TS; d.scrolls = G.scrollsGot; d.score = G.score; d.loop = G.loopN;
}
void copyState(void* dst) { *(Game*)dst = G; }
void restoreState(const void* src) { G = *(const Game*)src; }
size_t stateSize() { return sizeof(Game); }
void startAt(int level) { G = Game(); G.lives = 3; G.hearts = 3; startLevel(level); G.state = ST_PLAY; }
bool tileSolidAt(float x, float y) { return solidCell(cellAt((int)floorf(x / TS), (int)floorf(y / TS))); }
uint8_t tileAt(float x, float y) { return cellAt((int)floorf(x / TS), (int)floorf(y / TS)); }

static int thumbTex = -1;
static const Img THUMB = {&thumbTex, 0, 0, 960, 540, 0, 0, 1};

}  // namespace pp

static void ppInit() { pp::init(); pp::thumbTex = nova::loadTexture("pixelpeaks/thumb.png"); }

extern const nova::Game PIXEL_PEAKS;
const nova::Game PIXEL_PEAKS = {
  "pixelpeaks", "PIXEL PEAKS", "A LITTLE JUDOKA'S MOUNTAIN QUEST",
  ppInit, pp::enter, pp::step, pp::draw, pp::canPause, &pp::THUMB,
};
