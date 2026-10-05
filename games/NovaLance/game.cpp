// =====================================================================
//  NOVA LANCE HD  -  a synthwave side-scrolling shoot-'em-up (Nova Arcade HD)
//  Waves of drones, darts and turret pods, then a boss mothership every
//  stage. Power-ups: P weapon level, S shield, B bomb.
//
//  The original's rules and attack patterns, simulated in its units on a
//  field widened to 16:9 (426 x 240, drawn at 4.5x). New for HD: vector
//  craft (tools/make_art.py), a striped synthwave sun, parallax mountains
//  and a lit city, glowing shots, fire and shock rings.
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

namespace nl {

// ------------------------------------------------------------ music (the original's melodies)
static const Song SONG_TITLE = {
  104, 4, "Am F C G",
  {I_SAWLEAD, 0.6f, 0.1f, "A5 - - - - - - - G5 - - - E5 - - - | F5 - - - - - - - E5 - - - C5 - - - | G5 - - - - - - - C6 - - - B5 - - - | B5 - - - - - - - - - - - . . . ."},
  {}, I_PLUCKBASS, "1.3.5.3.8.5.3.5.", 0.25f, I_SUBBASS, "R.......R.......", 0.5f, I_PAD, 0.5f,
  "..h...h...h...h.", 0.6f, 0, true};
static const Song SONG_STAGE = {
  150, 16, "Am F C G Am F C E Am F C G Am F Dm E",
  {I_SQUARE, 0.6f, 0.05f,
   "A4 - C5 - E5 - A5 - G5 - E5 - C5 - D5 E5 | F5 - - - E5 - C5 - A4 - C5 - F5 - E5 C5 | G5 - - - E5 - G5 - C6 - B5 - G5 - E5 - | D5 - G5 - B5 - D6 - B5 - G5 - D5 - B4 - |"
   "A4 - C5 - E5 - A5 - G5 - E5 - C5 - D5 E5 | F5 - - - E5 - C5 - A4 - C5 - F5 - E5 C5 | G5 - - - E5 - G5 - C6 - B5 - G5 - E5 - | E5 - - - Ab5 - B5 - E6 - - - - - . . |"
   "A5 - - - G5 - E5 - C5 - E5 - A4 - - - | F5 - - - E5 - C5 - A4 - C5 - F5 - E5 C5 | G5 - - - E5 - G5 - C6 - B5 - G5 - E5 - | D5 - G5 - B5 - D6 - B5 - G5 - D5 - B4 - |"
   "A5 - - - G5 - E5 - C5 - E5 - A4 - - - | F5 - - - E5 - C5 - A4 - C5 - F5 - E5 C5 | D5 - G5 - B5 - D6 - B5 - G5 - D5 - B4 - | E5 - - - Ab5 - B5 - E6 - - - - - . ."},
  {}, I_NONE, nullptr, 0, I_PLUCKBASS, "RRORRRORRRORRROR", 0.55f, I_PAD, 0.3f,
  "K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.hKS.h.|K.h.S.h.K.S.SSSS",
  0.85f, 0, true};
static const Song SONG_BOSS = {
  168, 4, "Am Bb Am E",
  {I_SAWLEAD, 0.5f, 0.05f,
   "A4 A5 A4 G5 A4 E5 A4 G5 A4 A5 A4 G5 A4 E5 C5 D5 | Bb4 Bb5 Bb4 A5 Bb4 F5 Bb4 D5 Bb4 Bb5 Bb4 A5 F5 - D5 - |"
   "A4 A5 A4 G5 A4 E5 A4 G5 A4 A5 A4 G5 A4 E5 C5 D5 | Ab4 Ab5 Ab4 B5 Ab4 D6 Ab4 E6 Ab5 - B5 - D6 - E6 -"},
  {}, I_NONE, nullptr, 0, I_PLUCKBASS, "RRRRRRRRRRRRRRRR", 0.55f, I_STRINGS, 0.3f,
  "KhKhShKhKhKhShSh|KhKhShKhKhKhShSh|KhKhShKhKhKhShSh|K.h.S.h.K.S.SSSS", 0.9f, 0, true};
static const Song SONG_OVER = {
  96, 2, "Am Am", {I_FLUTE, 0.8f, 0, "E5 - C5 - A4 - E4 - - - - - - - . . | . . . . . . . . . . . . . . . ."},
  {}, I_NONE, nullptr, 0, I_SUBBASS, "R...............|R...............", 0.5f, I_PAD, 0.4f,
  "................|T...............", 0.7f, 0, false};

// ------------------------------------------------------------ the field (original units, widened to 16:9)
static const float K = 4.5f, FW = W / K, FH = 240;
static inline float X(float x) { return x * K; }
static inline float Y(float y) { return y * K; }
static const int START_LIVES = 3, START_BOMBS = 2;

enum EType : uint8_t { E_DRONE, E_DART, E_POD, E_BOSS };
static const float EW[4] = {18, 22, 22, 84}, EH[4] = {18, 13, 22, 62};   // the original sprite sizes (hit boxes)
struct Enemy { bool on; uint8_t type, flash; float x, y, vx, vy, baseY, t, param; int16_t hp, timer; };
struct Shot { bool on; float x, y, vx, vy; uint8_t kind; };
struct Particle { bool on; float x, y, vx, vy; uint8_t life, maxLife, kind, size; };
struct RingFx { bool on; float x, y, r, vr; uint8_t life, max; };
struct Pickup { bool on; float x, y, t; uint8_t type; };
struct Popup { bool on; float x, y; uint8_t life; char txt[8]; };
struct Pending { bool on; int16_t delay; uint8_t type; float y, param; };
static Enemy enemies[32];
static Shot pshots[48], eshots[120];
static Particle parts[400];
static RingFx rings[20];
static Pickup pickups[8];
static Popup popups[12];
static Pending pending[32];
struct Player { float x, y; int lives, bombs, weapon, fireCd, invuln, shield, respawn; bool alive; };
static Player pl;
enum State { ST_TITLE, ST_PLAY, ST_OVER };
static State state = ST_TITLE;
static uint32_t score = 0, hiscore = 0;
static const uint32_t HI_DEFAULT = 20000;
static int level = 1;
static uint32_t levelFrames = 0;
static int waveTimer = 120, warnTimer = 0, bannerTimer = 0, clearTimer = 0, overTimer = 0;
static bool bossActive = false;
static int bossIdx = -1;
static float shake = 0, shakeX = 0, shakeY = 0;
static bool newHi = false;
static int flashTimer = 0;

template <typename T, size_t N>
static T* alloc(T (&arr)[N]) {
  for (auto& a : arr) if (!a.on) { a = T(); a.on = true; return &a; }
  return nullptr;
}
static void addShake(float s) { shake = fmaxf(shake, s * K * 0.6f); }
static void popup(float x, float y, const char* t) {
  Popup* p = alloc(popups);
  if (!p) return;
  p->x = x; p->y = y; p->life = 40;
  snprintf(p->txt, sizeof(p->txt), "%s", t);
}
static void explode(float x, float y, int n, float spd, bool big) {
  for (int i = 0; i < n; i++) {
    Particle* p = alloc(parts);
    if (!p) break;
    float a = frand() * 6.2831853f, sp = frange(0.2f, 1.0f) * spd;
    p->x = x; p->y = y; p->vx = cosf(a) * sp; p->vy = sinf(a) * sp;
    p->maxLife = p->life = (uint8_t)(frange(16, 36) * (big ? 1.5f : 1.0f));
    p->kind = (rnd() % 5 == 0) ? 1 : 0;
    p->size = (big && rnd() % 3 == 0) ? 2 : 1;
  }
  RingFx* r = alloc(rings);
  if (r) { r->x = x; r->y = y; r->r = 2; r->vr = big ? 2.2f : 1.4f; r->life = r->max = big ? 22 : 14; }
  addShake(big ? 9 : 2.5f);
}

static void spawnEnemy(uint8_t type, float y, float param) {
  Enemy* e = alloc(enemies);
  if (!e) return;
  e->type = type; e->x = FW + 4; e->y = y; e->baseY = y; e->param = param;
  float lv = level - 1;
  switch (type) {
    case E_DRONE: e->hp = 2; e->vx = (-1.3f - lv * 0.12f) * speed(); e->timer = frames(60 + rnd() % 120); break;
    case E_DART: e->hp = 1; e->vx = (-3.0f - lv * 0.2f) * speed(); break;
    case E_POD: e->hp = 9 + level * 2; e->vx = -1.6f; e->param = frange(FW - 84, FW - 40); e->timer = 0; break;
    case E_BOSS:
      e->hp = 280 + level * 90; e->vx = -0.8f; e->x = FW + 10; e->y = 90; e->baseY = 90;
      bossActive = true; bossIdx = (int)(e - enemies);
      break;
  }
}
static void queueSpawn(int delay, uint8_t type, float y, float param = 0) {
  Pending* p = alloc(pending);
  if (!p) return;
  p->delay = delay; p->type = type; p->y = y; p->param = param;
}
static void fireEnemyShot(float x, float y, float angle, float spd) {
  Shot* s = alloc(eshots);
  if (!s) return;
  s->x = x - 3.5f; s->y = y - 3.5f; s->vx = cosf(angle) * spd; s->vy = sinf(angle) * spd;
}
static float aimAt(float x, float y) { return atan2f((pl.y + 8) - y, (pl.x + 16) - x); }
static float bulletSpeed() { return (1.9f + (level - 1) * 0.2f) * speed(); }

static void spawnWave() {
  int r = rnd() % 100;
  int extra = std::min(level - 1, 3);
  if (r < 34) {
    float y = frange(30, 170), ph = frand() * 6.28f;
    for (int i = 0; i < 5 + extra; i++) queueSpawn(i * 12, E_DRONE, y, ph);
  } else if (r < 58) {
    float y = frange(30, 185);
    for (int i = 0; i < 3 + extra; i++) queueSpawn(i * 9, E_DART, y + ((i & 1) ? 12 : -12));
  } else if (r < 80 && levelFrames > 60 * 8) {
    queueSpawn(0, E_POD, frange(30, 170));
    if (level > 1) queueSpawn(90, E_POD, frange(30, 170));
  } else {
    for (int i = 0; i < 4 + extra / 2; i++) { queueSpawn(i * 14, E_DRONE, 55, 0); queueSpawn(i * 14, E_DRONE, 160, 3.14159f); }
  }
}

static void resetGame() {
  for (auto& e : enemies) e.on = false;
  for (auto& s : pshots) s.on = false;
  for (auto& s : eshots) s.on = false;
  for (auto& p : parts) p.on = false;
  for (auto& r : rings) r.on = false;
  for (auto& p : pickups) p.on = false;
  for (auto& p : popups) p.on = false;
  for (auto& p : pending) p.on = false;
  pl = Player();
  pl.x = -32; pl.y = 110; pl.lives = START_LIVES; pl.bombs = START_BOMBS; pl.weapon = 1; pl.alive = true; pl.invuln = 120;
  score = 0; level = 1; levelFrames = 0; waveTimer = 90; newHi = false;
  bossActive = false; bossIdx = -1; warnTimer = clearTimer = overTimer = 0; bannerTimer = 150;
}

static void addScore(uint32_t v) {
  score += v;
  if (score > hiscore) { hiscore = score; newHi = true; }
}
static void dropPickup(float x, float y, int chancePct) {
  if ((int)(rnd() % 100) >= chancePct) return;
  Pickup* p = alloc(pickups);
  if (!p) return;
  int r = rnd() % 100;
  p->type = r < 58 ? 0 : r < 84 ? 1 : 2;   // P, S, B
  p->x = x; p->y = y;
}
static void killEnemy(Enemy& e) {
  float cx = e.x + EW[e.type] / 2, cy = e.y + EH[e.type] / 2;
  static const uint16_t pts[] = {100, 150, 500, 10000};
  uint32_t v = pts[e.type] * (e.type == E_BOSS ? level : 1);
  addScore(v);
  if (e.type == E_BOSS) { e.timer = -1; e.hp = 0; return; }   // the dying sequence runs in updateEnemies
  char buf[8]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)v);
  popup(cx - 6, cy - 8, buf);
  bool big = e.type == E_POD;
  explode(cx, cy, big ? 40 : 18, big ? 2.6f : 1.8f, big);
  sfx(SFX_EXPLODE, clampv(cx / FW * 2 - 1, -1.0f, 1.0f) * 0.6f, big ? 0.7f : 1.3f);
  dropPickup(cx - 6, cy - 6, e.type == E_POD ? 60 : 9);
  e.on = false;
}
static void damageEnemy(Enemy& e, int dmg) {
  if (e.hp <= 0) return;
  e.hp -= dmg;
  e.flash = 3;
  if (e.hp <= 0) killEnemy(e);
  else if ((frameNo & 3) == 0) sfx(SFX_BUMP, 0, 2.0f);
}
static void playerHit() {
  if (pl.invuln > 0 || !pl.alive) return;
  if (pl.shield > 0) {
    pl.shield = 0; pl.invuln = 60;
    explode(pl.x + 16, pl.y + 8, 14, 1.5f, false);
    sfx(SFX_HURT);
    rumble(0.4f, 150);
    return;
  }
  pl.alive = false;
  pl.respawn = 100;
  explode(pl.x + 16, pl.y + 8, 60, 3.0f, true);
  sfx(SFX_DIE);
  sfx(SFX_EXPLODE, 0, 0.55f);
  rumble(1.0f, 500);
  pl.weapon = std::max(1, pl.weapon - 1);
}
static void useBomb() {
  if (pl.bombs <= 0 || !pl.alive) return;
  pl.bombs--;
  flashTimer = 10;
  addShake(12);
  sfx(SFX_EXPLODE, 0, 0.4f); sfx(SFX_SPLASH, 0, 0.6f);
  rumble(1.0f, 600);
  for (auto& s : eshots) if (s.on) { explode(s.x + 3, s.y + 3, 2, 1.0f, false); s.on = false; }
  for (auto& e : enemies) if (e.on) damageEnemy(e, e.type == E_BOSS ? 40 : 30);
  pl.invuln = std::max(pl.invuln, 60);
}

