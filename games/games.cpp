// Every game built into Nova Arcade HD, in launcher order.
#include "nova.h"

extern const nova::Game PIXEL_PEAKS, BLOCKFALL, BRICK_STORM;

namespace nova {
extern const Game* const GAMES[];
const Game* const GAMES[] = {&PIXEL_PEAKS, &BLOCKFALL, &BRICK_STORM};
extern const int NGAMES;
const int NGAMES = sizeof(GAMES) / sizeof(GAMES[0]);
}  // namespace nova
