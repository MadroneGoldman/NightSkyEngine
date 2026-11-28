// Fill out your copyright notice in the Description page of Project Settings.


#include "State.h"

#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(State)

// Madrone change: a sound when the state starts (EnterSound).
void UState::PlayEnterSound() const
{
	if (!EnterSound || !Parent || !IsValid(Parent->GameState)) return;
	float Duration = EnterSoundMaxDuration > 0.f ? EnterSoundMaxDuration : EnterSound->GetDuration();
	// A looping sound reports INDEFINITELY_LOOPING_DURATION: cap it so its channel is freed again.
	Duration = FMath::Clamp(Duration, 0.05f, 60.f);
	// The game state records it in the rolled-back audio channels and only starts it when not resimulating.
	Parent->GameState->PlayCharaAudio(EnterSound, Duration);
}

void UState::Exec_Implementation()
{
}

void UState::CallExec()
{
	CelIndex = 0;
	Exec();
}

bool UState::CanEnterState_Implementation()
{
	return true;
}

FText UState::GetDisplayName() const
{
	if (!DisplayName.IsEmpty()) return DisplayName;
	// Fallback: the tag's last parts, e.g. Manny.Fireball.A -> "Fireball A", State.Universal.Walk.F -> "Walk F".
	TArray<FString> Parts;
	Name.ToString().ParseIntoArray(Parts, TEXT("."));
	if (Parts.Num() > 1 && (Parts[0] == TEXT("State") || Parts.Num() > 2)) Parts.RemoveAt(0);
	if (Parts.Num() > 1 && Parts[0] == TEXT("Universal")) Parts.RemoveAt(0);
	return FText::FromString(FString::Join(Parts, TEXT(" ")));
}
