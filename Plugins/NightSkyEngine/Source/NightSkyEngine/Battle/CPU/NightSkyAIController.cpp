// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyAIController.h"

#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/Miscellaneous/NightSkyBattleRules.h" // Madrone change
#include "Misc/ScopeExit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyAIController)

// Madrone change: tuning aid. 1 logs each CPU choice (state, weight, distance) and the best projectile's weight.
static TAutoConsoleVariable<int32> CVarLogCPUChoices(TEXT("ns.CPU.LogChoices"), 0,
	TEXT("1: log each CPU state choice with its weight and the enemy distance (LogTemp)."));

// Sets default values
ANightSkyAIController::ANightSkyAIController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ANightSkyAIController::BeginPlay()
{
	Super::BeginPlay();

	GameState = ANightSkyGameState::Get(this);
}

void ANightSkyAIController::Update()
{
	if (!Player) Player = GetPawn<APlayerObject>();
	
	if (GameState->BattleState.TimeUntilRoundStart || Player->PlayerFlags & PLF_RoundWinInputLock || !Player->IsOnScreen())
	{
		Player->StoredInputBuffer.Update(INP_Neutral, Player->IsStopped());
		return;
	}

	if (Player->CurrentHealth <= 0) return;

	Player->StoredInputBuffer.Update(INP_Neutral);
	bBlock = false;
	
	bool bDefendOnly = false;
	
	if (Player->Enemy->CheckIsAttacking()
		&& Player->Enemy->CheckEnemyInRange(
			Player->Enemy->PrimaryStateMachine.CurrentState->CPUData.AttackXBeginRange, 
			Player->Enemy->PrimaryStateMachine.CurrentState->CPUData.AttackXEndRange,
			Player->Enemy->PrimaryStateMachine.CurrentState->CPUData.AttackYBeginRange, 
			Player->Enemy->PrimaryStateMachine.CurrentState->CPUData.AttackYEndRange)
		&& WaitCount <= WaitLimit)
	{
		bDefendOnly = true;
	}
	else
	{
		WaitCount++;
		// Madrone change: while attacking, decide sooner the higher the fighter's CPU Difficulty (100: every frame), so
		// short cancel windows aren't missed.
		// At least 2 frames between tries, so a typed cancel gets read before the CPU types another.
		const int32 Limit = Player->CheckIsAttacking()
			                    ? FMath::Max(2, WaitLimit * (100 - FMath::Clamp(Player->CPUDifficulty, 0, 100)) / 100)
			                    : WaitLimit;
		if (WaitCount <= Limit) return;
	}
	
	TArray<UState*> AvailableStates{};
	for (auto State : Player->PrimaryStateMachine.States)
	{
		if (!State->bCPUUsable) continue; // Madrone change: the state's "CPU usable" flag was never read
		if (!Player->CanEnterState(State, StateMachine_Primary)) continue;
		AvailableStates.Add(State);
	}

	// Madrone change: attacks out of their CPU Data range aren't options at all (not even as the only one).
	AvailableStates.RemoveAll([this](const UState* State)
	{
		return State->CPUData.bAttack && !Player->CheckEnemyInRange(State->CPUData.AttackXBeginRange, State->CPUData.AttackXEndRange,
		                                                             State->CPUData.AttackYBeginRange, State->CPUData.AttackYEndRange);
	});
	if (AvailableStates.IsEmpty()) return;

	// Madrone change: while attacking, the CPU only acts to cancel, and only once the move has hit (the enemy is in
	// hitstun, not blocking): never on block or whiff. Then it weighs attacks only (the lone option early in a move was
	// the assist call, whose inputs came out as 4B).
	if (Player->CheckIsAttacking() && !bDefendOnly)
	{
		const bool bHitConfirmed = (Player->AttackFlags & ATK_HasHit) && Player->Enemy->CheckIsStunned()
			&& !Player->IsEnemyBlocking();
		if (!bHitConfirmed) return;
		// The move's frame stands still in hitstop: deciding (and typing a different motion) every frame of it garbled
		// the buffer. Decide in its last frames, so the cancel's inputs are fresh when the move goes on.
		if (Player->Hitstop > 2) return;
		AvailableStates.RemoveAll([](const UState* State) { return !State->CPUData.bAttack; });
		if (AvailableStates.IsEmpty()) return;
	}

	if (CVarLogCPUChoices.GetValueOnGameThread() >= 2 && Player->CheckIsAttacking()) // Madrone change: tuning aid
	{
		FString Names;
		for (const UState* State : AvailableStates) Names += State->Name.ToString() + TEXT(" ");
		UE_LOG(LogTemp, Log, TEXT("CPU %s attacking in %s, frame %d: %d options: %s"), *Player->GetName(),
		       *Player->GetCurrentStateName(StateMachine_Primary).ToString(), Player->ActionTime, AvailableStates.Num(), *Names);
	}
	if (AvailableStates.Num() == 1)
	{
		ResetParams();
		TargetState = AvailableStates[0];
	}
	else
	{
		auto MaxWeight = 0;
		UState* CurState = nullptr;
		int32 BestProjectileWeight = INT_MIN; // Madrone change: for ns.CPU.LogChoices
		FGameplayTag BestProjectile;
		for (const auto State : AvailableStates)
		{
			int Weight = 0;
			ON_SCOPE_EXIT
			{
				if (State->CPUData.bProjectile && State->CPUData.bAttack && Weight > BestProjectileWeight)
				{
					BestProjectileWeight = Weight;
					BestProjectile = State->Name;
				}
			};
			if (bDefendOnly)
			{
				if (!State->CPUData.bAttack)
				{
					Weight = CheckDefenseWeight(State);
					if (Weight > MaxWeight)
					{
						MaxWeight = Weight;
						CurState = State;
					}
				}
				else
				{
					Weight = CheckAttackWeight(State);
					if (Weight > MaxWeight)
					{
						MaxWeight = Weight;
						CurState = State;
					}
				}
			}
			else if (!State->CPUData.bAttack)
			{
				Weight = CheckBasicWeight(State);
				if (Weight > MaxWeight)
				{
					MaxWeight = Weight;
					CurState = State;
				}
			}
			else
			{
				Weight = CheckAttackWeight(State);
				if (Weight > MaxWeight)
				{
					MaxWeight = Weight;
					CurState = State;
				}
			}
		}
		if (CVarLogCPUChoices.GetValueOnGameThread() && CurState)
		{
			UE_LOG(LogTemp, Log, TEXT("CPU %s: %s weight %d, distance %d, defend %d, best projectile %s %d (of %d states)"),
			       *Player->GetName(), *CurState->Name.ToString(), MaxWeight, GetEnemyDistanceX(), bDefendOnly ? 1 : 0,
			       *BestProjectile.ToString(), BestProjectileWeight == INT_MIN ? 0 : BestProjectileWeight, AvailableStates.Num());
		}
		if (TargetState != CurState && CurState != nullptr)
		{
			ResetParams();
			TargetState = CurState;
		}
	}
	
	InputCount = FMath::Max(InputCount - InputCountLimit, 0);
	if (SetInputs()) ResetParams();
}

