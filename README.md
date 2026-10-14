# OpenClaw for macOS - Captain Claw / Kapitan Pazur

A fork of [pjasicek/OpenClaw](https://github.com/pjasicek/OpenClaw) (a C++ reimplementation of the Captain Claw platformer from 1997) that runs natively on macOS (Apple Silicon) and supports the Polish release of the game, "Kapitan Pazur". See the original repository for the project itself, building on other platforms and credits.

You need your own copy of the game to play, the game data is not included.

## Languages

Both the English release (Captain Claw) and the Polish release (Kapitan Pazur) are supported. The language is detected automatically from `CLAW.REZ`: the Polish release is recognized by its credits, which name "Kapitan Pazur". With the Polish data the game uses the Polish menu captions and their underlined hotkeys, and the texts added by this fork (key prompts, messages for unimplemented menu items, action names in Options -> Controls) are shown in Polish.

## macOS

Download `OpenClaw-<version>-arm64.dmg` from the [releases](https://github.com/skowronmateusz/OpenClaw-PL-macos/releases) (Apple Silicon):

1. Drag `OpenClaw.app` into Applications.
2. Start the app. The first time, macOS blocks it because it isn't notarized: right-click it and choose Open, or allow it in System Settings > Privacy & Security. If macOS says the app is damaged: `xattr -dr com.apple.quarantine /Applications/OpenClaw.app`.
3. Without the game data the app opens the folder `~/Library/Application Support/OpenClaw/`. Copy these files from your game disc (or a mounted ISO image) into it and start the app again:

| File on the disc | Copy as | Purpose |
|---|---|---|
| `DATA/CLAW.REZ` | `CLAW.REZ` | graphics, sounds, levels (required) |
| `MUSIC/CLAW4.SF2`, `MUSIC/CLAW.SF2` | same names | instruments for the MIDI music |
| `MOVIES/claw.fec` | `CLAW.FEC` | movies |

```bash
cd ~/Library/Application\ Support/OpenClaw
cp "/Volumes/KAPITAN PAZUR/DATA/CLAW.REZ" .
cp "/Volumes/KAPITAN PAZUR/MUSIC/"*.SF2 .
cp "/Volumes/KAPITAN PAZUR/MOVIES/claw.fec" CLAW.FEC
```

The settings (`config.xml`) and saved games (`SAVES.XML`) are stored in the same folder, delete them to restore the defaults. Movies can be disabled by adding `<Movies><PlayMovies>false</PlayMovies></Movies>` to `config.xml` (inside `<Configuration>`).

## Building from source

Requirements: Xcode Command Line Tools, [Homebrew](https://brew.sh) and CMake.

```bash
brew install cmake sdl2 sdl2_image sdl2_mixer sdl2_ttf sdl2_gfx
```

```bash
mkdir build && cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-I/opt/homebrew/include -I/opt/homebrew/include/SDL2" \
  -DCMAKE_EXE_LINKER_FLAGS="-L/opt/homebrew/lib"
make -j$(sysctl -n hw.ncpu)
```

`-DCMAKE_OSX_ARCHITECTURES=arm64` matters when CMake runs under Rosetta (e.g. installed in `/usr/local`), otherwise the build targets x86_64 and the linker rejects the Homebrew libraries. The executable ends up in `Build_Release/openclaw`.

To run it from the repository, copy the game files from the table above into `Build_Release` (they are in `.gitignore`), zip the content of `Build_Release/ASSETS` into `Build_Release/ASSETS.ZIP` and start it there:

```bash
cd Build_Release && ./openclaw
```

On the first start the game creates `config.xml` and `SAVES.XML` from `config.template.xml` and `SAVES.template.XML`; both are in `.gitignore`.

### App and disk image

`Scripts/package_macos.sh <version>` turns the built `Build_Release/openclaw` into `dist/OpenClaw.app` and `dist/OpenClaw-<version>-arm64.dmg` (needs `brew install dylibbundler`). The Homebrew libraries are copied into the app, which is signed ad-hoc. The app needs the macOS version the Homebrew libraries were built for.

GitHub Actions ([.github/workflows/macos-release.yml](.github/workflows/macos-release.yml)) builds the disk image for every push and pull request. Pushing a tag `v<version>` also publishes a release with it:

```bash
git tag v1.0.0 && git push origin v1.0.0
```

## Controls

| Default key | Action |
|---|---|
| ← → | move |
| ↑ ↓ | climb; ↓ also crouches |
| Space | jump |
| left Ctrl | attack; throw when Claw is carrying something |
| Z | lift a keg / enemy, press again to throw |
| left Option (Alt) | shoot / throw weapon (hold with dynamite to throw farther) |
| left Shift | switch weapon |
| Esc | pause menu |
| ` | developer console |

Keys can be changed in Options → Controls, both from the main menu and the pause menu: select an action, press Enter (or click it) and press the new key, Esc cancels. A key already used by another action is swapped with it, "Restore defaults" brings the default layout back. Esc and ` cannot be rebound. The keys are saved in `config.xml` under `<ControlOptions><KeyBindings>`.

The alternative default layout (WASD + mouse) is enabled with `<UseAlternateControls>true</UseAlternateControls>` in `config.xml`: A/D move, W/S climb (W also jumps), Space jump, J or right mouse button attack, K or left mouse button shoot, E switch weapon, Q lift and throw.

## Changes

Compared to the original repository:

### macOS and the Polish release

- Native build on macOS (Apple Silicon) with the Homebrew SDL2 libraries (also through sdl2-compat / SDL3).
- `OpenClaw.app` and a disk image built by GitHub Actions. Started from the app, the game keeps the data of the original game, settings and saved games in `~/Library/Application Support/OpenClaw/` and asks for the missing game files.
- Polish release support: PCX transparency is computed from the palette index (removes the pink background behind the Polish menu captions), captions are centered by their real width, the language is detected from `CLAW.REZ`, fixed a crash on the score screen and missing sounds with the Polish `CLAW.REZ`.
- `config.xml` and `SAVES.XML` are created from templates on the first start, so they are never versioned.

### Graphics and window

- The window can be resized freely, the image is scaled with the aspect ratio kept.
- The original 640x480 view at 2x scale by default. At scale 1 the 1280x960 frame showed four times more of the level than the original, including unintended parts of the background art.
- Level backgrounds are drawn with SDL3: wrapped tile planes used sizes that SDL3 rejects as an overflow, they also wrap on negative coordinates now.
- The loading screen is no longer zoomed in with the render scale.

### Sound and movies

- In-level MIDI music is played through FluidSynth with the game's original soundfont (`CLAW4.SF2` / `CLAW.SF2`, or `SDL_SOUNDFONTS`).
- Playback of the original Smacker movies from `CLAW.FEC`: logo, intro, chapter cutscenes and the "Play movie" page.

### Menu and options

- Menu hotkeys follow the underlined letters of the (localized) captions, like in the original.
- New "Screen" (fullscreen / windowed), "Difficulty" and "Controls" pages, settings are saved to `config.xml`.
- Key rebinding in Options -> Controls (see [Controls](#controls)).
- Menu items that aren't implemented and levels after the last implemented one show a short message instead of doing nothing.

### Gameplay

- Lifting, carrying and throwing powder kegs and enemies, with the rules read from the original `CLAW.EXE` (see [Lifting and throwing](#lifting-and-throwing)).
- Claw's damage and enemy health taken from `CLAW.EXE`: melee 5 (ducking 2, catnip doubles it), pistol 8, magic 25, dynamite explosion 15, sword projectiles 25. Thrown enemies and powder kegs deal their original damage too.
- 7 starting lives and game over when the last life is lost. A new game starts with fresh stats on any level, only Load Game uses the saved stats.
- Enemies attack only once they are on screen. Their aggro areas were tuned for a 1280 px wide view, so with the original view they attacked from off screen.
- Claw's melee hit area reaches from his center to the tip of the swing, so a crate right in front of him can be hit without stepping back.

### Fixes

- XML resources are null terminated before parsing (TinyXML read past the buffer, found by AddressSanitizer).
- Resetting a level no longer iterates the actor map while destroying actors.

## Lifting and throwing

Rules reconstructed from the original `CLAW.EXE` (the Polish release from 2000) by analyzing its machine code. Function addresses in the original are given in parentheses.

All damage and health values are the same as in the original game.

**What can be lifted**
- Powder kegs and regular enemies that have `LIFTED`/`LIFTED1`, `LIFTED2` and `THROWN` animations in the game data. Bosses cannot (they have separate combat logic).
- Claw must be standing on the ground, facing the target and close to it.
- An enemy cannot be grabbed while jumping or in the middle of a sword swing, nor for 1.5 s after getting up from a throw (`0x44ed11`, flag checked at `0x41ef73`). In the original an enemy leaves the attack state between blows; in OpenClaw it attacks continuously, so the protection covers only the first half of the attack animation.
- A lifted enemy does not think or attack. Claw can walk and jump with it, but not fight. When hit, crouching, grabbing a rope or stepping onto a ladder, Claw drops what is being carried.

**A thrown enemy**
- Every enemy (as well as crate or keg) it flies through loses as much health as the thrown enemy's attack strength (`0x44f340` -> `health -= attacker.damage`), e.g. 10 for an Officer. The thrown enemy keeps flying.
- It bounces off the ground twice, each time at 1/3 of the speed (`0x4412b0`, state `LS_BOUNCE`), and stops on the third contact. Hitting a wall kills its forward motion (not verified in the original).
- It lies there for 0.5 s (`LS_PAUSE`), then loses 4 health (`0x405240`). If health drops to zero it dies; otherwise it gets up and keeps fighting.
- If it is still in the air 2 s after the throw (e.g. it fell into a pit), it dies.

**Powder keg**
- Keg damage is 20 (`0x4714c5`).
- A thrown keg flies through enemies, dealing each of them that damage, and explodes on hitting the ground (`0x471bb0`). In OpenClaw it also explodes on hitting a wall; this was not verified in the original.
- It also explodes from any hit: sword, projectile, another explosion (`0x471bf0`).
- The explosion hurts everything in range, enemies and Claw, for 20 (`0x41e797`).

All values can be changed in the XML of the `LiftableEnemyComponent` and `PowderKegComponent` components.
