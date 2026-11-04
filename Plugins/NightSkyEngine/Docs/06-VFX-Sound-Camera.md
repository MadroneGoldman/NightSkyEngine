# 06 VFX, sound and camera

Everything here is looked up **by gameplay tag** in a data asset set on the player Blueprint.

## Particles

| Call | Searches | Notes |
|---|---|---|
| `CreateCommonParticle(tag, PosType, Offset, Rotation)` | `CommonParticleData` only | Fire-and-forget at a position |
| `CreateCharaParticle(tag, PosType, Offset, Rotation)` | `CharaParticleData` only | Same, character-specific |
| `LinkCommonParticle(tag)` / `LinkCharaParticle(tag)` | common / chara | Attaches a looping system to the object (**non-player objects only**), e.g. a fireball's trail |

`PosType` is where the effect spawns: `POS_Self`, `POS_Player`, `POS_Center`, `POS_Ground`,
`POS_Enemy`, or `POS_Col` (the hit-overlap point). When facing left, the offset X and the rotation
pitch are mirrored, and the Niagara user parameters `UVScale` (-1,1) and `PivotOffset` are set.
The engine also sets the Niagara user parameters `SpriteRotate` and `ScreenSpaceDepthOffset`, so a hit
spark system can use them. Spawned systems go to `AParticleManager`, which drives their age for rollback.

**Engine-spawned effects** (all through `CreateCommonParticle`, so they must be in `DA_CommonParticles`):

| When | Tag |
|---|---|
| Hit | `HitCommon.HitVFX`: from the level `Particle.Hit.S/M/L`, `VFX_Special` → `Particle.Hit.SP`, or `HitVFXOverride` |
| Block | `Particle.Guard` (offset 0,100,0, at `POS_Enemy`) |
| Clash | `Particle.Hit.Clash` |
| Others | `Particle.ThrowTech`, `Particle.JumpSmoke.Land`, `Particle.WallBounce` |

> **A `HitVFXOverride` tag must exist in the common particle data.** A character-only particle
> tag set as an override spawns nothing, because hit sparks never search `CharaParticleData`.

Particles called from state graphs (dash smoke, `Particle.DashStop`, `Particle.Manny.Fireball.End`)
use whichever function the graph calls. Their tags must be declared in `Config/DefaultGameplayTags.ini`
or the editor flags the asset with "Invalid GameplayTag" warnings.

## Sound

* `PlayCommonSound(tag)` searches `CommonSoundData`, `PlayCharaSound(tag)` searches `SoundData`, and `PlayVoiceLine(tag)` searches `VoiceData`.
  Each entry has a `MaxDuration` that caps the play length (and the rollback bookkeeping).
* Engine sounds: `Sound.Hit.<Punch|Kick|Slash>.<S|M|L|SP>` (from `SFXType` + level, or `HitSFXOverride`),
  `Sound.Guard`, `Sound.Parry`, `Sound.Hit.Clash`, `Sound.Land`, `Sound.Landing.Crash`.
* `AAudioManager` keeps the battle audio in sync with re-simulated frames.
* Announcer and music: `UNightSkyGameInstance::AnnouncerData` and `MusicData` (`USoundData`). The game
  instance `BattleData.MusicName` picks the track. In this project `BP_NightSkyGameInstance.MusicData`
  now points at upstream's `Shared/Music/DA_MusicData`.

## Camera shake

`CameraShake(PatternTag, Scale)` looks up `CameraShakeData` on the player (in this project,
`BP_PlayerObject` → `CharacterAssets/Common/CameraShake/DA_CommonCameraShake`) and plays the shake class through the
game state camera. Upstream tags: `CameraShake.Impact.Light/Medium`, `CameraShake.Vertical.Medium`.
Typical use: an `EVT_HitOrBlock` handler function calling `CameraShake(... 1500)`.

## Level sequences (supers, throws, intros)

`PlayCommonLevelSequence(tag)` / `PlayLevelSequence(tag)` look up `CommonSequenceData` / `SequenceData`
and hand them to `ANightSkyGameState::PlayLevelSequence(Target, Enemy, Sequence)`, which binds the
two players and switches to the sequence camera. `StopLevelSequence()` ends it. Sequence time is
part of the battle state, so it rolls back.

## Link actors

`LinkActor(tag)` gets a pooled `ALinkActor` (from `CommonLinkActorData` / `LinkActorData`, `MaxInstances` each)
and attaches it to the object. The actor's `Update()` runs in the simulation. Use it for gameplay-adjacent
visuals that need custom code. `RemoveLinkActor()` releases it.

## Super freeze

`StartSuperFreeze(Duration, SelfDuration)` freezes every object except the caller for `Duration`
frames (and the caller for `SelfDuration`), fires `EVT_SuperFreeze` / `EVT_SuperFreezeEnd`, and pauses the round timer.
`IgnoreSuperFreeze(true)` exempts an object.

## This fork's overlays

`BP_StateMacros` has `SetSuperOverlay`, `SetTagoutOverlay`, `SetCallAssistOverlay` and `SetSuperArmourOverlay`, which apply
an overlay material (via `MaterialData`'s `OverlayMaterial` per material struct) to flag the state visually.
`ST_CommonSuperAttack` calls `SetSuperOverlay` on entry. `DA_MannyMaterials01` has a `Body` entry with an
overlay material and upstream's new `Shadow` entry.
