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

	/** Madrone change: P1Name / P2Name show each side's fighter on screen, P1TagName / P2TagName its partners (" / "
	 * between them, hidden when there are none). Text blocks are found by name, so a layout without them still works.
	 * Runs at round init and on every tag, so the names follow who is in. */
	UFUNCTION(BlueprintCallable, Category="Names")
	void RefreshPlayerNames();

	/** Madrone change: restyles a side's bars (main and teammates), the main bar's size and the names. */
	virtual void ApplyBattleHudStyle(bool bIsP1, const FNSEBattleHudStyle* Style, USlateWidgetStyleAsset* StyleAsset) override;
	virtual void OnMainPlayersChanged() override;

	/** Madrone change: per side, under the names, the tag cooldown (TAG + seconds, when the side has a partner) and the
	 * dedicated assist's portrait (its CHR_ select icon) with its cooldown over it. Built in code on the root canvas, so
	 * every battle HUD Blueprint gets it; refreshed at round init, numbers every frame. */
	UFUNCTION(BlueprintCallable, Category="Cooldowns")
	void RefreshCooldownPanels();

#if WITH_EDITORONLY_DATA
	/** Designer only: shows this battle HUD style on both sides (an NSE Battle Hud Style asset). */
	UPROPERTY(EditAnywhere, Category="Style")
	TObjectPtr<USlateWidgetStyleAsset> DesignerPreviewStyle;
#endif
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
private:
	void BuildCooldownPanels();
	const class ANightSkyGameState* FindGameState() const;

	UPROPERTY(Transient) TObjectPtr<class UWidget> TagBox[2];
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> TagText[2];
	UPROPERTY(Transient) TObjectPtr<class UWidget> AssistBox[2];
	/** Borders, not images: in the world-space top HUD (a widget component) runtime-made UImages don't draw. */
	UPROPERTY(Transient) TObjectPtr<class UBorder> AssistIcon[2];
	UPROPERTY(Transient) TObjectPtr<class UBorder> AssistShade[2];
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> AssistText[2];
	/** The game state this HUD belongs to, and the assists shown: the panels refresh when an assist changes (the
	 * assists are set after the first round init). */
	TWeakObjectPtr<const class ANightSkyGameState> CachedGameState;
	TWeakObjectPtr<const class APlayerObject> ShownAssist[2];
	bool bCooldownPanelsRefreshed = false;

	/** The Blueprint's own main bar scale and name fonts, put back when a style is cleared. */
	TOptional<FVector2D> AuthoredBarScale[2];
	TMap<FName, TPair<FSlateFontInfo, FSlateColor>> AuthoredNames;
	
};
