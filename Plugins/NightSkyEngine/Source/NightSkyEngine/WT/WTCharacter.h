// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AlsCharacter.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "WTCharacter.generated.h"

struct FInputActionValue;
class UAlsCameraComponent;
class UInputMappingContext;
class UInputAction;

class ANightSkyWTGameState;
class APlayerObject;

UCLASS(AutoExpandCategories = ("Settings|Als Character Example"))
class NIGHTSKYENGINE_API AWTCharacter : public AAlsCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AWTCharacter();

	UPROPERTY(BlueprintReadOnly)
	ANightSkyWTGameState* GameState;
	
//	UPROPERTY(EditAnywhere)
//	TSubclassOf<APlayerObject> BattlePlayerClass;
	
	UPROPERTY(BlueprintReadOnly)
	APlayerObject* BattlePlayer;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UPrimaryCharaData> CharaData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere) //TODO: maybe delete
	TSoftObjectPtr<UPrimaryCharaData>  TeammateCharaData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<TSoftObjectPtr<UPrimaryCharaData> > Allies;
	
	//UPROPERTY(BlueprintReadWrite, EditAnywhere)
	//TMap<FName, TSoftObjectPtr<UPrimaryCharaData>> AlliesMap;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TSet<FName> AlliesSet;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName TagAllyName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CurrentTagAlly = -1;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CurrentAssistAlly = -1;
	
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

	UFUNCTION(BlueprintCallable) //Player initiates fight
	virtual void StartBattle(AWTCharacter* Opponent);

	UFUNCTION() //Response to gamestate delegate
	virtual void BattleStarted();
	
	UFUNCTION()  //Response to gamestate delegate
	virtual void EndBattle();
	
	UFUNCTION(BlueprintImplementableEvent)
	void Battle_Started_BP();
	
	UFUNCTION(BlueprintImplementableEvent)
	void EndBattle_BP(EWinSide WinSide);
	
	UFUNCTION(BlueprintCallable)
	FText GetCharacterName();
	
	UFUNCTION(BlueprintCallable)
	void AddAlly(UPrimaryCharaData* NewAllyData);

};
