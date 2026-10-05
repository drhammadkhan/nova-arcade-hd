// The shared system layer: settings, difficulty, hi-scores, the pause menu and the launcher.
#include "nova.h"
#include "audio.h"
#include "common_art.h"
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
static const int MAX_GAMES = 32;
static int thumbTex[MAX_GAMES];
static Img thumbs[MAX_GAMES];
static int sel = 0;
static Repeat launchRep[2];

static void startGame(const Game* g) {
  game = g;
  diff = (Difficulty)clampv(loadInt(g->id, "diff", NORMAL), 0, 2);
  for (int i = 0; i < NGAMES; i++) if (GAMES[i] == g) saveInt("arcade", "last", i);
  audio::sfx(audio::SFX_START);
  if (g->enter) g->enter();
}

// a grid of cards, four across; the view scrolls to keep the chosen row in sight
static const int GRID_COLS = 4;
static const float CARD_W = 400, CARD_H = 300, GAP_X = 40, ROW_H = 340, GRID_Y = 210;
static float scrollY = 0;
static Repeat launchRep2[2];

static void launcherStep(const Pad& in) {
  int before = sel;
  if (launchRep[0](in.down(BTN_LEFT)) && sel > 0) sel--;
  if (launchRep[1](in.down(BTN_RIGHT)) && sel < NGAMES - 1) sel++;
  if (launchRep2[0](in.down(BTN_UP)) && sel >= GRID_COLS) sel -= GRID_COLS;
  if (launchRep2[1](in.down(BTN_DOWN)) && sel + GRID_COLS < NGAMES) sel += GRID_COLS;
  if (sel != before) audio::sfx(audio::SFX_MOVE);
  if (in.hit(BTN_A | BTN_START)) startGame(GAMES[sel]);
  int rows = (NGAMES + GRID_COLS - 1) / GRID_COLS;
  float maxScroll = fmaxf(0, rows * ROW_H - (H - GRID_Y - 200));
  float want = clampv((sel / GRID_COLS) * ROW_H - ROW_H * 0.6f, 0.0f, maxScroll);
  scrollY = approach(scrollY, want, 0.15f);
}

static void launcherDraw() {
  // a slow aurora over deep blue, with stars
  rectGrad(0, 0, W, H, Color(18, 14, 52), Color(46, 22, 76));
  for (int i = 0; i < 5; i++) {
    float ph = frameNo * 0.004f + i * 1.3f;
    glow(W * (0.15f + 0.18f * i) + 120 * sinf(ph), 300 + 90 * cosf(ph * 1.3f), 520, Color(80 + 30 * i, 120, 255 - 30 * i, 60));
  }
  for (int i = 0; i < 90; i++) {
    uint32_t h = i * 2654435761u;
    float x = (float)(h % 1920), y = (float)((h >> 11) % 1080);
    float tw = 0.5f + 0.5f * sinf(frameNo * 0.05f + i);
    disc(x, y, 1.5f + (h % 3), Color(255, 255, 255, (uint8_t)(60 + 120 * tw)));
  }
  float gx = (W - (GRID_COLS * CARD_W + (GRID_COLS - 1) * GAP_X)) / 2;
  float thumbW = CARD_W - 24, thumbH = thumbW * 9 / 16.0f;
  for (int pass = 0; pass < 2; pass++)   // the chosen card last, so it sits on top
    for (int i = 0; i < NGAMES; i++) {
      bool on = i == sel;
      if (on != (pass == 1)) continue;
      const Game* g = GAMES[i];
      float x = gx + (i % GRID_COLS) * (CARD_W + GAP_X), y = GRID_Y + (i / GRID_COLS) * ROW_H - scrollY;
      if (y > H || y + CARD_H < 120) continue;
      float k = on ? 1.06f + 0.01f * sinf(frameNo * 0.08f) : 1.0f;
      float w = CARD_W * k, h = CARD_H * k;
      x -= (w - CARD_W) / 2; y -= (h - CARD_H) / 2;
      if (on) glow(x + w / 2, y + h / 2, w * 0.8f, Color(255, 210, 100, 70));
      roundRect(x + 10, y + 16, w, h, 26, Color(0, 0, 0, 90));
      roundRect(x - 6, y - 6, w + 12, h + 12, 30, on ? Color(255, 214, 90) : Color(255, 255, 255, 36));
      roundRect(x, y, w, h, 24, Color(26, 22, 56));
      const Img* th = g->thumb ? g->thumb : (i < MAX_GAMES && thumbTex[i] >= 0 ? &thumbs[i] : nullptr);
      float tx = x + 12 * k, ty = y + 12 * k;
      if (th) drawRect(*th, tx, ty, thumbW * k, thumbH * k, on ? WHITE : Color(200, 200, 215));
      else roundRect(tx, ty, thumbW * k, thumbH * k, 14, Color(50, 40, 100));
      TextStyle n; n.size = 40 * k; n.align = CENTER; n.color = on ? WHITE : Color(215, 220, 245);
      text(g->title, x + w / 2, ty + thumbH * k + 22 * k, n);
    }
  // the header, over anything scrolled up under it
  rect(0, 0, W, 150, Color(18, 14, 52));
  rectGrad(0, 150, W, 50, Color(18, 14, 52, 255), Color(18, 14, 52, 0));
  TextStyle ts; ts.size = 96; ts.align = CENTER; ts.color = Color(255, 226, 120); ts.shadow = 6; ts.outline = Color(40, 20, 70);
  text("NOVA ARCADE", W / 2 - 50, 40, ts);
  TextStyle hd; hd.size = 56; hd.color = Color(150, 220, 255); hd.outline = Color(30, 20, 60);
  text("HD", W / 2 - 50 + textWidth("NOVA ARCADE", 96) / 2 + 24, 60, hd);
  // the footer: what the chosen game is, and the keys
  rectGrad(0, H - 230, W, 60, Color(40, 20, 70, 0), Color(40, 20, 70, 255));
  rect(0, H - 170, W, 170, Color(40, 20, 70));
  TextStyle tag; tag.size = 34; tag.align = CENTER; tag.color = Color(200, 214, 255);
  textf(W / 2, H - 140, tag, "%s  -  %s", GAMES[sel]->title, GAMES[sel]->tagline);
  TextStyle hint; hint.size = 30; hint.align = CENTER; hint.color = Color(255, 220, 120); hint.outline = CLEAR;
  if ((frameNo / 30) & 1) textf(W / 2, H - 90, hint, "PRESS %s TO PLAY", btnName(BTN_A));
  TextStyle keys; keys.size = 22; keys.align = CENTER; keys.color = Color(150, 156, 200); keys.outline = CLEAR;
  text("ARROWS / D-PAD CHOOSE    Z OR A PLAY    ESC PAUSE IN GAME    F11 FULL SCREEN    ANY GAMEPAD WORKS", W / 2, H - 44, keys);
}

// ------------------------------------------------------------ the frame
void appInit(const char* startGameId) {
  sel = clampv(loadInt("arcade", "last", 0), 0, NGAMES - 1);
  loadAtlas(cart::TEX_FILES, cart::TEX, cart::NTEX);
  for (int i = 0; i < NGAMES && i < MAX_GAMES; i++) {
    if (GAMES[i]->init) GAMES[i]->init();
    char path[96];
    snprintf(path, sizeof(path), "%s/thumb.png", GAMES[i]->id);
    thumbTex[i] = GAMES[i]->thumb ? -1 : loadTexture(path);
    thumbs[i] = {&thumbTex[i], 0, 0, 960, 540, 0, 0, 1};
  }
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
