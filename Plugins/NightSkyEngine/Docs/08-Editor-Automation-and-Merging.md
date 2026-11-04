# 08 Editor automation and merging upstream

## Tools available

* **Unreal MCP server** (`ModelContextProtocol` plugin + toolsets, `http://localhost:8000/mcp`, starts with the editor).
  `editor_toolset.toolsets.blueprint.BlueprintTools` is the important toolset.
* **Rider MCP**: `ue_execute_python` (runs Python in the editor), `ue_export_blueprint_nodes` / `ue_import_blueprint_nodes`
  (Blueprint clipboard text, i.e. copy/paste between graphs), `ue_status`, logs.
* Headless commandlets (`UnrealEditor-Cmd.exe <uproject> -run=... -unattended -nopause -NullRHI`):
  * `CompileAllBlueprints`: the summary line is `Compiling Completed with N errors ...`, and per-asset errors are logged as `LogBlueprint: Error: [AssetLog] <path>: [Compiler] ...`.
  * `NightSkyRefreshBlueprints [-Path=/Game/...] [-Assets=A,B] [-DryRun]`: Refresh All Nodes + compile + **save** (it saves even if errors remain).
* Build: `Engine/Build/BatchFiles/Build.bat NightSkyEngineEditor Win64 Development -Project=<uproject> -WaitMutex`.
  The editor must be closed for header changes.

## Reading Blueprints as text

`BlueprintTools.read_graph_dsl(graph)` returns a Lisp-like view of a graph:

```lisp
(event EventInit
  (bind _parent (Variables|State|GetParent))
  (Class|BattleObject|SetAttacking _parent true true)
  (if (Utilities|Enum|NotEqual(Enum) (...GetStance (...GetPlayer _parent)) "ACT_Jumping")
    (Utilities|SetSuperOverlay) ...))
```

Use it to diff versions of a state. Its limits:

* It **drops** array indices on `Get`, which `Break` struct pin is used, and node ordering details. It's fine for diffing,
  but **don't round-trip it with `write_graph_dsl`** on real assets.
* Some node labels are misleading (a `PlayerObject.Stance` getter showed up as `AlsAnimationInstance|GetStance`).
* Macro graphs show only their inputs.

In editor Python, the same functions are under `import editor_toolset.toolsets.blueprint as m`
(`m.BlueprintTools.read_graph_dsl(g)`, `find_nodes(graph, title_substring, None, False)`,
`get_node_infos([node])`, which gives pins, values and links).

## Editing Blueprints

* Delete a node: `BlueprintTools.delete_node`. **Exec links aren't bridged**, so reconnect the previous
  node's `then` to the next node's `execute` yourself.
* Connect or disconnect: `connect_pins` / `break_pins` with
  `PinID {direction: EGPD_Output|EGPD_Input, index_id: n, node: {refPath}}`. Pin indices count inputs and
  outputs separately, in `get_node_infos` order.
  **Call these through the MCP tool. The same call from editor Python returned success but did nothing.**
* Set a pin default: `set_pin_value` (`"true"`, numbers, enum names, tag strings like `(TagName="Manny.6B")`).
* Copy nodes between assets: export with `ue_export_blueprint_nodes` (node names), import with
  `ue_import_blueprint_nodes`. On import, links to nodes that weren't copied are dropped, so reconnect them.
  Names that collide get renumbered, and **the existing node may be the one renamed** (`SetFieldsInStruct_1` → `_4`). Re-query names after importing.
  You can trim the clipboard text (keep only the `ShowPinForProperties` entries you need for `SetFieldsInStruct`).
* Class defaults: `unreal.get_default_object(bp.generated_class())`, then `set_editor_property`, or
  `editor_toolset.toolsets.object.ObjectTools.list_properties` / `get_properties` / `set_properties` (JSON).
  Compile, then `unreal.EditorAssetLibrary.save_loaded_asset(bp, False)`.
* Always compile and check `bp.get_editor_property('status')` (`BS_UP_TO_DATE`), then save.

## Comparing two versions of an asset in one editor

Copy the other version **with the same filename** into a scratch folder (e.g.
`Content/_MergeTmp/UP/ST_Manny5A.uasset`), scan it
(`AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/_MergeTmp'], True)`), and load it.
A renamed copy won't load. Delete the scratch folder with the editor closed when you're done, and **don't save** the scratch assets.

## How the November 2026 upstream merge was done

**Inputs:**
* Local content: `NightSkyEngineNick/Content`.
* Upstream: the 5.8 working tree at `../NightSkyEngine` (v2 plus its uncommitted 5.8 resaves).
* History: upstream `v2`, fetched into this repo as `upstream/v2`.

1. **Find the real base.** The git merge-base of the fork and v2 (`f26e5a17`) was **not** the content base.
   The local content was a copy of upstream at **`8d4da558` (2026-01-13)**. It was found by scoring every upstream
   commit on how many local file hashes (`git hash-object --no-filters`) matched that commit's tree.
2. **Classify every file 3-way** (base / local / upstream). A local file counts as "unchanged" if its hash matches
   *any* historical upstream blob for that path (it's just older). That separated real local edits (21 files, plus 34 changed on both sides)
   from 218 stale copies.
3. **Apply mechanically:** add upstream-new files (677), take upstream for unchanged files (218), keep local-only edits,
   keep local deletions (383, all unreferenced). Keep upstream deletions that local content still references (54 Manny states).
   Back up everything that gets overwritten (`Saved/Backup_pre-NickWTChanges_2026-11-04`).
4. **Changed on both sides:** for each Blueprint, diff `read_graph_dsl` per graph and the CDO properties (base vs local vs upstream).
   * Only local changed the graphs: keep local, and apply upstream's property changes (`cameraShakeData`, `musicData`).
   * Only upstream changed the graphs: take the upstream file, then re-apply local property values and nodes
     (`ST_Manny5A` + the `Manny.6B` cancel node, `ST_Manny5B`, the game states).
   * Both changed graphs: port the smaller side's changes node by node (`SB_CmnAttackLevel0/3/4`, `ST_CommonSuperAttack`, `ST_MannyFireballObj`).
   * Rewritten on one side (e.g. `ST_Manny5C`, `ST_CommonNormalThrow`): keep local. Local changes take precedence.
   * Data assets: merge arrays by key (collision cels by `CelName`, material structs by `Name`, IMC mappings by index plus action).
5. **Text configs:** `git merge-file --union` for the gameplay tag ini, then dedupe the `+GameplayTag*` lines.
6. **Verify:** `CompileAllBlueprints`. The target is zero errors under `Content/NightSkyEngine`.

Gotchas from the merge:
* `git ls-files` escapes non-ASCII paths (the Japanese SFX filenames). Use `git -c core.quotepath=off`.
* Grepping binaries for references needs the NUL-terminated pattern ([07](07-Pitfalls.md)).
* Kill the editor instead of quitting when packages are dirty only from loading. The quit dialog blocks automation.
* Maps (`CharaSelect_PL`, `TestMap_PL`) and World Partition external actors weren't merged. They need a person in the editor.
