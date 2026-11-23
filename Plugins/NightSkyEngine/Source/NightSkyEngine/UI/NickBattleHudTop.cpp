// Fill out your copyright notice in the Description page of Project Settings.


#include "NickBattleHudTop.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/UI/NightSkyBattleHudActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NickBattleHudTop)

void UNickBattleHudTop::NativeConstruct()
{
	Super::NativeConstruct();
	
	BingHealthDelegates();
}

void UNickBattleHudTop::NativePreConstruct()
{
	Super::NativePreConstruct();
#if WITH_EDITORONLY_DATA
	if (IsDesignTime() && DesignerPreviewStyle)
	{
		const FNSEBattleHudStyle* Style = DesignerPreviewStyle->GetStyle<FNSEBattleHudStyle>();
		ApplyBattleHudStyle(true, Style, DesignerPreviewStyle);
		ApplyBattleHudStyle(false, Style, DesignerPreviewStyle);
	}
#endif
}

void UNickBattleHudTop::ApplyBattleHudStyle(const bool bIsP1, const FNSEBattleHudStyle* Style, USlateWidgetStyleAsset* StyleAsset)
{
	UHealthBar* Bar = bIsP1 ? P1HealthBar : P2HealthBar;
	UTeammateBarBox* Teammates = bIsP1 ? P1TeammateBox : P2TeammateBox;
	const int32 Side = bIsP1 ? 0 : 1;
	if (Bar)
	{
		if (!AuthoredBarScale[Side].IsSet()) AuthoredBarScale[Side] = Bar->GetRenderTransform().Scale;
		if (Style)
		{
			Bar->SetStyleOverride(Style->MainHealthBar);
			Bar->SetRenderScale(AuthoredBarScale[Side].GetValue() * Style->MainBarScale);
		}
		else
		{
			Bar->ClearStyleOverride();
			Bar->SetRenderScale(AuthoredBarScale[Side].GetValue());
		}
	}
	if (Teammates)
	{
		if (Style) Teammates->SetStyleOverride(Style->TeammateHealthBar);
		else Teammates->ClearStyleOverride();
	}
	for (const TCHAR* Name : bIsP1 ? TArray<const TCHAR*>{TEXT("P1Name"), TEXT("P1TagName")} : TArray<const TCHAR*>{TEXT("P2Name"), TEXT("P2TagName")})
	{
		UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Name));
		if (!Text) continue;
		const TPair<FSlateFontInfo, FSlateColor>& Authored = AuthoredNames.FindOrAdd(Name, {Text->GetFont(), Text->GetColorAndOpacity()});
		FSlateFontInfo Font = Authored.Key;
		if (Style && Style->bOverrideNameFont)
		{
			const float Size = Font.Size; // keep each label's own size (partner names are smaller)
			Font = Style->NameFont;
			Font.Size = Size;
		}
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Style && Style->bOverrideNameColor ? Style->NameColor : Authored.Value);
	}
	Super::ApplyBattleHudStyle(bIsP1, Style, StyleAsset);
}

void UNickBattleHudTop::OnMainPlayersChanged()
{
	// The team arrays list the fighter on screen first, so the main bars always show index 0.
	if (P1HealthBar) P1HealthBar->SetMainPlayerIndex(0);
	if (P2HealthBar) P2HealthBar->SetMainPlayerIndex(0);
	RefreshPlayerNames();
}

void UNickBattleHudTop::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
//	P1TeammateBox->AddHealthbars(3);
//	P2TeammateBox->AddHealthbars(3);
}


void UNickBattleHudTop::BingHealthDelegates()
{
	OnP1HealthChangedDelegate.AddUObject(P1HealthBar, &UHealthBar::SetHealthPercent);
	OnP1RecoverableHealthChangedDelegate.AddUObject(P1HealthBar, &UHealthBar::SetRecoverableHealthPercent);
	OnP2HealthChangedDelegate.AddUObject(P2HealthBar, &UHealthBar::SetHealthPercent);
	OnP2RecoverableHealthChangedDelegate.AddUObject(P2HealthBar, &UHealthBar::SetRecoverableHealthPercent);
	
	
	OnP1HealthChangedDelegate.AddUObject(P1TeammateBox, &UTeammateBarBox::SetHealthPercent);
	OnP1RecoverableHealthChangedDelegate.AddUObject(P1TeammateBox, &UTeammateBarBox::SetRecoverableHealthPercent);
	OnP2HealthChangedDelegate.AddUObject(P2TeammateBox, &UTeammateBarBox::SetHealthPercent);
	OnP2RecoverableHealthChangedDelegate.AddUObject(P2TeammateBox, &UTeammateBarBox::SetRecoverableHealthPercent);
}


void UNickBattleHudTop::RoundInitImplementation(int P1TeamSize, int P2TeamSize)
{
	P1TeammateBox->AddHealthbars(P1TeamSize - 1);
	P2TeammateBox->AddHealthbars(P2TeamSize - 1);
	
	RoundInit(P1TeamSize, P2TeamSize);
	PlayRoundInitAnim();
	RefreshPlayerNames(); // Madrone change
}

void UNickBattleHudTop::SetMainPlayerIndex(bool IsP1, int PlayerIndex)
{
	// Madrone change: PlayerIndex is the side, not a team index: the team arrays (GetTeam) list the fighter on screen
	// first, so the main bar always shows index 0 (P2's showed its partner after a tag). UpdateHUD also calls
	// OnMainPlayersChanged for this after a rollback.
	OnMainPlayersChanged();
}

void UNickBattleHudTop::RefreshPlayerNames()
{
	// The battle's game state is the one whose HUD this is (in overworld battles the world's game state is Narrative's).
	const ANightSkyGameState* GameState = nullptr;
	for (TActorIterator<ANightSkyGameState> It(GetWorld()); It; ++It)
	{
		if (It->BattleHudActor && It->BattleHudActor->TopWidget == this)
		{
			GameState = *It;
			break;
		}
	}
	if (!GameState) return;
	for (const bool bIsP1 : {true, false})
	{
		const TArray<APlayerObject*> Team = GameState->GetTeam(bIsP1); // the fighter on screen first
		const APlayerObject* Main = GameState->GetMainPlayer(bIsP1);
		if (!Main) continue;
		TArray<FString> Partners;
		for (const APlayerObject* Player : Team)
		{
			if (Player && Player != Main && !Player->CharacterName.IsEmpty()) Partners.Add(Player->CharacterName.ToString());
		}
		if (UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(bIsP1 ? TEXT("P1Name") : TEXT("P2Name"))))
		{
			Name->SetText(Main->CharacterName);
		}
		if (UTextBlock* Tag = Cast<UTextBlock>(GetWidgetFromName(bIsP1 ? TEXT("P1TagName") : TEXT("P2TagName"))))
		{
			Tag->SetText(FText::FromString(FString::Join(Partners, TEXT(" / "))));
			Tag->SetVisibility(Partners.Num() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
