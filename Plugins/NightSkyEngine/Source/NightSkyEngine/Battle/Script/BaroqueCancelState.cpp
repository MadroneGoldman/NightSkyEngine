// Madrone change: Baroque Cancel, a universal Roman Cancel paid with recoverable health.

#include "BaroqueCancelState.h"

#include "NightSkyEngine/Battle/Misc/Bitflags.h"
#include "NightSkyEngine/Battle/Misc/Globals.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/Data/StateData.h"
#include "NightSkyEngine/Miscellaneous/NightSkyBattleRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BaroqueCancelState)

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cel_BaroqueCancel_Blue, "Cel.BaroqueCancel.Blue", "Baroque Cancel pose: Blue (neutral)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cel_BaroqueCancel_Red, "Cel.BaroqueCancel.Red", "Baroque Cancel pose: Red (attack made contact)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cel_BaroqueCancel_Purple, "Cel.BaroqueCancel.Purple", "Baroque Cancel pose: Purple (attack before contact)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cel_BaroqueCancel_Yellow, "Cel.BaroqueCancel.Yellow", "Baroque Cancel pose: Yellow (blocking)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Particle_BaroqueCancel_Blue, "Particle.BaroqueCancel.Blue", "Baroque Cancel effect: Blue");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Particle_BaroqueCancel_Red, "Particle.BaroqueCancel.Red", "Baroque Cancel effect: Red");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Particle_BaroqueCancel_Purple, "Particle.BaroqueCancel.Purple", "Baroque Cancel effect: Purple");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Particle_BaroqueCancel_Yellow, "Particle.BaroqueCancel.Yellow", "Baroque Cancel effect: Yellow");

namespace BaroqueCancel
{
	// Night Sky runs a state's Exec twice on the frame it's entered: ObjectReg8 (saved for rollback) remembers the last
	// frame this state ran, so each frame runs once.
	static bool FirstRunThisFrame(ABattleObject* Parent)
	{
		constexpr int32 Magic = 0x5BA20000;
		const int32 Stamp = Magic + Parent->ActionTime;
		if (Parent->ObjectReg8 == Stamp) return false;
		Parent->ObjectReg8 = Stamp;
		return true;
	}
}

UBaroqueCancelState::UBaroqueCancelState()
{
	StateType = EStateType::BaroqueCancel;
	// Strive's Roman Cancel numbers (Dustloop, GGST Mechanics): super freeze, total, slowdown. Blue, Red and Purple can act
	// straight after the freeze; Yellow recovers.
	Blue.Cel = Cel_BaroqueCancel_Blue;
	Blue.Particle = Particle_BaroqueCancel_Blue;
	Blue.Color = FLinearColor(0.15f, 0.45f, 1.f, 1);
	Blue.SuperFreezeFrames = 20;
	Blue.TotalFrames = 45;
	Blue.SlowdownFrames = 59;
	Blue.bPersistentSlowdown = true;

	Red.Cel = Cel_BaroqueCancel_Red;
	Red.Particle = Particle_BaroqueCancel_Red;
	Red.Color = FLinearColor(1.f, 0.12f, 0.1f, 1);
	Red.SuperFreezeFrames = 25;
	Red.TotalFrames = 45;
	Red.SlowdownFrames = 39;

	Purple.Cel = Cel_BaroqueCancel_Purple;
	Purple.Particle = Particle_BaroqueCancel_Purple;
	Purple.Color = FLinearColor(0.7f, 0.15f, 1.f, 1);
	Purple.SuperFreezeFrames = 20;
	Purple.TotalFrames = 45;
	Purple.SlowdownFrames = 19;

	Yellow.Cel = Cel_BaroqueCancel_Yellow;
	Yellow.Particle = Particle_BaroqueCancel_Yellow;
	Yellow.Color = FLinearColor(1.f, 0.85f, 0.1f, 1);
	Yellow.SuperFreezeFrames = 12;
	Yellow.TotalFrames = 47;
	Yellow.SlowdownFrames = 10;
	Yellow.bActionableAfterFreeze = false; // 33 frames of recovery after the freeze
	Yellow.InvulnerableFrames = 12;
	Yellow.ScreenDarken = 0.45f; // shorter freeze, a lighter dim
}

FBaroqueCancelKind UBaroqueCancelState::GetKind(const EBaroqueCancelType Type) const
{
	switch (Type)
	{
	case EBaroqueCancelType::Red: return Red;
	case EBaroqueCancelType::Purple: return Purple;
	case EBaroqueCancelType::Yellow: return Yellow;
	case EBaroqueCancelType::Blue:
	default: return Blue;
	}
}