// ------------------------------------------------------------ update
static void updatePlayer(const Pad& in) {
  if (!pl.alive) {
    if (--pl.respawn <= 0) {
      if (pl.lives <= 0) { state = ST_OVER; overTimer = 0; music(&SONG_OVER); if (newHi) saveHi(hiscore); return; }
      pl.lives--;
      pl.alive = true; pl.x = -32; pl.y = 110; pl.invuln = 150; pl.bombs = std::max(pl.bombs, START_BOMBS);
    }
    return;
  }
  const float spd = 2.5f;
  if (pl.x < 20 && pl.invuln > 100) pl.x += 1.5f;   // fly in after respawning
  else { pl.x += in.ax * spd; pl.y += in.ay * spd; }
  pl.x = clampv(pl.x, 0.0f, FW - 34);
  pl.y = clampv(pl.y, 14.0f, FH - 18);
  if (pl.invuln > 0) pl.invuln--;
  if (pl.fireCd > 0) pl.fireCd--;
  if (in.down(BTN_A | BTN_R | BTN_X) && pl.fireCd == 0) {
    pl.fireCd = 7;
    auto shot = [](float x, float y, float vx, float vy, uint8_t k) {
      Shot* s = alloc(pshots);
      if (s) { s->x = x; s->y = y; s->vx = vx; s->vy = vy; s->kind = k; }
    };
    if (pl.weapon == 1) shot(pl.x + 26, pl.y + 5, 7.5f, 0, 0);
    else { shot(pl.x + 24, pl.y + 2, 7.5f, 0, 0); shot(pl.x + 24, pl.y + 9, 7.5f, 0, 0); }
    if (pl.weapon >= 3) { shot(pl.x + 20, pl.y + 4, 6.5f, -1.6f, 1); shot(pl.x + 20, pl.y + 5, 6.5f, 1.6f, 1); }
    sfx(SFX_SHOOT, -0.3f, pl.weapon >= 3 ? 1.25f : 1.4f);
  }
  if (in.hit(BTN_B | BTN_L | BTN_Y)) useBomb();
  Particle* p = alloc(parts);   // engine exhaust
  if (p) {
    p->x = pl.x + 1; p->y = pl.y + 7.5f + frange(-1, 1);
    p->vx = -frange(1.0f, 2.2f); p->vy = frange(-0.2f, 0.2f);
    p->maxLife = p->life = 14; p->kind = 2; p->size = 1;
  }
}

