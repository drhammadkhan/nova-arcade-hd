// Tests for every game, run headless (no window, no sound).
//   test_games smoke [ID]        every game (or one): title, start, then 3 minutes of play driven by the game's
//                                bot (or random input), checking nothing crashes and the game reacts
//   test_games shots DIR [ID]    screenshots of each game's title and play
//   test_games thumbs [ID]       regenerate assets/<id>/thumb.png for the launcher
//   test_games launcher DIR      the launcher at the top of the grid and scrolled to the last game
#include "headless.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace nova;

static void hold(uint32_t b, int n) {
  for (int i = 0; i < n; i++) { Pad p; p.held = b; headless::step(p); }
}

// one frame of play: the game's bot if it has one, otherwise mashing random buttons
static void play(const Game* g, uint32_t& rndHeld) {
  Pad p;
  if (g->bot) g->bot(p);
  else {
    if (rnd() % 12 == 0) rndHeld = rnd() & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT | BTN_A | BTN_B | BTN_X);
    p.held = rndHeld;
  }
  p.held &= ~(BTN_START | BTN_HOME | BTN_SELECT);   // never pause from the test
  if (p.ax == 0) p.ax = (p.held & BTN_RIGHT) ? 1.0f : (p.held & BTN_LEFT) ? -1.0f : 0.0f;
  if (p.ay == 0) p.ay = (p.held & BTN_DOWN) ? 1.0f : (p.held & BTN_UP) ? -1.0f : 0.0f;
  headless::step(p);
}

static void status(const Game* g, const char* when) {
  char buf[160] = "";
  if (g->debug) g->debug(buf, sizeof(buf));
  printf("  %-12s %-8s %s\n", g->id, when, buf);
}

static bool runGame(const Game* g, const char* shotDir, bool thumb) {
  // back to the launcher, then pick this game
  if (currentGame()) exitToLauncher();
  hold(0, 2);
  int idx = 0;
  for (int i = 0; i < NGAMES; i++) if (GAMES[i] == g) idx = i;
  // the launcher remembers the last game: walk left to the start, then right to ours
  for (int i = 0; i < NGAMES; i++) { hold(BTN_LEFT, 1); hold(0, 1); }
  for (int i = 0; i < idx; i++) { hold(BTN_RIGHT, 1); hold(0, 1); }
  hold(BTN_A, 1); hold(0, 60);
  if (currentGame() != g) { printf("  %s: launcher didn't start it\n", g->id); return false; }
  std::string base = shotDir ? std::string(shotDir) + "/" + g->id : "";
  if (shotDir) headless::shot((base + "_title.png").c_str(), 2);
  hold(BTN_A, 1); hold(0, 30);
  hold(BTN_A, 1); hold(0, 10);   // some games show an intro card first
  status(g, "start");
  uint32_t rh = 0;
  int frames = thumb ? 60 * 12 : 60 * 180;
  for (int f = 0; f < frames; f++) {
    play(g, rh);
    if (shotDir && (f == 60 * 8 || f == 60 * 40)) headless::shot((base + (f == 60 * 8 ? "_play1.png" : "_play2.png")).c_str(), 2);
    if (!thumb && f % (60 * 60) == 60 * 60 - 1) status(g, "playing");
    if (!thumb && f % 97 == 0) headless::shot(nullptr);   // draw now and then, so drawing bugs show up too
    if (currentGame() != g) { printf("  %s: left the game unexpectedly at frame %d\n", g->id, f); return false; }
  }
  if (thumb) {
    std::string out = std::string(getenv("NOVA_ASSETS")) + "/" + g->id + "/thumb.png";
    headless::shot(out.c_str(), 2);
    printf("wrote %s\n", out.c_str());
  }
  status(g, "end");
  return true;
}

int main(int argc, char** argv) {
  const char* mode = argc > 1 ? argv[1] : "smoke";
  setenv("NOVA_SETTINGS", "/tmp/nova-games-settings.txt", 1);
  remove("/tmp/nova-games-settings.txt");
  if (!getenv("NOVA_ASSETS")) setenv("NOVA_ASSETS", ASSETS, 1);
  bool shots = !strcmp(mode, "shots"), thumbs = !strcmp(mode, "thumbs");
  const char* dir = shots && argc > 2 ? argv[2] : nullptr;
  const char* only = argc > (shots ? 3 : 2) ? argv[shots ? 3 : 2] : nullptr;
  if (!headless::begin(true, nullptr)) { printf("headless start failed\n"); return 2; }
  if (!strcmp(mode, "launcher") && argc > 2) {
    std::string d = argv[2];
    hold(0, 30);
    headless::shot((d + "/launcher.png").c_str(), 1);
    for (int i = 0; i < NGAMES; i++) { hold(BTN_RIGHT, 2); hold(0, 4); }
    hold(0, 60);
    headless::shot((d + "/launcher_end.png").c_str(), 1);
    headless::end();
    return 0;
  }
  int fails = 0, n = 0;
  for (int i = 0; i < NGAMES; i++) {
    const Game* g = GAMES[i];
    if (only && strcmp(only, g->id)) continue;
    if (!strcmp(g->id, "pixelpeaks") && !only) continue;   // has its own test
    n++;
    if (!runGame(g, dir, thumbs)) fails++;
  }
  headless::end();
  printf("%d game(s), %d failed\n", n, fails);
  return fails ? 1 : 0;
}
