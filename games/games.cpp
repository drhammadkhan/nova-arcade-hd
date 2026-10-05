// Every game built into Nova Arcade HD, in launcher order.
#include "nova.h"

extern const nova::Game PIXEL_PEAKS, BLOCKFALL, BRICK_STORM, NEON_SERPENT, ASTRO_DRIFT, NOVA_LANCE, ALIEN_TIDE, HOP_RUSH, MAZE_MUNCH, VOLT_RALLY,
    GEM_CASCADE, CITY_SHIELD, TURBO_HORIZON;

namespace nova {
extern const Game* const GAMES[];
const Game* const GAMES[] = {&PIXEL_PEAKS, &BLOCKFALL, &BRICK_STORM, &NEON_SERPENT, &ASTRO_DRIFT, &NOVA_LANCE, &ALIEN_TIDE, &HOP_RUSH, &MAZE_MUNCH, &VOLT_RALLY,
                             &GEM_CASCADE, &CITY_SHIELD, &TURBO_HORIZON};
extern const int NGAMES;
const int NGAMES = sizeof(GAMES) / sizeof(GAMES[0]);
}  // namespace nova
