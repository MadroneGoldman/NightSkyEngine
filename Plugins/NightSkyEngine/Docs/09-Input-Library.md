# 09 Input Library

Reusable input condition lists. A state's `InputConditionLists` entry can be saved to an asset and loaded into
any other state, so the same motion (`236A`, a dash, a jump) is set up once and copied exactly.

## The pieces

| Piece | Where |
|---|---|
| `UInputConditionListAsset` (data asset) | `Source/NightSkyEngine/Battle/Script/InputConditionListAsset.h`. Holds one `FInputConditionList` (the same type as one entry of a state's `InputConditionLists`), a `Notation` refreshed on save, and a `Description`. |
| Details panel customisation | `Source/NightSkyEngineEditor/Private/InputConditionListCustomization.*`, registered for `FInputConditionList` in `FNightSkyEngineEditorModule::StartupModule`. |
| The library | `Content/InputLibrary` in this plugin (`/NightSkyEngine/InputLibrary`). WitchGame sees the same folder through a junction (WitchTools `Scripts/plugins.json`, NightSkyEngine `Dirs` includes `Content`). |
| Library builder | `witch_tools_battle.input_library` (WitchToolsBattle Python): scans states and makes one asset per distinct list. |

## Using it

Open a state Blueprint and look at **Class Defaults > Input Condition Lists**. Each entry now shows its notation
(e.g. `236! + A`) instead of "1 members".

* **Right-click an entry > Load from Input Library...** opens an asset picker (search by name) and replaces that
  entry with the chosen asset's list. It goes through the property system, so it's one undo step and child
  Blueprints and placed instances update as for a hand edit.
* **Right-click an entry > Save to Input Library...** opens the save-asset dialog in `/NightSkyEngine/InputLibrary`
  with a name from the notation (`236A`). Pick an existing asset to overwrite it (you're asked first).
* The **folder button** at the end of the entry's row opens the same two commands.
* To add a list rather than replace one, add an entry with **+** first, then Load into it.

Only one entry is read or written at a time. Saving needs a single state selected.

## Notation

`DescribeInputConditionList` (Blueprint-callable, also used for the row text):

| Mark | Meaning |
|---|---|
| `1`-`9` | Numpad directions, relative to facing (`6` forward, `4` back, `5` = the Neutral flag) |
| leading `5` | A list with no direction at all is written like a standing normal: a list that only checks A reads `5A` |
| `A`-`H` | Buttons |
| `*` | A step with no input set: it matches anything |
| `[x]` | The step has a Hold time |
| `!` | Strict method (diagonals don't count as the cardinals) |
| `x~` / `~x` | Press-and-release / negative edge (release) |
| `up(x)` | IsUp / IsUpStrict |
| ` + ` | Separate conditions in the list: all must pass |

Lenience, disallowed inputs and imprecise counts aren't shown; the library assets' Description lists them.

## The built library (2026-11-08)

`input_library.build()` scanned the Common and Manny template states (`/WitchTools/BattleCommon/.../Common/States`,
`.../Manny/States` and `/Game/NightSkyEngine/.../Common/States`): 49 distinct lists, 49 assets. Every asset is
checked field for field against the state it came from (`InputConditionListsMatch`).

Asset names are the move itself (no prefix), where direction-only conditions merge into the button (`2 + A` -> `2A`, `236! + A` ->
`236A`, `8 + 4` -> `7`) and wildcard steps are dropped, then what makes it differ from a look-alike:
(and a button with no direction gets a `5`: `A` -> `5A`, the same as the row notation)

| Suffix | Meaning | Example |
|---|---|---|
| `_Tap` | The direction must be freshly pressed (PositiveEdge) | `8_Tap` air jump vs `8` jump |
| `_Held` | The button only has to be down (Normal method), not freshly pressed | `5A_Held` techs vs `5A` |
| `_No<x>` | Invalid while `x` is held (disallowed inputs) | `5C_NoD` 5C (so C+D throws instead) |
| `_Charge<n>` | Held for n frames | |
| `_Release`, `_Up` | Release or is-up methods | |

Highlights: `236A/B/C` (fireballs, DPs, supers), `2A..2D`, `3C_NoD`, `6C_NoD`, `4B`, `8B..8D`
(air), `656_*` / `454_*` / `44_*` (dashes), `5G_No4` / `4G` (dash button), `7`/`8`/`9` and `_Tap`
(jumps), `27`/`28`/`29` (super jumps), `5CD_No2` (throw), `5A/5B/5C/5D_Held` (techs), `5H` (baroque),
`2H` (tag out).

Some template lists look odd but were copied as they are, e.g. the Manny DPs use `236A` like the fireball.
Fix those in the states, not in the library.

To rebuild after adding states (existing assets with the same contents are reused, never duplicated):

```python
from witch_tools_battle import input_library as il
print(il.report())    # what it would make
il.build()
```
