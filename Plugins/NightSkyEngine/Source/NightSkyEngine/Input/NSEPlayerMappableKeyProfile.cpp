// Fill out your copyright notice in the Description page of Project Settings.


#include "NSEPlayerMappableKeyProfile.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "NSEPlayerMappableKeySettings.h"
#include "Algo/AnyOf.h"
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

int32 UNSEPlayerMappableKeyProfile::RemoveForeignMappings()
{
	// The merge also overwrote the row's own default keys, so the whole row goes; re-registering the contexts
	// (UWitchBattleInputSubsystem) rebuilds it from the owner's mapping context.
	TArray<FName> Rows;
	for (const TPair<FName, FKeyMappingRow>& Row : PlayerMappedKeys)
	{
		for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
		{
			const UInputAction* Action = Mapping.GetAssociatedInputAction();
			const UNSEPlayerMappableKeySettings* Settings = Action ? Cast<UNSEPlayerMappableKeySettings>(Action->GetPlayerMappableKeySettings().Get()) : nullptr;
			if (Settings && Settings->Name != Row.Key)
			{
				Rows.Add(Row.Key);
				break;
			}
		}
	}
	// Saves from before NSE's battle pause was renamed (PauseGame -> BattlePause): Narrative's PauseGame row kept NSE's
	// keys (Enter) as its own keys and defaults. If it still has BattlePause's keys, rebuild it.
	const FKeyMappingRow* Battle = PlayerMappedKeys.Find(TEXT("BattlePause"));
	const FKeyMappingRow* Pause = PlayerMappedKeys.Find(TEXT("PauseGame"));
	if (Battle && Pause && !Rows.Contains(TEXT("PauseGame")))
	{
		for (const FPlayerKeyMapping& A : Pause->Mappings)
		{
			const bool bSame = Algo::AnyOf(Battle->Mappings, [&A](const FPlayerKeyMapping& B)
			{
				return B.GetSlot() == A.GetSlot() && B.GetDefaultKey() == A.GetCurrentKey();
			});
			if (bSame)
			{
				Rows.Add(TEXT("PauseGame"));
				break;
			}
		}
	}
	for (const FName& Name : Rows)
	{
		PlayerMappedKeys.Remove(Name);
	}
	return Rows.Num();
}
