#include "headless.h"
#include "audio.h"
#include <SDL3/SDL.h>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace nova {
void toggleFullscreen() {}
bool isFullscreen() { return false; }
void rumble(float, int) {}
}  // namespace nova

namespace nova::headless {

static SDL_Surface* surface = nullptr;
static SDL_Renderer* renderer = nullptr;
static uint32_t prevHeld = 0;

bool begin(bool withRenderer, const char* startGame) {
  if (!SDL_Init(0)) return false;
  if (withRenderer) {
    surface = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_RGBA32);
    renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) { SDL_Log("software renderer: %s", SDL_GetError()); return false; }
    setRendererInternal(renderer);
  }
  audio::init(false);
  seed(12345);
  appInit(startGame);
  return true;
}

void step(const Pad& p) {
  pad.ax = p.ax; pad.ay = p.ay; pad.held = p.held;
  pad.pressed = p.held & ~prevHeld;
  pad.released = prevHeld & ~p.held;
  pad.usingGamepad = p.usingGamepad;
  prevHeld = p.held;
  appStep();
}

bool shot(const char* path, int scaleDown) {
  if (!renderer) return false;
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  appDraw();
  SDL_RenderPresent(renderer);
  int w = W / scaleDown, h = H / scaleDown;
  std::vector<uint8_t> out(w * h * 3);
  const uint8_t* px = (const uint8_t*)surface->pixels;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int acc[3] = {0, 0, 0};
      for (int dy = 0; dy < scaleDown; dy++)
        for (int dx = 0; dx < scaleDown; dx++) {
          const uint8_t* p = px + (y * scaleDown + dy) * surface->pitch + (x * scaleDown + dx) * 4;
          for (int c = 0; c < 3; c++) acc[c] += p[c];
        }
      for (int c = 0; c < 3; c++) out[(y * w + x) * 3 + c] = (uint8_t)(acc[c] / (scaleDown * scaleDown));
    }
  return stbi_write_png(path, w, h, 3, out.data(), w * 3) != 0;
}

void end() {
  if (renderer) SDL_DestroyRenderer(renderer);
  if (surface) SDL_DestroySurface(surface);
  SDL_Quit();
}

}  // namespace nova::headless
