// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "WTCharacter.generated.h"

class ANightSkyWTGameState;
class APlayerObject;

UCLASS()
class NIGHTSKYENGINE_API AWTCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AWTCharacter();

	UPROPERTY(BlueprintReadOnly)
	ANightSkyWTGameState* GameState;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<APlayerObject> BattlePlayerClass;
	
	UPROPERTY(BlueprintReadOnly)
	APlayerObject* BattlePlayer;

	UPROPERTY(EditAnywhere)
	UPrimaryCharaData* CharaData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UPrimaryCharaData* TeammateCharaData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<UPrimaryCharaData*> Allies;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CurrentTagAlly = 0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CurrentAssistAlly = 0;
	
	UPROPERTY(EditAnywhere)
	bool bIsNPC = false;
	
	bool bNPCInBattle = false;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable)
	void StartBattle(AWTCharacter* Opponent);

	UFUNCTION()
	void EndBattle();

	UFUNCTION(BlueprintImplementableEvent)
	void EndBattle_BP(EWinSide WinSide);
	
	UFUNCTION(BlueprintCallable)
	FText GetCharacterName();
	
	UFUNCTION(BlueprintCallable)
	void AddAlly(UPrimaryCharaData* NewAllyData);
};
