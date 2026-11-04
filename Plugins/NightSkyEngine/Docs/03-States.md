# 03 States

A state (`UState`, `Battle/Script/State.h`) is a Blueprint object that defines one action:
standing, a jump, a 5A, a hitstun reaction, a fireball object. Player states live in a
`FStateMachine` on the player. Object states drive pooled `ABattleObject`s.

## Class defaults that matter

| Property | Meaning |
|---|---|
| `Name` (tag) | The state's identity. Cancels, `JumpToState`, and `AddBattleObject` for object states all use it. |
| `StateType` / `CustomStateType` | Category (`NormalAttack`, `SpecialAttack`, `Hitstun`, `Custom`, ...) that enable flags and cancels check. |
| `EntryStance` | `Standing`/`Crouching`/`Jumping`/`None`. The required stance, which is also forced on entry. |
| `InputConditionLists` | **Any one list** must pass. Inside a list **every** `FInputCondition` must pass. An empty list means the state is entered with no input. |
| `StateConditions` | All must pass (`AirJumpOk`, `MeterOneBar`, `CloseNormal`, `PlayerReg1True`, ...). |
| `IsFollowupState` | The state can only be reached by a cancel option, never entered directly. |
| `ObjectID` | The state can't be entered while an owned object with this ID is active (one fireball on screen). |
| `MaxChain`, `ShareChainName` | Limit repeats within a chain. Names that share a chain count together. |
| `bEnableReverseBeat` | Whether reverse-beat chains allow it. |
| `MaxInstances` | Object states only: how many copies are pooled. |
| `CPUData`, `bCPUUsable`, `bHumanUsable` | AI range/role data and usability flags. |

Input conditions (`FInputCondition`): a `Sequence` of `FInputBitmask` (input flags + `Lenience`
frames + `Hold` frames), an `EInputMethod` (`Normal`, `Strict`, `PositiveEdge` for presses,
`NegativeEdge` for releases, `PressAndRelease`, `IsUp`), disallowed inputs, and
`ImpreciseInputCount` for strict motions. Inputs are `INP_Up/Down/Left/Right`, `INP_A..H`
(`Battle/Misc/Bitflags.h`), resolved relative to facing.

## Lifecycle

```
entry       FStateMachine::SetState / ForceSetState
              ├─ USubroutineState? → just calls its subroutine, no state change
              ├─ UStateAlias?      → enters StateToEnter instead (different conditions, same state)
              ├─ old state EVT_Exit
              ├─ OnStateChange()   (primary machine only) ← big reset, see below
              ├─ CurrentState = new; PostStateChange()
              ├─ Init()            ← set hit data, cancel options, event handlers here
              └─ Exec() once (CelIndex = 0), UpdateCel()
every frame CallExec(): CelIndex = 0; Exec()   ← the whole Exec graph re-runs each frame
            EVT_Update handler, UpdateCel(), ActionTime++
```

`APlayerObject::OnStateChange` (`PlayerObject.cpp`) resets nearly everything, so **state setup
always belongs in `Init`**. It:

* runs `Subroutine.Cmn.OnStateChange` and `Subroutine.OnStateChange`, and deactivates owned objects flagged `EnableDeactivateOnStateChange`;
* **disables all state types** (`DisableAll`), clears custom state types, and clears chain, whiff and auto-combo cancel options;
* resets cancel flags (chain cancel and reverse beat back on, kara cancel on unless one was just used), attack/misc/invuln flags, gravity, speed rates, angles and offsets;
* clears `HitCommon`, `NormalHit` and `CounterHit`, cel and blend cel, `ActionReg1..8`, all event handlers, homing, super armor and visual transform.

So a new state starts out unable to cancel into anything. Neutral states call `EnableAll` or
`EnableState(...)` (`ST_CommonStand` etc.), attacks call `AddChainCancelOption` /
`EnableSpecialCancel` / `EnableWhiffCancel` at the right cel.

## Cels and the state macros

States step through **cels** using macros from `BP_StateMacros`. In DSL form, a typical attack `Exec` is:

```lisp
(SetCelAndBlendCel "Manny.5A.00" "Manny.5A.01" 4
  (:"Next Cel"
    (SetCelAndBlendCel "Manny.5A.01" "Manny.5A.02" 4
      (:"On Enter" (EnableHit parent true))           ; active frames start
      (:"Next Cel"
        (SetCelAndBlendCel "Manny.5A.02" "Manny.5A.03" 3
          (:"Next Cel" ... (ExitState)))))))
```

How it works: `Exec` re-runs from the top every frame with the state's `CelIndex` reset to 0.
Each `SetCel*` macro bumps the state's local counter and only runs when it matches the object's
`CelIndex`. On the first frame of its turn it sets the cel name (`SetCelName`, which loads boxes
and animation from collision data), sets the duration and fires **On Enter**. When
`TimeUntilNextCel` reaches 0, `ABattleObject::UpdateCel` increments `CelIndex`, and the next
macro's **Next Cel** branch takes over.