static void updateEnemies() {
  float py = pl.y + 8;
  for (auto& e : enemies) {
    if (!e.on) continue;
    if (e.flash) e.flash--;
    e.t += 1;
    float cx = e.x + EW[e.type] / 2, cy = e.y + EH[e.type] / 2;
    switch (e.type) {
      case E_DRONE:
        e.x += e.vx;
        e.y = e.baseY + 26 * sinf(e.t * 0.055f + e.param);
        if (level >= 2 && --e.timer <= 0 && e.x < FW - 20 && e.x > 60) {
          fireEnemyShot(cx, cy, aimAt(cx, cy), bulletSpeed());
          e.timer = frames(150 + rnd() % 120);
        }
        break;
      case E_DART:
        if (e.t < 45) { e.vy += (py > cy ? 0.05f : -0.05f); e.vy = clampv(e.vy, -0.9f, 0.9f); }
        e.x += e.vx; e.y += e.vy;
        break;
      case E_POD:
        e.timer++;
        if (e.timer < 420) {
          if (e.x > e.param) e.x += e.vx;
          e.y = e.baseY + 8 * sinf(e.t * 0.04f);
          if (e.timer % 70 == 10 && e.x < FW - 10) {
            float a = aimAt(e.x, cy);
            for (int k = -1; k <= 1; k++) fireEnemyShot(e.x + 2, cy, a + k * 0.22f, bulletSpeed());
          }
        } else e.x -= 1.4f;
        break;
      case E_BOSS: {
        if (e.timer < 0) {   // dying: a chain of explosions
          e.timer--;
          if ((e.timer & 7) == 0) { explode(e.x + frange(10, 74), e.y + frange(8, 54), 16, 2.0f, false); sfx(SFX_EXPLODE, 0.3f, 1.0f); }
          if (e.timer < -100) {
            explode(cx, cy, 120, 4.0f, true);
            flashTimer = 8; addShake(16);
            sfx(SFX_EXPLODE, 0, 0.45f);
            rumble(1.0f, 700);
            e.on = false; bossActive = false; bossIdx = -1;
            clearTimer = 240;
            addScore(5000 * level);
            music(&SONG_STAGE);
          }
          break;
        }
        if (e.x > FW - 106) e.x += e.vx;
        e.y = e.baseY + 55 * sinf(e.t * 0.012f);
        e.timer++;
        bool angry = e.hp < (280 + level * 90) / 2;
        int cyc = e.timer % 600;
        float coreX = e.x + 12, coreY = e.y + 32;
        if (cyc < 200) {
          if (cyc % (angry ? 50 : 70) == 0)
            for (int k = -5; k <= 5; k++) fireEnemyShot(coreX, coreY, 3.14159f + k * 0.16f, bulletSpeed() * 0.9f);
        } else if (cyc < 380) {
          if (cyc % (angry ? 16 : 24) == 0) {
            float a = aimAt(coreX, coreY);
            for (int k = -1; k <= 1; k++) fireEnemyShot(coreX, coreY, a + k * 0.12f, bulletSpeed() * 1.2f);
          }
        } else if (cyc < 520) {
          if (cyc % (angry ? 4 : 6) == 0) {
            float a = e.timer * 0.21f;
            fireEnemyShot(coreX + 20, coreY, a, bulletSpeed() * 0.8f);
            if (angry) fireEnemyShot(coreX + 20, coreY, a + 3.14159f, bulletSpeed() * 0.8f);
          }
        } else if (cyc == 540) {
          queueSpawn(0, E_DRONE, 40, 0); queueSpawn(12, E_DRONE, 40, 0);
          queueSpawn(0, E_DRONE, 190, 3.14f); queueSpawn(12, E_DRONE, 190, 3.14f);
        }
        break;
      }
    }
    if (e.x < -EW[e.type] - 10 || e.y > FH + 40 || e.y < -60) e.on = false;
    if (e.on && pl.alive && e.hp > 0) {
      float bx = e.x + 2, by = e.y + 2, bw = EW[e.type] - 4, bh = EH[e.type] - 4;
      if (e.type == E_BOSS) { bx = e.x + 6; by = e.y + 12; bw = EW[e.type] - 14; bh = EH[e.type] - 24; }
      if (overlap(pl.x + 6, pl.y + 5, 20, 7, bx, by, bw, bh)) { playerHit(); if (e.type != E_BOSS) damageEnemy(e, 50); }
    }
  }
}

