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
	if (IsP1)
	{
		P1HealthBar->SetMainPlayerIndex(PlayerIndex);
	}
	else
	{
		P2HealthBar->SetMainPlayerIndex(PlayerIndex);
	}
	RefreshPlayerNames(); // Madrone change: a tag changes who is in
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
