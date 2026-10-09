// =====================================================================
//  ALIEN TIDE HD  -  hold back the descending waves (Nova Arcade HD)
//  A marching formation that speeds up as it thins out, destructible
//  shields, a bonus saucer, and waves that start lower each time.
//
//  The original's rules and timing on its 320 x 240 field (drawn 4.5x,
//  centred). New for HD: animated vector aliens (tools/make_art.py),
//  shields that crumble block by block with glowing edges, a moonlit
//  horizon, bolts with light trails, and shock rings.
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

namespace at {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  96, 4, "Em C D B",
  {I_BELL, 0.75f, 0.1f, "E5 - - - G5 - - - B5 - A5 - G5 - - - | G5 - - - E5 - - - C5 - D5 - E5 - - - | F#5 - - - D5 - - - A5 - - - F#5 - - - | D#5 - - - F#5 - - - B5 - - - . . . ."},
  {}, I_MARIMBA, "1.3.5.3.1.3.5.3.", 0.25f, I_SUBBASS, "R.......R.......", 0.5f, I_CHOIR, 0.35f,
  "..h...h...h...h.|..h...h...h...h.|..h...h...h...h.|K.......K.......", 0.6f, 0, true};
static const Song SONG_PLAY = {   // the original plays only a march here; HD adds a low pad
  80, 4, "Em C Em B", {}, {}, I_NONE, nullptr, 0, I_SUBBASS, "R.......R.......", 0.45f, I_CHOIR, 0.22f,
  "................", 0.5f, 0, true};
