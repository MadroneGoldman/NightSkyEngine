// Madrone change: Blueprint helpers for character systems. See NSECharacterLibrary.h.

#include "NSECharacterLibrary.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Actors/ParticleManager.h"
#include "NightSkyEngine/Battle/Misc/Globals.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/Battle/Script/NSECharacterExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NSECharacterLibrary)

FGameplayTag UNSECharacterLibrary::GetCharacterTag(const APlayerObject* Fighter)
{
	return Fighter ? Fighter->GetCharacterTag() : FGameplayTag();
}

bool UNSECharacterLibrary::IsFighterOnScreen(const APlayerObject* Fighter)
{
	return IsValid(Fighter) && Fighter->IsOnScreen() && Fighter->IsActive;
}

TArray<APlayerObject*> UNSECharacterLibrary::FindFightersByTag(const UObject* WorldContext, const FGameplayTag CharacterTag)
{
	TArray<APlayerObject*> Result;
	const ANightSkyGameState* GameState = ANightSkyGameState::Get(WorldContext);
	if (!GameState || !CharacterTag.IsValid()) return Result;
	for (APlayerObject* Fighter : GameState->GetRollbackPlayers())
	{
		if (IsValid(Fighter) && Fighter->GetCharacterTag().MatchesTagExact(CharacterTag)) Result.Add(Fighter);
	}
	return Result;
}

void UNSECharacterLibrary::SetFighterHealth(APlayerObject* Fighter, const int32 Health)
{
	if (!Fighter) return;
	Fighter->CurrentHealth = FMath::Clamp(Health, 0, Fighter->MaxHealth);
	if (Fighter->CurrentHealth == 0) Fighter->RecoverableHealth = 0;
}

bool UNSECharacterLibrary::EnterState(APlayerObject* Fighter, const FGameplayTag StateName)
{
	if (!Fighter || !StateName.IsValid() || !Fighter->PrimaryStateMachine.StateNames.Contains(StateName)) return false;
	return Fighter->JumpToStatePrimary(StateName);
}

void UNSECharacterLibrary::PlayParticleOnFighter(APlayerObject* Fighter, UNiagaraSystem* System, FVector Offset, const float Scale)
{
	if (!Fighter || !System || !Fighter->GameState || !Fighter->GameState->ParticleManager) return;
	const ANightSkyGameState* GameState = Fighter->GameState;
	if (Fighter->Direction == DIR_Left) Offset.X = -Offset.X;
	FVector Location = Offset + FVector(Fighter->PosX / COORD_SCALE, 0, Fighter->PosY / COORD_SCALE);
	Location = GameState->BattleSceneTransform.GetRotation().RotateVector(Location) + GameState->BattleSceneTransform.GetLocation();
	const FRotator Rotation = GameState->BattleSceneTransform.GetRotation().Rotator();
	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Fighter, System, Location, Rotation,
		Fighter->GetActorScale() * Scale);
	if (!Component) return;
	// As ABattleObject::CreateCharaParticle: aged by the particle manager, so rollbacks rewind and replay it.
	GameState->ParticleManager->BattleParticles.Add(FBattleParticle(Component, nullptr));
	Component->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
	Component->SetDesiredAge(0);
	Component->SetCustomDepthStencilValue(2);
	Component->SetBoundsScale(40000);
}

bool UNSECharacterLibrary::BringFighterOnScreen(APlayerObject* Fighter, const FGameplayTag State, const bool bAtMainPlayer, const int32 PosX)
{
	if (!Fighter || !Fighter->GameState) return false;
	ANightSkyGameState* GameState = Fighter->GameState;
	const bool bIsP1 = Fighter->PlayerIndex == 0;
	if (APlayerObject* Opponent = GameState->GetMainPlayer(!bIsP1)) Fighter->Enemy = Opponent;
	const APlayerObject* Main = GameState->GetMainPlayer(bIsP1);
	Fighter->SetOnScreen(true);
	Fighter->IsActive = true;
	Fighter->PosX = bAtMainPlayer && Main ? Main->PosX : PosX;
	Fighter->PosY = 0;
	Fighter->FaceOpponent();
	if (State.IsValid() && Fighter->PrimaryStateMachine.StateNames.Contains(State)) Fighter->JumpToStatePrimary(State);
	GameState->SetDrawPriorityBack(Fighter);
	return true;
}

void UNSECharacterLibrary::ShowBattleSubtitle(const UObject* WorldContext, const FText Speaker, const FText Line, const float Seconds)
{
	if (ANightSkyGameState* GameState = ANightSkyGameState::Get(WorldContext)) GameState->ShowBattleSubtitle(Speaker, Line, Seconds);
}

UNSECharacterExtension* UNSECharacterLibrary::GetCharacterExtension(const APlayerObject* Fighter, const TSubclassOf<UNSECharacterExtension> Class)
{
	if (!Fighter || !Class) return nullptr;
	for (UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
	{
		if (Extension && Extension->IsA(Class)) return Extension;
	}
	return nullptr;
}

void UNSECharacterLibrary::SetAssistCooldown(const UObject* WorldContext, const bool bIsP1, const int32 Frames)
{
	if (ANightSkyGameState* GameState = ANightSkyGameState::Get(WorldContext))
		GameState->BattleState.AssistCooldownTimer[bIsP1 ? 0 : 1] = FMath::Max(0, Frames);
}

APlayerObject* UNSECharacterLibrary::TagOut(APlayerObject* Fighter, const int32 TeamIndex, const bool bForce)
{
	return Fighter ? Fighter->SwitchMainPlayer(TeamIndex, bForce) : nullptr;
}