static void updateShots() {
  for (auto& s : pshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (s.x > FW || s.y < -10 || s.y > FH) { s.on = false; continue; }
    float w = s.kind ? 8 : 12, h = s.kind ? 8 : 5;
    for (auto& e : enemies) {
      if (!e.on || e.hp <= 0) continue;
      float bx = e.x + 2, by = e.y + 2, bw = EW[e.type] - 4, bh = EH[e.type] - 4;
      if (e.type == E_BOSS) { bx = e.x + 4; by = e.y + 10; bw = EW[e.type] - 10; bh = EH[e.type] - 20; }
      if (overlap(s.x, s.y, w, h, bx, by, bw, bh)) {
        damageEnemy(e, 1);
        for (int i = 0; i < 3; i++) {
          Particle* p = alloc(parts);
          if (p) { p->x = s.x + w; p->y = s.y + h / 2; p->vx = frange(-2, 0.5f); p->vy = frange(-1.2f, 1.2f); p->maxLife = p->life = 8; p->kind = 3; p->size = 1; }
        }
        s.on = false;
        break;
      }
    }
  }
  float hx = pl.x + 17, hy = pl.y + 8;
  for (auto& s : eshots) {
    if (!s.on) continue;
    s.x += s.vx; s.y += s.vy;
    if (s.x < -8 || s.x > FW + 8 || s.y < -8 || s.y > FH + 8) { s.on = false; continue; }
    if (pl.alive) {
      float dx = (s.x + 3.5f) - hx, dy = (s.y + 3.5f) - hy;
      if (pl.shield && dx * dx + dy * dy < 13 * 13) {
        s.on = false; explode(s.x + 3, s.y + 3, 4, 1.0f, false);
        if (--pl.shield <= 0) pl.shield = 0;
        continue;
      }
      if (fabsf(dx) < 6 && fabsf(dy) < 4) { s.on = false; playerHit(); }
    }
  }
}

