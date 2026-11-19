// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "NSEPlayerMappableKeyProfile.generated.h"

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UNSEPlayerMappableKeyProfile : public UEnhancedPlayerMappableKeyProfile
{
	GENERATED_BODY()

public:
	/**
	 * NSE binds a Press and a Release action per battle button (IA_PressA / IA_ReleaseA) on the same keys. Only the
	 * Press actions are player mappable (rows Press<X>), so settings menus show one row per button; a mapping of
	 * IA_Release<X> takes the player's keys of row Press<X>, so a rebind moves both.
	 */
	virtual int32 GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const override;

	/** Drops Release<X> rows that have a Press<X> row: left in saves from when release actions were mappable. Returns how many. */
	UFUNCTION(BlueprintCallable, Category="Input")
	int32 RemoveReleaseRows();

	/** Row whose keys a release action follows: IA_Release<X> -> Press<X>. None for any other action. */
	static FName GetPressRowForReleaseAction(const UInputAction* Action);
};
