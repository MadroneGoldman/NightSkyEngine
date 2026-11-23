// Fill out your copyright notice in the Description page of Project Settings.


#include "State.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(State)

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
