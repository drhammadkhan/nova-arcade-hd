// Pixel Peaks HD tests, run headless (no window, no sound).
//   test_pixelpeaks bot [--damage]   a look-ahead bot must clear all six levels without losing a life
//                                    (foes and spikes are harmless unless --damage; pits and water never are)
//   test_pixelpeaks shots DIR        screenshots of the title, every level, the menus and the end screens
//   test_pixelpeaks thumb            regenerates assets/pixelpeaks/thumb.png for the launcher
// Bot options from the environment: BOT_LEVEL=n starts at level n, BOT_TRACE=1 prints her position.
#include "headless.h"
#include "audio.h"
#include "PixelPeaks/pixelpeaks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace nova;


// one tick of game logic only, with the pad built the same way the real input layer builds it
static void simStep(uint32_t held, uint32_t& prev) {
  Pad p;
  p.held = held;
  p.pressed = held & ~prev;
  p.released = prev & ~held;
  p.ax = (held & BTN_RIGHT) ? 1.0f : (held & BTN_LEFT) ? -1.0f : 0.0f;
  prev = held;
  pad = p;
  pp::logicStep(p);
}

// The follow-through after a planned move: run right, and jump at edges and walls. jumpT counts the
// frames the jump button has been held.
static uint32_t autoRun(int& jumpT) {
  pp::DebugInfo d;
  pp::debugState(d);
  const float HW = 45, HH = 99;
  if (jumpT > 0) { jumpT++; if (jumpT < 14) return BTN_RIGHT | BTN_A; jumpT = 0; return BTN_RIGHT; }
  if (d.onGround) {
    float feet = d.y + HH;
    float ahead = d.x + HW + 30;
    uint8_t below = pp::tileAt(ahead, feet + 8);
    bool gap = !pp::tileSolidAt(ahead, feet + 8) && below != 3 /* plank */;
    bool wall = pp::tileSolidAt(ahead, feet - 10) || pp::tileSolidAt(ahead, feet - 80);
    uint8_t hazard = pp::tileAt(ahead, feet - 10);
    if (gap || wall || hazard == 4 /* spikes */) { jumpT = 1; return BTN_RIGHT | BTN_A; }
  }
  return BTN_RIGHT;
}

// how good the position is after a simulated future
static double score(const pp::DebugInfo& d0, const pp::DebugInfo& d) {
  if (d.state == pp::DBG_CLEAR || d.state == pp::DBG_WIN || d.level != d0.level) return 1e9;
  if (!d.alive || d.state == pp::DBG_DEAD || d.lives < d0.lives) return -1e9;
  double s = d.x;
  s -= (d0.hearts - d.hearts) * 4000.0;
  s += d.scrolls * 300.0;
  if (d.onGround) s += 40;
  return s;
}

static bool runBot(bool damage) {
  pp::invincible = !damage;
  pp::startAt(getenv("BOT_LEVEL") ? atoi(getenv("BOT_LEVEL")) - 1 : 0);
  std::vector<uint8_t> save(pp::stateSize()), save2(pp::stateSize());
  const uint32_t R = BTN_RIGHT, L = BTN_LEFT, A = BTN_A, B = BTN_B;
  // first chunk choices (10 frames) then a second chunk, then running right for a while
  const uint32_t opts[] = {R, R | A, A, 0, L, L | A, R | B};
  const int NO = sizeof(opts) / sizeof(opts[0]);
  const int holds[] = {10, 4};   // full jump or a short hop
  int lastLevel = -1, frames = 0, levelFrames = 0, livesLost = 0;
  float bestX = 0;
  int sinceBest = 0;
  bool ok = true;
  uint32_t prev = 0;
  int startLives = 3;
  while (frames < 60 * 60 * 30) {
    pp::DebugInfo d;
    pp::debugState(d);
    if (d.state == pp::DBG_WIN) break;
    if (d.state == pp::DBG_OVER) { printf("  game over on level %d\n", d.level + 1); ok = false; break; }
    if (d.level != lastLevel) {
      if (lastLevel >= 0) printf("  level %d cleared in %.1f s\n", lastLevel + 1, levelFrames / 60.0);
      lastLevel = d.level; levelFrames = 0; bestX = 0; sinceBest = 0;
      startLives = d.lives;
    }
    if (d.lives < startLives) { livesLost += startLives - d.lives; startLives = d.lives; printf("  lost a life on level %d at x=%.0f\n", d.level + 1, d.x / 72); }
    if (d.state == pp::DBG_INTRO) { simStep(frames & 1 ? A : 0, prev); frames++; continue; }
    if (d.state != pp::DBG_PLAY) { simStep(0, prev); frames++; continue; }
    if (getenv("BOT_TRACE") && frames % 20 == 0) printf("    f%d level %d x %.2f y %.2f ground %d state %d lives %d\n", frames, d.level + 1, d.x / 72, d.y / 72, d.onGround, d.state, d.lives);
    if (d.x > bestX + 1) { bestX = d.x; sinceBest = 0; }
    if (++sinceBest > 60 * 25) { printf("  stuck on level %d at tile %.0f\n", d.level + 1, d.x / 72); ok = false; break; }
    // look ahead
    audio::mute++;
    pp::copyState(save.data());
    double best = -1e18;
    uint32_t bestA = R, bestB = R; int bestHold = 10;
    for (int i = 0; i < NO; i++)
      for (int hv = 0; hv < 2; hv++) {
        if (hv == 1 && !(opts[i] & A)) continue;
        for (int j = 0; j < NO; j++) {
          pp::restoreState(save.data());
          uint32_t pv = prev;
          for (int f = 0; f < 10; f++) simStep(f < holds[hv] ? opts[i] : (opts[i] & ~A), pv);
          pp::DebugInfo mid;
          pp::debugState(mid);
          for (int f = 0; f < 10; f++) simStep(opts[j], pv);
          int jt = 0;
          for (int f = 0; f < 50; f++) simStep(autoRun(jt), pv);
          pp::DebugInfo e;
          pp::debugState(e);
          double s = score(d, e);
          if (s > -1e8 && s < 1e8) s += 0.5 * (mid.x - d.x);   // progress now beats the same progress later
          // prefer simple choices when they tie
          s -= (i != 0) * 2 + (j != 0) * 1;
          if (s > best) { best = s; bestA = opts[i]; bestB = opts[j]; bestHold = holds[hv]; }
        }
      }
    pp::restoreState(save.data());
    audio::mute--;
    // play the whole plan for real (re-planning after one chunk lets "wait, then go" win forever)
    for (int f = 0; f < 20; f++) {
      simStep(f >= 10 ? bestB : f < bestHold ? bestA : (bestA & ~A), prev);
      frames++; levelFrames++;
      pp::DebugInfo e;
      pp::debugState(e);
      if (e.state != pp::DBG_PLAY) break;
    }
  }
  pp::DebugInfo d;
  pp::debugState(d);
  if (d.state != pp::DBG_WIN) { if (ok) printf("  ran out of time on level %d\n", d.level + 1); ok = false; }
  else printf("  level %d cleared in %.1f s\n", lastLevel + 1, levelFrames / 60.0);
  printf("bot (%s): %s, %d lives lost, score %u, %.1f minutes\n", damage ? "foes on" : "foes off", ok ? "finished all levels" : "FAILED", livesLost, d.score, frames / 3600.0);
  if (!damage && livesLost > 0) ok = false;
  return ok;
}

