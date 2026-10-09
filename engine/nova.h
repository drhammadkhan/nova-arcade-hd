// =====================================================================
//  NOVA ARCADE HD - engine
//  Every game draws a fixed 1920x1080 frame. The window scales it to fit
//  (letterboxed), so game code never thinks about window or Retina size.
//  Logic runs at a fixed 60 steps per second.
//
//  Backends: platform.cpp (SDL3 window, Mac / Linux / browser) and the
//  headless runner used by tests (software renderer, no window, no sound).
// =====================================================================
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

namespace nova {

constexpr int W = 1920, H = 1080;
constexpr int FPS = 60;

// ------------------------------------------------------------ maths helpers
template <class T> static inline T clampv(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float approach(float v, float target, float rate) { return v + (target - v) * rate; }
uint32_t rnd();                                   // xorshift, seeded once per run
static inline float frand() { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static inline float frange(float a, float b) { return a + (b - a) * frand(); }
void seed(uint32_t s);
static inline bool overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
static inline float easeOut(float t) { t = clampv(t, 0.0f, 1.0f); return 1 - (1 - t) * (1 - t) * (1 - t); }
static inline float easeInOut(float t) { t = clampv(t, 0.0f, 1.0f); return t * t * (3 - 2 * t); }

// ------------------------------------------------------------ input
enum Btn : uint32_t {
  BTN_UP = 1 << 0, BTN_DOWN = 1 << 1, BTN_LEFT = 1 << 2, BTN_RIGHT = 1 << 3,
  BTN_A = 1 << 4,      // south face button / Z / Space   (jump, confirm)
  BTN_B = 1 << 5,      // east / X                         (action, back in menus)
  BTN_X = 1 << 6,      // west / C
  BTN_Y = 1 << 7,      // north / V
  BTN_L = 1 << 8, BTN_R = 1 << 9,
  BTN_START = 1 << 10,  // Enter / Menu button             (pause)
  BTN_SELECT = 1 << 11, // Tab / View button
  BTN_HOME = 1 << 12,   // Esc / Guide button              (pause, or back to the launcher)
};
struct Pad {
  float ax = 0, ay = 0;          // left stick (or arrows), -1..1
  uint32_t held = 0, pressed = 0, released = 0;
  bool usingGamepad = false;     // last input came from a pad (affects button hints)
  bool down(uint32_t b) const { return held & b; }
  bool hit(uint32_t b) const { return pressed & b; }
};
extern Pad pad;
void rumble(float strength, int ms);

// Auto-repeat for menu navigation: true on the press, then every `rate` frames after `delay`.
struct Repeat {
  int t = 0;
  bool operator()(bool held, int delay = 18, int rate = 6) {
    if (!held) { t = 0; return false; }
    t++;
    return t == 1 || (t > delay && (t - delay) % rate == 0);
  }
};

// ------------------------------------------------------------ graphics
struct Color {
  uint8_t r = 255, g = 255, b = 255, a = 255;
  constexpr Color() = default;
  constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
  constexpr Color alpha(float f) const { return Color(r, g, b, (uint8_t)(a * (f < 0 ? 0 : f > 1 ? 1 : f))); }
};
static inline Color mix(Color a, Color b, float t) {
  return Color((uint8_t)lerp(a.r, b.r, t), (uint8_t)lerp(a.g, b.g, t), (uint8_t)lerp(a.b, b.b, t), (uint8_t)lerp(a.a, b.a, t));
}
constexpr Color WHITE(255, 255, 255), BLACK(0, 0, 0), CLEAR(0, 0, 0, 0);

// A picture in a texture atlas. Atlases are drawn at 2x density, so `scale` is 0.5:
// the size on screen is w*scale x h*scale, and (px, py) is the pivot in atlas pixels.
struct Img {
  int* tex;              // slot holding the loaded texture id (filled by loadAtlas)
  float x, y, w, h;      // source rect in the atlas page
  float px, py;          // pivot, relative to the rect
  float scale;           // atlas pixels -> screen pixels
  float sw() const { return w * scale; }
  float sh() const { return h * scale; }
};

enum Blend : uint8_t { BLEND_ALPHA, BLEND_ADD, BLEND_MUL };

struct Fx {
  float sx = 1, sy = 1;      // scale (negative flips)
  float rot = 0;             // degrees clockwise about the pivot, on screen (after any flip)
  Color tint = WHITE;        // multiplies the colours; tint.a is opacity
  Blend blend = BLEND_ALPHA;
  bool flip = false;         // mirror horizontally about the pivot
};

// Load a texture from the assets folder. Returns an id (>= 0), or -1 if it failed.
int loadTexture(const char* path);
// Load every page of a generated atlas into its slots.
void loadAtlas(const char* const* pages, int* slots, int n);

void clear(Color c);
// draw an image with its pivot at (x, y)
void draw(const Img& img, float x, float y, const Fx& fx = Fx());
// draw a texture region stretched to a rectangle (no pivot)
void drawRect(const Img& img, float x, float y, float w, float h, Color tint = WHITE, Blend b = BLEND_ALPHA);
// tile a texture region horizontally across the screen, starting at offset x (parallax layers)
void drawStrip(const Img& img, float x, float y, float scale = 1, Color tint = WHITE);
void rect(float x, float y, float w, float h, Color c, Blend b = BLEND_ALPHA);
void rectGrad(float x, float y, float w, float h, Color top, Color bottom, Blend b = BLEND_ALPHA);
void roundRect(float x, float y, float w, float h, float r, Color c);
void disc(float x, float y, float r, Color c, Blend b = BLEND_ALPHA);
void ring(float x, float y, float r, float thick, Color c, Blend b = BLEND_ALPHA);
void line(float x0, float y0, float x1, float y1, float thick, Color c, Blend b = BLEND_ALPHA);
// soft radial glow (additive by default): lanterns, sparkles, the sun
void glow(float x, float y, float r, Color c, Blend b = BLEND_ADD);
// raw triangles: n vertices, every three make a triangle
struct Vtx { float x, y; Color c; };
void tris(const Vtx* v, int n, Blend b = BLEND_ALPHA);
// clip everything drawn after this to a rectangle, until noClip()
void clip(float x, float y, float w, float h);
void noClip();

// ------------------------------------------------------------ text (Fredoka, baked at 2x)
enum Align : uint8_t { LEFT = 0, CENTER = 1, RIGHT = 2 };
struct TextStyle {
  float size = 40;                   // cap-to-baseline-ish height in pixels (the font's em size)
  Color color = WHITE;
  Color outline = Color(20, 16, 40);  // a = 0 for none
  Align align = LEFT;
  float shadow = 0;                   // drop shadow offset (0 for none)
};
// y is the top of the line
void text(const char* s, float x, float y, const TextStyle& st);
void textf(float x, float y, const TextStyle& st, const char* fmt, ...);
float textWidth(const char* s, float size);

// ------------------------------------------------------------ saved settings
int loadInt(const char* ns, const char* key, int def);
void saveInt(const char* ns, const char* key, int v);

// ------------------------------------------------------------ audio (audio.cpp)
namespace audio {
enum Sfx : uint8_t {
  SFX_JUMP, SFX_LAND, SFX_COIN, SFX_STOMP, SFX_ROLL, SFX_HURT, SFX_DIE, SFX_POWERUP,
  SFX_BUMP, SFX_SCROLL, SFX_CHECKPOINT, SFX_SPLASH, SFX_KNOCK, SFX_ONEUP,
  SFX_MOVE, SFX_SELECT, SFX_BACK, SFX_START, SFX_PAUSE,
  SFX_SHOOT,      // a laser shot (pitch sets the tone)
  SFX_EXPLODE,    // an explosion (pitch < 1 for bigger)
  SFX_MARCH,      // a low march beat (pitch picks the note)
  SFX_COUNT
};
void sfx(Sfx s, float pan = 0, float pitch = 1);
struct Song;
void music(const Song* s);           // nullptr = silence
const Song* currentMusic();
void setVolume(int v);               // 0..10
int volume();
void duck(bool on);                  // quieter music while paused
// set while bots look ahead, so simulated futures stay silent
extern int mute;
}  // namespace audio

// ------------------------------------------------------------ the shared system layer
// Difficulty, hi-scores, pause menu, volume: the same rules every game follows.
enum Difficulty : uint8_t { EASY, NORMAL, HARD };
Difficulty difficulty();
void setDifficulty(Difficulty d);
float speed();                       // hazard speed scale: 0.7 / 1.0 / 1.25
static inline int frames(int n) { int f = (int)(n / speed() + 0.5f); return f < 1 ? 1 : f; }   // a delay, scaled
const char* difficultyName(Difficulty d);
uint32_t loadHi(uint32_t def);       // hi-score for the current game + difficulty
void saveHi(uint32_t v);
bool titleInput(const Pad& in);      // title screen: Up/Down difficulty, Left/Right volume. true if changed
void drawTitleOptions(float y);      // difficulty + volume row for title screens
// button names for hints, matched to the device being used ("Z" vs "A")
const char* btnName(uint32_t b);

struct Game {
  const char* id;                    // save namespace
  const char* title;
  const char* tagline;
  void (*init)();                    // load assets (once)
  void (*enter)();                   // every time the game is started from the launcher
  void (*step)(const Pad& in);       // one 1/60 s tick
  void (*draw)();                    // one frame
  bool (*paused)();                  // optional: false = this game doesn't want the pause menu right now
  const Img* thumb;                  // launcher card picture (optional: else assets/<id>/thumb.png)
  void (*bot)(Pad& out);             // optional autopilot: tests (and attract modes) play with it
  void (*debug)(char* buf, int n);   // optional one-line status for tests (score, level...)
};
void pause();                        // open the pause menu (games call this on START)
void toggleFullscreen();             // platform: window <-> full screen
bool isFullscreen();
bool isPaused();
void exitToLauncher();
const Game* currentGame();
extern uint32_t frameNo;             // counts every step, from start-up

// all games built into this binary (games.cpp)
extern const Game* const GAMES[];
extern const int NGAMES;

// ------------------------------------------------------------ platform glue (shared by every backend)
void appInit(const char* startGame);  // after the renderer exists
void appStep();                        // one logic tick
void appDraw();
void setRendererInternal(void* sdlRenderer);
void flushDraw();
const char* assetPath(const char* rel); // path inside the assets folder
}  // namespace nova