static const Song SONG_OVER = {
  90, 2, "Em Em", {I_FLUTE, 0.8f, 0, "B4 - G4 - E4 - D#4 - E4 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};
static const Song SONG_CLEAR = {
  130, 2, "Em Em", {I_MARIMBA, 0.8f, 0, "E5 G5 B5 E6 - - D6 - E6 - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_STRINGS, 0.4f,
  "T...t.t.T.......|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ the field (original units)
static const float K = 4.5f, OX = (W - 320 * K) / 2;
static inline float X(float x) { return OX + x * K; }
static inline float Y(float y) { return y * K; }
static const int SW_ = 320;
static const int AROWS = 5, ACOLS = 10, CW = 22, CH_ = 18;
static const int CANNON_Y = 206, GROUND_Y = 220, SHIELD_Y = 176;
static const int NSH = 4, SHW = 26, SHH = 14;

static bool alive[AROWS][ACOLS];
static int nAlive = 0;
static float fx = 50, fy = 36;
static int fdir = 1, stepT = 0, marchNote = 0, animF = 0;
static uint8_t shield[NSH][SHH][SHW];
struct Shot { bool on; float x, y, vy; uint8_t kind; };
static Shot pshot;
static Shot eshots[8];
static float px = 150;
static int lives = 3, wave = 1;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 5000;
static bool extraGiven = false, newHi = false;
enum State { ST_TITLE, ST_PLAY, ST_DEAD, ST_CLEAR, ST_OVER };
static State state = ST_TITLE;
static int stateT = 0;
static float ufoX = 0;
static int ufoDir = 0, ufoT = 1200, ufoScore = 0;

// effects (original units)
struct Part { bool on; float x, y, vx, vy, life, max; Color col; bool spark; };
static Part parts[400];
struct Ring { bool on; float x, y, t, size; Color c; };
static Ring rings[12];
struct Popup { bool on; float x, y; int t; char txt[8]; };
static Popup popups[4];
static float shake = 0, shakeX = 0, shakeY = 0, flash = 0;
static ui::Stars stars;

static const nova::Img& alienImg(int row, int f) {
  if (row == 0) return f ? atart::IMG_ORB1 : atart::IMG_ORB0;
  if (row <= 2) return f ? atart::IMG_MANTIS1 : atart::IMG_MANTIS0;
  return f ? atart::IMG_JELLY1 : atart::IMG_JELLY0;
}
static int alienPts(int row) { return row == 0 ? 30 : row <= 2 ? 20 : 10; }
static Color alienCol(int row) { return row == 0 ? Color(255, 120, 210) : row <= 2 ? Color(80, 210, 240) : Color(100, 220, 110); }

static void addScore(uint32_t v) {
  score += v;
  if (!extraGiven && score >= 10000) { extraGiven = true; lives++; sfx(SFX_ONEUP); }
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void burst(float x, float y, int n, Color col, float sp, bool spark = true) {
  for (int i = 0; i < n; i++)
    for (auto& p : parts)
      if (!p.on) { float a = frand() * 6.283f, s = frange(0.3f, 1) * sp; p = {true, x, y, cosf(a) * s, sinf(a) * s, frange(14, 32), 32, col, spark && i % 2 == 0}; break; }
}
static void ring(float x, float y, float size, Color c) { for (auto& r : rings) if (!r.on) { r = {true, x, y, 0, size, c}; return; } }

static void buildShields() {
  for (int s = 0; s < NSH; s++)
    for (int y = 0; y < SHH; y++)
      for (int x = 0; x < SHW; x++) {
        bool on = true;
        if (y < 4 && (x < 4 - y || x > SHW - 5 + y)) on = false;   // rounded top corners
        if (y >= 9 && x >= 8 && x < SHW - 8) on = false;            // arch
        if (y >= 7 && y < 9 && x >= 10 && x < SHW - 10) on = false;
        shield[s][y][x] = on;
      }
}
static int shieldX(int s) { return 34 + s * 76; }
static bool shieldHit(int x, int y, int radius) {
  for (int s = 0; s < NSH; s++) {
    int lx = x - shieldX(s), ly = y - SHIELD_Y;
    if (lx < 0 || lx >= SHW || ly < 0 || ly >= SHH || !shield[s][ly][lx]) continue;
    for (int dy = -radius; dy <= radius; dy++)
      for (int dx = -radius; dx <= radius; dx++) {
        int xx = lx + dx, yy = ly + dy;
        if (xx >= 0 && xx < SHW && yy >= 0 && yy < SHH && dx * dx + dy * dy <= radius * radius + (int)(rnd() % 3)) shield[s][yy][xx] = 0;
      }
    burst((float)x, (float)y, 5, Color(140, 255, 140), 1.0f, false);
    return true;
  }
  return false;
}

static void startWave() {
  for (auto& r : alive) for (auto& a : r) a = true;
  nAlive = AROWS * ACOLS;
  fx = 50; fy = 36 + std::min(wave - 1, 5) * 8; fdir = 1; stepT = 0;
  for (auto& s : eshots) s.on = false;
  pshot.on = false;
  ufoDir = 0; ufoT = 900 + rnd() % 900;
  buildShields();
  state = ST_PLAY; stateT = 0;
  music(&SONG_PLAY);
}
static void resetGame() {
  score = 0; lives = 3; wave = 1; extraGiven = false; newHi = false; px = 150;
  for (auto& p : parts) p.on = false;
  for (auto& p : popups) p.on = false;
  startWave();
}
static void gameOver() {
  state = ST_OVER; stateT = 0;
  music(&SONG_OVER);
  if (newHi) saveHi(hiscore);
}
static void killPlayer() {
  burst(px + 9, CANNON_Y + 5, 50, Color(255, 200, 120), 3.0f);
  ring(px + 9, CANNON_Y + 5, 1.3f, Color(255, 200, 120));
  shake = 20; flash = 0.3f;
  sfx(SFX_DIE);
  sfx(SFX_EXPLODE, 0, 0.6f);
  rumble(1.0f, 500);
  lives--;
  for (auto& s : eshots) s.on = false;
  if (lives <= 0) gameOver(); else { state = ST_DEAD; stateT = 0; }
}

static void formationBounds(int& minC, int& maxC, int& maxR) {
  minC = ACOLS; maxC = -1; maxR = -1;
  for (int r = 0; r < AROWS; r++)
    for (int c = 0; c < ACOLS; c++)
      if (alive[r][c]) { minC = std::min(minC, c); maxC = std::max(maxC, c); maxR = std::max(maxR, r); }
}

static void updateFormation() {
  int interval = frames(std::max(1, 2 + nAlive * 30 / 50 - std::min(wave - 1, 4)));
  if (++stepT < interval) return;
  stepT = 0;
  int minC, maxC, maxR;
  formationBounds(minC, maxC, maxR);
  if (maxC < 0) return;
  float left = fx + minC * CW, right = fx + maxC * CW + 16;
  if ((fdir > 0 && right + 3 > SW_ - 6) || (fdir < 0 && left - 3 < 6)) { fy += 8; fdir = -fdir; }
  else fx += fdir * 3;
  animF ^= 1;
  static const float MARCH[4] = {1.0f, 0.89f, 0.79f, 0.75f};   // the four-note descending march
  sfx(SFX_MARCH, 0, MARCH[marchNote]);
  marchNote = (marchNote + 1) & 3;
  float bottom = fy + maxR * CH_ + 12;   // aliens chew through shields they touch
  if (bottom >= SHIELD_Y)
    for (int r = 0; r < AROWS; r++)
      for (int c = 0; c < ACOLS; c++)
        if (alive[r][c]) {
          int ax = (int)fx + c * CW, ay = (int)fy + r * CH_;
          for (int s = 0; s < NSH; s++)
            for (int y = 0; y < SHH; y++)
              for (int x = 0; x < SHW; x++)
                if (shield[s][y][x] && shieldX(s) + x >= ax && shieldX(s) + x < ax + 16 && SHIELD_Y + y >= ay && SHIELD_Y + y < ay + 12)
                  shield[s][y][x] = 0;
        }
  if (bottom >= CANNON_Y) { killPlayer(); if (state != ST_OVER) gameOver(); }
}

static void alienFire() {
  int active = 0;
  for (auto& s : eshots) if (s.on) active++;
  int maxShots = std::min(2 + wave, 6);
  if (active >= maxShots || rnd() % frames(std::max(8, 40 - wave * 4)) != 0) return;
  int col = rnd() % 2 ? clampv((int)((px + 9 - fx) / CW), 0, ACOLS - 1) : (int)(rnd() % ACOLS);
  for (int k = 0; k < ACOLS; k++) {
    int c = (col + k) % ACOLS;
    for (int r = AROWS - 1; r >= 0; r--)
      if (alive[r][c]) {
        for (auto& s : eshots)
          if (!s.on) {
            uint8_t kind = rnd() % 3 == 0 ? 1 : 0;
            s = {true, fx + c * CW + 7, fy + r * CH_ + 12, (kind ? 2.8f + wave * 0.1f : 1.6f + wave * 0.08f) * speed(), kind};
            return;
          }
        return;
      }
  }
}

static void updatePlay(const Pad& in) {
  if (in.hit(BTN_START)) { nova::pause(); return; }
  px = clampv(px + in.ax * 2.3f, 6.0f, (float)SW_ - 25);
  if (in.hit(BTN_A | BTN_R | BTN_X | BTN_B) && !pshot.on) {
    pshot = {true, px + 8, (float)CANNON_Y - 4, -6.0f, 0};
    sfx(SFX_SHOOT, (px - 160) / 160 * 0.5f);
  }
  updateFormation();
  if (state != ST_PLAY) return;
  alienFire();
  if (pshot.on) {
    pshot.y += pshot.vy;
    if (pshot.y < 14) { pshot.on = false; burst(pshot.x, 16, 4, Color(180, 240, 255), 1); }
    else if (shieldHit((int)pshot.x, (int)pshot.y, 2)) pshot.on = false;
    else {
      if (ufoDir && pshot.y < 32 && pshot.x > ufoX && pshot.x < ufoX + 28) {
        static const int US[4] = {50, 100, 150, 300};
        ufoScore = US[rnd() % 4];
        addScore(ufoScore);
        for (auto& p : popups) if (!p.on) { p = {true, ufoX + 4, 20, 90, {0}}; snprintf(p.txt, 8, "%d", ufoScore); break; }
        burst(ufoX + 14, 26, 40, Color(255, 150, 60), 2.5f);
        ring(ufoX + 14, 26, 1.2f, Color(255, 150, 60));
        sfx(SFX_EXPLODE, 0, 0.7f);
        ufoDir = 0; ufoT = 1500 + rnd() % 900;
        pshot.on = false;
      }
      for (int r = 0; r < AROWS && pshot.on; r++)
        for (int c = 0; c < ACOLS; c++) {
          if (!alive[r][c]) continue;
          float ax = fx + c * CW, ay = fy + r * CH_;
          if (pshot.x >= ax && pshot.x < ax + 16 && pshot.y >= ay && pshot.y < ay + 12) {
            alive[r][c] = false; nAlive--;
            addScore(alienPts(r));
            burst(ax + 8, ay + 6, 14, alienCol(r), 1.6f);
            ring(ax + 8, ay + 6, 0.5f, alienCol(r));
            sfx(SFX_EXPLODE, clampv((ax - 160) / 160, -1.0f, 1.0f) * 0.6f, 1.6f + 0.1f * (4 - r));
            pshot.on = false;
            break;
          }
        }
    }
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.y += s.vy;
    if (s.y > GROUND_Y - 6) { s.on = false; burst(s.x, GROUND_Y - 2, 6, Color(255, 220, 90), 1, false); continue; }
    if (shieldHit((int)s.x + 1, (int)s.y + 7, 3)) { s.on = false; continue; }
    if (pshot.on && fabsf(pshot.x - s.x - 1) < 3 && fabsf(pshot.y - s.y - 4) < 6) {   // shots collide
      pshot.on = false; s.on = false; burst(s.x, s.y + 4, 8, WHITE, 1.2f); continue;
    }
    if (s.x + 3 > px + 1 && s.x < px + 18 && s.y + 7 > CANNON_Y + 2 && s.y < CANNON_Y + 10) { s.on = false; killPlayer(); return; }
  }
  if (ufoDir) {
    ufoX += ufoDir * 1.2f * speed();
    if ((frameNo % 20) == 0) sfx(SFX_MOVE, ufoX / 160 - 1, 0.6f);
    if (ufoX < -30 || ufoX > SW_ + 2) { ufoDir = 0; ufoT = 1500 + rnd() % 900; }
  } else if (--ufoT <= 0 && nAlive > 8) {
    ufoDir = rnd() % 2 ? 1 : -1; ufoX = ufoDir > 0 ? -28 : SW_;
  }
  if (nAlive == 0) {
    state = ST_CLEAR; stateT = 0;
    addScore(500 * wave);
    music(&SONG_CLEAR);
  }
}

static void updateFx() {
  for (auto& p : parts) { if (!p.on) continue; p.x += p.vx; p.y += p.vy; p.vx *= 0.96f; p.vy *= 0.96f; if (--p.life <= 0) p.on = false; }
  for (auto& r : rings) if (r.on && (r.t += 0.04f) >= 1) r.on = false;
  for (auto& p : popups) if (p.on && --p.t <= 0) p.on = false;
  stars.step(0.05f, 0);
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.86f; }
  else { shake = 0; shakeX = shakeY = 0; }
  flash *= 0.9f;
}

static void step(const Pad& in) {
  updateFx();
  stateT++;
  switch (state) {
    case ST_TITLE:
      if (stateT % 30 == 0) animF ^= 1;
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); sfx(SFX_START); }
      break;
    case ST_PLAY: updatePlay(in); break;
    case ST_DEAD: if (stateT > 100) { state = ST_PLAY; px = 150; } break;
    case ST_CLEAR: if (stateT > 150) { wave++; startWave(); } break;
    case ST_OVER:
      if (stateT > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ drawing
static void drawBackground() {
  rectGrad(0, 0, W, Y(GROUND_Y), Color(4, 4, 18), Color(30, 12, 56));
  stars.draw(Color(200, 210, 255));
  // a big pale moon low on the horizon
  float mx = W * 0.78f, my = Y(GROUND_Y) - 120;
  glow(mx, my, 420, Color(140, 120, 255, 50));
  disc(mx, my, 150, Color(200, 190, 240));
  disc(mx + 40, my - 30, 34, Color(170, 160, 220));
  disc(mx - 60, my + 40, 22, Color(170, 160, 220));
  disc(mx + 70, my + 70, 18, Color(175, 165, 225));
  // hills and the ground
  const int N = 48;
  Vtx v[N * 6];
  int k = 0;
  for (int i = 0; i < N; i++) {
    float x0 = i * W / (float)N, x1 = (i + 1) * W / (float)N;
    auto h = [](float x) { return Y(GROUND_Y) - 40 - 26 * sinf(x * 0.004f + 1) - 14 * sinf(x * 0.013f); };
    Color top(40, 70, 70), bot(20, 30, 40);
    v[k++] = {x0, h(x0), top}; v[k++] = {x1, h(x1), top}; v[k++] = {x1, Y(GROUND_Y), bot};
    v[k++] = {x0, h(x0), top}; v[k++] = {x1, Y(GROUND_Y), bot}; v[k++] = {x0, Y(GROUND_Y), bot};
  }
  tris(v, k);
  rectGrad(0, Y(GROUND_Y), W, H - Y(GROUND_Y), Color(30, 60, 50), Color(12, 22, 26));
  rect(0, Y(GROUND_Y) - 3, W, 6, Color(100, 230, 120));
  glow(W / 2, Y(GROUND_Y), 900, Color(80, 220, 120, 30));
}

static void drawShields(float ox, float oy) {
  const float cs = K;   // one shield cell is one original pixel
  for (int s = 0; s < NSH; s++) {
    float x0 = X(shieldX(s)) + ox, y0 = Y(SHIELD_Y) + oy;
    glow(x0 + SHW * cs / 2, y0 + SHH * cs / 2, 120, Color(90, 255, 120, 30));
    for (int y = 0; y < SHH; y++)
      for (int x = 0; x < SHW; x++) {
        if (!shield[s][y][x]) continue;
        bool edge = y == 0 || !shield[s][y - 1][x];
        rect(x0 + x * cs, y0 + y * cs, cs + 0.5f, cs + 0.5f, edge ? Color(200, 255, 170) : mix(Color(90, 220, 110), Color(40, 140, 80), y / (float)SHH));
      }
  }
}

static void drawHud() {
  float lx = OX / 2;
  char buf[32];
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)score);
  ui::stat("SCORE", buf, lx, 110);
  snprintf(buf, sizeof(buf), "%07lu", (unsigned long)hiscore);
  ui::stat("HI-SCORE", buf, lx, 250, Color(255, 220, 110));
  snprintf(buf, sizeof(buf), "%d", wave);
  ui::stat("WAVE", buf, W - lx, 110, Color(150, 230, 255));
  TextStyle l = ui::style(26, Color(150, 170, 220)); l.outline = CLEAR;
  text("LIVES", W - lx, 250, l);
  for (int i = 0; i < std::min(lives - 1, 6); i++) {
    Fx f; f.sx = f.sy = 0.6f;
    draw(atart::IMG_CANNON, W - lx - 80 + (i % 3) * 56, 300 + (i / 3) * 44, f);
  }
}

static void draw() {
  drawBackground();
  float ox = shakeX, oy = shakeY;
  if (state == ST_TITLE) {
    static const char* HELP[] = {"LEFT / RIGHT MOVE    A FIRE"};
    ui::titleScreen(cart::IMG_LOGO_ALIENTIDE, "HOLD BACK THE DESCENDING WAVES", hiscore, HELP, 1, Color(190, 255, 170));
    const nova::Img* tbl[3] = {&alienImg(0, animF), &alienImg(1, animF), &alienImg(3, animF)};
    const char* pts[3] = {"30", "20", "10"};
    for (int i = 0; i < 3; i++) {
      float x = W / 2 - 330 + i * 220;
      draw(*tbl[i], x, 900);
      text(pts[i], x + 60, 880, ui::style(40, WHITE, LEFT));
    }
    draw(animF ? atart::IMG_SAUCER1 : atart::IMG_SAUCER0, W / 2 + 330, 880);
    text("???", W / 2 + 480, 880, ui::style(40, Color(255, 160, 80), LEFT));
    return;
  }
  drawShields(ox, oy);
  for (int r = 0; r < AROWS; r++)
    for (int c = 0; c < ACOLS; c++)
      if (alive[r][c]) {
        float ax = X(fx + c * CW + 8) + ox, ay = Y(fy + r * CH_ + 6) + oy;
        glow(ax, ay, 60, alienCol(r).alpha(0.12f));
        draw(alienImg(r, animF), ax, ay);
      }
  if (ufoDir) {
    glow(X(ufoX + 14), Y(26), 140, Color(255, 150, 60, 70));
    draw((frameNo >> 3) & 1 ? atart::IMG_SAUCER1 : atart::IMG_SAUCER0, X(ufoX), Y(18));
  }
  if (state == ST_PLAY || state == ST_CLEAR || (state == ST_DEAD && stateT > 60 && (stateT & 4))) {
    glow(X(px + 9) + ox, Y(CANNON_Y + 6) + oy, 90, Color(90, 255, 120, 50));
    draw(atart::IMG_CANNON, X(px) + ox, Y(CANNON_Y) + oy);
  }
  if (pshot.on) {
    float x = X(pshot.x + 1) + ox, y = Y(pshot.y + 3) + oy;
    Fx f; f.rot = 90; f.blend = BLEND_ADD; f.tint = Color(170, 240, 255); f.sx = 1.3f; f.sy = 1.5f;
    draw(cart::IMG_STREAK, x, y, f);
    glow(x, y, 30, Color(150, 230, 255, 160));
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    float x = X(s.x + 1.5f) + ox, y = Y(s.y + 3.5f) + oy;
    if (s.kind) {   // a fast straight bolt
      line(x, y - 22, x, y + 18, 8, Color(255, 220, 90));
      glow(x, y, 34, Color(255, 200, 70, 150));
    } else {        // a slow zigzag
      float w = ((frameNo >> 2) & 1) ? 8 : -8;
      line(x - w, y - 18, x + w, y - 6, 6, Color(255, 120, 200)); line(x + w, y - 6, x - w, y + 6, 6, Color(255, 120, 200));
      line(x - w, y + 6, x + w, y + 18, 6, Color(255, 120, 200));
      glow(x, y, 30, Color(255, 100, 200, 120));
    }
  }
  for (auto& p : parts) {
    if (!p.on) continue;
    float t = p.life / p.max, x = X(p.x) + ox, y = Y(p.y) + oy;
    Color c = mix(p.col, WHITE, t * 0.4f).alpha(fminf(1, t * 1.6f));
    if (p.spark) { Fx f; f.tint = c; f.blend = BLEND_ADD; f.sx = f.sy = 0.4f; f.rot = p.life * 10; draw(cart::IMG_SPARK, x, y, f); }
    else disc(x, y, 6 * t + 2, c);
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    Fx f; f.sx = f.sy = r.size * (0.3f + r.t * 2.2f); f.tint = r.c.alpha(1 - r.t); f.blend = BLEND_ADD;
    draw(cart::IMG_RING, X(r.x) + ox, Y(r.y) + oy, f);
  }
  for (auto& p : popups) if (p.on) text(p.txt, X(p.x), Y(p.y), ui::style(44, (p.t & 8) ? Color(255, 220, 110) : WHITE, LEFT));
  drawHud();
  if (flash > 0.01f) rect(0, 0, W, H, Color(255, 240, 220, (uint8_t)(flash * 255)), BLEND_ADD);
  if (state == ST_PLAY && stateT < 100) { char s[24]; snprintf(s, sizeof(s), "WAVE %d", wave); ui::banner(s, W / 2, 640, 100 - stateT, 100, WHITE, 100); }
  if (state == ST_CLEAR) {
    ui::banner("WAVE CLEARED!", W / 2, 420, 150 - stateT + 30, 150, Color(180, 255, 120), 110);
    if (stateT > 20) textf(W / 2, 560, ui::style(52), "BONUS %d", 500 * wave);
  }
  if (state == ST_OVER) ui::gameOver(stateT, newHi);
}

// ------------------------------------------------------------ bot: stand under the lowest alien, dodge bolts
static void bot(Pad& p) {
  p = Pad();
  if (state == ST_TITLE || state == ST_OVER) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (state != ST_PLAY) return;
  float target = px;
  float bestR = -1e9f;
  int interval = frames(std::max(1, 2 + nAlive * 30 / 50 - std::min(wave - 1, 4)));
  int minC, maxC, maxR;
  formationBounds(minC, maxC, maxR);
  for (int c = 0; c < ACOLS; c++)
    for (int r = AROWS - 1; r >= 0; r--)
      if (alive[r][c]) {
        // lead the target: where will this alien be when a shot gets up to it?
        float ay = fy + r * CH_ + 6, t = (CANNON_Y - ay) / 6.0f;
        float steps = (t + stepT) / interval, ax = fx + c * CW + 8;
        float left = fx + minC * CW, right = fx + maxC * CW + 16, dir = (float)fdir;
        for (int k = 0; k < (int)steps && k < 200; k++) {   // march it forward, bouncing at the edges
          if ((dir > 0 && right + 3 > SW_ - 6) || (dir < 0 && left - 3 < 6)) dir = -dir;
          else { ax += dir * 3; left += dir * 3; right += dir * 3; }
        }
        float score = r * 40 - fabsf(ax - (px + 9));
        if (score > bestR) { bestR = score; target = ax - 9; }
        break;
      }
  if (ufoDir && fabsf(ufoX + 14 - (px + 9)) < 60) target = ufoX + 14 + ufoDir * 12 - 9;
  // dodge: if a bolt is coming down on us, step to whichever side is clear
  for (auto& s : eshots)
    if (s.on && s.y > 120 && s.x + 3 > px - 4 && s.x < px + 22) target = (s.x > px + 9) ? s.x - 26 : s.x + 8;
  float d = target - px;
  p.ax = clampv(d / 2.3f, -1.0f, 1.0f);
  if (fabsf(d) > 1) p.held |= d > 0 ? BTN_RIGHT : BTN_LEFT;
  if (!pshot.on && (frameNo & 1) && fabsf(d) < 4) p.held |= BTN_A;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d wave %d score %lu lives %d aliens %d", state, wave, (unsigned long)score, lives, nAlive);
}

static void init() { loadAtlas(atart::TEX_FILES, atart::TEX, atart::NTEX); }
static void enter() {
  state = ST_TITLE; stateT = 0;
  stars.init(120);
  buildShields();
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state != ST_TITLE && state != ST_OVER; }

}  // namespace at

extern const nova::Game ALIEN_TIDE;
const nova::Game ALIEN_TIDE = {
  "alientide", "ALIEN TIDE", "HOLD BACK THE DESCENDING WAVES",
  at::init, at::enter, at::step, at::draw, at::canPause, nullptr, at::bot, at::debugInfo,
};
