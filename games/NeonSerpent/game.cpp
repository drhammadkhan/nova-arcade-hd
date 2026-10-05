// =====================================================================
//  NEON SERPENT HD  -  a snake game (Nova Arcade HD)
//  Eat the glowing orbs, grow longer, don't bite yourself. Every level
//  brings a new wall layout; bonus stars appear for a short time.
//
//  Rules and timing are the original's, on its 30x21 grid. New for HD: the
//  serpent glides smoothly between cells as one glowing tube, with neon
//  walls, pulsing orbs, spinning stars and a light grid.
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

namespace ns {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  100, 4, "Em C Em D",
  {I_FLUTE, 0.8f, 0.1f, "E5 - - - G5 - - - B5 - - - A5 - G5 - | E5 - - - C5 - - - D5 - - - . . . . | E5 - - - G5 - - - B5 - - - A5 - G5 - | E5 - - - C5 - - - D5 - - - . . . ."},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.25f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.45f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_GAME = {
  128, 8, "Em C D Am Em C D Em",
  {I_SAWLEAD, 0.55f, 0.05f,
   "E5 - G5 - B5 - - G5 A5 - G5 - E5 - D5 - | C5 - E5 - G5 - - E5 F5 - E5 - C5 - - - |"
   "D5 - F#5 - A5 - - F#5 G5 - A5 - B5 - - - | A5 - - - G5 - E5 - C5 - - - B4 - - - |"
   "E5 - G5 - B5 - - G5 A5 - G5 - E5 - D5 - | C5 - E5 - G5 - - E5 F5 - E5 - C5 - - - |"
   "D5 - F#5 - A5 - - F#5 G5 - A5 - B5 - - - | A5 - - - G5 - E5 - C5 - - - B4 - - -"},
  {}, I_BELL, "1...5...3...5...", 0.18f, I_PLUCKBASS, "R.R.R.RRR.R.R.5.", 0.6f, I_PAD, 0.25f,
  "K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K.h.S.hhK.h.S.h.|K...S...K...S.SS",
  0.85f, 0, true};
static const Song SONG_OVER = {
  92, 2, "Em Em", {I_FLUTE, 0.8f, 0, "B4 - A4 - G4 - E4 - Eb4 - - - E4 - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_CLEAR = {
  140, 2, "Em Em", {I_MARIMBA, 0.8f, 0, "E5 G5 B5 E6 - - B5 - E6 - G6 - - - . . | . . . . . . . . . . . . . . . ."},
  {I_BELL, 0.4f, 0.3f, ". . . . B5 - - - . . . . E6 - - - | . . . . . . . . . . . . . . . ."},
  I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ board
static const int GCOLS = 30, GROWS = 21, MAXLEN = GCOLS * GROWS, FOOD_PER_LEVEL = 10;
static const float CELL = 48, GX0 = (W - GCOLS * CELL) / 2, GY0 = 56;
static const int8_t DX[4] = {1, 0, -1, 0}, DY[4] = {0, 1, 0, -1};   // right, down, left, up
static uint8_t wall[GROWS][GCOLS];
static uint16_t body[MAXLEN], prevBody[MAXLEN];   // body[0] is the head; cell = r * GCOLS + c
static int len = 0, prevLen = 0, grow = 0;
static int dir = 0, queued[2], nQueued = 0;
static int moveT = 0, moveNo = 0;
static int foodC = 0, foodR = 0;
static int starC = -1, starR = 0, starT = 0;
static int eaten = 0, level = 1, lives = 3;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 2000;
static bool newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;

struct Part { bool on; float x, y, vx, vy, life, max, size; Color col; bool spark; };
static Part parts[500];
struct Popup { bool on; float x, y; int t; char txt[12]; };
static Popup popups[6];
static float shake = 0, shakeX = 0, shakeY = 0, eatPulse = 0;
static float demoA = 0;

static inline int cellC(uint16_t v) { return v % GCOLS; }
static inline int cellR(uint16_t v) { return v / GCOLS; }
static inline float cx(float c) { return GX0 + c * CELL + CELL / 2; }
static inline float cy(float r) { return GY0 + r * CELL + CELL / 2; }

static Color bodyColor(float t) { return mix(Color(70, 220, 240), Color(220, 70, 230), t); }

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, bool spark = false) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) {
        float a = frand() * 6.283f, s = frange(0.3f, 1) * sp * 4.5f;
        p = {true, x, y, cosf(a) * s, sinf(a) * s, frange(16, 34), 34, frange(4, 9), col, spark && i % 2 == 0};
        break;
      }
}
static void popup(float x, float y, uint32_t v) {
  for (auto& p : popups) if (!p.on) { p = {true, x, y, 50, {0}}; snprintf(p.txt, sizeof(p.txt), "%lu", (unsigned long)v); return; }
}

