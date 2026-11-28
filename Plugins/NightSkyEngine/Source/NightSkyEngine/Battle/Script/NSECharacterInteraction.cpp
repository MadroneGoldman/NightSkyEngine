// Madrone change: character interactions. See NSECharacterInteraction.h.

#include "NSECharacterInteraction.h"

#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NSECharacterInteraction)

void UNSECharacterInteractionEvent::Finish()
{
	if (bActive) StopEvent(false);
}

bool UNSECharacterInteractionEvent::IsResimulating() const
{
	return GameState && GameState->IsResimulating();
}

bool UNSECharacterInteractionEvent::CanStart_Implementation() const
{
	return Subject && Partner && GameState && Subject->CurrentHealth > 0 && Partner->CurrentHealth > 0
		&& GameState->BattleState.BattlePhase == EBattlePhase::Battle;
}

void UNSECharacterInteractionEvent::Begin_Implementation() {}
void UNSECharacterInteractionEvent::Update_Implementation() {}
void UNSECharacterInteractionEvent::End_Implementation(bool bInterrupted) {}

void UNSECharacterInteractionEvent::StartEvent(const ENSEInteractionTrigger Trigger)
{
	bActive = true;
	Frame = 0;
	StartedBy = Trigger;
	TimesFiredThisRound++;
	TimesFiredThisMatch++;
	Begin();
}

void UNSECharacterInteractionEvent::StepEvent()
{
	if (!bActive) return;
	Frame++;
	Update();
}

void UNSECharacterInteractionEvent::StopEvent(const bool bInterrupted)
{
	bActive = false;
	bHoldSubject = false;
	bHoldPartner = false;
	End(bInterrupted);
}

void UNSECharacterInteractionEvent::ResetForRound()
{
	if (bActive) StopEvent(true);
	TimesFiredThisRound = 0;
}
