// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightSkyEngine/UI/NSEStyle.h"
#include "GameplayTagContainer.h"
#include "NightSkyEngine/Battle/Script/NSECharacterInteraction.h" // Madrone change
#include "PrimaryCharaData.generated.h"

class APlayerObject;
class UNSECharacterExtension;

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UPrimaryCharaData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("PrimaryCharaData", GetFName());
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName CharaName;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText CharaFriendlyName;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText CharaDescription;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UTexture2D* CharaHUDIcon;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UTexture2D* CharaSelectIcon;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UTexture2D* CharaSplashIcon;
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TSubclassOf<APlayerObject> PlayerClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool IsAssist = false;

	/** Madrone change: the battle HUD look whenever this character fights (e.g. a boss): an NSE Battle Hud Style asset,
	 * its priority, whole HUD or own side, and whether only while on screen. An overworld encounter can set its own. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="HUD")
	FNSEHudStyleRequest BattleHudStyle;

	/** Madrone change: what this character adds to the battle HUD above its super meter while it's on screen (stocks,
	 * special meters), e.g. the witch's potions. Nick Battle Hud Bottom places them. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="HUD")
	TArray<TSubclassOf<class UNSEFighterHudElement>> BattleHudElements;

	/** Madrone change: who this character is, for character interactions (pairs of characters on screen together) and
	 * extensions that look for a character. Empty: Character.<CharaName> when that tag exists (GetCharacterTag). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Character Systems", meta=(Categories="Character"))
	FGameplayTag CharacterTag;

	/** Madrone change: this character's unique battle systems, one instance per fighter for the whole match. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Character Systems")
	TArray<TSubclassOf<UNSECharacterExtension>> CharacterExtensions;

	/** Madrone change: pairs this character takes part in (an empty Character A means this character). Merged with
	 * Battle Rules > Character Interactions and the game state's at match start. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Character Systems")
	TArray<FNSECharacterInteractionPair> CharacterInteractions;

	/** Madrone change: a partner this character always fights with: when it's alone on its side in a battle started
	 * from Battle Data (VS, training, PIE stages), this character is added as its tag partner and the battle becomes a
	 * tag battle. Overworld battles pick their teams themselves. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Character Systems")
	TSoftObjectPtr<UPrimaryCharaData> DefaultTagPartner;

	/** Madrone change: CharacterTag, or Character.<CharaName> when that tag is registered, else empty. */
	UFUNCTION(BlueprintPure, Category="Character Systems")
	FGameplayTag GetCharacterTag() const
	{
		if (CharacterTag.IsValid()) return CharacterTag;
		if (CharaName.IsNone()) return FGameplayTag();
		return FGameplayTag::RequestGameplayTag(FName(*(TEXT("Character.") + CharaName.ToString())), false);
	}
};
