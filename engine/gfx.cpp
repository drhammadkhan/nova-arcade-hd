// Drawing on top of SDL_Renderer (Metal on the Mac, WebGL in the browser, software in tests).
#include "nova.h"
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "font_data.h"

namespace nova {

static SDL_Renderer* R = nullptr;
static std::vector<SDL_Texture*> textures;
static SDL_Texture* discTex = nullptr;   // anti-aliased white disc, 256 px
static SDL_Texture* glowTex = nullptr;   // soft radial falloff, 128 px
static int fontTex = -1, fontTexOutline = -1;
static const int DISC = 256, GLOW = 128;

static uint32_t rngState = 0x9E3779B9u;
uint32_t rnd() { uint32_t x = rngState; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return rngState = x; }
void seed(uint32_t s) { rngState = s ? s : 1; }

static SDL_BlendMode sdlBlend(Blend b) {
  return b == BLEND_ADD ? SDL_BLENDMODE_ADD : b == BLEND_MUL ? SDL_BLENDMODE_MOD : SDL_BLENDMODE_BLEND;
}

static SDL_Texture* makeTex(int w, int h, const uint8_t* rgba) {
  SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, (void*)rgba, w * 4);
  if (!s) return nullptr;
  SDL_Texture* t = SDL_CreateTextureFromSurface(R, s);
  SDL_DestroySurface(s);
  if (t) { SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR); }
  return t;
}

static void makeBuiltins() {
  std::vector<uint8_t> px(DISC * DISC * 4);
  for (int y = 0; y < DISC; y++)
    for (int x = 0; x < DISC; x++) {
      float dx = x + 0.5f - DISC / 2.0f, dy = y + 0.5f - DISC / 2.0f;
      float d = sqrtf(dx * dx + dy * dy);
      float a = clampv(DISC / 2.0f - 1.0f - d + 0.5f, 0.0f, 1.0f);
      uint8_t* p = &px[(y * DISC + x) * 4];
      p[0] = p[1] = p[2] = 255; p[3] = (uint8_t)(a * 255);
    }
  discTex = makeTex(DISC, DISC, px.data());
  px.assign(GLOW * GLOW * 4, 0);
  for (int y = 0; y < GLOW; y++)
    for (int x = 0; x < GLOW; x++) {
      float dx = (x + 0.5f) / (GLOW / 2.0f) - 1, dy = (y + 0.5f) / (GLOW / 2.0f) - 1;
      float d = clampv(sqrtf(dx * dx + dy * dy), 0.0f, 1.0f);
      float a = (1 - d) * (1 - d);
      a = a * a * 0.6f + a * 0.4f;
      uint8_t* p = &px[(y * GLOW + x) * 4];
      p[0] = p[1] = p[2] = 255; p[3] = (uint8_t)(a * 255);
    }
  glowTex = makeTex(GLOW, GLOW, px.data());
}

void setRendererInternal(void* r) {
  R = (SDL_Renderer*)r;
  makeBuiltins();
  fontTex = loadTexture("font.png");
  fontTexOutline = loadTexture("font_outline.png");
}

int loadTexture(const char* rel) {
  if (!R) return -1;   // headless without a renderer (the bot): nothing to draw with
  const char* path = assetPath(rel);
  size_t size = 0;
  void* data = SDL_LoadFile(path, &size);
  if (!data) { SDL_Log("can't read %s: %s", path, SDL_GetError()); return -1; }
  int w, h, n;
  uint8_t* px = stbi_load_from_memory((const uint8_t*)data, (int)size, &w, &h, &n, 4);
  SDL_free(data);
  if (!px) { SDL_Log("can't decode %s", path); return -1; }
  SDL_Texture* t = makeTex(w, h, px);
  stbi_image_free(px);
  if (!t) { SDL_Log("can't make texture %s: %s", path, SDL_GetError()); return -1; }
  textures.push_back(t);
  return (int)textures.size() - 1;
}

void loadAtlas(const char* const* pages, int* slots, int n) {
  for (int i = 0; i < n; i++) slots[i] = loadTexture(pages[i]);
}

static SDL_Texture* texOf(const Img& img) {
  int id = img.tex ? *img.tex : -1;
  return id >= 0 && id < (int)textures.size() ? textures[id] : nullptr;
}

