# Brief for remaking one Nova Arcade game in HD

You are remaking one game from the original Nova Arcade (an ESP32 console with a 320x240 screen) for
**Nova Arcade HD** (C++17 + SDL3, a fixed 1920x1080 frame, 60 Hz logic, runs as a Mac app and in the
browser). Eight games are already done; follow them closely so yours fits in.

## Read first
- `CLAUDE.md`: engine, conventions, gotchas.
- `engine/nova.h` (drawing, text, input, saves, difficulty: `speed()`, `frames()`), `engine/ui.h` (shared
  title screen `ui::titleScreen`, `ui::gameOver`, `ui::banner`, `ui::box`, `ui::stat`, `ui::Stars`),
  `engine/audio.h` (song format) and the `Sfx` list in `nova.h`.
- Reference games: `games/AlienTide/game.cpp` + `games/AlienTide/tools/make_art.py` (game with its own
  vector art), `games/HopRush/game.cpp` + its `tools/make_art.py`, `games/NovaLance/game.cpp` (synthwave
  backdrop drawn at run time), `games/Blockfall/game.cpp` (a bot that plans moves).
- `tools/art/hd.py` (Skia drawing helpers, atlas packer) and `tools/art/common_art.py` (shared art:
  `cart::IMG_BLOCK/BRICK/ORB/SPARK/RING/PUFF/STREAK` and the logo `cart::IMG_LOGO_<NAME>` for your game,
  already made).
- `tests/test_games.cpp` (how your game is tested) and `tools/port_music.py`.
- The original: `/home/user/nova-arcade/games/<Game>/game.h` (+ its `art.h`/`tools` for reference only;
  don't reuse its pixel art).

## What to build
1. `games/<Game>/game.cpp`: the original's rules, timing and feel, ported faithfully. The established
   approach: keep the gameplay in the original's 320x240 units as floats and draw through
   `X(x) = OX + x*4.5`, `Y(y) = y*4.5` (centre a 4:3 field with `OX = (1920-1440)/2` and use the side
   margins for the HUD/scenery, or widen the field to 16:9 when the game benefits, as Astro Drift and
   Nova Lance do). Namespace your code; end the file with
   `extern const nova::Game NAME; const nova::Game NAME = {"id", "TITLE", "TAGLINE", init, enter, step,
   draw, canPause, nullptr, bot, debugInfo};` (the id is the save namespace and assets folder name; use
   the original's id from its `arcade::begin("...")`).
   - Title screen via `ui::titleScreen(cart::IMG_LOGO_..., tagline, hiscore, help, n)`, game over via
     `ui::gameOver`, difficulty via `titleInput`/`speed()`/`frames()`, hi-scores via `loadHi`/`saveHi`,
     pause with `nova::pause()` on START. Button names in hints via `btnName()`.
   - HD look: glows, gradients, particles (sparks, rings), screen shake, smooth motion/animation. All art
     and music original.
   - Music: run `python3 tools/port_music.py /home/user/nova-arcade/games/<Game>/game.h` and keep the
     original melodies (they're the owner's own), choosing instruments from the `Inst` list. Sound
     effects from the `Sfx` enum (SFX_SHOOT, SFX_EXPLODE, SFX_COIN, ...). Don't add new SFX to the engine
     unless essential (if you must, add before SFX_COUNT in nova.h and a case in engine/audio.cpp).
   - `bot(Pad& p)`: an autopilot good enough to actually play (the test runs it for 3 minutes); press A
     on title/game-over screens. `debugInfo` prints state/score/level/lives.
2. If the game needs sprites: `games/<Game>/tools/make_art.py` (Skia, like AlienTide's) writing
   `assets/<id>/atlas*.png` and `games/<Game>/art.h` (choose a unique namespace, e.g. `mmart`), loaded in
   `init()` with `loadAtlas(...)`. Run it with `--preview DIR` and look at the contact sheet.
3. Register the game: add it to `games/games.cpp` (extern list and GAMES array, after HOP_RUSH).

## Build and test (must pass)
```
cmake -S . -B build-<id> -DCMAKE_PREFIX_PATH=/home/user/deps/sdl3 -DCMAKE_BUILD_TYPE=Release
cmake --build build-<id> -j8 --target test_games
./build-<id>/test_games smoke <id>          # 3 minutes of bot play, must report 0 failed
./build-<id>/test_games shots /tmp/<id>-shots <id>   # then LOOK at the PNGs (title, play1, play2) and fix what looks wrong
mkdir -p assets/<id> && ./build-<id>/test_games thumbs <id>   # launcher thumbnail
```
No compiler warnings (-Wall). Use the Read tool on the screenshots and iterate until it looks polished.
Use your own build directory (`build-<id>`), never `build/`.

## Finish
Commit on your current branch with a clear message (what was ported, what's new in HD, how the bot did),
ending with:
```
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01QebsijPssjQ8AAsJCJBA3e
```
Do not push. Don't modify other games, engine files (except a justified SFX addition) or CLAUDE.md.
Report: the branch name, what you built, test output, and anything left rough.