static void updateFx() {
  for (auto& p : parts) {
    if (!p.on) continue;
    p.x += p.vx; p.y += p.vy; p.vx *= 0.96f; p.vy *= 0.96f;
    if (p.kind == 1) p.vy += 0.03f;
    if (--p.life == 0) p.on = false;
  }
  for (auto& r : rings) { if (!r.on) continue; r.r += r.vr; r.vr *= 0.94f; if (--r.life == 0) r.on = false; }
  for (auto& p : popups) { if (!p.on) continue; p.y -= 0.5f; if (--p.life == 0) p.on = false; }
  for (auto& p : pickups) {
    if (!p.on) continue;
    p.t += 1; p.x -= 0.7f; p.y += sinf(p.t * 0.08f) * 0.5f;
    if (p.x < -16) { p.on = false; continue; }
    if (pl.alive && overlap(pl.x, pl.y, 32, 16, p.x, p.y, 13, 13)) {
      p.on = false;
      sfx(SFX_POWERUP);
      rumble(0.25f, 80);
      if (p.type == 0) {
        if (pl.weapon < 3) { pl.weapon++; popup(p.x - 8, p.y - 6, "POWER"); }
        else { addScore(1000); popup(p.x - 8, p.y - 6, "1000"); }
      } else if (p.type == 1) { pl.shield = 3; popup(p.x - 8, p.y - 6, "SHIELD"); }
      else { pl.bombs = std::min(pl.bombs + 1, 5); popup(p.x - 4, p.y - 6, "BOMB"); }
    }
  }
  if (shake > 0.5f) { shakeX = frange(-shake, shake); shakeY = frange(-shake, shake); shake *= 0.85f; }
  else { shake = 0; shakeX = shakeY = 0; }
  if (flashTimer) flashTimer--;
}

static void updateDirector() {
  levelFrames++;
  for (auto& p : pending) { if (!p.on) continue; if (--p.delay <= 0) { spawnEnemy(p.type, p.y, p.param); p.on = false; } }
  if (bannerTimer) bannerTimer--;
  if (clearTimer) {
    if (--clearTimer == 0) { level++; levelFrames = 0; bannerTimer = 150; waveTimer = 60; }
    return;
  }
  const uint32_t bossAt = 60 * 55;
  if (!bossActive && levelFrames == bossAt) { warnTimer = 180; music(nullptr); sfx(SFX_HURT, 0, 0.5f); }
  if (warnTimer) {
    if (warnTimer % 40 == 0) sfx(SFX_HURT, 0, 0.5f);
    if (--warnTimer == 0) { spawnEnemy(E_BOSS, 90, 0); music(&SONG_BOSS); }
    return;
  }
  if (bossActive || levelFrames > bossAt) return;
  if (--waveTimer <= 0) { spawnWave(); waveTimer = frames(std::max(50, 125 - level * 12 - (int)(levelFrames / 500))); }
}

// ------------------------------------------------------------ the synthwave backdrop
static float mountScroll = 0, mountScroll2 = 0, cityScroll = 0, planetX = 0.72f * W;
static const int NB = 70;
struct Building { float x, w, h; uint32_t seed; bool antenna; };
static Building city[NB];
static float cityLen = 0;
static ui::Stars stars;

static void buildCity() {
  float x = 0;
  for (auto& b : city) {
    b.w = frange(45, 120); b.h = frange(80, 260); b.seed = rnd(); b.antenna = rnd() % 4 == 0;
    b.x = x;
    x += b.w + frange(0, 12);
  }
  cityLen = x;
}

static void scrollBackground(float spd) {
  mountScroll += 0.35f * spd * K; mountScroll2 += 0.18f * spd * K; cityScroll += 1.1f * spd * K;
  planetX -= 0.04f * spd * K;
  if (planetX < -400) planetX = W + 50;
  stars.step(-0.8f * spd * K, 0);
}

static float mountain(float x, float seed) {
  float a = x * 6.2831853f / 2304.0f;
  return 44 + 16 * sinf(a * 3 + seed) + 9 * sinf(a * 7 + 1.3f + seed) + 4 * sinf(a * 19 + 2.1f);
}

