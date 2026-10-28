# Braindust — Game Specification

Living document. Updated as prototypes change. Last update: 2026-10-28.

## 1. Vision

A 2D action-exploration game about a black mage whose magic lives in his cranium. Spending magic beyond your means costs blood, and blood-casting turns the brain into stardust. Set in an ocean of thalassophobia and cosmic horror: giant skulls on the seafloor, universes inside their cracks, things that escaped from minds.

**World:** a set of **areas**, each its own **biome** (see 7b). The sea is the first; a lake of blood and, late in the game, a crocodile swamp follow.

**Core feeling:** dread and temptation. Power is always available, but it is paid for with your body and your mind.

**Pillars** (from the design theory notes):
- Deep combat with real mastery (you win because you are good, not because of loot)
- Deep exploration, no hand holding, secrets
- Theory crafting (helmets, wands, risk budgets)
- Deep lore, discovered rather than explained
- Real thalassophobia: scale, darkness, sound, things you only half see

## 2. Format decisions

| Decision | Choice | Why |
|---|---|---|
| Gameplay plane | 2D side view | Character silhouette shows the cranium mechanic; precise controls; magic can double as traversal |
| Rendering | 2D playfield + 3D backgrounds | 3D only where it buys immersion (depth, scale, fog); keeps asset cost down |
| Isometric (TUNIC/Death's Door) | Explored, not chosen | Prototype removed 2026-10-27 |

## 3. Art direction

- **Not cute.** Raw, heavy, desaturated. Gaunt proportions, ragged cloth, tarnished metal, bone.
- 3D assets are lumpy and organic (noise-displaced), never sleek primitives. Fog hides detail, so 3D background assets can stay low-detail.
- Readability rule: the 2D playfield is sharp and high-contrast; the 3D background is soft and fogged. Nothing in the background may read as a platform.
- Light is scarce and diegetic: the cranium, spells, lures, the universe in the giant skull.
- Film grain, heavy vignette, near-black at depth.
- Palette (abyss): void `#010204`, ink `#b9c2c4`, tarnished brass `#8f7448`, mana `#7a98a8`, blood `#8e1f2b`, stardust `#8c7aa8`.
- Reference: `docs/Gemini_Generated_Image_bzuxnnbzuxnnbzux.jpeg` (tiny diver, giant cracked skull with a galaxy inside, bone field).
- Each biome has its own palette and references (7b); this palette is the sea's. Concept art in `docs/` and `docs/Concept-Arts/` is inspiration, not canon.

## 3b. Tileset (land)

- Source: Craftpix "World Tree Roots 2D Platformer Tileset" (`assets/craftpix-net-712113-…zip`); PNGs extracted to `assets/tileset/{platformer,environment,collectable,background}` with lower-case names (e.g. `platformer_ground_02`, `environment_statue`).
- The art is bright and cartoony, against our direction, so it is **toned down in `shaders/tile.fs`**: 50% desaturation, then a warm multiply (`#e0b49a`) toward the sunset. Not used: the green forest backgrounds, coins, keys.
- **Land solids** (top above the sea) are tiled at 64 world px per tile. Style per solid (`tile` field, else picked from shape): `ground` (grass top with corners, dirt fill, rocky side edges; edges and grass are skipped where another solid continues the mass), `float` (h ≤ 48: floating island row with caps), `bridge` (wooden planks), `stone` (w ≤ 60: stone-block column), `box` (wooden crate). Underwater rock stays the dark flat style.
- **Sprite props:** any tileset image placed with `{type: "sprite", name, x, y, scale?, flip?}`, standing on (x, y), drawn at half its pixel size × scale, through the same filter.

## 4. Character: the player

- **Sprite:** the mage-warrior from the concept sheet `docs/Gemini_Generated_Image_l6z59hl6z59hl6z5.jpeg`: woven cranium wrapper, ivory wrapper armour, skull wand, carved spear, shield.
- **Pipeline:** `python3 tools/extract_player_sprites.py` cuts the sheet (grey background removed, frame numbers and borders dropped, pieces spilling in from neighbouring cells removed, feet aligned on one baseline) into `assets/player.png` (254×212 cells: idle ×4, run ×6, attack ×6) and `assets/player.json` (cell, feet anchor, head position per frame). Re-run it if the sheet changes.
- **In game:** 96 world px from feet to the top of the wrap (hitbox 28×70; the head rises above it), never rotated, mirrored for left; a cool tint (`#dbe3e6`) to sit in the water; red tint when hurt, flicker while invulnerable.
- **Cranium:** as stardust rises it swells out of the top of the headwrap (per-frame head position), translucent and veined, with faint motes inside. It leaks stardust particles in proportion to stardust %.

### 4.1 Animation

| State | Sprite |
|---|---|
| Idle, focus | idle cycle, 4 fps |
| Running on land | run cycle, 6–14 fps with speed |
| Airborne on land | run frame 2 rising, frame 4 falling |
| In the water (falling in, swimming, sinking) | idle cycle at a slow 2.5 fps, a drift (never the walk cycle) |
| Stroke (Space) | body squashes ~8% |
| Wand strike | attack frames 1, 2, 5, 6 over 0.36 s (raise, thrust, recover) |
| Star bolt | attack frame 3 (thrust with aura) then 4 (open-palm aura) |
| Flare | attack frame 4, held |

On top of the sprite: a slow breathing bob and a squash on each stroke. The player never rotates.

### 4.2 Post-processing (`shaders/post.fs`)

The 3D background, the 3D near-pass layer and the 2D world are composited by one full-screen shader; the HUD sits on its own canvas above it, undistorted.
- Refraction wobble (stronger with depth)
- Chromatic aberration toward the edges (stronger in the pocket universe)
- Bloom on every light: lures, windows, bolts, cranium, the universe in the skull
- God rays from the surface, broken by the shaft walls (shallow only)
- World-anchored caustics, subtle, fading out by ~150 m
- Grade: teal floor in the shadows, slight desaturation, cool tint
- Vignette and film grain

## 5. Core mechanics

### 5.1 Mana → blood → stardust
- Spells cost mana (each its own `mana` in `data/moves.json`). If mana is short, the **missing amount is paid in HP** at the spell's own `bloodRatio` (HP per mana point; 1.2 for all spells: blood costs 20% more than mana).
- Why above 1: with focus turning mana into HP (33 → 25, 0.76 HP per mana), a `bloodRatio` of 0.6 made blood a profit: 1 mana → 0.76 HP → 1.26 mana of spells. Above 0.76 (1 / 1.32) the round trip loses, so blood is a real sacrifice. The reward has to be worth it: seals, hidden pickups and stronger spells (below).
- Every mana point paid in blood adds `dustPerBlood` % **stardust**. Stardust is permanent by default (`dustDecay` = 0).
- Stardust raises spell power: `power = 1 + dust/100 × dustPower`.
- Stardust raises sight in the dark: `sight = lightBase + dust × dustLight` (+ flare).
- 100% stardust = death ("your mind scattered"). Blood-casting at 0 HP also kills you.

### 5.1b Land and water
- The sea surface is y = 0. With the chest above it the player is **on land**; below it the swimming rules apply (`sink`, `stroke`, `swim`): gravity changes at the surface.
- **Land jump (Blasphemous-like, weighty):** fast committed rise (`jump` 780 px/s against `landGravity` 2800 → ~110 px, ~0.28 s up), heavier fall (`fallGravity` 4600, `maxFall` 1150) so there is almost no float at the apex; ~0.5 s total airtime. Releasing jump early multiplies the rise by `jumpCut` 0.45 (a short hop ~22 px). Air control is committed (`airControl` 0.35 of `groundAccel` 18). `coyoteTime` 0.08 s and `jumpBuffer` 0.1 s keep it forgiving. Landings from speed squash the sprite, kick dust and thud.
- Crossing the surface splashes and slows the fall into the water. A stroke just under the surface **breaches**: a leap out of the water back onto land.
- Enemies stay in the water.
- Above the water: a sunset sky, a distant coastal city with lit windows, ships under sail, headlands, and level props (huts, fire, villagers, pier posts, a pirogue). Underwater post effects (refraction, caustics, god rays, teal grade) stop at the waterline; the sky gets a warm grade.

### 5.2 Mana sources
- Passive regen (`manaRegen`/s).
- Wand melee hits (`manaGain` per enemy hurt), Hollow Knight "soul" style. Nothing for hits that do no damage (an immune enemy is not a mana well).
- A clean **parry** (`manaGain`).
- **Dust vials** (`data/items.json`): +45 mana, +6 % stardust. The stardust drug of the lore: mana now, mind later. Carried (max 3), drunk with Q.

### 5.2b Moves (`data/moves.json`, F2 → Moves)
| Move | Input | What it does |
|---|---|---|
| Wand | J (tap) | Melee, `slash`, 10, gives mana on a hit. |
| Thrust | J held 0.45 s, released | Charged melee, `pierce`, 26, longer reach, lunges forward; a glint and a raised spear show the charge (like Hollow Knight's nail arts). Breaks shells. |
| Star bolt | K | Spell, 25 mana, `star`, scales with stardust. The only thing that opens seals. |
| Flare | L | Spell, 35 mana, `light` burst around you and 300 px of light for 2.5 s. |
| Star leap | Space in the air (land) | Spell, 12 mana: a second jump (+~93 px). Magic as movement: you spend mana all the time, and blood when you overreach. |
| Dodge | Shift | A 0.18 s dash with 0.25 s of invulnerability; 0.7× in the water. |
| Parry | U | A 0.2 s window: a touch inside it does no harm, stuns the enemy 1.4 s, hits it back, and gives 22 mana. |

Each move has a `group` cooldown, a `medium` (`any`, `water`, `land`, `ground`, `air`) and a `noise` that listeners hear.

### 5.2c Stardust as a key
- **Seals** (`seal` on a solid): violet-veined rock with a ring of motes, one per 10 % of stardust it wants, lit for each 10 % you carry. A star bolt opens it only with that much stardust; less glances off ("The seal drinks the bolt. It wants more of your mind."). Paid for with blood, vials, dust zones or pressure.
- **Hidden pickups** (`hidden` on items and wrappers): seen and taken only inside your light, and with a number, only with that much stardust: a mind far enough gone sees it.

### 5.3 Focus
- Hold F, standing still: spend `focusCost` mana over `focusTime` s to heal `focusHeal` HP. Cannot be paid in blood.

### 5.4 Light and being seen (abyss)
- Darkness thickens slowly with depth, capped at 68% (reached around 400 m). You see clearly only inside your light; the post pass keeps the deep dark teal, never dead black.
- Lurkers notice you within `sight` × your light. **More light = see more, and be seen from farther.**
- Flare (L): floods light for its `time`, damages nearby enemies, and makes the distant photophores go dark (something noticed you).

### 5.5 Pressure
- Below `pressureDepth` metres, stardust rises by `pressure` %/s even without casting.

### 5.6 Tunables (current defaults, `tunables.json`; moves, enemies and items are in `data/`)

| Variable | Default | Meaning |
|---|---|---|
| maxHp / maxMana | 100 / 100 | |
| manaRegen | 3 | mana/s |
| dustPerBlood | 0.3 | stardust % per mana paid in blood |
| dustPower | 1.5 | spell damage bonus at 100% dust |
| dustDecay | 0 | stardust %/s recovered |
| lightBase | 160 | px of sight with intact brain |
| dustLight | 2.5 | px of sight per stardust % |
| focusCost / focusHeal / focusTime | 33 / 25 / 0.9 | |
| pressureDepth / pressure | 300 m / 0.15 %/s | |
| swim / stroke / sink | 200 / 260 / 180 | water movement px/s |
| runSpeed / groundAccel / airControl | 230 / 18 / 0.35 | land movement |
| jump / landGravity / fallGravity / maxFall | 780 / 2800 / 4600 / 1150 | land jump arc |
| jumpCut / coyoteTime / jumpBuffer | 0.45 / 0.08 s / 0.1 s | jump forgiveness |

## 6. Controls

←/→ move · Space jump / stroke up (in the air: star leap) · ↓ dive · J wand (hold: thrust) · Shift dodge · U parry · Q dust vial · K star bolt · L flare · F (hold) focus · M mute · R restart. Gamepad: A jump, X wand, Y bolt, B flare, RB focus, RT dodge, LB parry, D-pad up vial.

## 7. Enemies

Types live in `data/enemies.json` (F2 → Enemies): a `behaviour` (`stalker` or `chaser`), a `sense` (what they notice you by), and a damage multiplier per damage type (`resist`: 0 immune with a dry glance, below 0 it feeds on it, above 1 a weak spot with a gold burst). Levels place them with `enemies [{type, x, y}]`.

| Enemy | Where | Behaviour |
|---|---|---|
| **Jelly** | Abyss, Sleeping Generation, Shore pool | Chaser, **drawn to light** (2.4 × your light radius, 2.5× more while a flare burns). Glows; trails threads. **Immune to light** (the flare that saves you from lurkers draws them in), weak to the wand. Contact 8. |
| **Isopod** | Sleeping Generation, Abyss floor, Shore pool | Stalker, **blind: it hears you** (420 px × your recent noise: strokes, jumps, landings, casts, flares, hits). A quiet swimmer passes. **Shelled: the wand glances off**; the thrust pierces (1.5×), bolts 1.25×. Contact 16, lunge 620. |
| **Leech** | Sleeping Generation (by the dust zones), Abyss | Chaser, **smells stardust** (9 px per %): invisible to an intact mind, drawn to a loosened one. **Feeds on star bolts** (they heal it), weak to light. Its touch pulls 4 % stardust out of your skull. |
| **Lurker** (anglerfish) | Abyss shaft | Drifts with lure lit → notices you (light-based) → **lure goes dark**, stalks at ~110–160 px, jaw working → **lunges** with jaws wide → recovers. Contact damage 12. |
| **Escaped thought** | Pocket universe | Pale flesh mass with several eyes and tendrils; chases within 320 px. Lore: a creature that escaped from a cranium. |
| **Leviathan** | 3D background (not hittable) | 48-segment eel body with spines and needle teeth. Far passes behind you at depth > 120 m; every third pass at depth > 180 m it slides **between the camera and you**, lit only by your light, with a pale eye. Groans on each pass. |

## 7b. World: areas and biomes

The game is a set of **areas**. Each area is a **biome**: its own ground and liquid, palette and light, creatures, hazards, sound, lore, and **one twist on the core mechanic** (mana → blood → stardust). Levels are built inside an area; an area can hold several levels.

### What a biome defines
| Part | Examples |
|---|---|
| Palette and sky | sea: teal to purple-black, sunset sky above; blood lake: purple storm sky over red water |
| Liquid and its rules | sea water (swim, sink, pressure); swamp water (shallow, wading); blood (see Lake of Blood) |
| Terrain | tile family for land, rock style for walls, props (huts, logs, corpses) |
| Background | 3D decor (skulls, tower) or painted layers; fog colour and density |
| Creatures | the enemy roster of `data/enemies.json` that lives there, and its boss |
| Hazards | dust zones, pressure, spawners, liquids that hurt |
| Twist on the core mechanic | what blood, mana or stardust means here |
| Sound | drone colour, creature voices, ambience |
| Fear | each biome has its own phobia, as the sea has thalassophobia |
| Lore | who lived or died here, and why the place is the way it is |

### Areas
| Area | Biome | Levels | Status |
|---|---|---|---|
| The Shore of Kingdoms | Sunset coast: villages, rooftops, the monks' tower, the pier | `levels/shore.json` | Built |
| The Sea | Deep ocean: darkness, pressure, giant infant skulls, lurkers, leviathans | `levels/abyss.json`, `levels/corpses.json` | Built |
| Inside the crack | A dreaming cosmos: low gravity, galaxy backdrop, escaped thoughts | Fixed pocket room | Built (one room) |
| The Lake of Crocos | Crocodile swamp: mist, black-green water, mound huts and stepped temples; horrifying, hostile crocos | — | Concept, **late game** |
| The Lake of Blood | A river of blood in a dry savanna, choked with corpses and a mosquito swarm | — | Concept |

### The Lake of Crocos (concept, late game)
A late-game area: the player arrives with the full moveset, and the crocos are among the hardest enemies.

Art: `docs/Concept-Arts/CrocoMage.jpeg`, `CrocoMage2.jpeg`, `CrocoMage3.jpeg`, `LakeOfCrocos.jpeg`.
- **Look:** purple-to-amber dusk, low mist on still black-green water, algae and drowned logs, dead trees hung with moss, rock arches and spires. Mound huts of earth and grass; stepped temples behind them.
- **The crocos:** horrifying, hostile creatures, not people to meet. The art shows them living like a tribe (mound huts, temples, a throne): warriors with spears, round shields, bone and leather armour; plain crocodiles basking and lurking in the shallows.
- **The Croco Mage:** a powerful mage, but **not necessarily smarter than a human mage**: the skulls' power swelled his cranium, not his mind. His cranium has swollen far beyond any wrapper: veined, translucent, one blind white eye. The three CrocoMage images show the skull growing: **the stardust mechanic, on someone else.** In `LakeOfCrocos.jpeg` he sits enthroned, old and robed, with a staff hung with glowing vials, books and a scroll at his feet.
- **Lore (open):** how the Croco Mage found the power of the skulls and got high on it, to the point of such a cranium. Still to be found.
- **Fear:** what waits under shallow, murky water.
- **Gameplay ideas (proposals):**
  - Shallow water you wade through: slower, no swimming, and you cannot see what is under it. Crocodiles wait there.
  - Shield warriors that block the wand from the front; the charged thrust breaks the guard. This needs a resistance that depends on which way the enemy faces.
  - Mosquitoes in the reeds, kin to the Lake of Blood's swarm.
- **Twist on the core mechanic:** open.
- **Open questions:** the Croco Mage's origin (above). Is he the area's boss, fought in stages as his skull swells?

### The Lake of Blood (concept)
Art: `docs/Concept-Arts/LakeOfBlood.jpeg` to `LakeOfBlood4.jpeg` (escalating: the swarm, then eggs and larvae, then human faces, then the brood mother).
- **Look:** a winding river of blood through a dry savanna under a purple storm sky; red acacias, rock spires, distant mountains. Corpses and skeletons half-sunk everywhere.
- **Creatures:**
  - **Mosquitoes:** giant, spined. Later ones have skull-like or human faces, and their abdomens swell red with blood they drank.
  - **Egg clusters:** dark pods in the blood.
  - **Larvae:** pale grubs crawling over the corpses, some with faces.
  - **The brood mother:** a bloated grub with about ten eyes, spindly legs and vestigial wings. The area's boss.
- **Fear:** the swarm; insects that drink you.
- **Gameplay ideas (proposals):**
  - Mosquito: a flying chaser (the first flying enemy above land) that senses **blood**: it notices you while you bleed or just after you blood-cast. Its bite drains HP and heals it, and its abdomen visibly swells.
  - Egg clusters: destructible spawners that release larvae on a timer, so you choose between pushing on and clearing them.
  - Larvae: slow ground crawlers (the first enemies that walk).
  - The brood mother: a boss fight in the blood.
- **Twist on the core mechanic (proposal):** the lake is blood you did not pay. Spells cast while standing in it take their blood price from the lake instead of from you, but blood in the water draws the swarm.
- **Open question:** whose blood is it? A battle, a massacre of blood-casters, or something the mosquitoes carried here? The human faces suggest the swarm carries what it drank.

### What the engine needs for biomes (proposal)
- **Today:** a level sets its fog `palette`, `props`, 3D `decor` and per-solid `tile` style; the sky (sunset), the liquid (sea water with fixed rules), the tileset (Craftpix World Tree Roots) and the drone are the same everywhere.
- **Needed:** a level names its biome, and `data/biomes.json` defines each one: sky colours, liquid (colour and rules: swim, wade, blood), tile family, background, drone and ambience, and the creature roster. Like moves and enemies, it is data, edited on the F2 pages.
- **Enemies:** flying (not held under the surface), walking (gravity on land), spawners, bosses, a new `blood` sense, and resistance that depends on facing.
- **Art:** the concept art sets mood and colour. Creatures in game need **side-view sprite sheets** (consistent frames on a plain background, as the player's sheet), or a procedural drawing first, as the jelly and the leech have.

## 8. Level: the Abyss (prototype)

- Vertical shaft, 520 m deep (10 px = 1 m), jagged walls and ledges, surface light fading by ~90 m.
- 3D layers: lumpy rock spires, marine snow, surface light shafts, far leviathan, photophore rows far below.
- **The drowned tower** (3D background): visible as a murky silhouette near the surface, its burst cranium crown glowing faint violet; at depth only ~12 flickering amber windows (some dead) remain, drawn through the darkness.
- **Seafloor graveyard:** a giant half-buried skull with a galaxy visible through a crack (it lights the graveyard), 18 smaller skulls, ribcages, scattered bones.
- **Playfield skulls (3D, Blender model):** two half-buried infant skulls standing in the playfield itself, cracks pinned to the gameplay plane so hitboxes line up at any camera position.
  - **The dreamer** (scale 2.6, tilted face-down into the silt): its crack is the **portal** → pocket universe. Inside: a dimmed galaxy sphere. **Stardust pours out of the crack** (the dreaming mind evaporating, per the lore): violet, gold and white motes rising and swirling, 30/s, up to 120/s as you come near; the first time you come within 320 px it surges out (burst + low hiss). Its pulsing light opens a ~300 px gap in the darkness.
  - **The empty one** (scale 1.25, on its side): its crack is empty ("Whoever dreamed here is gone").
  - 2D drawn skull and cracks remain only as a fallback if the skull model fails to load.
- **Pocket universe:** open space, low gravity, galaxy backdrop, floating platforms, escaped thoughts, golden altar = end of run ("A mind, still dreaming inside its skull"). Blue crack returns to the seafloor.

## 9. Audio (procedural, Web Audio)

- Drone: low-passed brown noise + beating low sines; louder and darker with depth.
- Leviathan groans (far: quiet; near: loud with screen shake).
- Heartbeat driven by **fear** = depth + missing HP + nearby lurkers + near pass. Tempo 50–140 bpm, silent below 0.3 fear.
- SFX: cast hiss, blood-cast wet hiss, flare sweep, hurt thump, lurker lunge hiss, swim stroke.

## 10. Lore

- Themes: thalassophobia, cosmic horror, magic, conspiracies.
- A **cosmic drug** created the first generation of mages. Ocean floors hold the giant corpses of babies with giant skulls.
- Some skull cracks are empty; others contain **universes**. One single most giant skull holds an entire dungeon.
- Mages grow large, increasingly fragile craniums and wear protective wrappers/helmets (aesthetic, give bonuses).
- Wands are made from the craniums of former mages.
- When a cranium breaks, **stardust** leaks out: the mind evaporating. It can alter reality, and usually means the mage is dying and disappearing.
- Some mages harvest stardust and mix it into drugs that boost magic. They prey on other mages.
- Ancient monks could warp reality with their minds but had to discipline them to avoid conjuring calamities.
- Some creatures were created by minds and escaped from craniums.
- Some places exploded the skulls of the mages who imagined them.
- Mages are a minority: tribes and kingdoms have their own business and wars.
- Mages are not only human: the Croco Mage of the swamp (7b) found the power of the skulls and got high on it, until his cranium swelled past saving. He is powerful, but not necessarily smarter than a human mage. How he found that power is still open.

## 11. Planned / backlog

- [ ] **Escaped calamities:** at high stardust, your leaking mind spawns hostile creatures.
- [ ] **Helmets** as builds: each modifies crack thresholds, light, pressure resistance, blood ratio.
- [ ] **Skull wands:** each carries a dead mage's signature spell.
- [ ] **Giant skull dungeon** (playable).
- [x] Spells as traversal: the star leap. Still open: blink, levitate, wall-break.
- [x] Stardust drug, the consumer side: dust vials. Still open: harvesting dust from mages, predator mages.
- [ ] Decide whether tribes and kingdoms are met as people (quests, factions) or stay background. (The crocos are not people: hostile creatures, 7b.)
- [ ] **Areas and biomes** (7b): `data/biomes.json` and a `biome` per level; the Lake of Blood with the mosquito, egg clusters, larvae and the brood mother; the Lake of Crocos, late game.
- [ ] Lore: how the Croco Mage found the power of the skulls.
- [ ] Sound pass: authored ambience, creature voices.

## 12. Assets

Pipeline: modelled in Blender through the Blender MCP, exported as GLB (vertex colours as 8-bit RGBA), loaded by raylib.

| Asset | Source | Notes |
|---|---|---|
| `assets/giant_skull.glb` | `assets/giant_skull.blend` | Infant-proportioned skull: large lumpy cranium, shattered crown hole with hairline fractures, angular orbits, nasal aperture, crooked fangs (two missing), dislocated gaping mandible. ~26k tris, 880 KB. Vertex colours baked: bone, grime in cavities, silt staining low down, yellowed teeth. Separate `Universe` sphere inside gets the galaxy texture in game (back faces, no fog), seen through the crack. Faces +Z in glTF. Used at scale 12 (giant) and 0.8–3 (graveyard). |

| `assets/drowned_tower.glb` | `assets/drowned_tower.blend` | The drowned tower: a ~62 m leaning mage spire. Flared root claws, irregular swollen sections, five twisting buttress ribs, four broken balconies with hanging stone spurs, three secondary spires growing out like tumours, pinched neck crowned by a burst cranium dome (crack facing the camera). Dark encrusted stone vertex colours, bone crown. ~10.7k faces, 485 KB. In game: scale 0.75, behind the shaft at z −40, crown just under the surface. |

Previews render to `assets/_preview/` (Workbench), since the Flatpak Blender cannot share `/tmp` with the MCP.

## Level: The Sleeping Generation (`levels/corpses.json`)

Second level, inspired by the concept art in `docs/` (the art is inspiration, not canon). 2400 px wide, 420 m deep.
- **New (2026-10-28):** jellies in the funnel and trench, leeches by the dust zones, isopods on the trench floor; three dust vials (one hidden, 15 % stardust); the mana wrapper's alcove is closed by a 20 % stardust seal (the nearby dust zone can pay for it).
- **Funnel (0–90 m):** narrow shaft on the left; a rock mass fills the right. First inscription: "They were born dreaming. Nobody woke them."
- **Trench (90–270 m):** opens under a ceiling of hanging rock teeth into a wide trench of broken bone shelves. Giant infant skulls lie in the 3D background at several depths. Two **dust zones** (places that burst the skull of the mage who imagined them) bend the water and add stardust while you stay inside. A **mana wrapper** hides in an alcove in the left wall; a **dust wrapper** hides under the ceiling teeth on the far right.
- **The city-skull (270–420 m):** a jagged dome of shell with a gap high on the left, set inside a giant 3D skull whose universe glows through its crack. Tiers inside hold tombs and houses lit by warm lights, a **protection wrapper**, and four **escaped thoughts**. The dreaming portal skull lies on the floor at the centre; the empty crack is outside the dome on the left.
- Inscriptions carry the lore ("Each skull is a prison, a cosmos, a memory."; "The first mages drank the stars. These are their children.").
- Palette: surface `#173f4f`, deep `#06040c` (purple-black).

### Land prologue
A cliff-top fishing village at sunset (two huts, a fire, villagers watching the sea), a rocky step down to a pier with a villager at its end and a pirogue moored below, a far headland with another hut and fire. Inscription at the pier: "The ocean gives. The ocean takes." Walk the pier, dive, and the funnel begins.

### New level mechanics (native build)
| Field | Effect |
|---|---|
| `wrappers` | Pickups. `mana` +3 mana regen; `focus` focus time ×0.6; `protection` +25 max HP and heal; `dust` stardust per blood ×0.6. Reset on restart. |
| `dustZones` | Inside: +`rate` stardust %/s, strong refraction/aberration warp, HUD warning. |
| `texts` | Inscriptions fade in within ~220 px; a whisper plays the first time. |
| `lights` | Static lights: open the darkness and glow. |
| `thoughts` | Escaped-thought enemies in the sea level. |
| `decor` | 3D background placement: `skull`, `giant` (with a universe, light and darkness gap), `tower` (with lit windows). |
| `palette` | Fog/background colours from surface to deep. |
| `top` | Highest y of the world (negative = land and sky above the sea; default −140). |
| `props` | Land dressing: `hut`, `villager`, `fire`, `post` (pier post, hangs down 260 px), `boat`, or `sprite` with `name` (any `assets/tileset` image), `scale`, `flip`. |
| solid `tile` | Land tile style: `ground`, `float`, `bridge`, `stone`, `box`. |

## Level: The Shore of Kingdoms (`levels/shore.json`)

A land level for testing jumps and platforming, sized to the jump (rise ~110 px, ~115–130 px horizontal at full run). 4300 px wide, 150 m of sky, a 130 m pool at the end. Falls never strand you: a lower path runs under everything.
1. **Village** (start): huts, fire, villagers on flat ground.
2. **Stairs:** four 70 px steps up.
3. **Rooftops:** thin platforms (two with huts) with 50–80 px gaps and ±70 px height changes.
4. **The monks' tower:** two walls, open at the bottom for the lower path; ten alternating ledges 95 px apart and 80 px across, ~1000 px of climb. Inscription: "The monks climbed to quiet their minds."
5. **The ridge** at −115 m: plates with 90 / 100 / 70 px gaps; a ledge one full-height jump (110 px) above holds a **focus wrapper**. A villager looks out; "From here you can see the kingdoms."
6. **Descent:** stepping stones down to the **pier**; "The ocean gives. The ocean takes."
7. **The pool:** two lurkers, a jelly and an isopod, a mana wrapper, the dreaming portal skull and the empty crack; a giant skull and the drowned tower behind.
Lower path: crates to hop, a fire and a villager.
8. **Above the focus ledge:** a small ledge 160 px up and to the right holds a dust vial. A jump reaches ~109 px; only the star leap (Space again in the air) gets there.

## Tooling

- **Level editor server:** `python3 serve.py` (stdlib only; binds 127.0.0.1) serves the editor, and accepts `PUT /api/levels/<name>.json` (name restricted to `[A-Za-z0-9_-]`, body must be a level object, written atomically). `GET /api/levels` lists levels. three.js r128 + GLTFLoader are vendored in `vendor/`.
- **Levels** are data in `levels/*.json`: `name, width, height (seafloor, px), spawn, portal, emptyCrack (crack centres), solids [{x,y,w,h}], lurkers [{x,y}]`. The game takes the level path as its first argument (default `levels/abyss.json`); `--spawn x,y` overrides the start. `levels/abyss.json` is the former procedural shaft, exported with the same seed.
- **Editor** (`editor.html`): pan/zoom canvas with depth ruler; tools: select/move (corner-resize walls), draw wall, lurker, spawn, portal crack, empty crack; inspector for level name/width/depth and selection x/y/w/h; undo/redo, duplicate, nudge, 10 px snap, frame; Save, Copy play command, Copy from view.
- **Native game** (root: `src/`, `shaders/`, `CMakeLists.txt`; raylib 5.5, C++20): the game (the web prototype it was ported from was removed on 2026-10-27). Layers: 3D background → 2D world + darkness (premultiplied) → 3D near pass → post shader (`shaders/post.fs`, same effects as the web pass) → HUD. 3D models from `assets/*.glb` (vertex colours converted to 8-bit RGBA for raylib), lighting/fog in `shaders/scene.*` with 4 point lights. Procedural audio (`src/synth.hpp`). Code layout: `World` (simulation, no drawing) / `Scene3D` / `Renderer` (passes, scenery, actors, HUD) / `Panel` (generic live-edit list: the tune menu is one, any object can have its own) / `Character` + `Animator` (data-driven sprite sheet and animation state machine, `characters/*.json`). Text is rasterised at its exact pixel size per size used (font cache) and drawn on whole pixels so it stays crisp; inscriptions are drawn in screen space. Tunables in `tunables.json`: edited live in the **F2/Tab tune menu** (grouped Land, Water, Body, Blood & stardust, Spells, Light, Enemies; pauses the game). Keyboard: arrows select/adjust, Shift ×10, Ctrl ×0.1, Backspace resets, S saves. Mouse: hover, click a row, wheel scrolls, −/+ buttons, drag a value to scrub (6 px per step), right-click resets, Save / Defaults / Close buttons, or by hand and reloaded with F5 together with the level. Gamepad supported. `--shot N file.png` renders headless for checks.
- Not yet editable in the editor: the pocket universe layout, and the native-only fields (wrappers, dust zones, texts, lights, thoughts, decor, palette); the editor preserves them when saving.

## 13. Prototypes

| File | What | Status |
|---|---|---|
| `editor.html` | Level editor for `levels/*.json` (needs `serve.py`); copies the command to play a level | Active |
| `src/`, `shaders/` | The game: native raylib C++ | Active |
| `characters/*.json` | Characters: sprite sheet + animation state machine (format in README) | Active |


## 13. Changelog

- **2026-10-28** Fixes: seam lines between tiles (textures now clamp instead of repeat at their edges); light holes and inscriptions no longer jitter against the world during screen shake; fonts no longer leak on every window resize; the far leviathan no longer hangs frozen during the near one's passes; a leech's bite can be the stardust that kills you, with its own message; malformed atlases and `\u` escapes fail cleanly; the editor opens levels without `lurkers`.

- **2026-10-28** The Lake of Crocos is a late-game area of horrifying, hostile crocos; the Croco Mage is powerful but no smarter than a human mage, and his origin is open.
- **2026-10-28** World structure: areas, each its own biome (7b). Two new areas from the concept art in `docs/Concept-Arts/`: the Lake of Crocos and the Lake of Blood.

- **2026-10-28** Land tiles: the grass top-left and top-right corners were swapped (a notch of sky at every step); the last column of a solid is now flush with its right edge, so the rocky rim shows when the width is not a multiple of 64; bridges use only the plank that tiles (the narrow piece left gaps); one-tile-wide pieces get both end caps; grass stays under crates and stones, and is replaced by dirt only where higher ground actually rises from it.

- **2026-10-28** Moves, enemies and items are data (`data/moves.json`, `enemies.json`, `items.json`), edited on new F2 pages (Tune → Moves → Enemies → Items) and saved back. New moves: charged thrust, dodge, parry, star leap. Per-spell blood rate, now 1.2 (blood used to be cheaper than mana, and with focus a profit). Damage types and per-enemy resistances (immune, absorb, weak), shown by glance, heal and gold bursts. New enemies: jelly (light), isopod (noise, shelled), leech (stardust). Dust vials (mana for stardust). Stardust seals and hidden pickups. Levels: content placed; a sealed alcove in The Sleeping Generation; a ledge on the Shore only the star leap reaches. Fixed: two wrappers in The Sleeping Generation were out of reach since the player's hitbox grew (mana, protection). Tests: `tests/gameplay_test.cpp`.
- **2026-10-28** Fixes: bodies no longer pass through rock where solids overlap; a level path relative to your shell works; the tune menu keeps multiplicative wrapper bonuses right. `tests/collision_test.cpp`.

- **2026-10-28** Code split from one 1550-line `main.cpp` into modules (world, character, level, tunables, input, ui, panel, scene3d, renderer + scenery/actors/hud). The player's animation is now data: `characters/mage.json` (sheet + state machine, same behaviour as before, checked by `tests/character_test.cpp`). The tune menu is a generic `Panel`, ready for per-object editing. F5 also reloads the character; a failed reload's banner is no longer overwritten by the level title.
- **2026-10-28** Repo cleanup: the game moved from `port/` to the root (`CMakeLists.txt`, `src/`, `shaders/`, `tunables.json`; build in `build/`). Added `.gitignore` (build output, Blender backups, preview renders, the Craftpix zip).
- **2026-10-27** In the water the player drifts on a slow idle cycle; the run cycle is land-only.
- **2026-10-27** Fix: the level's left and right edges are walls for the player (the land in The Sleeping Generation extended past the level width, so you could walk off-screen where the camera stops, or off the cliff into the void).
- **2026-10-27** Craftpix World Tree Roots tileset: land solids tiled (ground/float/bridge/stone/box) through a desaturate-and-warm shader; sprite props from the pack. Shore and Sleeping Generation dressed with signposts, fences, bushes, rocks, roots, ivy, barrels, crates and the stone idol on the monks' ridge.
- **2026-10-27** Portal skull: the eye is gone, stardust pours out of the crack instead. New platforming level `levels/shore.json`. Tune menu: full mouse support. Crisp text (per-size font rasterisation, pixel-snapped). Solid rock below the waterline no longer gets water light (god rays, caustics, teal lift).
- **2026-10-27** Weighty Blasphemous-like land jump (separate rise/fall gravity, jump cut, coyote time, jump buffer, committed air control, landing squash/dust/thud); water keeps its own gravity. F2/Tab in-game tune menu for all 38 tunables, saving to `tunables.json`.
- **2026-10-27** Land before the dive: run/jump physics above the sea, splash and breach, sunset sky with distant city and ships, land props; The Sleeping Generation now starts in a cliff village with a pier. Ocean much brighter: darkness cap 90% → 68% over 400 m, lightBase 85 → 160, lighter fog/ambient/default palette, stronger teal shadow floor, softer vignette. HUD text shadowed.
- **2026-10-27** Player enlarged (sprite 60 → 96 px, hitbox 20×34 → 28×70, spear reach 52 → 70 px) and never rotates (lean, dive tilt, recoil and hurt jolt removed).
- **2026-10-27** Player is now the sprite from the concept sheet (`assets/player.png`, cut by `tools/extract_player_sprites.py`), replacing the procedural mage rig and its cloth.
- **2026-10-27** Web version removed (abyss.html, index.html, tunic.html, vendored three.js, `.gltf.json` copies). raylib is the only build. The editor stays; its Play buttons now copy the native run command.
- **2026-10-27** Native raylib C++ port (`port/`). New level `levels/corpses.json` (The Sleeping Generation) with wrappers, dust zones, inscriptions, lights, thoughts, 3D decor and palette. Tower exported as `assets/drowned_tower.glb`; GLB vertex colours converted to RGBA8.
- **2026-10-26** Tooling: levels are JSON (`levels/abyss.json`), local dev server `serve.py`, level editor `editor.html`, vendored three.js; game runs offline.
- **2026-10-26** Abyss v3.5: procedural character rig (legs, arms, lean, squash, per-action poses) with verlet cloth; WebGL post-processing pass (refraction, caustics, god rays, bloom, grade, aberration, vignette, grain); HUD on its own canvas; ocean less dark (darkness cap 90% reached at ~220 m, lightBase 85, teal shadow floor).
- **2026-10-26** Abyss v3.4: the drowned tower, second Blender asset, in the background with lit windows that pierce the dark.
- **2026-10-26** Abyss v3.3: the seafloor portal skull is now the 3D Blender skull in the playfield, with a tracking bloodshot eye (galaxy iris) inside its crack; second 3D skull holds the empty crack.
- **2026-10-26** Abyss v3.2: first Blender asset. `assets/giant_skull.gltf.json` (source `assets/giant_skull.blend`) replaces the giant skull and the 18 graveyard skulls; primitive skulls remain as a fallback while it loads.
- **2026-10-26** Abyss v3.1: fixed a fully black screen (the foreground near-pass canvas was clearing to opaque black).
- **2026-10-26** Abyss v3: art direction reset to raw/non-cute (lumpy noise geometry, muted palette, grain), gaunt mage with wrapped riveted helmet and skull wand, lurkers stalk and lunge with lures going dark, eel leviathan with near pass in front of the camera, photophores below, procedural audio (drone, groans, heartbeat), camera framed to show more abyss below.
- **2026-10-26** Abyss v2: seafloor skull graveyard, empty/universe cracks, pocket universe, diving helmet.
- **2026-10-26** Abyss v1: vertical descent, cranium light, flare, lurkers, pressure, far leviathan.
- **2026-10-26** Side-scroller and isometric prototypes.
