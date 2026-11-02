// Fill out your copyright notice in the Description page of Project Settings.


#include "NickBattleHudTop.h"

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
}
