// Fill out your copyright notice in the Description page of Project Settings.


#include "WTCharacter.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "NightSkyWTGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WTCharacter)

// Sets default values
AWTCharacter::AWTCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AWTCharacter::BeginPlay()
{
	Super::BeginPlay();

	GameState = Cast<ANightSkyWTGameState>(GetWorld()->GetGameState());
	if (GameState)
		GameState->OnBattleEndDelegate.AddUniqueDynamic(this, &AWTCharacter::EndBattle);
		GameState->OnBattleStartDelegate.AddUniqueDynamic(this, &AWTCharacter::BattleStarted);
}

// Called every frame
void AWTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AWTCharacter::StartBattle(AWTCharacter* Opponent)
{
}

void AWTCharacter::EndBattle()
{
	SetActorHiddenInGame(false);
	if (bIsNPC && bNPCInBattle)
	{
		bNPCInBattle = false;
		
		//DestroyBattleActor
		BattlePlayer->SetActorHiddenInGame(true);
		BattlePlayer->Destroy();
		
		EndBattle_BP(GameState->BattleState.CurrentWinSide);
	}
}

void AWTCharacter::BattleStarted()
{
	SetActorHiddenInGame(true);
	Battle_Started_BP();
}

FText AWTCharacter::GetCharacterName()
{
	return CharaData.LoadSynchronous()->CharaFriendlyName;
}

void AWTCharacter::AddAlly(UPrimaryCharaData* NewAllyData)
{
	bool bAlreadyAdded = false;
	AlliesSet.Add(NewAllyData->CharaName, &bAlreadyAdded);
	
	if (!bAlreadyAdded)
	{
		Allies.Add(NewAllyData);
	
		//Set assist ally if none
		if (NewAllyData->IsAssist)
		{
			if (CurrentAssistAlly < 0)
			{
				CurrentAssistAlly = Allies.Num() -1;
			}
		}
		//Set tag ally if none
		else
		{
			if (CurrentTagAlly < 0)
			{
				TagAllyName = NewAllyData->CharaName;
				CurrentTagAlly = Allies.Num() -1;
			}
		}	
	}
}

void AWTCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
}

