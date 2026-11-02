// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NightSkyBattleWidget.generated.h"

//Nick changes
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnP1HealthChangedSignature, int /* index */, float /* value */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnP2HealthChangedSignature, int /* index */, float /* value */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnP1RecoverableHealthChangedSignature, int /* index */, float /* value */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnP2RecoverableHealthChangedSignature, int /* index */, float /* value */);


USTRUCT(BlueprintType)
struct FRollbackAnimation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	UWidgetAnimation* Anim;
	
	UPROPERTY(BlueprintReadWrite, SaveGame)
	float Time;

	UPROPERTY(BlueprintReadWrite, SaveGame)
	bool bPlaying;
};

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UNightSkyBattleWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Health")
	TArray<float> P1Health;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Health")
	TArray<float> P2Health;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Health")
	TArray<float> P1RecoverableHealth;
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category="Health")
	TArray<float> P2RecoverableHealth;
	
	//Nick changes
	FOnP1HealthChangedSignature OnP1HealthChangedDelegate;
	FOnP2HealthChangedSignature OnP2HealthChangedDelegate;
	FOnP1RecoverableHealthChangedSignature OnP1RecoverableHealthChangedDelegate;
	FOnP2RecoverableHealthChangedSignature OnP2RecoverableHealthChangedDelegate;
	
	UPROPERTY(BlueprintReadOnly)
	float Timer;
	UPROPERTY(BlueprintReadOnly)
	float P1Meter;
	UPROPERTY(BlueprintReadOnly)
	float P2Meter;
	UPROPERTY(BlueprintReadOnly)
	TArray<float> P1Gauge;
	UPROPERTY(BlueprintReadOnly)
	TArray<float> P2Gauge;
	UPROPERTY(BlueprintReadOnly)
	int P1RoundsWon;
	UPROPERTY(BlueprintReadOnly)
	int P2RoundsWon;
	UPROPERTY(BlueprintReadOnly)
	int P1ComboCounter;
	UPROPERTY(BlueprintReadOnly)
	int P2ComboCounter;
	UPROPERTY(BlueprintReadOnly)
	int Ping;
	UPROPERTY(BlueprintReadOnly)
	int RollbackFrames;
	
	//Madrone change
	UPROPERTY(BlueprintReadOnly)
	TArray<float> P1Cooldowns;
	UPROPERTY(BlueprintReadOnly)
	TArray<float> P2Cooldowns;
	
	UPROPERTY(BlueprintReadWrite, SaveGame)
	TArray<FRollbackAnimation> WidgetAnimationRollback;

	UFUNCTION(BlueprintImplementableEvent)
	void RegisterAnimationsForRollback();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayFadeAnim();
	
	UFUNCTION(BlueprintImplementableEvent)
	void PlayFadeOutAnim();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayHealthAnim();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayRecoverableHealthAnim();
	
	UFUNCTION(BlueprintImplementableEvent)
	void PlayTimerAnim();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayMeterAnim();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayGaugeAnim();
	
	UFUNCTION(BlueprintImplementableEvent)
	void PlayRoundInitAnim();
	
	UFUNCTION() //Nick change
	virtual void SetMainPlayerIndex(bool IsP1, int PlayerIndex);
	
	UFUNCTION() //Nick change
	virtual void RoundInitImplementation(int P1TeamSize, int P2TeamSize);
	
	UFUNCTION(BlueprintImplementableEvent)
	void RoundInit(int P1TeamSize, int P2TeamSize);

	UFUNCTION(BlueprintImplementableEvent)
	void PlayRoundWinAnim(bool bIsP1);

	UFUNCTION(BlueprintImplementableEvent)
	void PlayMatchWinAnim(bool bIsP1);

	UFUNCTION(BlueprintImplementableEvent)
	void PlayComboCounterAnim();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayTookDamageAnim(bool bIsP1);
	
	UFUNCTION(BlueprintImplementableEvent)
	void PlayTagAnim(bool bIsP1);
	
	UFUNCTION(BlueprintImplementableEvent)
	void PlayAssistAnim(bool bIsP1);
	
	void PlayStandardAnimations();
	void SetAnimationRollbackData();
	void RollbackAnimations();

	void SaveForRollback(TArray<uint8>& Data);
	uint64 LoadForRollback(const TArrayView<const uint8>& InBytes);

	void UpdateAndNotifyP1Health(int index, float value);
	void UpdateAndNotifyP2Health(int index, float value);
	void UpdateAndNotifyP1RecoverableHealth(int index, float value);
	void UpdateAndNotifyP2RecoverableHealth(int index, float value);
};
