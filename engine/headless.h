// Headless runner for tests and tools: no window and no sound device. Drawing goes to a
// 1920x1080 software surface that can be saved as a PNG.
#pragma once
#include "nova.h"

namespace nova::headless {
bool begin(bool withRenderer, const char* startGame);
void step(const Pad& p);                  // one logic tick with this input (pressed/released are worked out)
bool shot(const char* path, int scaleDown = 1);   // draw a frame and save it
void end();
}  // namespace nova::headless