// ------------------------------------------------------------ levels (the original's layouts)
static void wallRect(int c, int r, int w, int h) {
  for (int y = r; y < r + h; y++)
    for (int x = c; x < c + w; x++)
      if (x >= 0 && x < GCOLS && y >= 0 && y < GROWS) wall[y][x] = 1;
}
// The serpent starts on row 10 heading right from column 3, so layouts keep columns 2-13 of that row clear.
static void buildWalls(int lvl) {
  memset(wall, 0, sizeof(wall));
  switch ((lvl - 1) % 6) {
    case 0: break;
    case 1: wallRect(7, 4, 16, 1); wallRect(7, 16, 16, 1); break;
    case 2: wallRect(15, 2, 1, 7); wallRect(15, 12, 1, 7); wallRect(19, 10, 8, 1); break;
    case 3:
      wallRect(4, 3, 6, 1); wallRect(4, 3, 1, 4); wallRect(20, 3, 6, 1); wallRect(25, 3, 1, 4);
      wallRect(4, 17, 6, 1); wallRect(4, 14, 1, 4); wallRect(20, 17, 6, 1); wallRect(25, 14, 1, 4);
      wallRect(15, 7, 1, 7);
      break;
    case 4:
      wallRect(8, 0, 1, 7); wallRect(15, 14, 1, 7); wallRect(22, 0, 1, 7);
      wallRect(8, 14, 1, 7); wallRect(22, 14, 1, 7); wallRect(15, 0, 1, 7);
      break;
    case 5:
      wallRect(5, 5, 20, 1); wallRect(5, 15, 20, 1); wallRect(24, 6, 1, 4); wallRect(5, 11, 1, 4);
      wallRect(14, 8, 3, 1); wallRect(14, 12, 3, 1);
      break;
  }
}

static bool occupied(int c, int r, bool ignoreTail) {
  if (wall[r][c]) return true;
  int n = ignoreTail && !grow ? len - 1 : len;
  for (int i = 0; i < n; i++) if (body[i] == r * GCOLS + c) return true;
  return false;
}

static void placeFood() {
  int hc = cellC(body[0]), hr = cellR(body[0]);
  for (int tries = 0; tries < 2000; tries++) {
    int c = rnd() % GCOLS, r = rnd() % GROWS;
    if (occupied(c, r, false) || abs(c - hc) + abs(r - hr) < 4) continue;
    if (c == starC && r == starR) continue;
    foodC = c; foodR = r;
    return;
  }
}
static void placeStar() {
  for (int tries = 0; tries < 2000; tries++) {
    int c = rnd() % GCOLS, r = rnd() % GROWS;
    if (occupied(c, r, false) || (c == foodC && r == foodR)) continue;
    starC = c; starR = r; starT = frames(420);
    return;
  }
}

static void startLife() {
  len = 4; grow = 0; dir = 0; nQueued = 0; moveT = 0;
  for (int i = 0; i < len; i++) body[i] = 10 * GCOLS + (6 - i);
  memcpy(prevBody, body, sizeof(body[0]) * len);
  prevLen = len;
  starC = -1;
  placeFood();
  state = ST_PLAY; stateT = 0;
}
static void startLevel() {
  buildWalls(level);
  eaten = 0;
  startLife();
  music(&SONG_GAME);
}
static void resetGame() {
  score = 0; lives = 3; level = 1; newHi = false;
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  startLevel();
}
static int moveInterval() { return frames(std::max(4, 9 - (level - 1) / 2 - len / 24)); }

