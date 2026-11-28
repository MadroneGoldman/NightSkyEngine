# 10 Character systems: extensions, interactions, subtitles

Madrone change (2026-11-26). Three engine pieces for things one character (or one pair of characters) does that the
rest of the cast doesn't:

- **Character extensions** (`UNSECharacterExtension`): a character's own battle system, made per fighter.
- **Character interactions** (`FNSECharacterInteractionPair` + `UNSECharacterInteractionEvent`): when two given
  characters are on screen together, something happens.
- **Battle subtitles** (`UNSEBattleSubtitleWidget`, `ANightSkyGameState::ShowBattleSubtitle`): a speaker's line on the
  battle screen, one shared element styled like Narrative Pro's dialogue subtitles.

Examples in WitchGame (WitchTools `Docs/CharacterInteractions.md`, `Docs/UniqueCharacters.md`): The Hacker swoons and
loses the round when the Babysitter is on screen (and Golden Boy does too); Officer Jofficer's assist never comes back and
he walks in to finish P1's victory; Boy Mom throws her seven sons (extra teammate bars) and may only tag Golden Boy in
under 25% health.

## Identifying characters

`UPrimaryCharaData::GetCharacterTag()`: the CHR_ asset's **Character Tag**, or `Character.<CharaName>` when that tag is
registered. `APlayerObject::GetCharacterTag()` uses its `CharaData` (set by every spawner: NSE's Init and the overworld's
`UWitchBattleComponent::SpawnFighter`), else the Blueprint's `DefaultCharacterTag`.

## Character extensions

| Where | What |
|---|---|
| CHR_ > Character Systems > Character Extensions | classes made for that character |
| fighter Blueprint > Character Extension Classes | the same, for fighters without character data |
| `APlayerObject::CharacterExtensions` | the instances (Transient), Owner + GameState set |

Made in `ANightSkyGameState::InitCharacterSystems` (from `MatchInit` and `MatchInitWt`, before the first `RoundInit`), so
a rematch makes them again. Hooks (BlueprintNativeEvent; where the engine calls them):

| Hook | Called from |
|---|---|
| `MatchInit` | `InitCharacterSystems` |
| `RoundInit` | end of `RoundInit` (`RoundInitCharacterSystems`) |
| `Update` | `UpdateGameState`, after `OnFighterInputs`, before the objects update; not during a super freeze |
| `OnAssistCalled(Caller, Assist)` / `CanCallAssist` | `CallAssist` (caller's and assist's extensions) |
| `ModifyCooldown(Frames)` | `APlayerObject::SetTeamCooldown` (dedicated assist), `ANightSkyGameState::SetTeamCooldown`, the tag cooldown in `SwitchMainPlayer` |
| `OnTaggedIn` / `OnTaggedOut` / `CanTagOut` / `CanBeTaggedIn` | `SwitchMainPlayer` (the Can* only when not forced) and `CanTag` |
| `GetVictoryState` / `OnVictory` | `EnterWinState`, which replaced every `JumpToStatePrimary(RoundWin / MatchWin)` for the winner in `HandleRoundWin` / `HandleMatchWin` |
| `IsVictorySequenceRunning` | `HandleMatchWin` (no `EndMatch`) and `NextRoundTransition` (no fade) while any is true |
| `GetVirtualTeammateBars` / `GetOwnBarColor` | `UpdateHUD` -> `GatherTeammateBars` -> `UNightSkyBattleWidget::SetCharacterTeammateBars` (Nick HUD: `UTeammateBarBox::SetCharacterBars`) |
| `GetCPUForcedState` | `ANightSkyAIController::Update`: the CPU enters that state (`SetStateForCPU`) before choosing |

Overworld (WT) battles: `MatchInitWt` sets the Tag format when either side has more than one fighter (a tag partner,
an NPC's allies or a CHR_ `DefaultTagPartner`, which `UWitchBattleComponent` now spawns too), since `CanTag` refuses
tags in any other format. Dedicated assists work in every format.

Teammate bars: with any character-extension bars, the Nick HUD's teammate box lays all its bars out as one squashed
vertical list in its own space, above the fighter's bar: never a second column (the user's call, 2026-11-26). Past four
bars the rows get thinner and the names smaller (`CharacterBarNameSize` x 4 / bars, at least 5). Each bar has its name
(`FNSETeammateBar.Name`; real partners get their `CharacterName`). Hidden virtual bars keep their place.

