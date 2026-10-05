// The shared system layer: settings, difficulty, hi-scores, the pause menu and the launcher.
#include "nova.h"
#include "audio.h"
#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace nova {

uint32_t frameNo = 0;
Pad pad;

const char* assetPath(const char* rel) {
  static std::string base;
  static bool init = false;
  if (!init) {
    init = true;
#ifdef __EMSCRIPTEN__
    base = "/assets/";
#else
    if (const char* e = SDL_getenv("NOVA_ASSETS")) base = std::string(e) + "/";
    else { const char* b = SDL_GetBasePath(); base = std::string(b ? b : "") + "assets/"; }
#endif
  }
  static std::string out;
  out = base + rel;
  return out.c_str();
}

// ------------------------------------------------------------ saved settings
// Desktop: a small text file in the user's application-support folder. Browser: localStorage.
#ifdef __EMSCRIPTEN__
EM_JS(int, jsLoadInt, (const char* k, int def), {
  try { var v = localStorage.getItem("nova-hd/" + UTF8ToString(k)); return v === null ? def : (parseInt(v) | 0); } catch (e) { return def; }
});
EM_JS(void, jsSaveInt, (const char* k, int v), {
  try { localStorage.setItem("nova-hd/" + UTF8ToString(k), "" + v); } catch (e) {}
});
int loadInt(const char* ns, const char* key, int def) { std::string k = std::string(ns) + "/" + key; return jsLoadInt(k.c_str(), def); }
void saveInt(const char* ns, const char* key, int v) { std::string k = std::string(ns) + "/" + key; jsSaveInt(k.c_str(), v); }
#else
static std::map<std::string, int> settings;
static bool settingsLoaded = false;
static std::string settingsPath() {
  if (const char* e = SDL_getenv("NOVA_SETTINGS")) return e;   // tests point this at a scratch file
  char* p = SDL_GetPrefPath("Nova Arcade", "Nova Arcade HD");
  std::string s = p ? std::string(p) + "settings.txt" : "settings.txt";
  SDL_free(p);
  return s;
}
static void loadSettings() {
  if (settingsLoaded) return;
  settingsLoaded = true;
  FILE* f = fopen(settingsPath().c_str(), "r");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '=');
    if (!eq) continue;
    *eq = 0;
    settings[line] = atoi(eq + 1);
  }
  fclose(f);
}
int loadInt(const char* ns, const char* key, int def) {
  loadSettings();
  auto it = settings.find(std::string(ns) + "/" + key);
  return it == settings.end() ? def : it->second;
}
void saveInt(const char* ns, const char* key, int v) {
  loadSettings();
  std::string k = std::string(ns) + "/" + key;
  auto it = settings.find(k);
  if (it != settings.end() && it->second == v) return;
  settings[k] = v;
  FILE* f = fopen(settingsPath().c_str(), "w");
  if (!f) return;
  for (auto& kv : settings) fprintf(f, "%s=%d\n", kv.first.c_str(), kv.second);
  fclose(f);
}
#endif

// ------------------------------------------------------------ difficulty and hi-scores
static const Game* game = nullptr;     // nullptr = the launcher
static Difficulty diff = NORMAL;
Difficulty difficulty() { return diff; }
void setDifficulty(Difficulty d) { diff = d; if (game) saveInt(game->id, "diff", d); }
float speed() { return diff == EASY ? 0.7f : diff == HARD ? 1.25f : 1.0f; }
const char* difficultyName(Difficulty d) { return d == EASY ? "EASY" : d == HARD ? "HARD" : "NORMAL"; }
static const char* hiKey() { return diff == EASY ? "hiE" : diff == HARD ? "hiH" : "hi"; }
uint32_t loadHi(uint32_t def) { return game ? (uint32_t)loadInt(game->id, hiKey(), (int)def) : def; }
void saveHi(uint32_t v) { if (game) saveInt(game->id, hiKey(), (int)v); }
const Game* currentGame() { return game; }