| Macro | Use |
|---|---|
| `SetCel(CelName, Duration)` | Show a cel for N frames |
| `SetCelAndBlendCel(Cel, BlendCel, Duration)` | Same, interpolating animation toward `BlendCel` (smooth 3D animation) |
| `CelGate(Duration)` | A timed step with no cel change |
| `SetCelDurationIf` | Conditional duration |
| `IsOnFrame(Frame)` | Branch on an exact `ActionTime` frame |
| `LabelGate(Name)` | Entry point targeted by `GotoLabel(tag)`, which jumps within the state (loops, hit/whiff branches, "Common.Loop") |
| `ExitState` | Return to neutral |
| `SetSuperOverlay`, `SetTagoutOverlay`, `SetCallAssistOverlay`, `SetSuperArmourOverlay` | This fork: overlay material helpers |

`BP_SubroutineMacros` holds the macros for subroutine graphs.

## Priority

`HandleStateMachine` walks the state list **from the last element to the first**, and the first
state that can be entered and whose inputs match wins. **States later in `DA_*States` have
higher priority**: specials and supers go near the end, neutral states near the start. State
name lookups (`StateNames.Find`) return the **first** match, so two states with the same `Name`
tag in one list is a bug.

`CanEnterState` requires all of:

* the state type is enabled and the state isn't a follow-up, **or** it is a chain, whiff or auto-combo
  cancel option, **or** it's a valid kara cancel into a later state;
* the moves-used-in-chain limit isn't hit, no blocking `ObjectID` object is active, and the
  stance matches `EntryStance`;
* all `StateConditions` pass, and the Blueprint `CanEnterState()` override returns true.

During hitstop, inputs are **buffered** (`HandleStateMachine(true, ...)`) and `BufferedStateName` is
entered when hitstop ends.

## Cancels

* **Chain cancel**: `AddChainCancelOption(tag)` in `Init`. Becomes usable after the move hits or is
  blocked (`CNC_ChainCancelEnabled`), and `SetAttacking(true, AllowCancel)` controls whether
  cancelling is allowed.
* **Whiff cancel**: `AddWhiffCancelOption(tag)` + `EnableWhiffCancel(true)` (off by default).
* **Special/super/jump/air-dash cancel**: `EnableSpecialCancel`, `EnableSuperCancel`, `EnableJumpCancel`,
  `EnableForwardAirdashCancel`.
* **Auto combo**: `AddAutoComboCancel(tag, button)`.
* **Kara cancel**: automatic during the first 3 frames (`ActionTime < 3`) of a state, into a *later* state (higher index) of a cancellable type, when `CNC_EnableKaraCancel` is set. It's on by default, and off right after a kara cancel was used.
* **Baroque cancel** (this fork): `StateType::BaroqueCancel`, `FindBaroqueCancelOption`.
* `EnableCancelIntoSelf(true)` lets a state restart itself.

## Sub state machines

`SubStateData` (map of tag → `UStateData`) creates extra state machines that run alongside the
primary one (`HandleStateMachine(false, ...)` for each, so they don't buffer). Use these for
independent layers. Only the primary machine calls `OnStateChange` and `PostStateChange`.

## Events

`InitEventHandler(EventType, FunctionName, Value, SubroutineName)` makes the engine call a
**zero-parameter function** on the current state (or on a subroutine, if `SubroutineName` is set)
when the event fires. Types: `Update`, `Exit`, `Landing`, `Hit`, `Block`, `HitOrBlock`, `CounterHit`,
`Parried`, `Kill`, `ReceiveHit`, `SuperFreeze(End)`, `Timer0/1` (`Value` = frames), and
`*MainPlayer` variants for tag battles. Handlers are cleared on every state change.

Example (`ST_Manny5C` upstream): `(InitEventHandler player "EVT_HitOrBlock" "OnHitOrBlock")` plus a
function graph `OnHitOrBlock` that calls `CameraShake`.

## Subroutines

`USubroutine` (`Battle/Script/Subroutine.h`) is reusable Blueprint code called by tag:
`CallSubroutine(tag)` or `CallSubroutineWithArgs(tag, a, b, c, d)`. The arguments land in
`SubroutineReg1..4` on the caller, and results go in `SubroutineReturnVal1..4` (cleared before each call).
`Parent` is the calling object.

The engine itself calls these when they exist: `Subroutine.Cmn.MatchInit/RoundInit/OnUpdate/
OnStateChange/PostStateChange/OnComboEnd/OnLanding/PostUpdateDebug` (and the non-`Cmn` character
versions), `Subroutine.Cmn.HitCollision` / `OnHit` / `OnBlock` / `OnCounterHit` / `IsCorrectBlock`,
and `ThrowParam.Ground/Air`. Attack-level presets are subroutines: `SB_CmnAttackLevel0..5`
(`Subroutine.Cmn.AttackLevel.N`). A subroutine class must not inherit from another subroutine
that already has a `Name` set (InitPlayer logs an error).

## Registers (scratch variables)

| Registers | Scope | Reset |
|---|---|---|
| `ActionReg1..8` | Player and its objects, per action | Every state change |
| `ObjectReg1..8` | Per object (projectile HP, counters) | Object reset |
| `PlayerReg1..8` | Character-specific | Each round (usable in `StateConditions`) |
| `CmnPlayerReg1..8` | Shared conventions across characters | Each round |
| `SubroutineReg1..4` / `SubroutineReturnVal1..4` | Subroutine arguments and results | Per call |
| `Timer0/1` | Countdown that fires `EVT_Timer0/1` | Via `InitEventHandler` |

Registers are rollback-safe, because they sit inside the sync block. Blueprint variables only are if they're marked **SaveGame**.
