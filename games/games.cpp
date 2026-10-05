// Every game built into Nova Arcade HD, in launcher order.
#include "nova.h"

extern const nova::Game PIXEL_PEAKS;

namespace nova {
extern const Game* const GAMES[];
const Game* const GAMES[] = {&PIXEL_PEAKS};
extern const int NGAMES;
const int NGAMES = sizeof(GAMES) / sizeof(GAMES[0]);
}  // namespace nova