void UBaroqueCancelState::CallExec()
{
	APlayerObject* Self = Parent && Parent->IsPlayer ? Parent->Player : nullptr;
	if (Self && BaroqueCancel::FirstRunThisFrame(Parent))
	{
		const EBaroqueCancelType Type = Self->GetBaroqueCancelType();
		const FBaroqueCancelKind Kind = GetKind(Type);
		const int32 Frame = Parent->ActionTime;
		// The freeze's last frame (the press is frame 0): the shockwave and the return to neutral happen here, so the
		// opponent is slowed and the fighter can act from the first frame after the freeze.
		const int32 LastFreezeFrame = FMath::Max(0, Kind.SuperFreezeFrames - 1);

		if (Frame == 0)
		{
			// Paid in recoverable health: the cost, or what's left of it.
			const int32 Cost = FMath::Max(0, GetDefault<UNightSkyBattleRules>()->BaroqueCancelCost);
			Self->RecoverableHealth = FMath::Max(0, Self->RecoverableHealth - Cost);
			Parent->HaltMomentum();
			// Out of blockstun (Yellow): otherwise the stun running out sends the fighter to standing, mid-cancel.
			Self->StunTime = 0;
			Self->PlayerFlags &= ~PLF_IsStunned;
			// Out of an air dash: its timer would keep the fighter at one height (no drift up or down).
			Self->AirDashTimer = 0;
			Parent->SetAttacking(false);
			Parent->EnableHit(false);
			// Landing mid-cancel (a drift down, an air cancel) doesn't end it: it ends standing instead.
			Self->SetDefaultLandingAction(false);
			Parent->SetCelName(Kind.Cel);
			Parent->AddColor = FLinearColor(Kind.Color.R * 0.15f, Kind.Color.G * 0.15f, Kind.Color.B * 0.15f, 1); // a light flash, not a recolour
			Parent->AddFadeSpeed = 0.04f;
			if (Kind.InvulnerableFrames > 0) Self->SetStrikeInvulnerableForTime(Kind.InvulnerableFrames);
			// Everyone else freezes, the fighter doesn't (it drifts).
			if (Kind.SuperFreezeFrames > 0) Parent->StartSuperFreeze(Kind.SuperFreezeFrames, 0);
			if (Kind.Particle.IsValid()) Parent->CreateGuardParticle(Kind.Particle, Self, -60.f, false);
			if (LastFreezeFrame == 0) Shockwave(Self, Type, Kind);
		}
		else if (Frame == LastFreezeFrame)
		{
			Shockwave(Self, Type, Kind);
		}

		// Drift: move the way the fighter dashed, slowing to a stop (no gravity in here: HaltMomentum).
		if (const EBaroqueDrift Drift = Self->GetBaroqueDrift(); Drift != EBaroqueDrift::None && Frame >= 1)
		{
			const int32 Frames = FMath::Max(1, DriftFrames);
			if (Frame <= Frames)
			{
				const int32 Left = Frames - Frame + 1;
				const int32 SpeedX = DriftSpeedX * Left / Frames;
				const int32 SpeedY = DriftSpeedY * Left / Frames;
				Parent->SpeedX = Drift == EBaroqueDrift::Forward ? SpeedX : Drift == EBaroqueDrift::Back ? -SpeedX : 0;
				Parent->SpeedY = Drift == EBaroqueDrift::Up ? SpeedY
					                 : Drift == EBaroqueDrift::Down && Self->PosY > Self->GroundHeight ? -SpeedY : 0;
			}
			else if (Frame == Frames + 1)
			{
				Parent->SpeedX = 0;
				Parent->SpeedY = 0;
			}
		}

		// No attacks during the freeze (a press in its last few frames comes out on the first frame after it).
		Self->DisableState(ENB_NormalAttack | ENB_SpecialAttack | ENB_SuperAttack, StateMachine_Primary);

		// Blue, Red, Purple: free to act the moment the freeze ends (the shockwave just went off). Yellow recovers first.
		if ((Kind.bActionableAfterFreeze && Frame >= LastFreezeFrame) || Frame >= Kind.TotalFrames)
		{
			Self->StartBaroqueRelock(RelockFrames);
			// Like the Exit State macro: back to standing / crouching, or falling from the top of a jump.
			if (Self->PosY > Self->GroundHeight)
			{
				Parent->GotoLabel(FGameplayTag::RequestGameplayTag(TEXT("State.Label.Jump.Top")));
				Self->JumpToState(FGameplayTag::RequestGameplayTag(TEXT("State.Universal.Jump")), StateMachine_Primary, true);
				// The state change turned everything off and the new state turns it back on only in its own update, after
				// the input check: on now.
				Self->EnableAttacks();
				Self->EnableState(ENB_Jumping | ENB_ForwardAirDash | ENB_BackAirDash, StateMachine_Primary);
			}
			else
			{
				Self->JumpToStatePrimary(Self->Stance == ACT_Crouching ? State_Universal_Crouch : State_Universal_Stand);
				Self->EnableAll(StateMachine_Primary);
			}
			// Night Sky enters a move the frame after its input is read: read it now (buffered, nothing changes yet), so a
			// move pressed at the end of the freeze starts on the first frame after it. Only presses within a button's
			// lenience (3 frames by default) count, like Strive: the extra and released buttons of the Baroque Cancel's own
			// A+C+E early in the freeze are too old by now. Motions keep their own (longer) step windows.
			Self->HandleStateMachine(true, Self->PrimaryStateMachine);
			return;
		}
	}
	Super::CallExec();
}