void ANightSkyAIController::ResetParams()
{
	WaitCount = 0;
	InputCount = 0;
	WaitLimit = 7;
	TargetState = nullptr;
}

bool ANightSkyAIController::SetInputs()
{
	if (!TargetState || TargetState->InputConditionLists.Num() == 0) return false;
	// Madrone change: the limit is checked once, then the whole list is written. Checked per condition, a 3-direction
	// motion (236) went over it before its button, the next update started again from the motion, and the CPU never
	// did a motion special or super.
	if (InputCount > InputCountLimit) return false;
	// Madrone change: a direction (held, or a motion's last one) stays held through the button that follows. Written as
	// down, neutral, button, a crouching normal (2A: down held on the button frame) came out as 5A.
	uint32 HeldDirection = 0;
	for (auto Condition : TargetState->InputConditionLists[0].InputConditions)
	{
		if (HeldDirection && Condition.Method == EInputMethod::PositiveEdge)
		{
			Player->StoredInputBuffer.Update(HeldDirection);
			for (const FInputBitmask& Bitmask : Condition.Sequence)
			{
				Player->StoredInputBuffer.Update(Bitmask.InputFlag | HeldDirection);
			}
		}
		else
		{
			Player->StoredInputBuffer.WriteInputCondition(Condition);
		}
		const bool bHolds = (Condition.Method == EInputMethod::Normal || Condition.Method == EInputMethod::Strict)
			&& Condition.Sequence.Num() > 0;
		HeldDirection = bHolds ? Condition.Sequence.Last().InputFlag & INP_Directions : 0;
		InputCount += Condition.Sequence.Num();
	}

	if (bBlock)
	{
		Player->StoredInputBuffer.InputBufferInternal[InputBufferSize - 1] |= INP_Left;
	}
	
	TargetState = nullptr;
	return true;
}

