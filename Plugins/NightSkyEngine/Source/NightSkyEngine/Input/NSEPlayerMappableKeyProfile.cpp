// Fill out your copyright notice in the Description page of Project Settings.


#include "NSEPlayerMappableKeyProfile.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(NSEPlayerMappableKeyProfile)

FName UNSEPlayerMappableKeyProfile::GetPressRowForReleaseAction(const UInputAction* Action)
{
	static const FString Prefix = TEXT("IA_Release");
	if (!Action) return NAME_None;
	const FString Name = Action->GetName();
	if (!Name.StartsWith(Prefix) || Name.Len() == Prefix.Len()) return NAME_None;
	return FName(TEXT("Press") + Name.Mid(Prefix.Len()));
}

int32 UNSEPlayerMappableKeyProfile::GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const
{
	const FName PressRow = DefaultMapping.IsPlayerMappable() ? NAME_None : GetPressRowForReleaseAction(DefaultMapping.Action);
	if (PressRow.IsNone() || !FindKeyMappingRow(PressRow))
	{
		return Super::GetPlayerMappedKeysForRebuildControlMappings(DefaultMapping, OutKeys);
	}
	// Same query the engine makes for a mappable action, on the press row.
	FPlayerMappableKeyQueryOptions QueryOptions = {};
	QueryOptions.MappingName = PressRow;
	QueryOptions.KeyToMatch = DefaultMapping.Key;
	QueryOptions.bMatchBasicKeyTypes = true;
	QueryOptions.bMatchKeyAxisType = false;
	return QueryPlayerMappedKeys(QueryOptions, OutKeys);
}

int32 UNSEPlayerMappableKeyProfile::RemoveReleaseRows()
{
	TArray<FName> ToRemove;
	for (const TPair<FName, FKeyMappingRow>& Row : PlayerMappedKeys)
	{
		const FString Name = Row.Key.ToString();
		if (Name.StartsWith(TEXT("Release")) && PlayerMappedKeys.Contains(FName(TEXT("Press") + Name.Mid(7))))
		{
			ToRemove.Add(Row.Key);
		}
	}
	for (const FName& Name : ToRemove)
	{
		PlayerMappedKeys.Remove(Name);
	}
	return ToRemove.Num();
}
