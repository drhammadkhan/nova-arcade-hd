# CLAUDE.md

Guidance for Claude Code (and humans) working on **Nova Arcade HD**: high-resolution remakes of the [Nova Arcade](https://github.com/drhammadkhan/nova-arcade) ESP32 games, for macOS (a native `.app`) and the browser (WebAssembly).

All 13 games are remade. The original repo is the reference for each game's rules. This fork is a clean break: gameplay is ported by hand, and nothing is shared with the original at build time.

## Toolchain

- **Language and libraries:** C++17 and SDL3 (3.2.x API). Drawing uses SDL_Renderer, which is Metal on the Mac, WebGL in the browser, and software in tests. Images load with `stb_image`.
- **CMake:**
  - Uses an installed SDL3 if `find_package` finds it.
  - Otherwise it fetches `release-3.2.24` and builds it statically.
  - The web build uses Emscripten's SDL3 port. To use an SDL3 you built for wasm instead, pass `-DNOVA_WEB_SDL_PORT=OFF` with `CMAKE_PREFIX_PATH` and `CMAKE_FIND_ROOT_PATH`.
- **Build scripts:**
  - `scripts/build-mac.sh`: universal `.app`, ad-hoc signed, zipped to `build/Nova-Arcade-HD-mac.zip`.
  - `scripts/build-web.sh`: builds into `build/site`.
- **CI** (`.github/workflows/build.yml`):
  - **test** (Linux, headless): bot plus screenshots.
  - **web:** Emscripten 6.0.10 plus a Playwright smoke test.
  - **mac** (macos-14): `.app` zip artifact, and a release on `v*` tags.
  - **deploy:** GitHub Pages, from `main` only.
- **Art tools:** `pip install skia-python numpy pillow`.
- **Cloud sessions:**
  - The proxy blocks Emscripten's port download (github archive zips return 403), but `git clone` works. To test the web build locally:
    1. Clone SDL `release-3.2.24`.
    2. Build it with `emcmake cmake -DSDL_SHARED=OFF -DSDL_STATIC=ON`, install it, and point CMake at it as above.
  - Playwright is in `/opt/node-tools/node_modules`. ESM ignores `NODE_PATH`, so run `tests/web_smoke.mjs` from a directory with a `node_modules` symlink to it.

## Engine (`engine/`)

- **Fixed frame and step:**
  - Every game draws a fixed **1920×1080** frame (`nova::W/H`). `SDL_SetRenderLogicalPresentation` letterboxes it to any window size, including Retina.
  - Logic steps at a fixed **60 Hz**, with at most 4 catch-up steps per frame.
  - Games never see window sizes.
- **Drawing:**
  - `draw(img, x, y, Fx)` puts an image's pivot at (x, y). `Fx` holds:
    - `sx`, `sy` for scale (negative flips);
    - `rot`, in degrees clockwise on screen, applied after any flip;
    - `tint`;
    - `blend`;
    - `flip`.
  - Shapes: `rect`, `rectGrad`, `roundRect` (anti-aliased geometry), `disc`, `ring`, `line`, `glow` (additive radial), `tris`.
  - `drawStrip` tiles a parallax layer horizontally.
  - `clip(x, y, w, h)` limits drawing to a rectangle until `noClip()` (Gem Cascade's well).
- **Atlases:**
  - Generated art is drawn at **2× density**: `Img.scale = 0.5`, and the pivot is in atlas pixels.
  - Big background layers are separate 1× textures.
  - `hd.py` bleeds colour into transparent pixels, so linear filtering has no dark fringes. Keep that whenever you write PNGs.
- **Shared art** (`common_art.h`, from `tools/art/common_art.py`, namespace `cart`): `IMG_BLOCK`, `BRICK`, `ORB`, `SPARK`, `RING`, `PUFF`, `STREAK`, and every game's logo `IMG_LOGO_<NAME>` (1× density). Headers use `inline int TEX[]`, so every file shares one copy of the texture slots.
- **Shared UI** (`ui.h`): `ui::titleScreen` (logo, tagline, hi-score, difficulty, volume, help lines), `ui::gameOver`, `ui::banner`, `ui::panel`/`box`/`stat` for HUDs, and `ui::Stars`.
- **Text:**
  - Fredoka SemiBold is baked at 128 px into `assets/font.png` and `font_outline.png`, which share one layout. `y` is the top of the capitals.
  - `TextStyle` sets size, colour, outline (`a = 0` for none), alignment and drop shadow.
  - `btnName(BTN_A)` gives "Z" or "A" depending on whether the last input came from the keyboard or a pad.
- **Input:**
  - `nova::pad` (a `Pad`) has `held`, `pressed` and `released` bitmasks, plus `ax`/`ay`.
  - Key presses shorter than a frame are latched, so they're never lost.
  - The keyboard map is in `platform.cpp`. Pads go through SDL_Gamepad and are mapped by position: south = A, east = B.
- **The system layer** (`system.cpp`):
  - The launcher: a scrolling grid of cards, four per row, one per entry in `games/games.cpp`. Each card shows `assets/<id>/thumb.png`.
  - `Game` holds `id` (save namespace and assets folder), `title`, `tagline`, `init`, `enter`, `step`, `draw`, `paused`, `thumb`, `bot` (an autopilot for the tests) and `debug` (a status line).
  - The pause menu: resume, volume, full screen, quit to menu. Esc or the Home button opens it, unless `Game::paused()` returns false.
  - Difficulty is Easy, Normal or Hard (`speed()` 0.7 / 1.0 / 1.25), saved per game under `diff`.
  - Hi-scores are kept per difficulty under the keys `hi`, `hiE` and `hiH`.
  - Settings are saved through `loadInt`/`saveInt`:
    - Desktop: `settings.txt` in SDL's pref path, or the file named by `NOVA_SETTINGS`.
    - Browser: localStorage, under `nova-hd/<ns>/<key>`.
- **Audio** (`audio.h`/`audio.cpp`):
  - 48 kHz stereo float.
  - Voices run under a mutex on SDL's audio thread.
  - Instruments are in the `INST` table. Drums and sound effects are synthesised in `renderVoice`.
  - Songs are text scores: see the format at the top of `audio.h`. Chords, a lead and counter-melody (16 tokens per bar), arp, bass and drum patterns of 16 characters per bar, and an optional pad.
  - New sound effects go before `SFX_COUNT` in `nova.h`, with a case in `audio::sfx`.
  - `audio::engine(hz, vol)` is a continuous engine drone (Turbo Horizon). Call it every frame; it fades out by itself when the calls stop or the game pauses.
  - `tools/port_music.py <original game.h>` converts an original game's tunes to the song format; the remakes keep the original melodies (they're the owner's own).
  - `audio::mute > 0` silences sound effects and music changes. Bots set it while looking ahead.
- **Headless runner** (`headless.cpp`):
  - A software renderer onto a 1920×1080 surface, with no audio device.
  - `headless::begin(withRenderer, game)`, `step(pad)`, `shot(path, scaleDown)`.
  - Tests set `NOVA_ASSETS` and `NOVA_SETTINGS`.

## Conventions

- Each game is one `game.cpp` in its own namespace, ending with an `extern const nova::Game NAME = {...}`. Register it in `games/games.cpp`.
  - Namespace-scope `const` objects have internal linkage, so anything referenced from another file needs `extern`. Missing it caused linker errors once already.
- Keep a game's gameplay state in one struct (Pixel Peaks: `pp::Game G`), so bots can copy it and play ahead.
  - Particles, popups, shake and other cosmetics live outside it.
  - Effects are spawned only when `fxOn()` is true, so simulated futures don't leave particles behind.
  - Logic goes in `logicStep()`; cosmetic updates go in a separate step.
- Generated files are committed and never edited by hand: `art.h`, `levels.h`, `engine/font_data.h` and everything in `assets/`. Edit the script and rerun it.
  - Assets are committed so a Mac build needs only CMake.
  - The web build relinks when an asset changes (`LINK_DEPENDS`).
- All art and music must stay original: no copied sprites, logos or tunes from commercial games.
- Games other than Pixel Peaks keep their gameplay in the original's 320×240 units as floats and draw through `X(x) = OX + x*4.5`, `Y(y) = y*4.5`, either centring a 4:3 field (`OX = 240`, HUD in the side margins) or widening it to 16:9 (Astro Drift, Nova Lance, City Shield). Tuning numbers stay comparable with the original.
- Every game has a `bot()`. `test_games smoke [id]` runs each one for 3 minutes of bot play and must report 0 failed; `test_games shots DIR [id]` writes title and play screenshots; `test_games launcher DIR` shoots the launcher.
- `docs/remake-brief.md` is the checklist for remaking a game (it was given to the sub-agents that built five of them).
- Check visuals with `test_pixelpeaks shots DIR`, `test_games shots DIR` and `--preview` on the art scripts before pushing.
- Regenerate launcher thumbnails (`test_pixelpeaks thumb`, `test_games thumbs [id]`) and `docs/screenshots/` when a game's look changes.

## Pixel Peaks HD (`games/PixelPeaks/`)

- **Physics and scale:**
  - Physics and rules are the original's, multiplied by `K = 4.5`: original pixels to ours, tiles 16 → 72.
  - Levels are 15 rows (1080 px), so the camera only scrolls sideways.
  - Keep tuning numbers written as `original * K`, so they stay comparable with the original.
- **The heroine rig:**
  - She's drawn by `drawHeroAt()` from a `Pose`. Angles are in degrees, clockwise in her right-facing space. Limbs hang down at 0, so a negative angle swings a leg or arm forward.
  - Parts are drawn facing right, hanging from their joint (the pivot).
  - Lengths: thigh 17, shin to sole 22, upper arm 15. The torso is 32 to the neck, and the shoulders sit at (1, -27).
  - Back limbs are tinted `(196, 200, 226)`.
  - The belt sprites are white and tinted with `beltColor()`.
  - The ponytail, headband tails and belt tails are springs in `cosmeticStep()`.
  - `HERO_SCALE` (1.15) draws her a little bigger than her hitbox, which is `HW` × `HH` (45 × 99, or 63 tall while rolling).
- **Art** (`tools/make_art.py`):
  - Ground tiles have 16 edge masks per world (up 1, left 2, right 4, down 8), plus two interior variants, all chosen by `autotile()`.
  - Tile sprites overhang their square by `OV = 12`. They're clipped on closed sides, so neighbouring tiles join without a seam.
  - Mochi colours change per world.
  - Water is drawn at run time (waves and gradient) in a pass after the heroine.
- **Levels** (`tools/make_levels.py`):
  - The ASCII layout language and `check_reach()` are carried over from the original, unchanged in tile units: climbs ≤ 3 tiles, gaps ≤ 4, collectables ≤ 4 rows above a standing spot.
  - Run it after editing a level, then run the bot.
- **The bot** (`tests/test_pixelpeaks.cpp bot`) must clear all six levels with foes and spikes harmless and no lives lost; `--damage` turns foes on.
  - It plans two 10-frame chunks over seven inputs, with a follow-through that jumps at edges and walls.
  - It plays the whole two-chunk plan before planning again. Re-planning after one chunk let "wait, then go" win forever.
  - Debug a stuck bot with `BOT_LEVEL=n BOT_TRACE=1`.
- **Music:** songs per world are in `game.cpp`: koto and marimba for Blossom, flute, koto and choir with taiko for Bamboo dusk, saw lead and strings for Misty Peaks. `render_music DIR` writes them as WAVs.

## The other games (`games/<Game>/`)

Each follows the original's rules and timings; the original's `game.h` is the reference. Art scripts are `games/<Game>/tools/make_art.py` (namespaces `nlart`, `atart`, `hrart`, `mmart`, `vrart`, `gcart`, `csart`, `thart`); Blockfall, Brick Storm, Neon Serpent and Astro Drift use only the shared art and run-time drawing.
- **Volt Rally** is paddle tennis (not a racer): first to 5 beats each of six rivals; holding A as the ball meets the paddle smashes it.
- **Maze Munch:** the maze is baked from the layout into big textures (tube, fill, glow); the wisps keep the original's hunting rules and scatter/chase schedule.
- **Gem Cascade:** a 4-match makes a flame gem, an L or T a star gem, 5 a nova; the level bar drains only while the board is idle.
- **City Shield:** X/A/B fire from the left, centre or right base. The ground profile in `art.h` matches the original's exactly.
- **Turbo Horizon:** the road is projected per segment at 1920×1080 and sent as one `tris` call per frame; sprites are sorted far to near and clipped at the hill crest in front of them. The bot is the original's `autopilot()`.
- Several games map the original's sounds onto the nearest HD `Sfx` (there is no skid, blip or siren yet); add real ones before `SFX_COUNT` if they sound wrong.

## Status

- **Builds and tests:**
  - It builds and runs on Linux, headless and native.
  - The browser build was checked in headless Chromium: launcher, title and gameplay, no page errors, about 55 fps on software GL.
  - The Pixel Peaks bot clears all six levels; every other game passes `test_games smoke`.
- **Not yet checked:**
  - On a real Mac: the `.app` build (CI builds it on macos-14), Retina sharpness, gamepads, the audio device.
  - On a real browser: mobile and touch controls, which don't exist yet.
  - Turbo Horizon's frame rate in a real browser (it's the heaviest game per frame) and the new games' sounds by ear.
- **Next:** touch controls for the browser on phones, as the original's web player has.