static void tintTex(SDL_Texture* t, Color c, Blend b) {
  SDL_SetTextureColorMod(t, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(t, c.a);
  SDL_SetTextureBlendMode(t, sdlBlend(b));
}

void clear(Color c) {
  SDL_SetRenderDrawColor(R, c.r, c.g, c.b, 255);
  SDL_RenderClear(R);
}

void draw(const Img& img, float x, float y, const Fx& fx) {
  SDL_Texture* t = texOf(img);
  if (!t || fx.tint.a == 0) return;
  float s = img.scale;
  float sx = fx.sx * s, sy = fx.sy * s;
  bool flipX = fx.flip != (sx < 0), flipY = sy < 0;
  sx = fabsf(sx); sy = fabsf(sy);
  float pivX = (flipX ? img.w - img.px : img.px) * sx;
  float pivY = (flipY ? img.h - img.py : img.py) * sy;
  SDL_FRect src = {img.x, img.y, img.w, img.h};
  SDL_FRect dst = {x - pivX, y - pivY, img.w * sx, img.h * sy};
  if (dst.x > W || dst.y > H || dst.x + dst.w < 0 || dst.y + dst.h < 0) {
    // a rotated sprite can swing back on screen; only cull the unrotated ones
    if (fx.rot == 0) return;
    float rr = (img.w * sx + img.h * sy);
    if (x + rr < 0 || x - rr > W || y + rr < 0 || y - rr > H) return;
  }
  tintTex(t, fx.tint, fx.blend);
  int flip = (flipX ? SDL_FLIP_HORIZONTAL : 0) | (flipY ? SDL_FLIP_VERTICAL : 0);
  // rot is the final on-screen rotation, applied after any flip
  if (fx.rot == 0 && !flip) { SDL_RenderTexture(R, t, &src, &dst); return; }
  SDL_FPoint c = {pivX, pivY};
  SDL_RenderTextureRotated(R, t, &src, &dst, fx.rot, &c, (SDL_FlipMode)flip);
}

void drawRect(const Img& img, float x, float y, float w, float h, Color tint, Blend b) {
  SDL_Texture* t = texOf(img);
  if (!t) return;
  tintTex(t, tint, b);
  SDL_FRect src = {img.x, img.y, img.w, img.h}, dst = {x, y, w, h};
  SDL_RenderTexture(R, t, &src, &dst);
}

void drawStrip(const Img& img, float x, float y, float scale, Color tint) {
  SDL_Texture* t = texOf(img);
  if (!t) return;
  tintTex(t, tint, BLEND_ALPHA);
  float w = img.w * img.scale * scale, h = img.h * img.scale * scale;
  // the strip's source is trimmed a hair at both ends so linear filtering doesn't bleed past the seam
  SDL_FRect src = {img.x + 0.5f, img.y, img.w - 1.0f, img.h};
  float start = fmodf(x, w);
  if (start > 0) start -= w;
  for (float xx = start; xx < W; xx += w) {
    SDL_FRect dst = {floorf(xx), y, ceilf(w) + 1, h};
    SDL_RenderTexture(R, t, &src, &dst);
  }
}

static SDL_FColor fc(Color c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; }

void rect(float x, float y, float w, float h, Color c, Blend b) {
  if (c.a == 0) return;
  SDL_SetRenderDrawBlendMode(R, sdlBlend(b));
  SDL_SetRenderDrawColor(R, c.r, c.g, c.b, c.a);
  SDL_FRect r = {x, y, w, h};
  SDL_RenderFillRect(R, &r);
}

void rectGrad(float x, float y, float w, float h, Color top, Color bottom, Blend b) {
  SDL_Vertex v[4];
  SDL_FColor ct = fc(top), cb = fc(bottom);
  v[0] = {{x, y}, ct, {0, 0}}; v[1] = {{x + w, y}, ct, {0, 0}};
  v[2] = {{x + w, y + h}, cb, {0, 0}}; v[3] = {{x, y + h}, cb, {0, 0}};
  int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_SetRenderDrawBlendMode(R, sdlBlend(b));
  SDL_RenderGeometry(R, nullptr, v, 4, idx, 6);
}

void tris(const Vtx* v, int n, Blend b) {
  static std::vector<SDL_Vertex> buf;
  buf.resize(n);
  for (int i = 0; i < n; i++) buf[i] = {{v[i].x, v[i].y}, fc(v[i].c), {0, 0}};
  SDL_SetRenderDrawBlendMode(R, sdlBlend(b));
  SDL_RenderGeometry(R, nullptr, buf.data(), n, nullptr, 0);
}

void disc(float x, float y, float r, Color c, Blend b) {
  if (!discTex || r <= 0 || c.a == 0) return;
  tintTex(discTex, c, b);
  SDL_FRect dst = {x - r, y - r, r * 2, r * 2};
  SDL_RenderTexture(R, discTex, nullptr, &dst);
}

void ring(float x, float y, float r, float thick, Color c, Blend b) {
  const int N = 48;
  Vtx v[N * 6];
  int k = 0;
  for (int i = 0; i < N; i++) {
    float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
    float r0 = r - thick / 2, r1 = r + thick / 2;
    Vtx p0 = {x + cosf(a0) * r0, y + sinf(a0) * r0, c}, p1 = {x + cosf(a0) * r1, y + sinf(a0) * r1, c};
    Vtx p2 = {x + cosf(a1) * r1, y + sinf(a1) * r1, c}, p3 = {x + cosf(a1) * r0, y + sinf(a1) * r0, c};
    v[k++] = p0; v[k++] = p1; v[k++] = p2; v[k++] = p0; v[k++] = p2; v[k++] = p3;
  }
  tris(v, k, b);
}

void line(float x0, float y0, float x1, float y1, float thick, Color c, Blend b) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  if (l < 0.001f) return;
  float nx = -dy / l * thick / 2, ny = dx / l * thick / 2;
  Vtx v[6] = {{x0 + nx, y0 + ny, c}, {x1 + nx, y1 + ny, c}, {x1 - nx, y1 - ny, c},
              {x0 + nx, y0 + ny, c}, {x1 - nx, y1 - ny, c}, {x0 - nx, y0 - ny, c}};
  tris(v, 6, b);
}

