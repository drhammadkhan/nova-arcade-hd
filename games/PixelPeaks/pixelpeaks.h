// Pixel Peaks hooks for the tests (tests/test_pixelpeaks.cpp). The game itself is PIXEL_PEAKS in games.cpp.
#pragma once
#include "nova.h"
#include "audio.h"
#include <stddef.h>

namespace pp {
struct DebugInfo { int state, level, lives, hearts, scrolls, loop; float x, y, mapW; bool alive, onGround; uint32_t score; };
enum { DBG_TITLE, DBG_INTRO, DBG_PLAY, DBG_DEAD, DBG_CLEAR, DBG_WIN, DBG_OVER };
void logicStep(const nova::Pad& in);
void debugState(DebugInfo& d);
size_t stateSize();
void copyState(void* dst);
void restoreState(const void* src);
void startAt(int level);
bool tileSolidAt(float x, float y);
uint8_t tileAt(float x, float y);
extern bool invincible;
extern const nova::audio::Song* const SONGS[];
extern const char* const SONG_NAMES[];
extern const int NSONGS;
}  // namespace pp
