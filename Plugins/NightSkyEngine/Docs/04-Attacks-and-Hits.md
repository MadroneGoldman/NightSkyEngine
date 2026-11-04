# 04 Attacks and hits

## Hit data lives on the attacker

Every `ABattleObject` carries three structs (`Battle/Objects/BattleObject.h`):

* **`HitCommon` (`FHitDataCommon`)**: shared by block, hit and counter hit. `AttackLevel` (0-5),
  `BlockType` (Mid/High/Low/Unblockable), blockstun, chip %, guard pushback and gravity,
  proximity-block range, `HitAngle`, `SFXType` (Punch/Kick/Slash), `VFXType`
  (Strike/Slash/Special), **`HitVFXOverride` / `HitSFXOverride` / `GuardVFXOverride` /
  `GuardSFXOverride`** (gameplay tags), and damage colours.
* **`NormalHit` (`FHitData`)**: hitstop, hitstun, untech, damage, recoverable %, minimum %, proration,
  pushback, gravity, over-time curves, hit position, **ground/air hit action** (`HACT_GroundNormal`,
  `HACT_AirFaceUp`, `HACT_Blowback`, `HACT_Crumple`, ...), knockdown, wall and ground bounce.
* **`CounterHit`**: the same layout, used when the defender was attacking.

**`INT_MAX` means "use the default".** When a hit resolves, `InitHitDataByAttackLevel` fills every
`INT_MAX` field from a table keyed on `AttackLevel` (e.g. level 0: hitstop 11, hitstun 10,
damage 300, ground pushback 20000). Counter-hit fields left at `INT_MAX` copy normal hit. It also
picks the default hit VFX/SFX tags from the level: levels 0-1 use `Particle.Hit.S` /
`Sound.Hit.<Type>.S`, 2-3 use `.M`, 4-5 use `.L`, and `VFX_Special` always uses `Particle.Hit.SP`.
**Overrides win** if set.

All three structs are **cleared on every state change**, so attacks set them in `Init`:

```lisp
(event EventInit
  (Parent:Init)                                             ; ST_CommonNormalAttack setup
  (CallSubroutine parent "Subroutine.Cmn.AttackLevel.1")     ; level preset
  (SetMembersInHitDataCommon (GetHitCommon parent) 0 "BLK_Mid")
  (SetMembersInHitData (GetNormalHit parent) ...)            ; per-move overrides
  (AddChainCancelOption player "Manny.5B"))
```

The `SB_CmnAttackLevelN` subroutines set `AttackLevel`, and in upstream v2 and this project,
`GroundPushbackX` too (8000/12000/16000/20000/25000...). In this project, `SB_CmnAttackLevel3` also sets
`RecoverableDamagePercent = 50`.

## Making something hit

* `EnableHit(true)` sets `ATK_HitActive` for the active frames, `EnableHit(false)` ends them.
* `SetAttacking(true[, AllowCancel])` marks the object as attacking (it can be counter-hit, can chain).
  Normal-attack bases set this in `Init`. In this project's `ST_CommonSuperAttack` it's `(true, true)`.
* Hitboxes come from the current cel's `BOX_Hit` boxes, hurtboxes from `BOX_Hurt` (see [05](05-Collision-and-Animation.md)).
* Each target is hit once per activation (`ObjectsToIgnoreHitsFrom`). Re-enable hit for multi-hit moves.
* Other attack attributes: `SetProjectileAttribute`, `SetHeadAttribute`, `SetProrateOnce`, `SetIgnoreOTG`,
  `SetHitOTG`, `SetPlayerHit` (an object's hit counts as the player's), `SetIgnorePushbackScaling`,
  `SetIgnoreHitstunScaling`, `SetHitCustomCollision`.

## The hit pipeline (attacker → player)

`ABattleObject::HandleHitCollision` (`BattleObject.cpp`), run from `ANightSkyGameState::HandleHitCollision` after all objects update:

