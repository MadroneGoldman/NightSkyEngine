# 05 Collision and animation

## Collision data = cels

`UCollisionData` (`Data/CollisionData.h`) is an array of `FCollisionStruct`, one per **cel**:

| Field | Meaning |
|---|---|
| `CelName` (tag) | Identity, e.g. `Manny.5A.02`. States reference it from `SetCel`. |
| `Anim[]` | `{PartName (default "Body"), AnimSequence or Flipbook}`. Multiple parts are allowed. |
| `AnimFrame` | Which frame of the animation this cel shows. |
| `AnimBlendIn` / `AnimBlendOut` | Montage blend times when the animation changes. |
| `Boxes[]` | `FCollisionBox` list |

`ABattleObject::GetBoxes()` searches `CommonCollisionData` and then `CollisionData` for the current
`CelName` (the character's cel wins if both have one) and copies the boxes, animation parts and frames.
It also reads the **blend cel's** `AnimFrame` and offset box. Lookup is a linear search by tag, and an
unknown tag returns an empty cel: no boxes, no animation, **no error**.

Cels are edited in the custom **Collision Editor** (open a `DA_*Collision` asset). It previews the
animation on a player class and lets you draw boxes. See the upstream "Collision Data" page.

## Box types (`EBoxType`, `Battle/Misc/CollisionBox.h`)

| Type | Use |
|---|---|
| `BOX_Hurt` | Can be hit |
| `BOX_Hit` | Hits hurtboxes while `ATK_HitActive` |
| `BOX_Offset` | **Root-motion-like offset.** When the cel is set, the object moves by the change in the offset box's X/Y since the previous cel (`SetCelName`). The blend cel's offset is used to interpolate visuals. Offsets reset on state change. |
| `BOX_Custom` + `CustomType` tag | Your own boxes. Query with `CheckBoxOverlap` / `GetBoxPosition`, and with `SetHitCustomCollision`, run custom collision (`HandleCustomCollision_PreHit` / `_PostHit` Blueprint event). |
| `BOX_Special_Hurtbox` / `BOX_Special_Hitbox` | Special-purpose variants |
| `BOX_Push` | Not implemented. Push collision comes from the character defaults (`StandPushWidth/Height`, `CrouchPush*`, `AirPush*`) plus `SetPushWidthExtend`. |

Box coordinates: `PosX/PosY/SizeX/SizeY` are engine ints (`1000` = 1 unit). Editor-only mirrors
are `*Edit` (= /1000) and `UE*` (= Unreal cm, divided by `COORD_SCALE`). Positions are relative
to the object's origin and mirrored by facing. Collision data written through scripts or JSON can
round-trip the editor-only float fields with tiny float noise. That's harmless, because only the ints are used in game.

## How animation follows cels

* The battle simulation never ticks animation in real time. Each frame, the object's
  `AnimStructs` / `AnimFrame` / `BlendAnimFrame` / `FrameBlendPosition` (from the cel) are applied in
  `UpdateVisuals*`. The `UNightSkyAnimInstance` plays the cel's sequence as a dynamic montage in the
  `Root` slot (`CreateRootMontage`), and its `Montage_Advance` override ignores delta time and calls
  `Montage_SetPosition(RootMontage, CurrentAnimTime)`, so the pose is always the frame the cel asks for.
  That keeps visuals deterministic under rollback. Your anim Blueprint needs a `Root` slot.
* `SetCelAndBlendCel(A, B, N)` blends from cel A's animation frame toward cel B's over the cel's
  duration. That's how 3D characters get smooth motion from a handful of key cels.
* `NightSkyAnimSequenceUserData` (asset user data on anim sequences) and `NightSkyAnimMetaData`
  attach engine metadata to animations, such as root-motion curves used by `ApplyRootMotion`.
* Sprite characters (based on `BP_SpritePlayerObject`) use Paper2D flipbooks in `Anim[].Flipbook`. Upstream's
  sprite sample character, KungFuMan, was removed from this project in Nov 2026, so there's no sprite example left here.
  `FlipbookData` maps them.
* `bMirrorWhenFlip` (this fork, on `BattleObject`) and `bMirrorAnimationsWhenFlip` (`PlayerObject`)
  control mirroring when facing left. `SocketRotationOffset` and `SocketNameLeft` are this fork's socket-attach tweaks.
* Materials: `MaterialData` + `ColorIndex` select per-colour materials. `MulColor`/`AddColor`/`Transparency`
  (with fade speeds) and `DamageColor` drive material parameters, and `MPC_Chara` (a material
  parameter collection) is set by the game states each tick (`OrthoBlendActive`, `CameraPosY`) for the ortho-blend
  character material functions (`MF_OrthoBlend*`).
