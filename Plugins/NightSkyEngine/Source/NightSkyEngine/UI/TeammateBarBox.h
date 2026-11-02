// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SHealthBar.h"
#include "Components/VerticalBox.h"
#include "TeammateBarBox.generated.h"

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UTeammateBarBox : public UVerticalBox
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Material/Image to draw */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	FSlateBrush ImageFill;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	int TeammateCount = 3;
	
	/** Defines the direction in which the Health bar fills */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	TEnumAsByte<EHealthBarFillType::Type> BarFillType;
	
	/** Sets the current value of the HealthBar. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetHealthPercent(int index, float InPercent);
	
	/** Sets the current value of the HealthBar. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetRecoverableHealthPercent(int index,float InPercent);
	
	UFUNCTION(BlueprintCallable, Category="Health")
	void AddHealthbars(int NewCount);
	
	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface
	
protected:
	TArray<TSharedPtr<SHealthBar>> TeammateHealthBars;
	
	void MakeDynamicFillImage(TSharedPtr<SHealthBar>& TeammateHealthBar);
};