static void die() {
  sfx(SFX_DIE);
  rumble(0.7f, 400);
  shake = 18;
  for (int i = 0; i < len; i++)
    burst(cx(cellC(body[i])), cy(cellR(body[i])), 3, bodyColor((float)i / len), 1.8f, true);
  lives--;
  state = lives > 0 ? ST_DEAD : ST_OVER;
  stateT = 0;
  if (state == ST_OVER) { music(&SONG_OVER); if (newHi) saveHi(hiscore); }
}

static void moveSnake() {
  if (nQueued) {
    int d = queued[0];
    queued[0] = queued[1]; nQueued--;
    if ((d + 2) % 4 != dir) dir = d;
  }
  int c = cellC(body[0]) + DX[dir], r = cellR(body[0]) + DY[dir];
  if (c < 0 || c >= GCOLS || r < 0 || r >= GROWS || occupied(c, r, true)) { die(); return; }
  memcpy(prevBody, body, sizeof(body[0]) * len);
  prevLen = len;
  if (grow) { grow--; if (len < MAXLEN) len++; }
  memmove(body + 1, body, (len - 1) * sizeof(body[0]));
  body[0] = r * GCOLS + c;
  if (len > prevLen) prevBody[len - 1] = body[len - 1];   // the new tail piece appears where it is
  moveNo++;
  float px = cx(c), py = cy(r);
  if (c == foodC && r == foodR) {
    grow += 3;
    eaten++;
    uint32_t v = 10 * level;
    addScore(v);
    popup(px, py - 50, v);
    burst(px, py, 16, Color(255, 220, 90), 1.6f, true);
    sfx(SFX_COIN, 0, 0.8f + 0.04f * eaten);
    rumble(0.25f, 60);
    eatPulse = 1;
    if (eaten >= FOOD_PER_LEVEL) {
      addScore(500 * level);
      state = ST_CLEAR; stateT = 0;
      music(&SONG_CLEAR);
      return;
    }
    placeFood();
    if (eaten % 4 == 0 && starC < 0) placeStar();
  }
  if (c == starC && r == starR) {
    uint32_t v = 50 * level + starT / 4;
    addScore(v);
    popup(px, py - 50, v);
    burst(px, py, 30, Color(255, 120, 220), 2.2f, true);
    sfx(SFX_POWERUP);
    starC = -1;
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  static const uint32_t DB[4] = {BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP};
  for (int d = 0; d < 4; d++)
    if (in.hit(DB[d]) && nQueued < 2) {
      int last = nQueued ? queued[nQueued - 1] : dir;
      if (d != last && (d + 2) % 4 != last) queued[nQueued++] = d;
    }
  if (starC >= 0 && --starT <= 0) starC = -1;
  if (++moveT >= moveInterval()) { moveT = 0; moveSnake(); }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.94f; p.vy *= 0.94f; if (--p.life <= 0) p.on = false; }
  for (auto& p : popups) if (p.on) { p.y -= 1.2f; if (--p.t <= 0) p.on = false; }
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  eatPulse *= 0.9f;
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      demoA += 0.02f;
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_PLAY:
      if (stateT > 50) updatePlay(in);
      else if (in.hit(BTN_START)) nova::pause();
      break;
    case ST_DEAD: if (stateT > 100) startLife(); break;
    case ST_CLEAR: if (stateT > 150) { level++; startLevel(); } break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static void drawBackground() {
  rectGrad(0, 0, W, H, Color(6, 10, 30), Color(26, 8, 44));
  glow(W * 0.3f, H * 0.4f, 700, Color(40, 120, 200, 40));
  glow(W * 0.75f, H * 0.6f, 700, Color(160, 40, 200, 40));
}

static void drawBoard(float ox, float oy) {
  float w = GCOLS * CELL, h = GROWS * CELL;
  roundRect(ox + GX0 - 16, oy + GY0 - 16, w + 32, h + 32, 18, Color(70, 220, 240, 200));
  roundRect(ox + GX0 - 10, oy + GY0 - 10, w + 20, h + 20, 12, Color(8, 10, 30));
  for (int r = 0; r < GROWS; r++)
    for (int c = 0; c < GCOLS; c++) disc(ox + cx(c), oy + cy(r), 2.5f, Color(80, 90, 170, 90));
  // walls: neon purple blocks, merged into runs
  for (int r = 0; r < GROWS; r++)
    for (int c = 0; c < GCOLS; c++) {
      if (!wall[r][c]) continue;
      float x = ox + GX0 + c * CELL, y = oy + GY0 + r * CELL;
      glow(x + CELL / 2, y + CELL / 2, CELL * 1.1f, Color(170, 80, 255, 50));
      roundRect(x + 2, y + 2, CELL - 4, CELL - 4, 10, Color(130, 60, 210));
      roundRect(x + 7, y + 6, CELL - 14, 10, 5, Color(220, 170, 255, 170));
    }
}

static void drawFood(float ox, float oy) {
  float x = ox + cx(foodC), y = oy + cy(foodR);
  float p = 0.5f + 0.5f * sinf(frameNo * 0.15f);
  glow(x, y, 70 + 10 * p, Color(255, 200, 70, 110));
  Fx f; f.sx = f.sy = (17 + 2 * p) / 32.0f; f.tint = Color(255, 210, 70);
  draw(cart::IMG_ORB, x, y, f);
  if (starC >= 0 && (starT > 90 || (starT & 4))) {
    float sx = ox + cx(starC), sy = oy + cy(starR);
    glow(sx, sy, 90, Color(255, 120, 220, 120));
    Fx s; s.rot = frameNo * 4.0f; s.sx = s.sy = 0.8f + 0.1f * sinf(frameNo * 0.3f); s.tint = Color(255, 150, 230); s.blend = BLEND_ADD;
    draw(cart::IMG_SPARK, sx, sy, s);
    s.rot += 45; s.sx = s.sy *= 0.6f; s.tint = WHITE;
    draw(cart::IMG_SPARK, sx, sy, s);
  }
}

// the serpent: segments slide from their previous cell to the new one as the move timer runs
static void drawSnake(float ox, float oy, float alpha) {
  float f = state == ST_PLAY && stateT > 50 ? clampv((moveT + 1.0f) / moveInterval(), 0.0f, 1.0f) : 1;
  static float px[MAXLEN], py[MAXLEN];
  for (int i = 0; i < len; i++) {
    uint16_t a = i < prevLen ? prevBody[i] : body[i], b = body[i];
    px[i] = ox + cx(lerp((float)cellC(a), (float)cellC(b), f));
    py[i] = oy + cy(lerp((float)cellR(a), (float)cellR(b), f));
  }
  // glow pass, then the tube, drawn tail first with dots along each link
  for (int pass = 0; pass < 2; pass++)
    for (int i = len - 1; i >= 0; i--) {
      float t = len > 1 ? (float)i / (len - 1) : 0;
      Color col = bodyColor(t);
      float r = 19 - 5 * t + (i == 0 ? 3 : 0) + 4 * eatPulse * (1 - t);
      if (pass == 0) { glow(px[i], py[i], r * 3.2f, col.alpha(0.18f * alpha)); continue; }
      if (i + 1 < len) {
        float dx = px[i + 1] - px[i], dy = py[i + 1] - py[i];
        float d = sqrtf(dx * dx + dy * dy);
        int n = (int)(d / 8);
        for (int k = 1; k <= n && d < CELL * 1.5f; k++) {
          float u = (float)k / (n + 1);
          disc(px[i] + dx * u, py[i] + dy * u, r, col.alpha(alpha));
        }
      }
      disc(px[i], py[i], r, col.alpha(alpha));
      disc(px[i] - r * 0.3f, py[i] - r * 0.35f, r * 0.45f, Color(255, 255, 255, (uint8_t)(70 * alpha)));
    }
  // eyes on the head, looking where it's going
  float ex = DX[dir], ey = DY[dir];
  for (int s = -1; s <= 1; s += 2) {
    float x = px[0] + ex * 6 - ey * 9 * s, y = py[0] + ey * 6 + ex * 9 * s;
    disc(x, y, 7, Color(255, 255, 255, (uint8_t)(255 * alpha)));
    disc(x + ex * 2.5f, y + ey * 2.5f, 3.5f, Color(14, 10, 30, (uint8_t)(255 * alpha)));
  }
}

static void drawHud() {
  char buf[32];
  float lx = GX0 / 2;
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 110);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 250, Color(255, 220, 110));
  snprintf(buf, sizeof(buf), "%d", level);
  ui::stat("LEVEL", buf, W - lx, 110, Color(150, 230, 255));
  TextStyle l = ui::style(26, Color(150, 170, 220)); l.outline = CLEAR;
  text("ORBS", W - lx, 250, l);
  for (int i = 0; i < FOOD_PER_LEVEL; i++) {
    float x = W - lx - 72 + (i % 5) * 36, y = 304 + (i / 5) * 36;
    disc(x, y, 12, i < eaten ? Color(255, 210, 70) : Color(60, 70, 110));
  }
  text("LIVES", W - lx, 420, l);
  for (int i = 0; i < std::min(lives - 1, 6); i++) {
    float x = W - lx - 50 + (i % 3) * 50, y = 476 + (i / 3) * 44;
    disc(x, y, 16, Color(70, 220, 240));
    disc(x + 5, y - 4, 4, WHITE);
  }
}

