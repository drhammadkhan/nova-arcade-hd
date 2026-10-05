// The real app: an SDL3 window (Mac, Linux, Windows) or a canvas (browser), keyboard and gamepads,
// a fixed 60 Hz logic step, and drawing once per display refresh.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "nova.h"
#include "audio.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, jsInnerW, (), { return window.innerWidth | 0; });
EM_JS(int, jsInnerH, (), { return window.innerHeight | 0; });
EM_JS(const char*, jsGameParam, (), {
  var m = /[?&]game=([a-z0-9]+)/.exec(location.search);
  return m ? stringToNewUTF8(m[1]) : 0;
});
#endif

namespace nova {

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static uint32_t kbHeld = 0, kbLatch = 0, gpHeld = 0, gpLatch = 0, prevHeld = 0;
static SDL_Gamepad* gamepads[8] = {};
static float stickX = 0, stickY = 0;
static uint64_t lastNs = 0;
static double acc = 0;
#ifdef __EMSCRIPTEN__
static int webW = 0, webH = 0;
#endif

void toggleFullscreen() {
  if (window) SDL_SetWindowFullscreen(window, !isFullscreen());
}
bool isFullscreen() { return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN); }

void rumble(float strength, int ms) {
  uint16_t v = (uint16_t)(clampv(strength, 0.0f, 1.0f) * 0xFFFF);
  for (auto* g : gamepads) if (g) SDL_RumbleGamepad(g, v, v, ms);
}

static uint32_t keyBit(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: case SDLK_W: return BTN_UP;
    case SDLK_DOWN: case SDLK_S: return BTN_DOWN;
    case SDLK_LEFT: case SDLK_A: return BTN_LEFT;
    case SDLK_RIGHT: case SDLK_D: return BTN_RIGHT;
    case SDLK_Z: case SDLK_SPACE: case SDLK_K: return BTN_A;
    case SDLK_X: case SDLK_J: case SDLK_LSHIFT: return BTN_B;
    case SDLK_C: case SDLK_L: return BTN_X;
    case SDLK_V: case SDLK_I: return BTN_Y;
    case SDLK_Q: return BTN_L;
    case SDLK_E: return BTN_R;
    case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_P: return BTN_START;
    case SDLK_TAB: case SDLK_BACKSPACE: return BTN_SELECT;
    case SDLK_ESCAPE: return BTN_HOME;
    default: return 0;
  }
}

static uint32_t padBit(int b) {
  switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return BTN_A;
    case SDL_GAMEPAD_BUTTON_EAST: return BTN_B;
    case SDL_GAMEPAD_BUTTON_WEST: return BTN_X;
    case SDL_GAMEPAD_BUTTON_NORTH: return BTN_Y;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return BTN_L;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return BTN_R;
    case SDL_GAMEPAD_BUTTON_START: return BTN_START;
    case SDL_GAMEPAD_BUTTON_BACK: return BTN_SELECT;
    case SDL_GAMEPAD_BUTTON_GUIDE: return BTN_HOME;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return BTN_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return BTN_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return BTN_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    default: return 0;
  }
}

static void readSticks() {
  float x = 0, y = 0;
  for (auto* g : gamepads) {
    if (!g) continue;
    float gx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float gy = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    if (fabsf(gx) > fabsf(x)) x = gx;
    if (fabsf(gy) > fabsf(y)) y = gy;
  }
  const float DZ = 0.22f;
  auto dz = [&](float v) { return fabsf(v) < DZ ? 0.0f : (v - (v > 0 ? DZ : -DZ)) / (1 - DZ); };
  stickX = dz(x); stickY = dz(y);
}

static void buildPad() {
  readSticks();
  uint32_t stick = 0;
  if (stickX < -0.5f) stick |= BTN_LEFT;
  if (stickX > 0.5f) stick |= BTN_RIGHT;
  if (stickY < -0.5f) stick |= BTN_UP;
  if (stickY > 0.5f) stick |= BTN_DOWN;
  uint32_t held = kbHeld | gpHeld | stick;
  pad.pressed = (held & ~prevHeld) | kbLatch | gpLatch;
  pad.released = prevHeld & ~held;
  pad.held = held;
  float ax = stickX, ay = stickY;
  if (held & BTN_LEFT) ax = (stick & BTN_LEFT) ? ax : -1;
  if (held & BTN_RIGHT) ax = (stick & BTN_RIGHT) ? ax : 1;
  if (held & BTN_UP) ay = (stick & BTN_UP) ? ay : -1;
  if (held & BTN_DOWN) ay = (stick & BTN_DOWN) ? ay : 1;
  pad.ax = ax; pad.ay = ay;
  prevHeld = held;
  kbLatch = gpLatch = 0;
}

}  // namespace nova

