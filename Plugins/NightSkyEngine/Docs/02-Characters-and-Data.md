# 02 Characters and data assets

## How a character is assembled

```
UPrimaryCharaData (CHR_Manny)          what menus and the roster see
  ├─ CharaName / FriendlyName / Description
  ├─ HUD / Select / Splash icons
  ├─ IsAssist
  └─ PlayerClass ──► BP_Manny  (Blueprint, parent BP_PlayerObject → APlayerObject)
                       Defaults:
                       ├─ movement/health/meter numbers (FWalkSpeed, JumpHeight, MaxHealth, ...)
                       ├─ IntroName (state played at round start)
                       ├─ CharaStateData        ─► DA_MannyStates       list of UState classes
                       ├─ SubStateData (map)    ─► extra state machines (optional)
                       ├─ CollisionData         ─► DA_MannyCollision    cels: boxes + animation
                       ├─ CommonCollisionData   ─► (optional shared cels)
                       ├─ CommonSubroutineData  ─► DA_CommonSubroutines
                       ├─ CharaSubroutineData   ─► (optional)
                       ├─ CommonObjectStateData ─► DA_CommonObjectStates (KO cams etc.)
                       ├─ ObjectStateData       ─► DA_MannyObjectStates  projectiles/effects
                       ├─ CommonParticleData    ─► DA_CommonParticles   hit sparks, guard, smoke
                       ├─ CharaParticleData     ─► DA_MannyParticles
                       ├─ CommonSoundData / SoundData / VoiceData
                       ├─ CommonSequenceData / SequenceData (level sequences for supers/throws)
                       ├─ CameraShakeData       ─► DA_CommonCameraShake
                       ├─ CommonLinkActorData / LinkActorData
                       └─ MaterialData          ─► DA_MannyMaterials01 (per-colour materials)
```

* The **common** assets are set on `BP_PlayerObject` and inherited. A character only overrides the
  character-specific slots. When you change a default on `BP_PlayerObject`, children that never
  overrode that property pick it up. Children that have their own value keep it.
* `InitPlayer()` (`PlayerObject.cpp`) instantiates everything when the match starts:
  1. every class in `CharaStateData->StateArray` becomes a state in the **primary** state machine;
  2. each `SubStateData` entry becomes a sub state machine;
  3. common and character subroutines are created;
  4. object states are created `MaxInstances` times each.
* There is **no separate common state list**. Each character's `DA_*States` lists the common states it
  uses (`ST_CommonBJumpPre`, `ST_CommonAirTech`, ...) next to its own subclasses
  (`ST_MannyStand`, a child of `ST_CommonStand`; `ST_Manny5A`, a child of `ST_CommonNormalAttack`).
  **Order matters**, see [03 States](03-States.md#priority).

## Characters reusing another character's states

In this project, Fangs, FireBoxer, Firefighter, GasAttend, JamieLee and Milkman put
**Manny's** state classes (`ST_MannyStand`, `ST_MannyHitstun0`, ...) in their own `DA_*States`.
Upstream v2 deleted those Manny states (it renamed them to `ST_Manny_*`), but this project keeps
the old ones because these characters depend on them. Don't delete `Blueprints/Characters/Manny/States/ST_Manny*`
without updating every `DA_*States` that lists them.

## Data asset reference

| Asset type | Content | Looked up by |
|---|---|---|
| `UPrimaryCharaData` | Display info, icons, `PlayerClass`, `IsAssist` | Soft pointer in `BattleData`, CSS grids, WT `CharaData` |
| `UStateData` | `TArray<TSubclassOf<UState>> StateArray` | State `Name` tag |
| `USubroutineData` | `TArray<TSubclassOf<USubroutine>>` | Subroutine `Name` tag |
| `UCollisionData` | `TArray<FCollisionStruct>`: CelName, Anim parts, AnimFrame, blend in/out, Boxes | Cel name tag |
| `UParticleData` | `{Name tag, UNiagaraSystem}` | Particle tag |
| `USoundData` | `{Name tag, USoundBase, MaxDuration}` | Sound tag |
| `USequenceData` | `{Name tag, ULevelSequence}` | Sequence tag |
| `UCameraShakeData` | `{Name tag, CameraShake class}` | Shake tag |
| `ULinkActorData` | `{Name tag, ALinkActor class, MaxInstances}` | Link actor tag |
| `UMaterialData` | `{Name, TArray<Material> per colour, OverlayMaterial}` | Component/slot name, with `ColorIndex` picking the array element |
| `UBattleExtensionData` | `TArray<TSubclassOf<UBattleExtension>>` (game-state-level rules) | `BattleExtension.MatchInit/RoundInit/Update` |
| `UPrimaryStageData` | Stage info for stage select | GameInstance `BattleData.Stage` |

## Content layout (`Content/NightSkyEngine`)

```
Blueprints/
  Battle/          BP_NightSkyGameState, player controller, HUD actor, AI controller
  Characters/
    Common/        BP_PlayerObject, BP_SpritePlayerObject, BP_StateMacros, BP_SubroutineMacros,
                   DA_CommonSubroutines, DA_CommonObjectStates, States/ (ST_Common*),
                   ObjectStates/, Subroutines/ (SB_Cmn*)
    <Character>/   BP_<Char>, CHR_<Char>, DA_<Char>States, DA_<Char>ObjectStates, DA_<Char>Collision,
                   States/, ObjectStates/
  Common/          BP_NightSkyGameInstance (default BattleData, music data), post process
  WT/              Overworld mode: game mode/state/controller, Characters, Narrative dialogue, WT_AI
  CharaSelect/, VSInfo/, Menu/, Transition/
CharacterAssets/<Character>/   meshes, animations (AS_*), materials (DA_*Materials*), particles (DA_*Particles)
CharacterAssets/Common/        common animations (ABP_Common), particles, audio (DA_CommonSounds), sequences, camera shake
Maps/  Shared/ (input IMC_Battle/IMC_WT, parameter collections MPC_Chara, UI data)  UI/  VFX/
```

## Adding a character (checklist)

1. Make `BP_<Char>` with parent `BP_PlayerObject` (or `BP_SpritePlayerObject` for 2D flipbooks).
2. Make `DA_<Char>States`. Start by duplicating an existing character's list so all the
   universal states are there (stand, crouch, jumps, hitstun, block, wakeup, throw lock, round win/lose).
3. Make `DA_<Char>Collision` with a cel for every cel tag your states use, and declare the tags.
4. Set the character-specific data slots on `BP_<Char>` (states, collision, object states, particles, materials).
5. Make `CHR_<Char>` (`UPrimaryCharaData`) and point `PlayerClass` at `BP_<Char>`.
6. Add `CHR_<Char>` to the character-select grid data or the game instance `BattleData`, and for WT, set `CharaData` on the overworld character.