int32 ANightSkyAIController::GetEnemyDistanceX() const
{
	if (!IsValid(Player)) return 0;
	return Player->CalculateDistanceBetweenPoints(DIST_DistanceX, OBJ_Self, POS_Player, OBJ_Enemy, POS_Player);
}

int32 ANightSkyAIController::GetEnemyDistanceY() const
{
	if (!IsValid(Player)) return 0;
	// On a floor that isn't flat, what matters is how far each is off the ground: standing uphill isn't being in the air.
	if (Player->GameState && Player->GameState->HasBattleFloor() && Player->Enemy)
	{
		return FMath::Abs((Player->PosY - Player->GroundHeight) - (Player->Enemy->PosY - Player->Enemy->GroundHeight));
	}
	return Player->CalculateDistanceBetweenPoints(DIST_DistanceY, OBJ_Self, POS_Player, OBJ_Enemy, POS_Player);
}

int32 ANightSkyAIController::CheckBasicWeight(const UState* State) const
{
	if (!IsValid(Player)) return 0;

	int Weight = 50;
	
	switch (State->StateType)
	{
	case EStateType::Standing:
	case EStateType::Crouching:
		break;
	case EStateType::NeutralJump:
	case EStateType::ForwardJump:
		if (GetEnemyDistanceY() > 105000 && Player->PosY - Player->GroundHeight < Player->Enemy->PosY - Player->Enemy->GroundHeight)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(10, 25);
		}
		// Madrone change: jump in on a crouching enemy (a forward jump from further, a neutral one from close), then an
		// air overhead gets the mix-up bonus in the air.
		{
			const UNightSkyBattleRules* Rules = GetDefault<UNightSkyBattleRules>();
			const int32 Distance = GetEnemyDistanceX();
			const bool bJumpInRange = State->StateType == EStateType::ForwardJump
				                          ? Distance >= 200000 && Distance <= 650000
				                          : Distance <= 250000;
			if (Rules->CPUMixupBonus > 0 && Player->Enemy->Stance == ACT_Crouching
				&& Player->Enemy->PosY <= Player->Enemy->GroundHeight && bJumpInRange)
			{
				Weight += GameState->BattleState.RandomManager.RandRange(Rules->CPUMixupBonus / 2, Rules->CPUMixupBonus * 3 / 2);
			}
		}
		break;
	case EStateType::ForwardAirDash:
		if (GetEnemyDistanceX() > 360000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::BackwardJump:
	case EStateType::BackwardAirDash:
		Weight = GameState->BattleState.RandomManager.RandRange(10, 15);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 360000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::ForwardWalk:
		Weight += GameState->BattleState.RandomManager.RandRange(25, 80);
		if (GetEnemyDistanceX() > 420000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(30, 50);
		}
		if (Player->Enemy->CheckIsStunned())
		{
			Weight += GameState->BattleState.RandomManager.RandRange(15, 30);
		}
		break;
	case EStateType::ForwardDash:
		Weight += GameState->BattleState.RandomManager.RandRange(50, 100);
		if (GetEnemyDistanceX() > 640000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(15, 30);
		}
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 480000)
		{
			Weight -= GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		if (Player->Enemy->CheckIsStunned())
		{
			Weight += GameState->BattleState.RandomManager.RandRange(15, 30);
		}
		if (Player->Direction == Player->Enemy->Direction)
		{
			Weight = 0;
		}
		break;
	case EStateType::BackwardWalk:
		Weight += GameState->BattleState.RandomManager.RandRange(10, 30);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 540000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(5, 20);
		}
		if (Player->Enemy->CheckIsStunned())
		{
			Weight -= GameState->BattleState.RandomManager.RandRange(5, 20);
		}
		break;
	case EStateType::BackwardDash:
		Weight += GameState->BattleState.RandomManager.RandRange(5, 10);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 540000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(5, 10);
		}
		if (Player->Enemy->CheckIsStunned())
		{
			Weight -= GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::Tech:
		// TODO tech weight
		Weight = 250;
		break;
	default:
		break;
	}

	// Madrone change: patience. Patient fighters wait, walk back (blocking) and backdash (more so up close, for space);
	// impatient ones walk, dash and jump in.
	if (const int32 Shift = PatienceShift(); Shift != 0)
	{
		switch (State->StateType)
		{
		case EStateType::Standing:
		case EStateType::Crouching:
		case EStateType::BackwardWalk:
			Weight += Shift;
			break;
		case EStateType::BackwardDash:
			Weight += Shift;
			if (GetEnemyDistanceX() < 300000) Weight += Shift / 2;
			break;
		case EStateType::ForwardWalk:
		case EStateType::ForwardDash:
		case EStateType::ForwardJump:
		case EStateType::ForwardAirDash:
			Weight -= Shift;
			break;
		default:
			break;
		}
	}

	Weight += GameState->BattleState.RandomManager.RandRange(-50, 50);

	return Weight;
}

