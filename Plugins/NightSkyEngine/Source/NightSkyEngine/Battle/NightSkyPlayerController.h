// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NightSkyBattleInputComponent.h"
#include "NightSkyPlayerController.generated.h"

class UInputMappingContext;
class ANetworkPawn;

UCLASS()
class NIGHTSKYENGINE_API ANightSkyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANightSkyPlayerController();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void SetupInputComponent() override;
	
	/** Battle input (INP_* bits). Any controller can host one of these; this controller always has one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UNightSkyBattleInputComponent> BattleInput;

	int Frame;
	bool bRematch;
	bool bRematchAccepted;
	bool bSentCharaData;

	/** Copied into BattleInput when its own mapping is unset (keeps existing controller Blueprints working). */
	UPROPERTY(EditAnywhere, Category="Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;
	/** Copied into BattleInput when its own actions are unset (keeps existing controller Blueprints working). */
	UPROPERTY(EditAnywhere, Category="Input")
	FBattleInputActions InputActions;

	void PauseGame();
	
	void SendGgpo(ANetworkPawn* InNetworkPawn, bool Client) const;
	
	UFUNCTION(BlueprintCallable)
	void SendBattleData();
	UFUNCTION(BlueprintCallable)
	void Rematch();
	
	void PostRematch();

	UPROPERTY(BlueprintReadOnly)
	ANetworkPawn* NetworkPawn;

	UFUNCTION(BlueprintImplementableEvent)
	void OpenPauseMenu();

	UFUNCTION(BlueprintImplementableEvent)
	void ClosePauseMenu();
};