bool titleInput(const Pad& in) {
  bool changed = false;
  if (in.hit(BTN_UP) && diff > EASY) { setDifficulty((Difficulty)(diff - 1)); changed = true; }
  if (in.hit(BTN_DOWN) && diff < HARD) { setDifficulty((Difficulty)(diff + 1)); changed = true; }
  if (changed) audio::sfx(audio::SFX_MOVE);
  if (in.hit(BTN_LEFT) && audio::volume() > 0) { audio::setVolume(audio::volume() - 1); audio::sfx(audio::SFX_MOVE); }
  if (in.hit(BTN_RIGHT) && audio::volume() < 10) { audio::setVolume(audio::volume() + 1); audio::sfx(audio::SFX_MOVE); }
  return changed;
}

static void volumeBar(float x, float y, float w, float h) {
  for (int i = 0; i < 10; i++) {
    float bw = (w - 9 * 6) / 10;
    bool lit = i < audio::volume();
    roundRect(x + i * (bw + 6), y + h * (1 - (i + 3) / 13.0f), bw, h * (i + 3) / 13.0f, 4, lit ? Color(255, 214, 90) : Color(255, 255, 255, 60));
  }
}

void drawTitleOptions(float y) {
  TextStyle st; st.size = 34; st.align = CENTER; st.color = Color(200, 214, 240);
  // difficulty: three pills
  for (int i = 0; i < 3; i++) {
    float x = W / 2 - 330 + i * 220;
    bool on = i == diff;
    roundRect(x - 95, y, 190, 60, 30, on ? Color(255, 214, 90) : Color(20, 20, 50, 150));
    TextStyle s2 = st; s2.color = on ? Color(40, 30, 20) : Color(220, 228, 250); s2.outline = CLEAR; s2.size = 30;
    text(difficultyName((Difficulty)i), x, y + 18, s2);
  }
  TextStyle hint = st; hint.size = 24; hint.color = Color(190, 200, 230); hint.outline = CLEAR;
  text("UP / DOWN: DIFFICULTY", W / 2 - 300, y + 88, hint);
  text("LEFT / RIGHT: VOLUME", W / 2 + 160, y + 88, hint);
  volumeBar(W / 2 + 300, y + 78, 150, 34);
}

const char* btnName(uint32_t b) {
  bool gp = pad.usingGamepad;
  switch (b) {
    case BTN_A: return gp ? "A" : "Z";
    case BTN_B: return gp ? "B" : "X";
    case BTN_X: return gp ? "X" : "C";
    case BTN_Y: return gp ? "Y" : "V";
    case BTN_START: return gp ? "START" : "ENTER";
    case BTN_SELECT: return gp ? "SELECT" : "TAB";
    case BTN_HOME: return gp ? "HOME" : "ESC";
    default: return "?";
  }
}

// ------------------------------------------------------------ the pause menu
static bool paused = false;
static int pauseSel = 0;
static int pauseT = 0;
static Repeat pauseRep[2];
enum { PI_RESUME, PI_VOLUME, PI_SCREEN, PI_QUIT, PI_COUNT };

void pause() {
  if (paused) return;
  paused = true; pauseSel = 0; pauseT = 0;
  audio::duck(true);
  audio::sfx(audio::SFX_PAUSE);
}
bool isPaused() { return paused; }

static void unpause() { paused = false; audio::duck(false); }

void exitToLauncher() {
  unpause();
  audio::music(nullptr);
  game = nullptr;
  audio::sfx(audio::SFX_BACK);
}