// ------------------------------------------------------------ screenshots
static void hold(uint32_t b, int n) {
  for (int i = 0; i < n; i++) {
    Pad p; p.held = b; p.ax = (b & BTN_RIGHT) ? 1.0f : (b & BTN_LEFT) ? -1.0f : 0.0f;
    headless::step(p);
  }
}

static void shots(const char* dir) {
  std::string d = dir;
  auto shot = [&](const char* name) {
    std::string p = d + "/" + name + ".png";
    headless::shot(p.c_str());
    printf("wrote %s\n", p.c_str());
  };
  hold(0, 30);
  shot("launcher");
  hold(BTN_A, 1); hold(0, 90);
  shot("title");
  hold(BTN_A, 1); hold(0, 40);
  shot("intro");
  hold(BTN_A, 1); hold(0, 5);
  hold(BTN_RIGHT, 50);
  shot("play1");
  hold(BTN_RIGHT | BTN_A, 14);
  shot("jump");
  hold(BTN_RIGHT, 40);
  hold(BTN_RIGHT | BTN_B, 1); hold(BTN_RIGHT, 8);
  shot("roll");
  hold(BTN_START, 1); hold(0, 20);
  shot("pause");
  hold(BTN_START, 1); hold(0, 5);
  for (int lv = 0; lv < 6; lv++) {
    pp::invincible = true;
    pp::startAt(lv);
    hold(0, 20);
    hold(BTN_RIGHT, 160 + lv * 10);
    hold(BTN_RIGHT | BTN_A, 12);
    hold(BTN_RIGHT, 6);
    char name[32];
    snprintf(name, sizeof(name), "level%d", lv + 1);
    shot(name);
  }
}

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "bot";
  char settings[] = "/tmp/nova-test-settings.txt";
  remove(settings);
  setenv("NOVA_SETTINGS", settings, 1);
  if (!getenv("NOVA_ASSETS")) setenv("NOVA_ASSETS", ASSETS, 1);
  bool render = strcmp(mode, "bot") != 0;
  if (!headless::begin(render, render ? nullptr : "pixelpeaks")) { printf("headless start failed\n"); return 2; }
  int rc = 0;
  if (!strcmp(mode, "bot")) {
    bool damage = argc > 2 && !strcmp(argv[2], "--damage");
    rc = runBot(damage) ? 0 : 1;
  } else if (!strcmp(mode, "shots")) {
    shots(argc > 2 ? argv[2] : ".");
  } else if (!strcmp(mode, "thumb")) {
    hold(0, 2); hold(BTN_A, 1); hold(0, 3);   // into the game
    pp::invincible = true;
    pp::startAt(0);
    hold(0, 10);
    hold(BTN_RIGHT, 150);
    hold(BTN_RIGHT | BTN_A, 10);
    std::string out = std::string(getenv("NOVA_ASSETS")) + "/pixelpeaks/thumb.png";
    headless::shot(out.c_str(), 2);
    printf("wrote %s\n", out.c_str());
  }
  headless::end();
  return rc;
}
