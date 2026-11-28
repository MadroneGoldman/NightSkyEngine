// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SHealthBar.h"
#include "Components/VerticalBox.h"
#include "NightSkyEngine/Battle/Script/NSECharacterExtension.h" // Madrone change
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

	/** Madrone change: a battle HUD style's teammate bar look, for the bars now and the ones AddHealthbars makes later.
	 * Its fill falls back to Image Fill when it has none. Cleared: Image Fill only (the Blueprint's look). */
	void SetStyleOverride(const FHealthBarStyle& InStyle);
	void ClearStyleOverride();

	/** Madrone change: character extensions' bars. RealOverrides: one per real teammate bar (recolour when
	 * bOverrideColor). Virtual: extra bars after the real ones (health, colour, shown or collapsed). */
	void SetCharacterBars(const TArray<struct FNSETeammateBar>& Virtual, const TArray<struct FNSETeammateBar>& RealOverrides);
	/** Madrone change: how many character-extension bars are shown now (tests). */
	UFUNCTION(BlueprintPure, Category="Health")
	int32 GetVisibleVirtualBarCount() const;

	/** Madrone change: with character-extension bars the box is one vertical list in its own space (rows get thinner
	 * past four bars, never a second column), each bar with its name (FNSETeammateBar.Name, the real partners' names) in
	 * this font size, scaled down by 4 / bars past four (at least 5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health")
	int32 CharacterBarNameSize = 9;
	
	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface
	
protected:
	TArray<TSharedPtr<SHealthBar>> TeammateHealthBars;

	TOptional<FHealthBarStyle> StyleOverride;

	// Madrone change: character-extension bars (after the real ones) and what each bar was last given
	TArray<TSharedPtr<SHealthBar>> VirtualHealthBars;
	TArray<TOptional<FLinearColor>> RealBarColors;
	TArray<TOptional<FLinearColor>> VirtualBarColors;
	TArray<bool> VirtualBarVisible;
	TArray<TSharedPtr<class STextBlock>> RealBarNames;
	TArray<TSharedPtr<class STextBlock>> VirtualBarNames;
	TArray<TSharedPtr<SWidget>> VirtualBarCells;
	bool bCharacterLayout = false;
	void RebuildCharacterLayout();
	TSharedRef<SWidget> MakeCharacterCell(const TSharedPtr<SHealthBar>& Bar, TSharedPtr<class STextBlock>& OutName, int32 NameSize) const;
	void ApplyStyle(SHealthBar& Bar, const TOptional<FLinearColor>& Color) const;

	void ApplyStyle(SHealthBar& Bar) const;
};
