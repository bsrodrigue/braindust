# Braindust

A 2D thalassophobia action game in raylib (C++). Design lives in `spec.md`; concept art in `docs/`.

## Build and run

Needs a C++20 compiler, CMake and raylib 5.5 (Fedora: `sudo dnf install raylib-devel`; without it, CMake downloads and builds raylib).

```sh
cmake -S port -B port/build -G Ninja && cmake --build port/build
./port/build/braindust                      # levels/abyss.json
./port/build/braindust levels/corpses.json  # The Sleeping Generation
./port/build/braindust levels/shore.json    # The Shore of Kingdoms: platforming on land
```

Keyboard: arrows/WASD move (run on land, swim in the sea), Space jump / stroke up, ↓ dive, J wand, K star bolt, L flare, F (hold) focus, R restart.
Gamepad: stick move, A stroke, X wand, Y bolt, B flare, RB focus.
`F2`/`Tab` opens the tune menu: every gameplay variable, live (keyboard or mouse: click a row, drag a value, wheel scrolls, right-click resets; `S` or the Save button writes `port/tunables.json`). `F1` help, `M` mute, `F5` reloads the level and `port/tunables.json`.
`--spawn x,y` starts elsewhere; `--shot N file.png` renders N frames in a hidden window and saves the last one.

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

Optional fields: `top` (negative: land and sky above the sea, y = 0 is the surface), `props [{type: hut|villager|fire|post|boat|sprite, x,y, name (sprite: any assets/tileset image), scale, flip}]`, solid `tile` (`ground|float|bridge|stone|box`), `palette {surface, deep}`, `thoughts [{x,y}]`, `wrappers [{x,y,kind: mana|focus|protection|dust}]`, `dustZones [{x,y,w,h,rate}]`, `lights [{x,y,r,color}]`, `texts [{x,y,text}]`, `decor [{model: skull|giant|tower, x,y (px), z (3D units, negative = behind), scale, rx,ry,rz}]`. Without `decor`, the original Abyss background is used. The editor places the core fields and keeps the optional ones when saving.

`portal` and `emptyCrack` are crack centres; the 3D skulls are placed around them. `height` is the seafloor: the 3D graveyard and the depth gauge use it, so keep a floor wall there.

## Files

- `port/`: the game (`src/`, `shaders/`, `tunables.json`).
- `levels/`: level files. `editor.html` + `serve.py`: the level editor.
- `assets/*.blend`: Blender sources; `assets/*.glb`: what the game loads.
- `assets/player.png` + `.json`: the player sprite, cut from the concept sheet in `docs/` by `python3 tools/extract_player_sprites.py` (needs Pillow).
- `assets/tileset/`: PNGs from the Craftpix "World Tree Roots" pack (the zip next to it), toned down in game by `port/shaders/tile.fs`.
- `docs/`: concept art and lore sheets (inspiration, not canon).
# braindust
