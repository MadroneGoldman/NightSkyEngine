# Elemental hits: Special Hit Action, Special Hitbox, Hit Custom Collision

NSE's only elemental system. The engine moves the element from an attack to whatever it touches; **state Blueprints
decide what it does**. No C++ in NSE reads `SpecialHitAction`.

## Pieces

| Piece | Where | What |
|---|---|---|
| `ESpecialHitAction` | `Battle/Objects/BattleObject.h` | `HACTS_None`, `HACTS_Fire`, `HACTS_Water`. BlueprintType since 2026-11-27 (Madrone change, for tools) |
| `FHitData::SpecialHitAction` | `BattleObject.h` | The element of a hit. Set with `MakeHitData(..., Special Hit Action)` then `SetNormalHit` / `SetCounterHit` |
| `ATK_CustomCollision` | `Battle/Misc/Bitflags.h` ("Hit Custom Collision") | `ABattleObject::SetHitCustomCollision(bool)`. Only an object with it starts an elemental meeting |
| `BOX_Special_Hitbox` / `BOX_Special_Hurtbox` | `Battle/Misc/CollisionBox.h` | Cel box types. Normal hits ignore them |
| `ABattleObject::HandleCustomCollision_PreHit(Other)` | `BattleObject.cpp` | The meeting. `ANightSkyGameState::HandleHitCollision` calls it for each pair before clash and normal hit collision |

## The meeting

`this` = the attack, `Other` = what it touches. It happens when all of these hold:
- `this` has `ATK_CustomCollision`, `ATK_IsAttacking` and `ATK_HitActive`.
- `Other` has `ATK_HitActive`. It doesn't need to be attacking.
- `this`'s Special Hitbox overlaps `Other`'s Special Hurtbox **with the same CustomType** (`CheckBoxOverlap`). In this
  project the tags are `Collision.Fire` and `Collision.Water`, and the box view prints them beside the boxes.
- The pair is two different players' objects. With the Madrone neutral-object change, any pair with a neutral battle
  prop also counts.

Then (the same for player/player, object/object and player/object pairs):
1. `Hitstop = 16` on both; both lose `ATK_HitActive`. For two players, both also get their attacks, self-cancel and forward dash re-enabled.
2. `Other->ReceivedHitCommon = HitCommon`, `Other->ReceivedHit = InitHitDataByAttackLevel(false)` (the attack's
   NormalHit with unset values filled from its level, element included).
3. `EVT_HitOrBlock` on both, `Particle_Hit_Clash` at the overlap, `Sound_Hit_Clash`.

The reacting Blueprint reads `ReceivedHit.SpecialHitAction` in its `EVT_HitOrBlock` handler. `this`'s own `ReceivedHit`
isn't written, so a handler that also runs for its own hits landing must not trust a stale value. Clear it after reading,
as WitchTools' battle props do.

`HandleCustomCollision_PostHit` is declared and called after normal hits; the meeting itself is all in PreHit.

## In this project

Written up with the characters that use it (FireBoxer's fire assist, Firefighter's water beam, Gas Attendant's gas
puddle and falling cigarette) and battle props (fire explosions, Reacts To Element, the oil barrel) in WitchTools
`Docs/ElementalHits.md`.
