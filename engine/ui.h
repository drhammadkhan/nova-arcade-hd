// Shared screens and widgets so every game's title, HUD and game-over look like one arcade.
// Header-only; include after nova.h.
#pragma once
#include "nova.h"
#include "common_art.h"

namespace ui {
using namespace nova;

static inline void panel(float x, float y, float w, float h, Color fill = Color(24, 20, 56, 235), float r = 36) {
  roundRect(x + 10, y + 14, w, h, r, Color(0, 0, 0, 80));
  roundRect(x, y, w, h, r, fill);
  roundRect(x + 6, y + 6, w - 12, fminf(70, h * 0.3f), r - 6, Color(255, 255, 255, 14));
}

// A titled box for HUD side panels (HOLD, NEXT, SCORE...)
static inline void box(float x, float y, float w, float h, const char* label, Color accent = Color(150, 220, 255)) {
  panel(x, y, w, h, Color(18, 14, 44, 220), 24);
  if (label) {
    TextStyle st; st.size = 30; st.align = CENTER; st.color = accent; st.outline = CLEAR;
    text(label, x + w / 2, y + 16, st);
  }
}

static inline TextStyle style(float size, Color c = WHITE, Align a = CENTER) {
  TextStyle st; st.size = size; st.color = c; st.align = a;
  return st;
}

// label / value pair stacked in a HUD box
static inline void stat(const char* label, const char* value, float x, float y, Color vc = WHITE) {
  TextStyle l = style(26, Color(150, 170, 220)); l.outline = CLEAR;
  text(label, x, y, l);
  text(value, x, y + 34, style(48, vc));
}

// The standard title screen: dim the game behind, logo, tagline, hi-score, press A, options, help.
// help: up to 4 short lines.
static inline void titleScreen(const Img& logo, const char* tagline, uint32_t hiscore, const char* const* help, int nhelp,
                               Color accent = Color(170, 230, 255)) {
  rectGrad(0, 0, W, H, Color(8, 6, 24, 120), Color(8, 6, 24, 200));
  Fx lf; lf.sx = lf.sy = 1 + 0.012f * sinf(frameNo * 0.04f);
  draw(logo, W / 2, 190, lf);
  text(tagline, W / 2, 320, style(40, accent));
  textf(W / 2, 395, style(40, Color(255, 220, 110)), "HI-SCORE %07lu", (unsigned long)hiscore);
  if ((frameNo >> 5) & 1) textf(W / 2, 470, [] { TextStyle s = style(64); s.shadow = 4; return s; }(), "PRESS %s", btnName(BTN_A));
  drawTitleOptions(590);
  TextStyle h = style(30, Color(220, 228, 250));
  for (int i = 0; i < nhelp; i++) text(help[i], W / 2, 780 + i * 46, h);
  TextStyle b = style(24, Color(170, 180, 220)); b.outline = CLEAR;
  textf(W / 2, 1010, b, "%s: PAUSE / BACK TO THE MENU", btnName(BTN_HOME));
}

// The standard game-over overlay. t counts frames since the game ended.
static inline void gameOver(int t, bool newHi, const char* title = "GAME OVER", Color c = Color(240, 70, 90)) {
  rect(0, 0, W, H, Color(8, 6, 24, (uint8_t)(150 * easeOut(t / 30.0f))));
  float k = easeOut(t / 24.0f);
  TextStyle a = style(150 * (0.8f + 0.2f * k), c.alpha(k)); a.shadow = 8;
  text(title, W / 2, 360, a);
  bool blink = (frameNo >> 5) & 1;
  if (newHi && blink) text("NEW HI-SCORE!", W / 2, 560, style(52, Color(255, 220, 110)));
  if (t > 90) textf(W / 2, 650, style(48), "PRESS %s", btnName(BTN_A));
}

// A short message that pops in and fades: t counts down from its start value.
static inline void banner(const char* s, float x, float y, int t, int t0, Color c = Color(255, 220, 110), float size = 84) {
  if (t <= 0) return;
  float age = (t0 - t) / 12.0f;
  float k = 1 + 0.4f * (1 - easeOut(age));
  float a = clampv(t / 15.0f, 0.0f, 1.0f);
  TextStyle st = style(size * k, c.alpha(a)); st.outline = Color(30, 16, 50, (uint8_t)(255 * a)); st.shadow = 4;
  text(s, x, y - size * (k - 1) / 2, st);
}

// a slowly drifting starfield / dust background used by several games
struct Stars {
  float x[160], y[160], z[160];
  int n = 0;
  void init(int count) {
    n = count < 160 ? count : 160;
    for (int i = 0; i < n; i++) { x[i] = frand() * W; y[i] = frand() * H; z[i] = frange(0.2f, 1.0f); }
  }
  void step(float vx, float vy) {
    for (int i = 0; i < n; i++) {
      x[i] += vx * z[i]; y[i] += vy * z[i];
      if (x[i] < 0) x[i] += W;
      if (x[i] >= W) x[i] -= W;
      if (y[i] < 0) y[i] += H;
      if (y[i] >= H) y[i] -= H;
    }
  }
  void draw(Color c, float size = 2.5f) const {
    for (int i = 0; i < n; i++) {
      float tw = 0.6f + 0.4f * sinf(frameNo * 0.05f + i * 1.7f);
      disc(x[i], y[i], size * z[i] + 0.5f, c.alpha(z[i] * tw));
    }
  }
};

}  // namespace ui
