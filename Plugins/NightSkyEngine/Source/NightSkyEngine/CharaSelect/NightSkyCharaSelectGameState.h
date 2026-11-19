// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "NightSkyCharaSelectGameState.generated.h"

class UPrimaryCharaData;
class UNightSkyGameInstance;
class APlayerObject;

UCLASS()
class NIGHTSKYENGINE_API ANightSkyCharaSelectGameState : public AGameStateBase
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TObjectPtr<UNightSkyGameInstance> GameInstance;
public:
	// Sets default values for this actor's properties
	ANightSkyCharaSelectGameState();

	UPROPERTY(BlueprintReadWrite)
	TArray<APlayerObject*> P1Charas;
	UPROPERTY(EditAnywhere)
	TArray<FVector> P1Positions;
	UPROPERTY(BlueprintReadWrite)
	TArray<APlayerObject*> P2Charas;
	UPROPERTY(EditAnywhere)
	TArray<FVector> P2Positions;
	/** Where each side's assist stands on character select: behind the team. */
	UPROPERTY(EditAnywhere)
	FVector P1AssistPosition = FVector(-250, 110, 0);
	UPROPERTY(EditAnywhere)
	FVector P2AssistPosition = FVector(250, 110, 0);
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerObject> P1AssistChara;
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerObject> P2AssistChara;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void AddPlayerObject(UPrimaryCharaData* Player, bool IsP1 = true);
	UFUNCTION(BlueprintCallable)
	void AddColorIndex(int InColor, bool IsP1 = true);
	/** Sets a side's assist (BattleData.P1Assist / P2Assist) and shows it standing behind the team. Null clears it. */
	UFUNCTION(BlueprintCallable)
	void SetAssist(UPrimaryCharaData* Assist, bool IsP1 = true);
};