1. Gates: attacker has `ATK_IsAttacking` and `ATK_HitActive`, target hasn't been hit by it yet, target isn't invulnerable, and the hit/hurt boxes overlap.
2. Target faces the attacker, momentum halts, attacker gets `ATK_HasHit`.
3. **Parry**: if the target is `SetParrying(true)`, it calls its `SetHandleParrySubroutine` subroutine. If that returns `SubroutineReturnVal1 = 1`, the parry succeeded: `EVT_Parried` fires on the attacker, `Sound.Parry` plays, both get hitstop, and processing stops.
4. Target gets `PLF_IsStunned`, then `EVT_HitOrBlock` (+`MainPlayer` variant).
5. The target's **`Subroutine.Cmn.HitCollision`** runs. Return 1 to cancel the hit entirely (custom rules).
6. **Block** (`IsCorrectBlock(BlockType)`): attacker runs `Subroutine.Cmn.OnBlock`, `Particle.Guard` spawns, `EVT_Block` fires, chip damage applies, guard values are set, block action, meter gains.
7. **Super armor** (`SuperArmorData`): absorbs the hit (guard-style or dodge), with optional chip and armor damage.
8. **Normal hit** (target not attacking): attacker runs `Subroutine.Cmn.OnHit`, `EVT_Hit` fires, target gets `EVT_ReceiveHit`. Hit data resolves, **`HitCommon.HitVFX` is spawned with `CreateCommonParticle` at `POS_Col`** (the box overlap point, rotated by `HitAngle`), `HitSFX` plays, then `HandleHitAction(Ground/AirHitAction)` and hitstop.
9. **Counter hit** (target was attacking): like a normal hit, plus `Subroutine.Cmn.OnCounterHit` and `EVT_CounterHit`, using `CounterHit` data.
10. Hitgrab (`SetHitgrabActive`) stops after the events, so the move can take over.

Hits on **non-player objects** (projectile vs projectile) only check armor, run `OnHit` and `EVT_Hit`, then spawn the hit spark and sound. Projectile clashes go through `HandleClashCollision` (`Particle.Hit.Clash`, `Sound.Hit.Clash`).

`HandleHitAction(HACT)` on the defender maps the hit action to a universal state
(`State.Universal.Hitstun.N`, `Launch.*`, `Blowback`, `Crumple`, ...) and `SetHitValues()` copies the received
data into speeds and timers. Hitstun states use `DamageReactionCels` / `SetDamageReactionCel` to choose reaction cels.

## Blocking

`IsCorrectBlock` checks the defender's stance and block input against `BlockType`. You can override it with
`Subroutine.Cmn.IsCorrectBlock`. Proximity block (`ProximityBlockDistanceX/Y`) puts the defender in
guard early. Instant-block and guard-break states are universal states (`State.Universal.*Block*`, `GuardBreak*`).

## Throws

1. The throw state calls `SetThrowActive(true)`, `SetThrowRange(range)` and `SetThrowExeState(tag)`.
   In this project's `ST_CommonNormalThrow`, the execute state is `State.Universal.Throw.Hit` and the range comes from a state variable `InThrowRange`.
2. `HandleThrowCollision` checks range, throw invulnerability and throw resist. On success, `ThrowExe()` jumps the attacker to the execute state and puts the defender in throw lock (`ST_CommonThrowLock`, with `PLF_IsThrowLock` set, so `Update` returns early).
3. The execute state positions the victim (`SetThrowPosition`), applies hit data, then calls `ThrowEnd()`.
4. `ThrowTechWindow` and `ThrowResistAfterWakeUp` are character defaults. `Subroutine.ThrowParam.Ground/Air` sets throw parameters.

## Projectiles and other objects

* The player state spawns it: `(AddBattleObject parent "Manny.Fireball.Obj" 311000 281000)` (state tag, X/Y offset, `PosType`).
  It returns the object, so you can seed `ObjectReg*` on it (`ST_MannyFireballA` sets `ObjectReg1 = 10000`).
* The object state (in `DA_*ObjectStates`) sets its own hit data in `Init`, links a looping particle
  (`LinkCharaParticle "Particle.Manny.Fireball"`), and registers `EVT_HitOrBlock` to count hits and
  `DeactivateObject` when it's spent.
* Use `EnableDeactivateIfBeyondBounds`, `EnableDeactivateOnStateChange` and `EnableDeactivateOnReceiveHit` for cleanup.
  Set `ObjectID` on the object and on the player state to prevent spawning a second one.
* Objects can `AttachToSocketOfObject` or follow via `PositionLinkObj`, share hitstop via `StopLinkObj`,
  and borrow draw priority or material via `DrawPriorityLinkObj` / `MaterialLinkObj`.
  `SetStopLinkObj(player, parent)` is what upstream's `ST_Manny5BSmear` uses.

## Meter, health and combos

* Meter gain on hit and block is a percentage of damage (`MeterPercentOnHit` 72, `...OnHitGuard` 18,
  `...OnReceiveHit` 40, `...OnReceiveHitGuard` 10). `SetMeterCooldownTimer` blocks gains, e.g. `ST_CommonSuperAttack`
  sets 10 frames here (upstream sets `INT_MAX`).
* **Recoverable health** (this fork): `RecoverableDamagePercent` of each hit becomes recoverable,
  regenerating at `RecoverableHealthRecoverySpeed` while the character is tagged out (off-screen).
  `StateCondition::RecoverableHealthNotZero` exists for moves that consume it.
* Proration: `ComboRate`, `InitialProration`, `ForcedProration`, `MinimumDamagePercent`, `OtgProration`, `MaxOTGCount`.