static void drawBackground() {
  if (flashTimer) { rect(0, 0, W, H, (flashTimer & 2) ? Color(255, 240, 220) : Color(255, 180, 210)); return; }
  rectGrad(0, 0, W, 225, Color(6, 5, 20), Color(14, 12, 44));
  rectGrad(0, 225, W, 405, Color(14, 12, 44), Color(52, 28, 100));
  rectGrad(0, 630, W, 290, Color(52, 28, 100), Color(170, 60, 120));
  rectGrad(0, 920, W, 160, Color(170, 60, 120), Color(240, 120, 110));
  stars.draw(Color(220, 220, 255));
  draw(nlart::IMG_PLANET, planetX, 80);
  // the sun: a gradient disc cut by widening slats towards the bottom
  float sx = 0.74f * W, sy = 855, sr = 207;
  glow(sx, sy, sr * 2.2f, Color(255, 120, 140, 60));
  for (float dy = -sr; dy < sr; dy += 3) {
    float d = dy + 1.5f;
    if (d > 18 && fmodf(d, 36) < d / 9 * 0.6f) continue;
    float hw = sqrtf(fmaxf(0, sr * sr - d * d));
    float t = (dy + sr) / (2 * sr);
    rect(sx - hw, sy + dy, hw * 2, 3.2f, Color(255, (uint8_t)(230 - 150 * t), (uint8_t)(90 + 60 * t)));
  }
  // two ranges of mountains
  for (int layer = 0; layer < 2; layer++) {
    float scroll = layer ? mountScroll : mountScroll2, base = layer ? 1080 : 1020, amp = layer ? 4.5f : 3.6f;
    Color top = layer ? Color(110, 60, 150) : Color(80, 50, 130), bot = layer ? Color(40, 22, 66) : Color(50, 30, 90);
    const int N = 64;
    Vtx v[N * 6];
    int k = 0;
    for (int i = 0; i < N; i++) {
      float x0 = i * (W / (float)N), x1 = (i + 1) * (W / (float)N);
      float h0 = base - mountain(x0 + scroll, layer * 2.0f) * amp, h1 = base - mountain(x1 + scroll, layer * 2.0f) * amp;
      v[k++] = {x0, h0, top}; v[k++] = {x1, h1, top}; v[k++] = {x1, (float)H, bot};
      v[k++] = {x0, h0, top}; v[k++] = {x1, (float)H, bot}; v[k++] = {x0, (float)H, bot};
    }
    tris(v, k);
  }
  // the city
  float off = fmodf(cityScroll, cityLen);
  for (int rep = 0; rep < 2; rep++)
    for (auto& b : city) {
      float x = b.x - off + rep * cityLen;
      if (x > W || x + b.w < 0) continue;
      float top = H - b.h;
      rect(x, top, b.w, b.h, Color(16, 13, 38));
      rect(x, top, b.w, 4, Color(70, 60, 130));
      rect(x, top, 3, b.h, Color(30, 26, 66));
      if (b.antenna) {
        rect(x + b.w / 2 - 2, top - 40, 4, 40, Color(56, 52, 110));
        if ((frameNo / 30 + b.seed) & 1) { disc(x + b.w / 2, top - 42, 5, Color(255, 60, 80)); glow(x + b.w / 2, top - 42, 26, Color(255, 60, 80, 120)); }
      }
      uint32_t h = b.seed;
      for (float wy = top + 16; wy < H - 10; wy += 22)
        for (float wx = x + 10; wx < x + b.w - 14; wx += 16) {
          h = h * 1103515245u + 12345u;
          int v = (h >> 16) & 15;
          if (v < 5) rect(wx, wy, 8, 10, Color(255, 206, 90));
          else if (v < 7) rect(wx, wy, 8, 10, Color(90, 210, 240));
        }
    }
}

// ------------------------------------------------------------ drawing the action
static const nova::Img& enemyImg(const Enemy& e) {
  switch (e.type) {
    case E_DRONE: return (frameNo >> 3) & 1 ? nlart::IMG_DRONE1 : nlart::IMG_DRONE0;
    case E_DART: return nlart::IMG_DART;
    case E_POD: return (e.timer % 70) < 10 ? nlart::IMG_POD1 : nlart::IMG_POD0;
    default: return nlart::IMG_BOSS;
  }
}

static const Color PICK_COL[3] = {Color(255, 150, 60), Color(70, 200, 250), Color(245, 70, 120)};
static const char* const PICK_CH[3] = {"P", "S", "B"};

