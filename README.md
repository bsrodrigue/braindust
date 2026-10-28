# Braindust

A 2D thalassophobia action game in raylib (C++). Design lives in `spec.md`; concept art in `docs/`.

## Build and run

Needs a C++20 compiler, CMake and raylib 5.5 (Fedora: `sudo dnf install raylib-devel`; without it, CMake downloads and builds raylib).

```sh
cmake -B build -G Ninja && cmake --build build
./build/braindust                      # levels/abyss.json
./build/braindust levels/corpses.json  # The Sleeping Generation
./build/braindust levels/shore.json    # The Shore of Kingdoms: platforming on land
```

Keyboard: arrows/WASD move (run on land, swim in the sea), Space jump / stroke up (Space again in the air: star leap), ↓ dive, J wand (hold and release: charged thrust), Shift dodge, U parry, Q drink a dust vial, K star bolt, L flare, F (hold) focus, R restart.
Gamepad: stick move, A jump/stroke, X wand, Y bolt, B flare, RB focus, RT dodge, LB parry, D-pad up vial, Select menu.
`Esc` (or Back / Select) opens the menu: restart, play another level, tune pages, help, sound, quit.
`F2`/`Tab` opens the tune pages, one after another: **Tune** (`tunables.json`), **Moves**, **Enemies**, **Items** (`data/*.json`). Every number, live, pausing the game (keyboard or mouse: click a row, drag a value, wheel scrolls, right-click resets; `S` or the Save button writes that page's file). `F1` help, `M` mute, `F5` reloads the level, the data, the character and `tunables.json`.
`--spawn x,y` starts elsewhere; `--shot N file.png` renders N frames in a hidden window and saves the last one; `--touch` shows the on-screen controls (the mouse is a finger); `--scale 0.6` renders at that fraction of the window (weak GPUs).

## Android

The same code builds for Android (OpenGL ES 2, landscape, minimum Android 7.0). On a phone, on-screen controls appear: a stick under the left thumb (push down to dive), JUMP, WAND (hold and let go: thrust), BOLT, FLARE, DODGE, PARRY, FOCUS (hold), VIAL, and a menu button at the top right (Back does the same). Connect a gamepad and they hide; it plays as on the desktop.

Build it with Android Studio: open the `android/` folder and press Run. Or from the command line, with the Android SDK, NDK and CMake installed (Android Studio installs them; set `sdk.dir` in `android/local.properties` or `ANDROID_HOME`):

```sh
cd android && ./gradlew assembleDebug        # android/app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The first build downloads raylib 5.5 and builds it for Android. Gradle copies the game's files into the APK (`levels/`, `data/`, `shaders/`, `characters/`, `assets/` without the Blender sources, `tunables.json`). Saving a tune page on the phone writes into the app's storage, and that copy is read from then on. The game renders at most 720 px tall and scales up (phones have dense screens and the post pass is heavy). Shaders are written in GLSL 330 and rewritten to GLSL ES 100 on load (`src/platform.cpp`); `--scale` and a desktop build of raylib with `-DOPENGL_VERSION="ES 2.0"` reproduce the phone's rendering path on a PC.

## Levels

Levels are JSON files in `levels/`. Edit them with the level editor:

```sh
python3 serve.py            # then open http://127.0.0.1:8000/
```

`Save` writes `levels/<name>.json`. `Copy play command` saves and copies the command that runs the game on that level (`Copy from view` adds `--spawn` at the centre of the view). Only Python 3 is needed.

Format (10 px = 1 m):

```json
{
  "name": "The Abyss", "width": 1000, "height": 5200,
  "spawn": { "x": 500, "y": 60 },
  "portal": { "x": 500, "y": 5095 },
  "emptyCrack": { "x": 765, "y": 5160 },
  "solids": [{ "x": -300, "y": 0, "w": 400, "h": 160 }],
  "lurkers": [{ "x": 420, "y": 700 }]
}
```

Optional fields: `enemies [{type, x,y}]` (any entry of `data/enemies.json`; `lurkers` and `thoughts [{x,y}]` still work), `items [{kind, x,y, hidden}]` (an entry of `data/items.json`), solid `seal` (stardust % a star bolt needs to open it), `hidden` on items and wrappers (`true`: seen and taken only inside your light; a number: also needs that much stardust), `top` (negative: land and sky above the sea, y = 0 is the surface), `props [{type: hut|villager|fire|post|boat|sprite, x,y, name (sprite: any assets/tileset image), scale, flip}]`, solid `tile` (`ground|float|bridge|stone|box`), `palette {surface, deep}`, `wrappers [{x,y,kind: mana|focus|protection|dust}]`, `dustZones [{x,y,w,h,rate}]`, `lights [{x,y,r,color}]`, `texts [{x,y,text}]`, `decor [{model: skull|giant|tower, x,y (px), z (3D units, negative = behind), scale, rx,ry,rz}]`. Without `decor`, the original Abyss background is used. The editor places the core fields and keeps the optional ones when saving.

`portal` and `emptyCrack` are crack centres; the 3D skulls are placed around them. `height` is the seafloor: the 3D graveyard and the depth gauge use it, so keep a floor wall there.

## Moves, enemies, items (`data/`)

Gameplay content is data: every entry is a set of named numbers and strings, edited live on the F2 pages and saved back by them (nesting one level deep shows as `resist.slash`). Code reads each field by name with a default.

- `data/moves.json`: what the player can do. `input` (`staff bolt flare up dodge parry`), `kind` (`melee projectile burst leap dash parry`), `medium` (`any water land ground air`), `mana` (spells: what mana cannot pay is paid in blood at `bloodRatio` HP per point, and turns to stardust), `damage`, `damageType` (`slash pierce star light`), `cooldown` per `group`, `hold` (fires on release after holding that long: the thrust), `anim`/`animTime` (a countdown the character's state machine reads), `noise` (what listeners hear), plus per-kind fields (`reach`, `manaGain`, `knockback`, `lunge`, `speed`, `life`, `radius`, `light`, `time`, `charges`, `invuln`, `waterScale`, `window`, `stun`).
- `data/enemies.json`: `behaviour` (`stalker`: drift, stalk, lunge, recover; `chaser`: comes straight at you), `sense` (`light`: `sight` × your light radius, × `flareLure` while a flare burns; `noise`: `sight` × how loud you have been; `dust`: `sight` px per stardust %; `fixed`: `sight` px), `draw` (`lurker thought jelly isopod leech`), `hp`, `w`, `h`, `contactDamage`, `contactDust`, `speed`, the stalker's distances and lunge, `respawn`, and `resist.<damageType>`: a damage multiplier. 0 is immune (a dry glance and spark), below 0 it heals the enemy (up to twice its health), above 1 is a weak spot.
- `data/items.json`: carried consumables (`max`), used with their `input`: `mana`, `heal`, `dust` (the dust vial is a stardust drug: mana now, mind later), `title`/`line` for the pickup banner.

## Characters

`characters/mage.json` is the player. Swap the sheet by pointing `sheet`/`atlas` elsewhere; change the animation by editing `states` and `transitions`; `F5` reloads it.

```json
{
 "sheet": "assets/player.png", "atlas": "assets/player.json",
 "height": 96, "tint": "#dbe3e6",
 "states": {
  "run":    { "anim": "run", "fps": 6, "fpsParam": "runRatio", "fpsGain": 8 },
  "strike": { "anim": "attack", "frames": [0, 1, 4, 5], "drive": "strike", "driveLength": 0.36 }
 },
 "transitions": [
  { "from": "any", "to": "strike", "when": ["strike > 0"] },
  { "from": "any", "to": "run", "when": ["!inWater", "speed > 30"] }
 ]
}
```

- `atlas`: the sheet layout (`cell`, `anchor` = feet, `anims {name: {row, frames, head}}`), as written by `tools/extract_player_sprites.py`.
- `height`: world px from the feet to the top of the head in the first state's first frame (sets the sprite scale).
- A state shows `frames` of one `anim` row (default: all). Looping at `fps + fpsGain × fpsParam`, or `drive`n by a countdown param going from `driveLength` to 0 (plays the frames once).
- Each frame, transitions are tried in order; the first whose `when` conditions all hold wins (`from`: `any` or a state name; staying in the current state counts as a match). Conditions: `param`, `!param`, `param op number` with `> >= < <= == !=`.
- Params the game sets: each move's `anim` countdown (`strike`, `thrust`, `castHold`, `flare`, `dodge`, `parry`, s), `charge` (s the hold input has been held), `inWater`, `ground`, `focusing`, `hurt` (0/1), `vx`, `vy`, `speed` (|vx|), `runRatio` (|vx| / runSpeed). Add more in `World::animatePlayer` (`src/world.cpp`).

## Files

- `CMakeLists.txt`, `src/`, `shaders/`, `tunables.json`, `data/`: the game. Tests, run from the project root: `cmake --build build --target character_test collision_test gameplay_test platform_test`, then `./build/character_test` (the mage's state machine), `./build/collision_test` (bodies stop at the nearest face where solids overlap; tuning keeps the wrapper bonuses), `./build/gameplay_test` (blood costs, moves, senses, resistances, parry, seals, items, saving data; expected values come from `data/`, so tuning keeps it passing), `./build/platform_test` (touch controls, the GLSL ES rewrite).
- `android/`: the Android build (Gradle; NativeActivity, no Java). `src/platform.*`: what differs on Android (files, shaders, render scale); `src/touch.*`: on-screen controls; `src/menu.*`: the pause menu.
- `assets/fonts/`: DejaVu Sans Mono and DejaVu Serif (license next to them), used where the system has no fonts of its own (Android).
- `characters/*.json`: a character = a sprite sheet + an animation state machine (see below).
- `levels/`: level files. `editor.html` + `serve.py`: the level editor.
- `assets/*.blend`: Blender sources; `assets/*.glb`: what the game loads.
- `assets/player.png` + `.json`: the player sprite, cut from the concept sheet in `docs/` by `python3 tools/extract_player_sprites.py` (needs Pillow).
- `assets/tileset/`: PNGs from the Craftpix "World Tree Roots" pack (the zip next to it), toned down in game by `shaders/tile.fs`.
- `docs/`: concept art and lore sheets (inspiration, not canon).
# braindust
