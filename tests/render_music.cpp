// Renders every Pixel Peaks song and all the sound effects to WAV files, to listen to the synth
// without running the game:   render_music OUT_DIR
#include "headless.h"
#include "audio.h"
#include "PixelPeaks/pixelpeaks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <math.h>

using namespace nova;

static void writeWav(const std::string& path, const std::vector<float>& s) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  uint32_t n = (uint32_t)s.size(), bytes = n * 2;
  auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
  auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
  fwrite("RIFF", 1, 4, f); u32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
  u32(16); u16(1); u16(2); u32(audio::RATE); u32(audio::RATE * 4); u16(4); u16(16);
  fwrite("data", 1, 4, f); u32(bytes);
  float peak = 0;
  for (float v : s) { peak = fmaxf(peak, fabsf(v)); int16_t q = (int16_t)(fmaxf(-1, fminf(1, v)) * 32767); fwrite(&q, 2, 1, f); }
  fclose(f);
  printf("wrote %s (peak %.2f)\n", path.c_str(), peak);
}

static std::vector<float> render(float seconds) {
  std::vector<float> out((size_t)(seconds * audio::RATE) * 2);
  audio::renderOffline(out.data(), (int)(out.size() / 2));
  return out;
}

int main(int argc, char** argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  setenv("NOVA_SETTINGS", "/tmp/nova-music-settings.txt", 1);
  headless::begin(false, nullptr);
  audio::setVolume(8);
  for (int i = 0; i < pp::NSONGS; i++) {
    const audio::Song* s = pp::SONGS[i];
    audio::music(s);
    float secs = s->bars * 4 * 60.0f / s->bpm + 2.5f;
    writeWav(dir + "/" + pp::SONG_NAMES[i] + ".wav", render(secs));
    audio::music(nullptr);
    render(1.5f);
  }
  std::vector<float> all;
  for (int i = 0; i < audio::SFX_COUNT; i++) {
    audio::sfx((audio::Sfx)i);
    auto part = render(0.9f);
    all.insert(all.end(), part.begin(), part.end());
  }
  writeWav(dir + "/sfx.wav", all);
  headless::end();
  return 0;
}