static void drawWorld() {
  float ox = shakeX, oy = shakeY;
  bool blink = (frameNo >> 2) & 1;
  for (auto& p : pickups) {
    if (!p.on) continue;
    float x = X(p.x) + ox, y = Y(p.y) + oy, s = 13 * K;
    Color c = PICK_COL[p.type];
    glow(x + s / 2, y + s / 2, 80, c.alpha(0.4f));
    roundRect(x, y, s, s, 16, Color(20, 16, 40));
    roundRect(x + 4, y + 4, s - 8, s - 8, 12, c);
    roundRect(x + 10, y + 8, s - 20, 10, 5, Color(255, 255, 255, 160));
    TextStyle st = ui::style(40, Color(20, 16, 40)); st.outline = CLEAR;
    text(PICK_CH[p.type], x + s / 2, y + 12, st);
    float a = frameNo * 0.15f;
    Fx f; f.blend = BLEND_ADD; f.sx = f.sy = 0.4f; f.tint = WHITE;
    draw(cart::IMG_SPARK, x + s / 2 + 40 * cosf(a), y + s / 2 + 40 * sinf(a), f);
  }
  for (auto& e : enemies) {
    if (!e.on) continue;
    const nova::Img& img = enemyImg(e);
    float x = X(e.x) + ox, y = Y(e.y) + oy;
    if (e.type == E_BOSS) {
      float pulse = 0.5f + 0.5f * sinf(frameNo * 0.15f);
      glow(x + 60, y + EH[3] * K / 2 + 4, 200 + 40 * pulse, Color(255, 80, 140, 70));
    }
    Fx f;
    if (e.type == E_DRONE) { f.rot = e.t * 3.0f; draw(img, x + EW[0] * K / 2, y + EH[0] * K / 2, f); }   // drones spin
    else draw(img, x, y, f);
    if (e.flash) {   // a white flash when hit
      Fx w = f; w.blend = BLEND_ADD; w.tint = Color(255, 255, 255, 200);
      if (e.type == E_DRONE) draw(img, x + EW[0] * K / 2, y + EH[0] * K / 2, w); else draw(img, x, y, w);
    }
    if (e.type == E_DART) glow(x + EW[1] * K, y + EH[1] * K / 2, 40, Color(255, 150, 60, 140));
  }
  for (auto& s : pshots) {
    if (!s.on) continue;
    float x = X(s.x) + ox, y = Y(s.y) + oy;
    Fx f; f.blend = BLEND_ADD; f.rot = atan2f(s.vy, s.vx) * 57.3f;
    if (s.kind) { f.tint = Color(255, 180, 255); f.sx = 1.0f; draw(cart::IMG_STREAK, x + 18, y + 18, f); }
    else { f.tint = Color(140, 230, 255); f.sx = 1.6f; f.sy = 1.4f; draw(cart::IMG_STREAK, x + 27, y + 11, f); }
  }
  if (pl.alive && !(pl.invuln && blink && pl.invuln < 140)) {
    float x = X(pl.x) + ox, y = Y(pl.y) + oy;
    float fl = 0.7f + 0.3f * frand();
    glow(x - 10, y + 36, 70 * fl, Color(80, 200, 255, 160));
    Vtx flame[3] = {{x + 6, y + 26, Color(220, 250, 255, 230)}, {x + 6, y + 46, Color(220, 250, 255, 230)}, {x - 40 * fl - 10, y + 36, Color(60, 160, 255, 0)}};
    tris(flame, 3, BLEND_ADD);
    draw(nlart::IMG_PLAYER, x, y);
    if (pl.shield) {
      float cx = x + 17 * K, cy = y + 8 * K;
      Fx r; r.blend = BLEND_ADD; r.tint = Color(120, 220, 255, (uint8_t)(150 + 60 * sinf(frameNo * 0.3f))); r.sx = r.sy = 1.15f;
      draw(cart::IMG_RING, cx, cy, r);
      glow(cx, cy, 90, Color(80, 200, 255, 40));
    }
  }
  for (auto& p : parts) {
    if (!p.on) continue;
    float x = X(p.x) + ox, y = Y(p.y) + oy, t = (float)p.life / p.maxLife;
    if (p.kind == 2) { glow(x, y, 14 + 10 * (1 - t), Color(80, 200, 255, (uint8_t)(160 * t))); continue; }
    if (p.kind == 3) { disc(x, y, 3, Color(255, 255, 200, (uint8_t)(255 * t))); continue; }
    if (p.kind == 1) { Fx f; f.blend = BLEND_ADD; f.tint = Color(255, 230, 150, (uint8_t)(255 * t)); f.sx = f.sy = 0.35f; f.rot = p.life * 10.0f; draw(cart::IMG_SPARK, x, y, f); continue; }
    Color c = t > 0.7f ? Color(255, 250, 210) : t > 0.45f ? Color(255, 200, 80) : t > 0.25f ? Color(255, 110, 50) : Color(120, 40, 60);
    float r = (p.size > 1 ? 14 : 9) * (0.6f + t);
    glow(x, y, r * 2.2f, c.alpha(0.35f * t));
    disc(x, y, r, c.alpha(fminf(1, t * 2)));
  }
  for (auto& r : rings) {
    if (!r.on) continue;
    float t = (float)r.life / r.max;
    Fx f; f.blend = BLEND_ADD; f.tint = Color(255, 220, 170, (uint8_t)(230 * t)); f.sx = f.sy = r.r * K / 54.0f;
    draw(cart::IMG_RING, X(r.x) + ox, Y(r.y) + oy, f);
  }
  for (auto& s : eshots) {
    if (!s.on) continue;
    float x = X(s.x + 3.5f) + ox, y = Y(s.y + 3.5f) + oy;
    glow(x, y, 34, Color(255, 80, 160, 170));
    disc(x, y, 12, blink ? Color(255, 120, 200) : Color(255, 230, 120));
    disc(x, y, 6, WHITE);
  }
  for (auto& p : popups) {
    if (!p.on) continue;
    text(p.txt, X(p.x), Y(p.y), ui::style(36, (p.life & 4) ? Color(255, 220, 110) : WHITE, LEFT));
  }
}

static void drawHud() {
  rectGrad(0, 0, W, 80, Color(6, 5, 20, 210), Color(6, 5, 20, 0));
  textf(36, 16, ui::style(44, WHITE, LEFT), "%07lu", (unsigned long)score);
  textf(W / 2, 22, ui::style(34, Color(255, 220, 110)), "HI %07lu", (unsigned long)hiscore);
  for (int i = 0; i < std::min(pl.bombs, 5); i++) {
    float x = 330 + i * 40, y = 40;
    glow(x, y, 24, Color(245, 70, 120, 120));
    disc(x, y, 12, Color(245, 70, 120)); disc(x - 3, y - 3, 4, WHITE);
  }
  for (int i = 0; i < 3; i++) roundRect(560 + i * 26, 26, 18, 30, 6, i < pl.weapon ? Color(255, 150, 60) : Color(255, 255, 255, 40));
  for (int i = 0; i < std::min(pl.lives, 5); i++) {
    Fx f; f.sx = f.sy = 0.32f;
    draw(nlart::IMG_PLAYER, W - 90 - i * 60, 24, f);
  }
  if (bossActive && bossIdx >= 0) {
    Enemy& b = enemies[bossIdx];
    int maxHp = 280 + level * 90;
    float w = std::max(0, (int)b.hp) * 900.0f / maxHp;
    roundRect(W / 2 - 456, 82, 912, 26, 13, Color(20, 10, 30, 220));
    roundRect(W / 2 - 450, 88, w, 14, 7, (frameNo & 8) && b.hp < maxHp / 4 ? Color(255, 240, 120) : Color(255, 70, 110));
  }
}