int32 ANightSkyAIController::PatienceShift() const
{
	// Madrone change: + for a patient fighter, - for an impatient one, 0 at CPU Patience 50. No random roll, so the
	// CPU's random sequence is the same at any patience.
	const int32 Strength = GetDefault<UNightSkyBattleRules>()->CPUPatienceStrength;
	return Strength * (FMath::Clamp(Player->CPUPatience, 0, 100) - 50) / 50;
}

int32 ANightSkyAIController::CheckAttackWeight(const UState* State) const
{
	if (!IsValid(Player)) return 0;

	int32 Weight = GameState->BattleState.RandomManager.RandRange(50, 100);

	const bool bInRange = Player->CheckEnemyInRange(State->CPUData.AttackXBeginRange, State->CPUData.AttackXEndRange,
	                                                State->CPUData.AttackYBeginRange, State->CPUData.AttackYEndRange);
	if (!bInRange)
		Weight -= 50;
	// Madrone change: an attack only from where its CPU Data range says: never out of range (bonuses used to outweigh the
	// -50, so the CPU threw fireballs point blank and swung normals from far away).
	if (!bInRange) return 0;

	// Madrone change: projectiles from far away. Without this, walking and dashing in outscored a fireball at range.
	const UNightSkyBattleRules* Rules = GetDefault<UNightSkyBattleRules>();
	if (State->CPUData.bProjectile && bInRange && Rules->CPUFarProjectileBonus > 0
		&& GetEnemyDistanceX() >= Rules->CPUFarProjectileDistance)
	{
		Weight += GameState->BattleState.RandomManager.RandRange(Rules->CPUFarProjectileBonus / 2, Rules->CPUFarProjectileBonus * 3 / 2);
	}

	// Madrone change: cancels. While the CPU is attacking, the attacks it can enter are its move's cancel options.
	if (Player->CheckIsAttacking() && bInRange && Rules->CPUCancelBonus > 0)
	{
		const int32 Bonus = Rules->CPUCancelBonus * FMath::Clamp(Player->CPUDifficulty, 0, 100) / 100;
		if (Bonus > 0) Weight += GameState->BattleState.RandomManager.RandRange(Bonus / 2, Bonus * 3 / 2);
	}

	// Madrone change: crouching attacks, and mix-ups by how the enemy stands: lows on a standing enemy, overheads on a
	// crouching one (blocking or not). Only when the move can reach.
	const bool bEnemyGrounded = Player->Enemy->PosY <= Player->Enemy->GroundHeight;
	if (bInRange && bEnemyGrounded)
	{
		if (State->EntryStance == EEntryStance::Crouching && Rules->CPUCrouchAttackBonus > 0)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(Rules->CPUCrouchAttackBonus / 2, Rules->CPUCrouchAttackBonus * 3 / 2);
		}
		const bool bBeatsStance = (State->CPUData.BlockType == BLK_Low && Player->Enemy->Stance == ACT_Standing)
			|| (State->CPUData.BlockType == BLK_High && Player->Enemy->Stance == ACT_Crouching);
		if (bBeatsStance && Rules->CPUMixupBonus > 0)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(Rules->CPUMixupBonus / 2, Rules->CPUMixupBonus * 3 / 2);
		}
	}

	if (Player->PosY - Player->GroundHeight < Player->Enemy->PosY - Player->Enemy->GroundHeight && State->CPUData.bAntiAir) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);

	if (State->CPUData.bUsesResource) Weight -= GameState->BattleState.RandomManager.RandRange(15, 40);
	if (State->CPUData.bBigDamage) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);
	if (State->CPUData.bNoCombo && Player->ComboCounter) Weight -= GameState->BattleState.RandomManager.RandRange(15, 40);
	
	switch (State->CPUData.AttackSpeed)
	{
	case ASPD_Fast:
		Weight += GameState->BattleState.RandomManager.RandRange(15, 30);
		break;
	case ASPD_Medium:
		break;
	case ASPD_Slow:
		Weight -= GameState->BattleState.RandomManager.RandRange(15, 30);
		break;
	default: break;
	}

	if (Player->Enemy->PrimaryStateMachine.CurrentState->StateType == EStateType::Hitstun)
	{
		if (State->CPUData.bThrow && !State->CPUData.bCombo) Weight = 0;
		if (State->CPUData.bCombo) Weight += GameState->BattleState.RandomManager.RandRange(80, 250);
	}
	else if (Player->IsEnemyBlocking())
	{
		if (State->CPUData.bBlockstring) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);
		if (Player->Enemy->GetCurrentStateName(StateMachine_Primary) == State_Universal_StandBlock)
		{
			if (State->CPUData.BlockType == BLK_Low) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);
		}
		else if (Player->Enemy->GetCurrentStateName(StateMachine_Primary) == State_Universal_CrouchBlock)
		{
			if (State->CPUData.BlockType == BLK_High) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);
		}
		else if (State->CPUData.BlockType == BLK_None) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);

		if (State->CPUData.bThrow)
		{
			if (Player->Enemy->CheckIsStunned()) Weight = 0;
			else Weight += GameState->BattleState.RandomManager.RandRange(15, 40);
		}
	}
	else
	{
		if (Player->Enemy->IsEnemyAttackState() && State->CPUData.bPunish)
		{
			bool bInPunishRange = false;
			switch (State->CPUData.PunishRange)
			{
			case RAN_Near:
				bInPunishRange = GetEnemyDistanceX() <= 360000;
				break;
			case RAN_Mid:
				bInPunishRange = GetEnemyDistanceX() > 360000 && GetEnemyDistanceX() <= 720000;
				break;
			case RAN_Far:
				bInPunishRange = GetEnemyDistanceX() > 720000;
				break;
			default:
				break;
			}

			Weight += bInPunishRange ? GameState->BattleState.RandomManager.RandRange(15, 40) : 0;
		}

		if (State->CPUData.bInvuln) Weight += GameState->BattleState.RandomManager.RandRange(15, 40);

		// Madrone change: patience. Starting an attack in neutral (not a cancel, combo, punish or pressure on a blocking
		// enemy) is less likely for a patient fighter, more for an impatient one.
		if (!Player->CheckIsAttacking() && !Player->Enemy->IsEnemyAttackState()) Weight -= PatienceShift();
	}
	Weight += GameState->BattleState.RandomManager.RandRange(-50, 50);
	
	return Weight;
}

