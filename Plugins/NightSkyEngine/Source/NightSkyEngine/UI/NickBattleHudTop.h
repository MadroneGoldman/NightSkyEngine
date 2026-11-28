// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HealthBar.h"
#include "NightSkyBattleWidget.h"
#include "TeammateBarBox.h"
#include "Engine/Texture2D.h"
#include "Slate/WidgetTransform.h"
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
	/** Madrone change: character-extension bars go in the side's teammate box. */
	virtual void SetCharacterTeammateBars(bool bIsP1, const TArray<struct FNSETeammateBar>& Virtual, const TArray<struct FNSETeammateBar>& RealOverrides) override;

	/** Madrone change: per side, under the names, the dedicated assist's portrait (its CHR_ select icon) with its cooldown
	 * over it (built in code on the root canvas, so every battle HUD Blueprint gets it), and the tag cooldown next to the
	 * tag partner's name (P1TagName / P2TagName). Ready to call: the partner's name and the portrait's frame pulse
	 * ReadyColor with an outline, and ReadySound plays when one becomes ready. Refreshed at round init, every frame. */
	UFUNCTION(BlueprintCallable, Category="Cooldowns")
	void RefreshCooldownPanels();

	/** Plays when a tag partner or an assist comes off cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cooldowns")
	TSoftObjectPtr<USoundBase> ReadySound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing")));

	/** Also for player 2's side (off: only the player's own, player 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cooldowns")
	bool bReadySoundForP2 = false;

	/** What a ready tag partner name and assist frame pulse in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cooldowns")
	FLinearColor ReadyColor = FLinearColor(1.f, 0.25f, 0.7f, 1.f);

	/** Madrone change: round marks as hearts: each side shows the rounds it can still lose, a heart turning black for
	 * every round the other side won. Images P1Round1.. / P2Round1.. (the Blueprint's colour bindings are dropped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rounds")
	bool bRoundHearts = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rounds")
	TSoftObjectPtr<UTexture2D> RoundHeartFull = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/WitchTools/BattleCommon/NightSkyEngine/UI/Battle/Textures/T_UI_RoundHeart_Full.T_UI_RoundHeart_Full")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rounds")
	TSoftObjectPtr<UTexture2D> RoundHeartLost = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/WitchTools/BattleCommon/NightSkyEngine/UI/Battle/Textures/T_UI_RoundHeart_Lost.T_UI_RoundHeart_Lost")));

	/** Madrone change: the combo counter's look: pixel font, white at 1 hit, light blue at 2, warming to red at Combo
	 * Red Hits; past it the number boils and glows pink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	bool bStyledComboCounter = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	TSoftObjectPtr<UObject> ComboFont = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/WitchTools/BattleCommon/NightSkyEngine/UI/Battle/Fonts/F_BoldPixels.F_BoldPixels")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	int32 ComboFontSize = 112;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo", meta=(ClampMin="2"))
	int32 ComboRedHits = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	FLinearColor ComboColdColor = FLinearColor(0.45f, 0.78f, 1.f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	FLinearColor ComboHotColor = FLinearColor(1.f, 0.08f, 0.06f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	FLinearColor ComboGlowColor = FLinearColor(1.f, 0.3f, 0.72f, 1.f);
	/** New boil poses a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo")
	float ComboBoilRate = 12.f;

	/** The colour of a combo of Hits hits. */
	UFUNCTION(BlueprintPure, Category="Combo")
	FLinearColor ComboColor(int32 Hits) const;

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

	UPROPERTY(Transient) TObjectPtr<class UWidget> AssistBox[2];
	UPROPERTY(Transient) TObjectPtr<class UBorder> AssistFrame[2];
	/** Borders, not images: in the world-space top HUD (a widget component) runtime-made UImages don't draw. */
	UPROPERTY(Transient) TObjectPtr<class UBorder> AssistIcon[2];
	UPROPERTY(Transient) TObjectPtr<class UBorder> AssistShade[2];
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> AssistText[2];
	/** The game state this HUD belongs to, and the assists shown: the panels refresh when an assist changes (the
	 * assists are set after the first round init). */
	TWeakObjectPtr<const class ANightSkyGameState> CachedGameState;
	TWeakObjectPtr<const class APlayerObject> ShownAssist[2];
	bool bCooldownPanelsRefreshed = false;
	/** The tag partners' names (as RefreshPlayerNames writes them) and their font without an outline. */
	FText PartnerNames[2];
	FSlateFontInfo TagNameFont[2];
	bool bTagNameFont[2] = {false, false};
	/** Last frame's seconds left, to play ReadySound when one reaches 0. */
	int32 LastTagSeconds[2] = {0, 0};
	int32 LastAssistSeconds[2] = {0, 0};
	void PlayReadySound();

	void UpdateRoundHearts();
	void UpdateComboCounters(float DeltaTime);
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedHeartFull;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedHeartLost;
	UPROPERTY(Transient) TObjectPtr<UObject> LoadedComboFont;
	bool bHeartTexturesLoaded = false;
	bool bComboFontLoaded = false;
	float ComboBoilTimer[2] = {0.f, 0.f};
	FWidgetTransform ComboBoilPose[2];
	FWidgetTransform ComboBoilBase[2];
	TOptional<FWidgetTransform> ComboBoilApplied[2];

	/** The Blueprint's own main bar scale and name fonts, put back when a style is cleared. */
	TOptional<FVector2D> AuthoredBarScale[2];
	TMap<FName, TPair<FSlateFontInfo, FSlateColor>> AuthoredNames;
	
};
