# Ustaad — Himachal bus driver

Game-ready driver character for OpenCircle (UE 5.8). Built from scripts, so it can be rebuilt any time.

## Rebuild
```
blender -b -P Tools/Blender/characters/create_ustaad.py                      # model, textures, FBX
blender -b SourceArt/Characters/Ustaad/Ustaad_BusDriver.blend -P Tools/Blender/characters/render_views.py -- D:/OpenCircle/SourceArt/Characters/Ustaad
```
Requires the **MPFB2** Blender extension and the **MakeHuman system assets (CC0)** pack in MPFB's user data folder.

## Contents
| File | What |
|---|---|
| `Ustaad_BusDriver.blend` | Editable source (MPFB human, shape keys live), textures packed. Collections `Ustaad_Character`, `Ustaad_Outfit`, `Studio` (lights + Cam_front/side/back/three_quarter/face) |
| `Ustaad_BusDriver.fbx` | UE export: 1 skeleton (53 bones), 18 skinned meshes, ~59k tris, 1.72 m, origin at feet |
| `Textures/T_Ustaad_*_D.png` | Skin, hair, brows, uniform (khaki + leather belt), shoes, cap, scarf, moustache |
| `turnaround.png`, `preview_*.png` | Inspection renders |

## Skeleton
MPFB `game_engine` rig using **UE Mannequin bone names** (`pelvis`, `spine_01..03`, `clavicle_l`, `hand_r`, fingers…),
A-pose. In UE 5.8 retarget Manny/Quinn animations with an IK Retargeter.
No jaw/face bones: the moustache and cap are weighted to `head`, the scarf to `clavicle_r` + `spine_03`.

## Modelled parts
Body/face (MPFB, middle-aged South-Asian phenotype + face targets), short black hair, brows, lashes, brown eyes,
teeth, tongue; khaki shirt + trousers (recoloured MakeHuman casual suit) with rolled sleeves and cuffs; brown belt;
dark leather shoes; green velvet Himachali topi with red piping and felt crown; off-white gamchha with red woven
bands and fringe over the right shoulder; moustache (layered alpha shells); wrist watch; red kalava thread; blue pen.

## Known limits
- Face/skin come from MakeHuman CC0 assets: realistic but not photoreal. For a hero close-up, use MetaHuman.
- Scarf is a static drape (no cloth sim); add Chaos Cloth in UE if it should swing.
- Driving poses (sit, steer, shift, pedals, look left/right, walk, board/exit) are animations to retarget or
  author in UE; the character ships in a neutral A-pose.

## Licences
MakeHuman base mesh, skins, hair, clothes: CC0. Generated textures and modelled items: project-owned.
