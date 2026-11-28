// Madrone change: per-character unique battle systems. See NSECharacterExtension.h.

#include "NSECharacterExtension.h"

#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NSECharacterExtension)

bool UNSECharacterExtension::IsResimulating() const
{
	return GameState && GameState->IsResimulating();
}

APlayerObject* UNSECharacterExtension::GetMainPlayer() const
{
	return Owner ? Owner->GetMainPlayer() : nullptr;
}

bool UNSECharacterExtension::IsP1() const
{
	return Owner && Owner->PlayerIndex == 0;
}

void UNSECharacterExtension::MatchInit_Implementation() {}
void UNSECharacterExtension::RoundInit_Implementation() {}
void UNSECharacterExtension::Update_Implementation() {}
void UNSECharacterExtension::OnAssistCalled_Implementation(APlayerObject* Caller, APlayerObject* Assist) {}
bool UNSECharacterExtension::CanCallAssist_Implementation(APlayerObject* Caller, APlayerObject* Assist) const { return true; }
int32 UNSECharacterExtension::ModifyCooldown_Implementation(const int32 Frames) const { return Frames; }
void UNSECharacterExtension::OnTaggedIn_Implementation(APlayerObject* From) {}
void UNSECharacterExtension::OnTaggedOut_Implementation(APlayerObject* To) {}
bool UNSECharacterExtension::CanTagOut_Implementation(APlayerObject* To) const { return true; }
bool UNSECharacterExtension::CanBeTaggedIn_Implementation(APlayerObject* From) const { return true; }
FGameplayTag UNSECharacterExtension::GetVictoryState_Implementation(APlayerObject* Winner, APlayerObject* Loser, bool bMatchWin) const { return FGameplayTag(); }
void UNSECharacterExtension::OnVictory_Implementation(APlayerObject* Winner, APlayerObject* Loser, bool bMatchWin) {}
bool UNSECharacterExtension::IsVictorySequenceRunning_Implementation() const { return false; }
void UNSECharacterExtension::GetVirtualTeammateBars_Implementation(TArray<FNSETeammateBar>& OutBars) const {}
bool UNSECharacterExtension::GetOwnBarColor_Implementation(FLinearColor& OutColor) const { return false; }
FGameplayTag UNSECharacterExtension::GetCPUForcedState_Implementation() { return FGameplayTag(); }
