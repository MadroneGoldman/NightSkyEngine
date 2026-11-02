// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HealthBar.h"
#include "NightSkyBattleWidget.h"
#include "TeammateBarBox.h"
#include "NickBattleHudTop.generated.h"

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UNickBattleHudTop : public UNightSkyBattleWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UHealthBar* P1HealthBar;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UHealthBar* P2HealthBar;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UTeammateBarBox* P1TeammateBox;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadWrite)
	UTeammateBarBox* P2TeammateBox;
	
	virtual void RoundInitImplementation(int P1TeamSize, int P2TeamSize) override;
	
	virtual void SetMainPlayerIndex(bool IsP1, int PlayerIndex) override;
	
	void BingHealthDelegates();
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	
private:
	
};