static void pauseStep(const Pad& in) {
  pauseT++;
  int items = PI_COUNT;
  if (pauseRep[0](in.down(BTN_UP))) { pauseSel = (pauseSel + items - 1) % items; audio::sfx(audio::SFX_MOVE); }
  if (pauseRep[1](in.down(BTN_DOWN))) { pauseSel = (pauseSel + 1) % items; audio::sfx(audio::SFX_MOVE); }
  if (pauseSel == PI_VOLUME) {
    if (in.hit(BTN_LEFT) && audio::volume() > 0) { audio::setVolume(audio::volume() - 1); audio::sfx(audio::SFX_MOVE); }
    if (in.hit(BTN_RIGHT) && audio::volume() < 10) { audio::setVolume(audio::volume() + 1); audio::sfx(audio::SFX_MOVE); }
  }
  if (pauseT > 2 && in.hit(BTN_START | BTN_B | BTN_HOME)) { unpause(); audio::sfx(audio::SFX_BACK); return; }
  if (in.hit(BTN_A)) {
    switch (pauseSel) {
      case PI_RESUME: unpause(); audio::sfx(audio::SFX_SELECT); break;
      case PI_SCREEN: toggleFullscreen(); audio::sfx(audio::SFX_SELECT); break;
      case PI_QUIT: exitToLauncher(); break;
      default: break;
    }
  }
}

static void pauseDraw() {
  float t = easeOut(pauseT / 12.0f);
  rect(0, 0, W, H, Color(8, 6, 24, (uint8_t)(150 * t)));
  float pw = 760, ph = 560, px = (W - pw) / 2, py = (H - ph) / 2 + (1 - t) * 40;
  roundRect(px + 10, py + 14, pw, ph, 36, Color(0, 0, 0, 90));
  roundRect(px, py, pw, ph, 36, Color(30, 26, 64, 245));
  roundRect(px + 6, py + 6, pw - 12, 110, 30, Color(255, 255, 255, 18));
  TextStyle ts; ts.size = 72; ts.align = CENTER; ts.color = Color(255, 226, 120); ts.shadow = 4;
  text("PAUSED", W / 2, py + 26, ts);
  static const char* names[PI_COUNT] = {"RESUME", "VOLUME", "FULL SCREEN", "QUIT TO MENU"};
  for (int i = 0; i < PI_COUNT; i++) {
    float y = py + 160 + i * 92;
    bool sel = i == pauseSel;
    if (sel) {
      float pulse = 0.5f + 0.5f * sinf(frameNo * 0.12f);
      roundRect(px + 50, y - 12, pw - 100, 74, 37, Color(255, 214, 90, (uint8_t)(200 + 55 * pulse)));
    }
    TextStyle s; s.size = 44; s.outline = CLEAR; s.color = sel ? Color(40, 28, 20) : Color(225, 230, 255);
    const char* label = names[i];
    if (i == PI_SCREEN) label = isFullscreen() ? "WINDOW" : "FULL SCREEN";
    if (i == PI_VOLUME) {
      s.align = LEFT;
      text(label, px + 96, y + 2, s);
      volumeBar(px + pw - 340, y - 2, 250, 52);
    } else {
      s.align = CENTER;
      text(label, W / 2, y + 2, s);
    }
  }
  TextStyle hint; hint.size = 24; hint.align = CENTER; hint.color = Color(170, 176, 220); hint.outline = CLEAR;
  textf(W / 2, py + ph + 30, hint, "%s: SELECT     %s / %s: RESUME", btnName(BTN_A), btnName(BTN_START), btnName(BTN_B));
}

// ------------------------------------------------------------ the launcher
static int sel = 0;
static Repeat launchRep[2];
static float selX = 0;

static void startGame(const Game* g) {
  game = g;
  diff = (Difficulty)clampv(loadInt(g->id, "diff", NORMAL), 0, 2);
  for (int i = 0; i < NGAMES; i++) if (GAMES[i] == g) saveInt("arcade", "last", i);
  audio::sfx(audio::SFX_START);
  if (g->enter) g->enter();
}

static void launcherStep(const Pad& in) {
  if (launchRep[0](in.down(BTN_LEFT)) && sel > 0) { sel--; audio::sfx(audio::SFX_MOVE); }
  if (launchRep[1](in.down(BTN_RIGHT)) && sel < NGAMES - 1) { sel++; audio::sfx(audio::SFX_MOVE); }
  if (in.hit(BTN_A | BTN_START)) startGame(GAMES[sel]);
}

