// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightSkyEngine/UI/NSEStyle.h"
#include "PrimaryCharaData.generated.h"

class APlayerObject;

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
};