int32 ANightSkyAIController::CheckDefenseWeight(const UState* State)
{
	if (!IsValid(Player)) return 0;

	int Weight = 0;
	
	switch (State->StateType)
	{
	case EStateType::Crouching:
		Weight += GameState->BattleState.RandomManager.RandRange(50, 80);
		if (Player->Enemy->GetAttackBlockType() == BLK_High)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::BackwardJump:
	case EStateType::BackwardAirDash:
		Weight += GameState->BattleState.RandomManager.RandRange(25, 50);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 360000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::BackwardWalk:
		Weight += GameState->BattleState.RandomManager.RandRange(25, 100);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 360000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(15, 35);
		}
		if (Player->Enemy->GetAttackBlockType() == BLK_Low)
		{
			Weight -= GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::BackwardDash:
		Weight += GameState->BattleState.RandomManager.RandRange(50, 80);
		if (Player->Enemy->CheckIsAttacking() && GetEnemyDistanceX() < 360000)
		{
			Weight += GameState->BattleState.RandomManager.RandRange(10, 20);
		}
		if (Player->Enemy->CheckIsStunned())
		{
			Weight -= GameState->BattleState.RandomManager.RandRange(25, 50);
		}
		break;
	case EStateType::Tech:
		// TODO tech weight
		Weight = 250;
		break;
	default:
		break;
	}
	
	bBlock = true;
	
	Weight += GameState->BattleState.RandomManager.RandRange(-25, 50);
	
	return Weight;
}
