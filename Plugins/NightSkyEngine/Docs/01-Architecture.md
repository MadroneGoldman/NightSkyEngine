# 01 Architecture

## Modules and plugins

`Plugins/NightSkyEngine/NightSkyEngine.uplugin` has four modules:

| Module | Type | Purpose |
|---|---|---|
| `NightSkyEngine` | Runtime | The engine: battle objects, states, data assets, game states, UI base classes, networking |
| `GGPOUE4` | Runtime | Third-party GGPO rollback netcode (C), wrapped for UE |
| `CollisionEditor` | Editor | Custom editor for `UCollisionData` (cel list, box editing, animation preview) |
| `NightSkyEngineEditor` | Editor | Style overrides, plus the `NightSkyRefreshBlueprints` commandlet (Refresh All Nodes, headless) |

Plugin dependencies: OnlineSubsystem(+Utils), EnhancedInput, Niagara, Paper2D, CommonUI,
StateTree, **GameplayStateTree** (needed by `BP_NightSkyAIController` on 5.8) and **ALS**
(ALS-Refactored, used by this fork's WT overworld mode). The `CommonLoadingScreen` plugin ships
in the same repo.

Source layout (`Source/NightSkyEngine/`):

```
Battle/        Objects (BattleObject, PlayerObject), Script (State, StateMachine, Subroutine,
               StateAlias, SubroutineState, BattleExtension), Misc (Bitflags, InputBuffer,
               CollisionBox, Globals, RandomManager), FighterRunners (local/GGPO/replay/synctest),
               Actors (ParticleManager, AudioManager, LinkActor), Animation, CPU (AI controller),
               NightSkyGameState, NightSkyPlayerController
Data/          All UDataAsset types (CollisionData, StateData, SubroutineData, ParticleData, ...)
Miscellaneous/ GameInstance (BattleData lives here), settings, save info
UI/            Battle widget/HUD base classes (incl. this fork's NickBattleHudTop, TeammateBarBox)
WT/            This fork's overworld mode: WTGameState, WTCharacter (ALS-based), WTPlayerCharacter
CharaSelect/, VSInfo/, Network/, Input/
```

## The frame loop

`ANightSkyGameState::UpdateGameState(Input1, Input2, bShouldResimulate)`
(`Battle/NightSkyGameState.cpp`) advances exactly one frame:

1. Rematch and training-reset checks; particle manager and screen update.
2. Intro handling: plays each side's `IntroName` state, then switches to `Battle` phase.
3. Round timer, `BattleExtension.Update` extensions, team cooldowns.
4. `SortObjects()`, then inputs are written to each main player (`Inputs`), or the CPU controller runs.
5. **Every active object's `Update()`**. During super freeze, frozen objects only buffer input and state transitions.
6. `SortObjects()` again, then **`HandleHitCollision()`** (all hit/hurt checks happen *after* all objects moved).
7. Super-freeze end events, `HandleEndCombo`, screen bounds, **push collision**, more screen bounds, camera and HUD.
8. Visuals (`UpdateVisuals`) are updated separately and are not part of the simulation.

What happens inside `APlayerObject::Update` (`Battle/Objects/PlayerObject.cpp`):

* Off-screen (tag-team) players only regenerate recoverable health.
* `ABattleObject::Update`: pushbox, hitstop countdown (returns early while in hitstop), timers (`EVT_Timer0/1`), flip.
* Subroutines `Subroutine.Cmn.OnUpdate` and `Subroutine.OnUpdate` run every frame.
* Death/KO handling, input buffer update, then **state machine transition checks**, then the current state's `Exec`, `EVT_Update`, cel advance (`UpdateCel`), `ActionTime++`.

What happens inside a non-player object's `Update` (projectiles etc.): link position, hitstop (it can share a
parent's hitstop through `StopLinkObj`), `Move()`, its `ObjectState->Init()` on frame 0, then `CallExec()`,
`EVT_Update`, link actor update, cel advance, and an optional deactivate once beyond the screen bounds.

## Battle objects and pooling

* `ABattleObject` is a **pawn**. Its gameplay values (position, speed, hit data, registers,
  cel/action data, flags) sit between the markers `ObjSync` and `ObjSyncEnd`. That whole block is
  `memcpy`'d for rollback (see below).
* The game state pre-spawns **`MaxBattleObjects` (default 400)** objects into a pool when the match
  starts. `AddBattleObject(StateName, X, Y, PosType)` takes a free object, gives it the named
  **object state** and activates it. `DeactivateObject()` returns it to the pool. A player can
  keep 16 objects in `StoredBattleObjects` (`OBJ_Child0..15`).
* Object states are instantiated **`MaxInstances` times** each at player init (`InitPlayer`). If you
  spawn more copies of a projectile at once than `MaxInstances`, the extra spawns fail.
* `AddCommonBattleObject` uses the common object states (e.g. `State.BattleObject.KO.*`, the KO-camera objects).

## Units and coordinates

* Positions, speeds, sizes and pushback are **integers**. `1000` = one engine unit ("pixel").
  Collision editor fields show `/1000`.
* Conversion to Unreal space: `COORD_SCALE = 1000 / OBJ_SCALE`, `OBJ_SCALE = 0.43`
  (`Battle/Misc/Globals.h`). So 1 Unreal cm ≈ 2325.6 engine units.
* The battle plays in the X/Y plane of `ANightSkyGameState::BattleSceneTransform`. Particles and
  sequences are placed through that transform.
* Default gravity is `1900`/frame². Angles are `*_x1000` integers.
* Timings (hitstun, cel durations, timers) are in **frames**. The sim is fixed-step, 60 fps.

## Rollback

* `SaveForRollback` copies the raw memory from `ObjSync` to `ObjSyncEnd` (and for players, from
  `PlayerSync` to `PlayerSyncEnd`). Members outside those ranges are only saved if they carry
  the `SaveGame` UPROPERTY flag (state machines, boxes, link actors, gauges, cancel option arrays).
* Blueprint state, subroutine and extension objects derive from `USerializableObj`, which
  serializes their **SaveGame-flagged** Blueprint variables. A Blueprint variable in a state that
  must survive rollback needs the **SaveGame** flag (in the variable's details, under Advanced).
* Particles and audio are not simulated. `AParticleManager` and `AAudioManager` keep them in sync by
  setting the Niagara *desired age* and audio playback position when frames are re-simulated.
  This is why `CreateCommonParticle` sets `ENiagaraAgeUpdateMode::DesiredAge`.
* Fighter runners (`Battle/FighterRunners`): local, GGPO multiplayer, replay, and a synctest runner
  for desync debugging. `ANightSkyGameState::CreateChecksum()` hashes the state.
* In the local game state header `MaxRollbackFrames = 1`. The network path uses GGPO's own buffers.

## Game modes in this fork

* **Standard battle** (`ANightSkyGameState`): versus, training, CPU. The roster comes from
  `UNightSkyGameInstance::BattleData` (`PlayerListP1/P2` are `TArray<TSoftObjectPtr<UPrimaryCharaData>>`,
  loaded synchronously in `Init`). Tag teams and assists come from the same lists.
* **WT** (`ANightSkyWTGameState`, `AWTCharacter`/`AWTPlayerCharacter`): an ALS third-person
  overworld. Walking into an NPC (`StartBattle`) spawns `APlayerObject`s from each character's
  `CharaData` (plus `Allies` for tag/assist) and runs a battle in place. Delegates
  `OnBattleStart`/`OnBattleEnd` hand control back. NPC dialogue uses the Narrative plugin (`Blueprints/WT/Narrative`).