static void draw() {
  drawBackground();
  if (state == ST_TITLE) {
    for (int i = 0; i < 40; i++) {   // a wandering serpent behind the logo
      float a = demoA - i * 0.06f;
      float x = W / 2 + 700 * sinf(a * 1.3f), y = 560 + 300 * sinf(a * 2.1f);
      Color c = bodyColor(i / 39.0f);
      glow(x, y, 60, c.alpha(0.12f));
      disc(x, y, 26 - i * 0.3f, c.alpha(0.8f));
    }
    static const char* HELP[] = {"STEER WITH THE ARROWS / D-PAD    EAT 10 ORBS TO CLEAR A LEVEL"};
    ui::titleScreen(cart::IMG_LOGO_NEONSERPENT, "EAT, GROW, DON'T BITE YOURSELF", hiscore, HELP, 1, Color(170, 255, 200));
    return;
  }
  float ox = shakeX, oy = shakeY;
  drawBoard(ox, oy);
  if (state != ST_CLEAR) drawFood(ox, oy);
  if (state == ST_PLAY || state == ST_CLEAR) drawSnake(ox, oy, 1);
  else if (state == ST_DEAD && stateT < 30 && (stateT & 4)) drawSnake(ox, oy, 0.6f);
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max;
    Color c = mix(p.col, WHITE, t * 0.5f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = p.size / 14; f.rot = p.life * 8; draw(cart::IMG_SPARK, p.x + ox, p.y + oy, f); }
    else disc(p.x + ox, p.y + oy, p.size * t, c);
  }
  for (auto& p : popups)
    if (p.on) { TextStyle st = ui::style(40, Color(255, 220, 110).alpha(clampv(p.t / 15.0f, 0.0f, 1.0f))); text(p.txt, p.x + ox, p.y + oy, st); }
  drawHud();
  if (state == ST_PLAY && stateT < 50) { char s[24]; snprintf(s, sizeof(s), "LEVEL %d", level); ui::banner(s, W / 2, 420, 50 - stateT, 50, WHITE, 100); }
  if (state == ST_CLEAR) {
    ui::banner("LEVEL CLEAR!", W / 2, 420, 150 - stateT + 30, 150, Color(180, 255, 120), 110);
    if (stateT > 20) textf(W / 2, 560, ui::style(52), "BONUS %d", 500 * level);
  }
  if (state == ST_OVER) ui::gameOver(stateT, newHi);
}