static void drawOverlays() {
  if (state == ST_TITLE) {
    float y = 820 + 20 * sinf(frameNo * 0.05f);
    glow(240, y + 36, 80, Color(80, 200, 255, 160));
    draw(nlart::IMG_PLAYER, 260, y);
    static const char* HELP[] = {"MOVE WITH THE STICK / ARROWS    A FIRE    B BOMB", "P: POWER   S: SHIELD   B: BOMB"};
    ui::titleScreen(cart::IMG_LOGO_NOVALANCE, "A SYNTHWAVE SHOOT-'EM-UP", hiscore, HELP, 2, Color(255, 200, 170));
    return;
  }
  if (state == ST_OVER) { ui::gameOver(overTimer, newHi); textf(W / 2, 540, ui::style(52), "SCORE %07lu", (unsigned long)score); return; }
  if (bannerTimer) {
    char buf[16]; snprintf(buf, sizeof(buf), "STAGE %d", level);
    ui::banner(buf, W / 2, 400, bannerTimer, 150, WHITE, 120);
    if (bannerTimer > 20) text("GET READY", W / 2, 560, ui::style(48, Color(255, 220, 110)));
  }
  if (warnTimer && ((warnTimer >> 3) & 1)) {
    rect(0, 450, W, 160, Color(200, 20, 60, 160));
    rect(0, 458, W, 8, Color(255, 220, 110)); rect(0, 594, W, 8, Color(255, 220, 110));
    text("WARNING", W / 2, 478, ui::style(110, WHITE));
  }
  if (clearTimer) {
    ui::banner("STAGE CLEAR", W / 2, 400, clearTimer, 240, Color(180, 255, 120), 120);
    char buf[24]; snprintf(buf, sizeof(buf), "BONUS %d", 5000 * level);
    if (clearTimer < 220) text(buf, W / 2, 560, ui::style(56));
  }
}

static void draw() {
  drawBackground();
  if (state != ST_TITLE) drawWorld();
  if (state == ST_PLAY) drawHud();
  drawOverlays();
}

// ------------------------------------------------------------ main loop
static void step(const Pad& in) {
  switch (state) {
    case ST_TITLE:
      scrollBackground(0.6f);
      if (titleInput(in)) hiscore = loadHi(HI_DEFAULT);
      if (in.hit(BTN_START | BTN_A)) { resetGame(); state = ST_PLAY; sfx(SFX_START); music(&SONG_STAGE); }
      break;
    case ST_PLAY:
      scrollBackground(1.0f);
      updateDirector();
      updatePlayer(in);
      if (state != ST_PLAY) break;
      updateEnemies();
      updateShots();
      updateFx();
      if (in.hit(BTN_START)) nova::pause();
      break;
    default:
      overTimer++;
      scrollBackground(0.4f);
      updateFx();
      if (overTimer > 90 && in.hit(BTN_START | BTN_A)) { state = ST_TITLE; music(&SONG_TITLE); }
      break;
  }
}

// ------------------------------------------------------------ bot: line up with targets, slide away from incoming shots
static void bot(Pad& p) {
  p = Pad();
  if (state != ST_PLAY) { if ((frameNo / 8) & 1) p.held = BTN_A; return; }
  if (!pl.alive) return;
  p.held = BTN_A;
  float hx = pl.x + 17, hy = pl.y + 8;
  float targetY = hy;
  float bestD = 1e9f;
  for (auto& e : enemies)
    if (e.on && e.hp > 0 && e.x > hx) {
      float d = e.x - hx;
      if (d < bestD) { bestD = d; targetY = e.y + EH[e.type] / 2; }
    }
  // score candidate heights by how close incoming shots and bodies will pass
  float best = -1e9f, bestY = hy;
  for (float dy = -40; dy <= 40; dy += 8) {
    float y = clampv(hy + dy, 22.0f, FH - 10);
    float s = -fabsf(y - targetY) * 0.3f - fabsf(dy) * 0.05f;
    for (auto& sh : eshots) {
      if (!sh.on) continue;
      for (int t = 0; t < 30; t += 3) {
        float sx = sh.x + 3.5f + sh.vx * t, sy = sh.y + 3.5f + sh.vy * t;
        float px = hx, py = hy + (y - hy) * fminf(1.0f, t / 16.0f);
        float d2 = (sx - px) * (sx - px) + (sy - py) * (sy - py);
        if (d2 < 18 * 18) s -= (18 * 18 - d2) * (1.2f - t / 30.0f);
      }
    }
    for (auto& e : enemies)
      if (e.on && e.type != E_BOSS) {
        float ex = e.x + EW[e.type] / 2 + e.vx * 10, ey = e.y + EH[e.type] / 2;
        float d2 = (ex - hx) * (ex - hx) + (ey - y) * (ey - y);
        if (d2 < 30 * 30) s -= (30 * 30 - d2) * 0.6f;
      }
    if (s > best) { best = s; bestY = y; }
  }
  p.ay = clampv((bestY - hy) / 2.5f, -1.0f, 1.0f);
  float wantX = bossActive ? 30 : 60;
  p.ax = clampv((wantX - pl.x) / 10.0f, -1.0f, 1.0f);
  if (best < -2000 && pl.bombs > 0 && (frameNo % 30) == 0) p.held |= BTN_B;
}

static void debugInfo(char* buf, int n) {
  snprintf(buf, n, "state %d stage %d score %lu lives %d bombs %d weapon %d boss %d", state, level, (unsigned long)score, pl.lives, pl.bombs, pl.weapon, bossActive);
}

static void init() {
  loadAtlas(nlart::TEX_FILES, nlart::TEX, nlart::NTEX);
}
static void enter() {
  state = ST_TITLE;
  buildCity();
  stars.init(140);
  hiscore = loadHi(HI_DEFAULT);
  music(&SONG_TITLE);
}
static bool canPause() { return state == ST_PLAY; }

}  // namespace nl

extern const nova::Game NOVA_LANCE;
const nova::Game NOVA_LANCE = {
  "novalance", "NOVA LANCE", "A SYNTHWAVE SHOOT-'EM-UP",
  nl::init, nl::enter, nl::step, nl::draw, nl::canPause, nullptr, nl::bot, nl::debugInfo,
};
