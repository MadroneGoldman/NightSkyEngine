# 07 Pitfalls

## Gameplay tags are the glue, and they fail silently

* Undeclared or misspelled tags don't break the build. A missing cel gives an empty cel (no boxes,
  no animation), a missing particle or sound tag plays nothing, a missing state tag logs
  `Could not find state X!` and stays put.
* The editor logs `Invalid GameplayTag X found in ...` on load for undeclared tags. Treat those as bugs.
  Declare tags in `Config/DefaultGameplayTags.ini`. Upstream assets can use tags that upstream's own ini
  doesn't declare (`Manny`, `Manny.AirTech.00/01` in `DA_MannyCollision`).
* The tag ini is project config (not in the plugin repo). When merging upstream, **union** the
  `+GameplayTagList` lines. Keep redirects (`+GameplayTagRedirects`) and check that no redirect's
  `OldTagName` is also declared.
* JSON/Python writes to tag-bearing structs can fail on the whole array when one entry has an
  invalid tag or the payload is large. Write big arrays in chunks and verify the count afterwards.

## State setup

* **Everything is reset on state change** (hit data, cancels, enable flags, event handlers, `ActionReg*`,
  speeds, offsets). Set it all again in `Init`. Setting hit data in `Exec` before the active cel also works,
  but `Init` is the convention.
* A new state **can't cancel into anything** until it enables state types or adds cancel options.
* **List order in `DA_*States` is priority** (last = highest). Two states with the same `Name` tag breaks lookups.
* `Exec` re-runs from the top every frame. Only code inside the active cel macro's pins should
  have effects. Effects placed before the first `SetCel` run **every frame**.
* `GotoLabel` re-runs `Exec` immediately. A label without a matching `LabelGate` does nothing.
* Blueprint variables on states/subroutines need **SaveGame** to survive rollback. Registers already do.
* Object states pool `MaxInstances` copies. Spawning more than that at once silently fails.
* `GotoLabel` had a `Reset State` pin in an older engine version. Old graphs keep an orphaned pin
  that holds a value (`ST_CommonGuardBreak`, `ST_MannyCrouch`, `ST_MannyFDash`, the FloatingCrumple states).
  Refresh doesn't clear it on purpose, so reset or delete the pin by hand.

## Hit data

* `INT_MAX` = "default by attack level". Setting a field to an explicit value disables the default.
  Counter hit copies normal hit only for `INT_MAX` fields.
* The attack-level subroutines set `GroundPushbackX` in v2. A state that also sets it overrides the preset.
* Hit sparks always come from **common** particle data, even with an override (see [06](06-VFX-Sound-Camera.md)).
* `SetAttacking` gained an `AllowCancel` parameter. Old graphs get the default `false`.

## C++ ↔ Blueprint drift (seen in this project)

* **Removed functions leave dead nodes.** `SmearToggle` was removed from `BP_PlayerObject`, so
  `ST_CommonThrowLock` failed to compile until the call was deleted (upstream did the same).
* **Type changes leave stale pins.** `BattleData.PlayerListP1/P2` became `TArray<TSoftObjectPtr<UPrimaryCharaData>>`.
  Widgets that wired them as hard arrays failed with "Array of Primary Chara Data Object References is not compatible
  with ... Soft Object References". The fixes were:
  * ForEach loops: disconnect **every** wildcard pin (`Array` *and* `Array Element`), then reconnect, so the type re-resolves (`WBP_CSelectGrid`).
  * Getters that lost their Target: feed them the resolved soft reference (`WBP_VSInfo`).
* **Soft references must be loaded.** `ResolveSoftReference` returns null if the asset isn't loaded.
  Upstream uses `LoadAsset_Blocking` + cast in `WBP_VSInfo`. This project's VS screen resolves without loading,
  and `NightSkyGameState::Init` only loads synchronously when the battle starts, so splash icons and descriptions can be blank on the VS screen.
* Header changes (new members in `BattleObject`/`PlayerObject`) change the rollback memory block, and
  **Live Coding can't apply them**. Close the editor and do a full build.
* **Root motion broke movement** (upstream PR #9, commit `71a73506`, "lydia-root-motion-fixes"). In `ABattleObject::Move()`,
  any body animation with `NightSkyAnimSequenceUserData` took the root motion path, and that path ended in a `return`,
  so speed, gravity and position integration never ran. Characters couldn't walk and hung in the air after jumping.
  The fix (from the local 5.8 upstream checkout, not yet committed upstream) adds `UNightSkyAnimSequenceUserData::HasRootMotion()`,
  only takes that path when root translation is non-zero, removes the early `return`, and fixes the integer division in
  `GetRootTranslationAtTime` (multiply by 1000 before dividing by 60). Check that this is still in place after merging upstream code.
* The 5.8 `UE_LOG` checks format strings at compile time. GGPO had a mismatched log in `connection_manager.cpp`.
* `FInputActionBindingHandle` collided with an engine type. This fork renamed it `FInputActionBindingHandleNSE`.
  `InputActionExecutedDelegate` has the same name in NarrativeCommonUI and NSE, which triggers a Python exposure warning (harmless).

## Inheritance and defaults

* Changing a default on `BP_PlayerObject` only reaches children that didn't override that property.
* Data-asset slots that are `None` on a character are inherited from the parent only if the child never saved its own value.
* `CommonCollisionData` is `None` on `BP_Manny`, so Manny's collision asset holds its common cels (stand, hitstun...) too.

## Content and asset hygiene

* Many characters reuse Manny's states. Check references before deleting `ST_Manny*` (see [02](02-Characters-and-Data.md)).
* Renaming a Blueprint file or copying it with a different filename breaks loading in 5.8 (package name vs filename).
  Copy with the **same filename into another folder** to inspect a second version side by side.
* Opening assets in 5.8 marks them dirty (in-memory upgrade). Saving them makes them unopenable in 5.7.
  Don't save unrelated dirty packages during a merge.
* Some content files were read-only on disk (`ST_CommonSpecialThrow.uasset`). Clear the attribute before overwriting.
* Asset references inside `.uasset` package name tables end with a NUL byte. When grepping binaries for references,
  search for `"/AssetName\x00"`, not `"/AssetName.AssetName"`.

## Repository and build

* The plugin repo (`MadroneGoldman/NightSkyEngine`, branch `NickWTChanges`) is checked out at the **project root**,
  and its `.gitignore` ignores everything except `Plugins/`. **Content, Config and Source aren't under version control.**
  Keep backups (`Saved/Backup_*`) before bulk content changes.
* An old local `.gitattributes` routed `*.png` through LFS, which this branch doesn't use, so every PNG showed as modified. It's gone now.
* Prebuilt plugin binaries aren't tracked on this branch. Always build `NightSkyEngineEditor` after pulling.
* The build needs the UE 5.8 launcher install and VS 2022 (MSVC 14.44). The plugin also needs `ALS` (Plugins/ALS-Refactored, patched for 5.8).
* Third-party content with known compile errors: CombatMagicAnims demo rigs, the Narrative Inventory integration
  (needs a plugin that isn't installed), NiagaraExamples utilities, MagicProjectiles demo. They don't affect NSE.