`NSECooldownForever` (1000000 frames): a dedicated-assist cooldown at or above it shows as never ready (no number).

## Character interactions

A pair: Character A (the subject, the event acts on it), Character B (the partner), the Event class, Triggers
(`RoundStart`, `TagIn`, `AssistCall`, `EveryFrame`), Sides (Any / Opponents / Teammates), Repeat (once per round, once
per match, unlimited), symmetric, priority. Pairs come from Battle Rules > Character Interactions > Interaction Sets
(`UNSECharacterInteractionData` assets), the game state's `CharacterInteractionData` and each fighter's CHR_
`CharacterInteractions` (an empty Character A there means that character). At match start they're merged into
`InteractionPairs` (by priority), the map `InteractionPairsByCharacter` (tag -> pair indices), and `InteractionEvents`:
one event instance per (subject fighter, partner fighter) matching a pair, in a fixed order.

Triggers are queued (`QueueInteractionTrigger`: `RoundInit`, a successful `SwitchMainPlayer`, a successful `CallAssist`)
and checked in `UpdateCharacterSystems` on the next frame where the round is being fought (BattlePhase Battle, round
start countdown over). An event starts when its pair's trigger is pending (or EveryFrame), the repeat rule allows it,
both fighters are on screen (`PLF_IsOnScreen` and `IsActive`: a called assist counts until `AssistExit`) and its
`CanStart` agrees. Then `Begin`, `Update` every frame until `Finish`; the next round interrupts it (`End(true)`).
`bHoldSubject` makes the subject get neutral inputs and no CPU decisions while it runs; `bHoldPartner` the same for the
partner.

## Rollback

- Extensions: each fighter's are saved after its state in `SaveGameState` / loaded in `LoadGameState`; events after the
  battle extensions. Only SaveGame properties (Blueprint: SaveGame variables) are saved (`SerializeBin`).
- `PendingInteractionTriggers` is a SaveGame property of the game state (saved with it).
- The lists are made once per match, so the buffer layout doesn't change during a match.
- Presentation skips resimulated frames: `ShowBattleSubtitle` also never shows the same line twice for the same
  round + frame.

## Subtitles

`ShowBattleSubtitle(Speaker, Line, Seconds)` (game state) or `UNSECharacterLibrary::ShowBattleSubtitle`. The element is
made on first use from Battle Rules > Battle Subtitle Widget (`WBP_BattleSubtitle` in WitchTools BattleCommon UI, a child
of `UNSEBattleSubtitleWidget`: SpeakerName, LineText / LineRichText, SubtitleRoot found by name) and added to the
viewport (screen space; the top battle HUD is a world-space widget component). `SubtitleLog` keeps what was said
(tests).

## Library (`UNSECharacterLibrary`)

`GetCharacterTag`, `IsFighterOnScreen`, `FindFightersByTag`, `SetFighterHealth`, `EnterState`, `PlayParticleOnFighter`
(a Niagara system at the fighter, aged by the particle manager like Night Sky particles), `BringFighterOnScreen`,
`ShowBattleSubtitle`, `GetCharacterExtension`, `SetAssistCooldown`, `TagOut`.

## Default tag partner

CHR_ `DefaultTagPartner`: in `Init` (Battle Data path), a side with only that character gets the partner added and the
format becomes Tag. Overworld battles pick their own teams.

## Pitfalls

- Hit states (Crumple, launches, knockdowns) read the hit that put the fighter in them (`SetHitValues` uses
  `AttackOwner` and `ReceivedHit`). `UNSECharacterLibrary::EnterState` into one without a hit crashed the editor (null
  `AttackOwner`): set `AttackOwner` (e.g. the partner) and `ReceivedHit = FHitData()` first.

- Native gameplay tags declared with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` aren't exported: other modules request tags by
  name, at runtime (not in a constructor: the tag table isn't loaded when class defaults are made).
- A Blueprint extension's out-array hook (`GetVirtualTeammateBars`) replaces the array it's given: the engine gathers
  each extension's bars into its own array, then appends.
