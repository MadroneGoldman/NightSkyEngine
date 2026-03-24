// Fill out your copyright notice in the Description page of Project Settings.


#include "WTCharacter.h"

#include "NightSkyWTGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WTCharacter)

// Sets default values
AWTCharacter::AWTCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AWTCharacter::BeginPlay()
{
	Super::BeginPlay();

	GameState = Cast<ANightSkyWTGameState>(GetWorld()->GetGameState());
	if (GameState)
		GameState->OnBattleEndDelegate.AddUniqueDynamic(this, &AWTCharacter::EndBattle);

	//BattlePlayer = GetWorld()->SpawnActor<APlayerObject>(BattlePlayerClass);
	BattlePlayer = GetWorld()->SpawnActor<APlayerObject>(CharaData->PlayerClass);
	BattlePlayer->CharacterName = CharaData->CharaFriendlyName;
	BattlePlayer->SetActorHiddenInGame(true);
}

// Called every frame
void AWTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AWTCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AWTCharacter::StartBattle(AWTCharacter* Opponent)
{
	// Hide WT character and show battle player.
	SetActorHiddenInGame(true);
	BattlePlayer->SetActorHiddenInGame(false);
	Opponent->SetActorHiddenInGame(true);
	Opponent->BattlePlayer->SetActorHiddenInGame(false);
	Opponent->bNPCInBattle = true; 

	APlayerObject* PlayerTagTeammate = nullptr;
	APlayerObject* PlayerAssistTeammate = nullptr;
	TArray<APlayerObject*> OpponentAllies = {};
	
	if (!Allies.IsEmpty())
	{
		//Set tag
		if (Allies[CurrentTagAlly] != nullptr && !Allies[CurrentTagAlly]->IsAssist)
		{
			PlayerTagTeammate = GetWorld()->SpawnActor<APlayerObject>(Allies[CurrentTagAlly]->PlayerClass); 
			PlayerTagTeammate->CharacterName = Allies[CurrentTagAlly]->CharaFriendlyName; 
			PlayerTagTeammate->SetActorHiddenInGame(true);
		}
		
		//Set assist
		if (Allies[CurrentAssistAlly] != nullptr && Allies[CurrentTagAlly]->IsAssist)
		{
			PlayerAssistTeammate = GetWorld()->SpawnActor<APlayerObject>(Allies[CurrentTagAlly]->PlayerClass); 
			PlayerAssistTeammate->CharacterName = Allies[CurrentTagAlly]->CharaFriendlyName; 
			PlayerAssistTeammate->SetActorHiddenInGame(true);
		}
	}
	if (!Opponent->Allies.IsEmpty())
	{	for (UPrimaryCharaData* OpponentTeammateData : Opponent->Allies )
	{
		APlayerObject* OpponentTeammate = nullptr;
		OpponentTeammate = GetWorld()->SpawnActor<APlayerObject>(OpponentTeammateData->PlayerClass); 
		OpponentTeammate->CharacterName = OpponentTeammateData->CharaFriendlyName; 
		OpponentTeammate->SetActorHiddenInGame(true);
		OpponentAllies.Add(OpponentTeammate);
	}
		
	}
	
	GameState->Init(BattlePlayer, Opponent->BattlePlayer, PlayerTagTeammate, PlayerAssistTeammate, OpponentAllies );
}

void AWTCharacter::EndBattle()
{
	if (!bIsNPC || bNPCInBattle)
	{
		bNPCInBattle = false;
		SetActorHiddenInGame(false);
		BattlePlayer->SetActorHiddenInGame(true);
		EndBattle_BP(GameState->BattleState.CurrentWinSide);
	}
}

FText AWTCharacter::GetCharacterName()
{
	return CharaData->CharaFriendlyName;
}

void AWTCharacter::AddAlly(UPrimaryCharaData* NewAllyData)
{
	Allies.Add(NewAllyData);
}