using namespace nova;

SDL_AppResult SDL_AppInit(void**, int argc, char** argv) {
  SDL_SetAppMetadata("Nova Arcade HD", "0.1", "io.github.drhammadkhan.nova-arcade-hd");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  int w = 1600, h = 900;
#ifdef __EMSCRIPTEN__
  w = webW = jsInnerW(); h = webH = jsInnerH();
#else
  // start at about three quarters of the screen, 16:9
  SDL_DisplayID d = SDL_GetPrimaryDisplay();
  SDL_Rect ub;
  if (d && SDL_GetDisplayUsableBounds(d, &ub)) {
    w = (int)(ub.w * 0.78f);
    h = w * 9 / 16;
    if (h > ub.h * 0.85f) { h = (int)(ub.h * 0.85f); w = h * 16 / 9; }
  }
#endif
  if (!SDL_CreateWindowAndRenderer("Nova Arcade HD", w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
    SDL_Log("window failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetWindowMinimumSize(window, 480, 270);
  SDL_SetRenderLogicalPresentation(renderer, W, H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(renderer, 1);
  setRendererInternal(renderer);
  audio::init(true);
  const char* start = nullptr;
  for (int i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "--game")) start = argv[i + 1];
#ifdef __EMSCRIPTEN__
  if (!start) start = jsGameParam();
#endif
  if (argc > 1 && !strcmp(argv[argc - 1], "--fullscreen")) SDL_SetWindowFullscreen(window, true);
  seed((uint32_t)SDL_GetTicksNS() | 1);
  appInit(start);
  lastNs = SDL_GetTicksNS();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* e) {
  switch (e->type) {
    case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN: {
      SDL_Keycode k = e->key.key;
      bool alt = e->key.mod & SDL_KMOD_ALT, gui = e->key.mod & SDL_KMOD_GUI, ctrl = e->key.mod & SDL_KMOD_CTRL;
      if (k == SDLK_F11 || (alt && k == SDLK_RETURN) || (gui && ctrl && k == SDLK_F)) { if (!e->key.repeat) toggleFullscreen(); break; }
      if (gui && k == SDLK_Q) return SDL_APP_SUCCESS;
      if (gui || e->key.repeat) break;
      uint32_t b = keyBit(k);
      kbHeld |= b; kbLatch |= b;
      if (b) pad.usingGamepad = false;
      break;
    }
    case SDL_EVENT_KEY_UP: kbHeld &= ~keyBit(e->key.key); break;
    case SDL_EVENT_GAMEPAD_ADDED: {
      SDL_Gamepad* g = SDL_OpenGamepad(e->gdevice.which);
      for (auto*& slot : gamepads) if (!slot) { slot = g; break; }
      break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
      for (auto*& slot : gamepads)
        if (slot && SDL_GetGamepadID(slot) == e->gdevice.which) { SDL_CloseGamepad(slot); slot = nullptr; }
      gpHeld = 0;
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
      uint32_t b = padBit(e->gbutton.button);
      gpHeld |= b; gpLatch |= b; pad.usingGamepad = true;
      break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_UP: gpHeld &= ~padBit(e->gbutton.button); break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (abs(e->gaxis.value) > 16000) pad.usingGamepad = true;
      break;
    default: break;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void*) {
#ifdef __EMSCRIPTEN__
  int iw = jsInnerW(), ih = jsInnerH();
  if (iw != webW || ih != webH) { webW = iw; webH = ih; SDL_SetWindowSize(window, iw, ih); }
#endif
  uint64_t now = SDL_GetTicksNS();
  acc += (now - lastNs) / 1e9;
  lastNs = now;
  if (acc > 0.25) acc = 0.25;   // after a stall, don't try to catch up
  const double dt = 1.0 / FPS;
  int steps = 0;
  while (acc >= dt - 0.002 && steps < 4) {   // the small slack keeps 60 Hz displays at one step per frame
    buildPad();
    appStep();
    acc -= dt;
    steps++;
  }
  if (acc < 0) acc = 0;
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  appDraw();
  SDL_RenderPresent(renderer);
  static int cursor = -1;   // hide the pointer in full screen
  int want = isFullscreen() ? 0 : 1;
  if (want != cursor) { cursor = want; if (want) SDL_ShowCursor(); else SDL_HideCursor(); }
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void*, SDL_AppResult) {}
