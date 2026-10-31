# Braindust art documentation

This folder is the visual reference library for Braindust. It establishes mood, silhouette, colour and production targets; concept art is not automatically canonical game content.

## Canonical player direction

The player is the braided African warrior-mage shown in `Frame-Concepts/`: four extremely long woven braids, an ivory African-inspired wrapper/headdress, light layered clothing, long cloth panels and sash, and tarnished-gold ornaments. The gameplay view is a polished, hand-painted three-quarter side view facing right. The ivory, black-braid, gold and violet-stardust silhouette is the visual baseline for all new player work.

The current game atlas is **transitional**: its eight walking frames use this direction, while idle and attack frames are older art. New idle, movement, wand/combat, cast, dodge, hurt, swim and death frames must be redrawn or rig-baked from the canonical design before the player art is considered complete.

## Asset status

| Location | Status | Use |
|---|---|---|
| `Concept-Arts/` | Reference library | Mood, palette, biome briefs, costume and creature direction. Do not load directly in-game. |
| `Frame-Concepts/walk-*.png` | Animation source/reference | The eight supplied walk poses. They are imported into the player atlas but still need a coherent rigged or corrected production cycle. |
| `Rigs/2D Character Rigging Asset Sheet.png` | Production reference | Separation plan for a future Krita/Blender cutout rig. |
| `assets/player.png` + `assets/player.json` | In-game asset | The current runtime atlas and its layout metadata. |

## Biome briefs

| Biome | Palette and fear | Motifs | Key references |
|---|---|---|---|
| The Sea | teal-black water, tarnished brass, violet stardust; thalassophobia | infant skulls, cracks containing universes, drowned towers, bone fields, sparse light | skull/ocean/atlas art in `Concept-Arts/` |
| Lake of Crocos | amber-purple dusk, black-green water, mist; fear of what waits beneath shallows | mound huts, stepped temples, drowned logs, hostile crocodile warriors, swollen Croco Mage | `CrocoMage*.jpeg`, `LakeOfCrocos.jpeg` |
| Lake of Blood | purple storm, crimson water, dry savanna; swarm/insect horror | corpses, red acacias, mosquito brood, eggs, larvae, human faces in the infestation | `LakeOfBlood*.jpeg` |
| Mage and monk civilization | weathered ivory, tarnished gold, deep blue/black; awe and ruins | wrappers, headpieces, ritual keepers, libraries, academies, coastal settlements | keeper, academy, headpiece and settlement art |

## Production rules

- Game-critical characters and enemies need consistent side-view sprites or a rig; full-scene concept art is reference only.
- Preserve the readability hierarchy: sharp/high-contrast gameplay in front; fogged, soft 3D scenery behind it; no background element should resemble a platform.
- The player must read at a glance by head wrapper, four braids, cloth silhouette and stardust light—not by tiny ornament detail.
- Use the animation workflow in [`../spec.md`](../spec.md): one canonical cleaned design, fixed root and foot baseline, rigged motion, then paint-over correction.
- When an image becomes a gameplay source, record its source file, intended animation/state, pivot/baseline and export path alongside it.
