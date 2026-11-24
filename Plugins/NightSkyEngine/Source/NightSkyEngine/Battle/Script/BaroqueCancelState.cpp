// Madrone change: Baroque Cancel, a universal Roman Cancel paid with recoverable health.

#include "BaroqueCancelState.h"

#include "NightSkyEngine/Battle/Misc/Bitflags.h"
#include "NightSkyEngine/Battle/Misc/Globals.h"
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
	// Strive's Roman Cancel numbers (Dustloop, GGST Mechanics): startup + recovery, super freeze, slowdown, cancel window.
	Blue.Cel = Cel_BaroqueCancel_Blue;
	Blue.Particle = Particle_BaroqueCancel_Blue;
	Blue.Color = FLinearColor(0.15f, 0.45f, 1.f, 1);
	Blue.SuperFreezeFrames = 20;
	Blue.TotalFrames = 45;
	Blue.SlowdownFrames = 59;
	Blue.bPersistentSlowdown = true;
	Blue.CancelWindowStart = 5;
	Blue.CancelWindowEnd = 11;

	Red.Cel = Cel_BaroqueCancel_Red;
	Red.Particle = Particle_BaroqueCancel_Red;
	Red.Color = FLinearColor(1.f, 0.12f, 0.1f, 1);
	Red.SuperFreezeFrames = 25;
	Red.TotalFrames = 45;
	Red.SlowdownFrames = 39;
	Red.CancelWindowStart = 7;
	Red.CancelWindowEnd = 13;

	Purple.Cel = Cel_BaroqueCancel_Purple;
	Purple.Particle = Particle_BaroqueCancel_Purple;
	Purple.Color = FLinearColor(0.7f, 0.15f, 1.f, 1);
	Purple.SuperFreezeFrames = 20;
	Purple.TotalFrames = 45;
	Purple.SlowdownFrames = 19;
	Purple.CancelWindowStart = 8;
	Purple.CancelWindowEnd = 14;

	Yellow.Cel = Cel_BaroqueCancel_Yellow;
	Yellow.Particle = Particle_BaroqueCancel_Yellow;
	Yellow.Color = FLinearColor(1.f, 0.85f, 0.1f, 1);
	Yellow.SuperFreezeFrames = 12;
	Yellow.TotalFrames = 47;
	Yellow.SlowdownFrames = 10;
	Yellow.CancelWindowStart = -1;
	Yellow.CancelWindowEnd = -1;
	Yellow.InvulnerableFrames = 12;
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

		if (Frame == 0)
		{
			// Paid in recoverable health: the cost, or what's left of it.
			const int32 Cost = FMath::Max(0, GetDefault<UNightSkyBattleRules>()->BaroqueCancelCost);
			Self->RecoverableHealth = FMath::Max(0, Self->RecoverableHealth - Cost);
			Parent->HaltMomentum();
			// Out of blockstun (Yellow): otherwise the stun running out sends the fighter to standing, mid-cancel.
			Self->StunTime = 0;
			Self->PlayerFlags &= ~PLF_IsStunned;
			Parent->SetAttacking(false);
			Parent->EnableHit(false);
			Parent->SetCelName(Kind.Cel);
			Parent->AddColor = FLinearColor(Kind.Color.R * 0.15f, Kind.Color.G * 0.15f, Kind.Color.B * 0.15f, 1); // a light flash, not a recolour
			Parent->AddFadeSpeed = 0.04f;
			if (Kind.InvulnerableFrames > 0) Self->SetStrikeInvulnerableForTime(Kind.InvulnerableFrames);
			// Everyone else freezes, the fighter doesn't: it can cancel this during the freeze.
			if (Kind.SuperFreezeFrames > 0) Parent->StartSuperFreeze(Kind.SuperFreezeFrames, 0);
			if (Kind.Particle.IsValid()) Parent->CreateGuardParticle(Kind.Particle, Self, -60.f, false);
			if (Kind.SuperFreezeFrames == 0) Shockwave(Self, Type, Kind);
		}
		else if (Frame == Kind.SuperFreezeFrames)
		{
			Shockwave(Self, Type, Kind);
		}

		// Cancel the Baroque Cancel: an attack during the window goes straight into the next move.
		if (Kind.CancelWindowStart >= 0 && Frame >= Kind.CancelWindowStart && Frame <= Kind.CancelWindowEnd)
		{
			Self->EnableAttacks();
		}
		else
		{
			Self->DisableState(ENB_NormalAttack | ENB_SpecialAttack | ENB_SuperAttack, StateMachine_Primary);
		}

		if (Frame >= Kind.TotalFrames)
		{
			// Like the Exit State macro: back to standing / crouching, or falling from the top of a jump.
			if (Self->PosY > Self->GroundHeight)
			{
				Parent->GotoLabel(FGameplayTag::RequestGameplayTag(TEXT("State.Label.Jump.Top")));
				Self->JumpToState(FGameplayTag::RequestGameplayTag(TEXT("State.Universal.Jump")), StateMachine_Primary, true);
			}
			else
			{
				Self->JumpToStatePrimary(Self->Stance == ACT_Crouching ? State_Universal_Crouch : State_Universal_Stand);
			}
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
			ShockwaveHit(HACT_AirNormal, HACT_AirNormal, [this](FHitData& Hit)
			{
				Hit.Untech = RedLaunchUntech;
				Hit.AirPushbackX = RedLaunchSpeedX;
				Hit.AirPushbackY = RedLaunchSpeedY;
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
	if (Kind.SlowdownFrames > 0) Enemy->StartSlowdown(Kind.SlowdownFrames, Kind.bPersistentSlowdown);
}