static void launcherDraw() {
  // a slow aurora over deep blue
  rectGrad(0, 0, W, H, Color(18, 14, 52), Color(46, 22, 76));
  for (int i = 0; i < 5; i++) {
    float ph = frameNo * 0.004f + i * 1.3f;
    glow(W * (0.15f + 0.18f * i) + 120 * sinf(ph), 300 + 90 * cosf(ph * 1.3f), 520, Color(80 + 30 * i, 120, 255 - 30 * i, 60));
  }
  for (int i = 0; i < 90; i++) {   // stars
    uint32_t h = i * 2654435761u;
    float x = (h % 1920), y = ((h >> 11) % 700);
    float tw = 0.5f + 0.5f * sinf(frameNo * 0.05f + i);
    disc(x, y, 1.5f + (h % 3), Color(255, 255, 255, (uint8_t)(80 + 150 * tw)));
  }
  TextStyle ts; ts.size = 120; ts.align = CENTER; ts.color = Color(255, 226, 120); ts.shadow = 6; ts.outline = Color(40, 20, 70);
  text("NOVA ARCADE", W / 2, 70, ts);
  TextStyle sub; sub.size = 40; sub.align = CENTER; sub.color = Color(150, 220, 255); sub.outline = Color(30, 20, 60);
  text("HD", W / 2, 210, sub);
  // cards
  float cw = 640, chh = 480, gap = 80;
  selX = approach(selX, (float)sel, 0.18f);
  for (int i = 0; i < NGAMES; i++) {
    const Game* g = GAMES[i];
    float x = W / 2 + (i - selX) * (cw + gap) - cw / 2, y = 320;
    bool on = i == sel;
    float lift = on ? 10 * sinf(frameNo * 0.06f) : 0;
    y -= lift;
    roundRect(x + 12, y + 18, cw, chh, 30, Color(0, 0, 0, 90));
    roundRect(x - 8, y - 8, cw + 16, chh + 16, 36, on ? Color(255, 214, 90) : Color(255, 255, 255, 40));
    roundRect(x, y, cw, chh, 30, Color(26, 22, 56));
    if (g->thumb) drawRect(*g->thumb, x + 16, y + 16, cw - 32, (cw - 32) * 9 / 16.0f);
    TextStyle n; n.size = 52; n.align = CENTER; n.color = WHITE;
    text(g->title, x + cw / 2, y + 16 + (cw - 32) * 9 / 16.0f + 24, n);
    TextStyle tg; tg.size = 26; tg.align = CENTER; tg.color = Color(180, 196, 240); tg.outline = CLEAR;
    text(g->tagline, x + cw / 2, y + 16 + (cw - 32) * 9 / 16.0f + 90, tg);
  }
  TextStyle hint; hint.size = 30; hint.align = CENTER; hint.color = Color(200, 206, 240); hint.outline = CLEAR;
  if ((frameNo / 30) & 1) textf(W / 2, 860, hint, "PRESS %s TO PLAY", btnName(BTN_A));
  TextStyle keys = hint; keys.size = 24; keys.color = Color(150, 156, 200);
  text("KEYS: ARROWS MOVE   Z JUMP / OK   X ACTION   ENTER PAUSE   F11 FULL SCREEN", W / 2, 960, keys);
  text("OR PLUG IN ANY GAMEPAD", W / 2, 1000, keys);
}

// ------------------------------------------------------------ the frame
void appInit(const char* startGameId) {
  sel = clampv(loadInt("arcade", "last", 0), 0, NGAMES - 1);
  selX = (float)sel;
  for (int i = 0; i < NGAMES; i++) if (GAMES[i]->init) GAMES[i]->init();
  if (startGameId)
    for (int i = 0; i < NGAMES; i++)
      if (!strcmp(GAMES[i]->id, startGameId)) { sel = i; startGame(GAMES[i]); }
}

void appStep() {
  frameNo++;
  if (paused) { pauseStep(pad); return; }
  if (!game) { launcherStep(pad); return; }
  if (pad.hit(BTN_HOME) && (!game->paused || game->paused())) { pause(); return; }
  game->step(pad);
}

void appDraw() {
  if (!game) { launcherDraw(); return; }
  game->draw();
  if (paused) pauseDraw();
}

}  // namespace nova