void UBaroqueCancelState::Shockwave(APlayerObject* Self, const EBaroqueCancelType Type, const FBaroqueCancelKind& Kind) const
{
	APlayerObject* Enemy = Self->Enemy;
	if (!IsValid(Enemy) || (Enemy->PlayerFlags & PLF_IsDead)) return;
	const int32 Radius = GetDefault<UNightSkyBattleRules>()->BaroqueShockwaveRadius;
	if (FMath::Abs(Enemy->PosX - Self->PosX) > Radius || FMath::Abs(Enemy->PosY - Self->PosY) > Radius) return;

	const EStateType EnemyState = Enemy->PrimaryStateMachine.CurrentState
		                              ? Enemy->PrimaryStateMachine.CurrentState->StateType
		                              : EStateType::Standing;
	const bool bEnemyBlocking = EnemyState == EStateType::Blockstun;
	const bool bEnemyHit = EnemyState == EStateType::Hitstun;
	UE_LOG(LogTemp, Verbose, TEXT("Baroque Cancel shockwave: %s kind %d, enemy %s (type %d), dx %d"), *Self->GetName(),
	       static_cast<int32>(Type), *Enemy->GetCurrentStateName(StateMachine_Primary).ToString(), static_cast<int32>(EnemyState),
	       FMath::Abs(Enemy->PosX - Self->PosX));

	// A hit with no damage, the shockwave's: the fighter's level-4 hit data with these changes.
	auto ShockwaveHit = [&](const EHitAction GroundAction, const EHitAction AirAction, TFunctionRef<void(FHitData&)> Adjust)
	{
		const int32 SavedLevel = Self->HitCommon.AttackLevel;
		Self->HitCommon.AttackLevel = 4;
		FHitData Hit = Self->InitHitDataByAttackLevel(false);
		Self->HitCommon.AttackLevel = SavedLevel;
		Hit.Damage = 0;
		Hit.Hitstop = 0;
		Hit.EnemyHitstopModifier = 0;
		Hit.GroundHitAction = GroundAction;
		Hit.AirHitAction = AirAction;
		Adjust(Hit);
		Enemy->AttackOwner = Self;
		Enemy->ReceivedHit = Hit;
		Enemy->ReceivedHitCommon = Self->HitCommon;
		Enemy->PlayerFlags |= PLF_IsStunned;
		Enemy->HandleHitAction(Enemy->PosY <= Enemy->GroundHeight ? GroundAction : AirAction);
	};

	switch (Type)
	{
	case EBaroqueCancelType::Red:
		if (bEnemyHit)
		{
			// Launches, hard knockdown.
			// After a drift the launch changes (Strive): forward lower and farther, up much higher, and so on.
			FBaroqueDriftLaunch Launch(RedLaunchUntech, 100, 100);
			switch (Self->GetBaroqueDrift())
			{
			case EBaroqueDrift::Forward: Launch = RedLaunchForward; break;
			case EBaroqueDrift::Back: Launch = RedLaunchBack; break;
			case EBaroqueDrift::Up: Launch = RedLaunchUp; break;
			case EBaroqueDrift::Down: Launch = RedLaunchDown; break;
			default: break;
			}
			ShockwaveHit(HACT_AirNormal, HACT_AirNormal, [this, Launch](FHitData& Hit)
			{
				Hit.Untech = Launch.Untech;
				Hit.AirPushbackX = RedLaunchSpeedX * Launch.SpeedXPercent / 100;
				Hit.AirPushbackY = RedLaunchSpeedY * Launch.SpeedYPercent / 100;
				Hit.HardKnockdown = 1;
			});
		}
		else if (bEnemyBlocking)
		{
			Enemy->StunTime = FMath::Max(Enemy->StunTime, RedBlockstun);
		}
		break;
	case EBaroqueCancelType::Yellow:
		if (!bEnemyBlocking && !bEnemyHit)
		{
			// Guard crush and a big push away.
			// The guard break that fits, else the standing one, else plain hitstun (not every character has them).
			const TArray<FGameplayTag>& Names = Enemy->PrimaryStateMachine.StateNames;
			EHitAction Crush = HACT_GroundNormal;
			if (Enemy->Stance == ACT_Crouching && Names.Contains(State_Universal_GuardBreakCrouch)) Crush = HACT_GuardBreakCrouch;
			else if (Names.Contains(State_Universal_GuardBreakStand)) Crush = HACT_GuardBreakStand;
			ShockwaveHit(Crush, HACT_AirNormal, [this](FHitData& Hit)
			{
				Hit.Hitstun = YellowGuardCrushFrames;
				Hit.Untech = YellowGuardCrushFrames;
				Hit.GroundPushbackX = YellowPushback;
			});
		}
		else
		{
			return; // blocked: no slowdown either
		}
		break;
	default:
		break;
	}
	if (Kind.SlowdownFrames > 0) Enemy->StartSlowdown(Kind.SlowdownFrames, Kind.bPersistentSlowdown, Type);
}