// ------------------------------------------------------------ bot: breadth-first search to the orb, else the roomiest way
static int floodFrom(int c, int r) {
  static uint8_t seen[GROWS][GCOLS];
  memset(seen, 0, sizeof(seen));
  static int q[MAXLEN];
  int qh = 0, qt = 0, n = 0;
  if (occupied(c, r, true)) return 0;
  q[qt++] = r * GCOLS + c; seen[r][c] = 1;
  while (qh < qt) {
    int v = q[qh++], vc = v % GCOLS, vr = v / GCOLS;
    n++;
    for (int d = 0; d < 4; d++) {
      int nc = vc + DX[d], nr = vr + DY[d];
      if (nc < 0 || nc >= GCOLS || nr < 0 || nr >= GROWS || seen[nr][nc] || occupied(nc, nr, true)) continue;
      seen[nr][nc] = 1; q[qt++] = nr * GCOLS + nc;
    }
  }
  return n;
}

static int botDir() {
  int hc = cellC(body[0]), hr = cellR(body[0]);
  // BFS from the head to the orb (or the star if closer)
  static int16_t from[GROWS][GCOLS];
  for (auto& row : from) for (auto& v : row) v = -1;
  static int q[MAXLEN];
  int qh = 0, qt = 0;
  q[qt++] = hr * GCOLS + hc; from[hr][hc] = 4;
  int goal = -1;
  while (qh < qt && goal < 0) {
    int v = q[qh++], vc = v % GCOLS, vr = v / GCOLS;
    for (int d = 0; d < 4; d++) {
      int nc = vc + DX[d], nr = vr + DY[d];
      if (nc < 0 || nc >= GCOLS || nr < 0 || nr >= GROWS || from[nr][nc] >= 0 || occupied(nc, nr, true)) continue;
      from[nr][nc] = d; q[qt++] = nr * GCOLS + nc;
      if ((nc == foodC && nr == foodR) || (nc == starC && nr == starR)) { goal = nr * GCOLS + nc; break; }
    }
  }
  int best = dir;
  if (goal >= 0) {
    int c = goal % GCOLS, r = goal / GCOLS, d = from[r][c];
    while (true) {
      int pc = c - DX[d], pr = r - DY[d];
      if (pc == hc && pr == hr) break;
      c = pc; r = pr; d = from[r][c];
    }
    best = d;
    int nc = hc + DX[best], nr = hr + DY[best];
    if (floodFrom(nc, nr) > len) return best;   // only follow the path if it doesn't trap us
  }
  int room = -1;
  for (int d = 0; d < 4; d++) {
    if ((d + 2) % 4 == dir) continue;
    int nc = hc + DX[d], nr = hr + DY[d];
    if (nc < 0 || nc >= GCOLS || nr < 0 || nr >= GROWS) continue;
    int n = floodFrom(nc, nr);
    if (n > room) { room = n; best = d; }
  }
  return best;
}

static int botMove = -1;
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY || stateT <= 50 || botMove == moveNo || nQueued) return;
  if (moveT < moveInterval() - 2) return;   // decide just before the next step
  botMove = moveNo;
  static const uint32_t DB[4] = {BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP};
  int d = botDir();
  if (d != dir) p.held = DB[d];
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d level %d score %lu lives %d length %d", state, level, (unsigned long)score, lives, len);
}

static void enter() {
  state = ST_TITLE; stateT = 0;
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace ns

extern const nova::Game NEON_SERPENT;
const nova::Game NEON_SERPENT = {
  "neonserpent", "NEON SERPENT", "EAT, GROW, DON'T BITE YOURSELF",
  nullptr, ns::enter, ns::step, ns::draw, ns::canPause, nullptr, ns::bot, ns::debugInfo,
};
