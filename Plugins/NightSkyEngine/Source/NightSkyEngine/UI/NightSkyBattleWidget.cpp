// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyBattleWidget.h"

#include "NightSkyEngine/Battle/Misc/NightSkyBlueprintFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyBattleWidget)

void UNightSkyBattleWidget::SetMainPlayerIndex(bool IsP1, int PlayerIndex)
{
	
}

void UNightSkyBattleWidget::RoundInitImplementation(int P1TeamSize, int P2TeamSize)
{
	RoundInit(P1TeamSize, P2TeamSize);
	PlayRoundInitAnim();
}

void UNightSkyBattleWidget::PlayStandardAnimations()
{
	PlayHealthAnim();
	PlayRecoverableHealthAnim();
	PlayTimerAnim();
	PlayMeterAnim();
	PlayGaugeAnim();
	PlayComboCounterAnim();
}

void UNightSkyBattleWidget::SetAnimationRollbackData()
{
	for (auto& [Anim, Time, bPlaying] : WidgetAnimationRollback)
	{
		if (IsAnimationPlaying(Anim))
		{
			bPlaying = true;
			Time = GetAnimationCurrentTime(Anim);
		}
	}
}

void UNightSkyBattleWidget::RollbackAnimations()
{
	for (const auto& [Anim, Time, bPlaying] : WidgetAnimationRollback)
	{
		if (bPlaying)
			PlayAnimation(Anim, Time);
		else
			SetAnimationCurrentTime(Anim, Time);
	}
}

void UNightSkyBattleWidget::SaveForRollback(TArray<uint8>& Data)
{
	UNightSkyBlueprintFunctionLibrary::SerializeBin(this, Data);
}

uint64 UNightSkyBattleWidget::LoadForRollback(const TArrayView<const uint8>& InBytes)
{
	return UNightSkyBlueprintFunctionLibrary::DeserializeBin(this, InBytes);
}

void UNightSkyBattleWidget::UpdateAndNotifyP1Health(int index, float value)
{
	if (P1Health[index] != value)
	{
		P1Health[index] = value;
		OnP1HealthChangedDelegate.Broadcast(index, value);
		//BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::P1Health);
	}
}

void UNightSkyBattleWidget::UpdateAndNotifyP2Health(int index, float value)
{
	if (P2Health[index] != value)
	{
		P2Health[index] = value;
		OnP2HealthChangedDelegate.Broadcast(index, value);
		//BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::P2Health);
	}
}

void UNightSkyBattleWidget::UpdateAndNotifyP1RecoverableHealth(int index, float value)
{
	if (P1RecoverableHealth[index] != value)
	{
		P1RecoverableHealth[index] = value;
		OnP1RecoverableHealthChangedDelegate.Broadcast(index, value);
		//BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::P1RecoverableHealth);
	}
}

void UNightSkyBattleWidget::UpdateAndNotifyP2RecoverableHealth(int index, float value)
{
	if (P2RecoverableHealth[index] != value)
	{
		P2RecoverableHealth[index] = value;
		OnP2RecoverableHealthChangedDelegate.Broadcast(index, value);
		//BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::P2RecoverableHealth);
	}
}