// A rounded rectangle from triangles, with a one-pixel soft fringe so the edges are anti-aliased.
void roundRect(float x, float y, float w, float h, float r, Color c) {
  if (c.a == 0 || w <= 0 || h <= 0) return;
  r = fminf(r, fminf(w, h) / 2);
  const int SEG = 10;                      // segments per corner
  const int N = SEG * 4;
  static SDL_FPoint in[N], out[N];
  float cx[4] = {x + w - r, x + r, x + r, x + w - r}, cy[4] = {y + h - r, y + h - r, y + r, y + r};
  for (int k = 0; k < 4; k++)
    for (int i = 0; i < SEG; i++) {
      float a = (k * 90 + i * 90.0f / (SEG - 1)) * 0.0174533f;
      float ca = cosf(a), sa = sinf(a);
      in[k * SEG + i] = {cx[k] + ca * fmaxf(r - 0.6f, 0), cy[k] + sa * fmaxf(r - 0.6f, 0)};
      out[k * SEG + i] = {cx[k] + ca * (r + 0.6f), cy[k] + sa * (r + 0.6f)};
    }
  SDL_FColor solid = fc(c), clearC = solid;
  clearC.a = 0;
  static SDL_Vertex v[N * 9];
  int n = 0;
  SDL_FPoint mid = {x + w / 2, y + h / 2};
  for (int i = 0; i < N; i++) {
    int j = (i + 1) % N;
    v[n++] = {mid, solid, {0, 0}}; v[n++] = {in[i], solid, {0, 0}}; v[n++] = {in[j], solid, {0, 0}};
    v[n++] = {in[i], solid, {0, 0}}; v[n++] = {out[i], clearC, {0, 0}}; v[n++] = {out[j], clearC, {0, 0}};
    v[n++] = {in[i], solid, {0, 0}}; v[n++] = {out[j], clearC, {0, 0}}; v[n++] = {in[j], solid, {0, 0}};
  }
  SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
  SDL_RenderGeometry(R, nullptr, v, n, nullptr, 0);
}

void glow(float x, float y, float r, Color c, Blend b) {
  if (!glowTex || r <= 0 || c.a == 0) return;
  tintTex(glowTex, c, b);
  SDL_FRect dst = {x - r, y - r, r * 2, r * 2};
  SDL_RenderTexture(R, glowTex, nullptr, &dst);
}

// ------------------------------------------------------------ text
static const FontGlyph* glyph(char ch) {
  int i = (unsigned char)ch - FONT_FIRST;
  if (i < 0 || i >= FONT_COUNT) i = '?' - FONT_FIRST;
  return &FONT_GLYPHS[i];
}

float textWidth(const char* s, float size) {
  float k = size / FONT_EM, w = 0;
  for (const char* p = s; *p; p++) w += glyph(*p)->adv * k;
  return w;
}

static void textPass(const char* s, float x, float y, float size, int texId, Color c) {
  if (texId < 0 || texId >= (int)textures.size()) return;
  SDL_Texture* t = textures[texId];
  tintTex(t, c, BLEND_ALPHA);
  float k = size / FONT_EM;
  float pen = x;
  for (const char* p = s; *p; p++) {
    const FontGlyph* g = glyph(*p);
    if (g->w > 0) {   // both font textures share one layout; each cell is padded for the outline
      SDL_FRect src = {(float)g->x, (float)g->y, (float)g->w, (float)g->h};
      SDL_FRect dst = {pen + g->xoff * k, y + g->yoff * k, g->w * k, g->h * k};
      SDL_RenderTexture(R, t, &src, &dst);
    }
    pen += g->adv * k;
  }
}

void text(const char* s, float x, float y, const TextStyle& st) {
  float w = textWidth(s, st.size);
  if (st.align == CENTER) x -= w / 2;
  else if (st.align == RIGHT) x -= w;
  if (st.shadow > 0) textPass(s, x + st.shadow, y + st.shadow, st.size, st.outline.a ? fontTexOutline : fontTex, Color(0, 0, 0, (uint8_t)(st.color.a * 0.35f)));
  if (st.outline.a) textPass(s, x, y, st.size, fontTexOutline, st.outline);
  textPass(s, x, y, st.size, fontTex, st.color);
}

void textf(float x, float y, const TextStyle& st, const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  text(buf, x, y, st);
}

}  // namespace nova
