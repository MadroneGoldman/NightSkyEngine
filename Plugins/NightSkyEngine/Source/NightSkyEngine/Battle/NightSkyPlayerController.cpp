// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyPlayerController.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "NightSkyGameState.h"
#include "FighterRunners/FighterMultiplayerRunner.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "NightSkyEngine/Battle/Misc/Bitflags.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "NightSkyEngine/Network/NetworkPawn.h"
#include "NightSkyEngine/Miscellaneous/NightSkyGameInstance.h"
#include "NightSkyEngine/Network/RpcConnectionManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyPlayerController)

// Sets default values
ANightSkyPlayerController::ANightSkyPlayerController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	BattleInput = CreateDefaultSubobject<UNightSkyBattleInputComponent>(TEXT("BattleInput"));
	Frame = 0;
	NetworkPawn = nullptr;
	bRematch = false;
}

// Called when the game starts or when spawned
void ANightSkyPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ANightSkyPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	NetworkPawn = Cast<ANetworkPawn>(GetPawn());
	
	if (NetworkPawn != nullptr)
	{
		const int PlayerIndex = Cast<UNightSkyGameInstance>(GetGameInstance())->PlayerIndex;
		TArray<ANetworkPawn*> NetworkPawns;
		for (TActorIterator<ANetworkPawn> It(GetWorld()); It; ++It)
		{
			NetworkPawns.Add(*It);
		}
		if (NetworkPawns.Num() > 1)
		{
			if (PlayerIndex == 0)
			{
				SendGgpo(NetworkPawns[1], true);
			}
			else
			{
				SendGgpo(NetworkPawns[0], false);
			}
		}
		if (NetworkPawn->bRematchAccepted && bRematch)
		{
			BattleInput->Inputs |= INP_Rematch;
		}
		else
		{
			BattleInput->Inputs &= ~INP_Rematch;			
		}
	}
}


void ANightSkyPlayerController::SetupInputComponent()
{
	// Older controller Blueprints set the mapping and actions on the controller: hand them to the component.
	if (BattleInput->InputMapping.IsNull()) BattleInput->InputMapping = InputMapping;
	if (!BattleInput->InputActions.HasAnyAction()) BattleInput->InputActions = InputActions;
	BattleInput->AddMapping();

	if (InputComponent == NULL)
	{
		InputComponent = NewObject<UEnhancedInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), TEXT("PC_InputComponent0"));
		InputComponent->RegisterComponent();
	}

	BattleInput->BindActions(Cast<UEnhancedInputComponent>(InputComponent));
	BattleInput->OnPauseGame.AddUObject(this, &ANightSkyPlayerController::PauseGame);
}

void ANightSkyPlayerController::PauseGame()
{
	const auto GameState = ANightSkyGameState::Get(this);
	if (!GameState) return;
	
	if (!GameState->bPauseGame)
	{
		GameState->SetPaused(true);
		OpenPauseMenu();
	}
}

void ANightSkyPlayerController::SendGgpo(ANetworkPawn* InNetworkPawn, bool Client) const
{
	if(InNetworkPawn->FighterMultiplayerRunner==nullptr)//TODO: CHECK IF MULTIPLAYERRUNNER IS SPAWNED BEFORE THIS, IF SO DO THIS IN BEGINPLAY
	{
		TArray<AActor*> FoundFighterGameStates;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFighterMultiplayerRunner::StaticClass(), FoundFighterGameStates);
		if(FoundFighterGameStates.Num()>0)
		{
			InNetworkPawn->FighterMultiplayerRunner = Cast<AFighterMultiplayerRunner>(FoundFighterGameStates[0]);
		}
	}

	if (InNetworkPawn->FighterMultiplayerRunner && InNetworkPawn->FighterMultiplayerRunner->connectionManager)
	{
		while(InNetworkPawn->FighterMultiplayerRunner->connectionManager->sendSchedule.Num()>0)
		{
			const auto SendVal = InNetworkPawn->FighterMultiplayerRunner->connectionManager->sendSchedule.GetTail();
			if(Client)
			{
				InNetworkPawn->SendGgpoToClient(SendVal->GetValue());
			}
			else
			{
				InNetworkPawn->SendGgpoToServer(SendVal->GetValue());
			}
			InNetworkPawn->FighterMultiplayerRunner->connectionManager->sendSchedule.Empty();
			//InNetworkPawn->FighterMultiplayerRunner->connectionManager->sendSchedule.RemoveNode(SendVal);
		}
	}
}

void ANightSkyPlayerController::SendBattleData()
{
	if (bSentCharaData) return;
	
	int PlayerIndex = Cast<UNightSkyGameInstance>(GetGameInstance())->PlayerIndex;
	TArray<ANetworkPawn*> NetworkPawns;
	for (TActorIterator<ANetworkPawn> It(GetWorld()); It; ++It)
	{
		NetworkPawns.Add(*It);
	}
	if (NetworkPawns.Num() > 1)
	{
		FNetworkMirror NetworkMirror{};
		UNightSkyGameInstance* GameInstance = Cast<UNightSkyGameInstance>(GetGameInstance());
		if (PlayerIndex == 0)
		{
			if (NetworkPawns[1]->CharaDataReceived) return;
			for (const auto& CharaData : GameInstance->BattleData.PlayerListP1)
			{
				auto Result = CharaData.LoadSynchronous();
				NetworkMirror.PlayerList.Add(CharaData->GetPrimaryAssetId());
			}
			NetworkPawns[1]->ClientGetBattleData(GameInstance->BattleData, NetworkMirror);
			NetworkPawns[1]->CharaDataReceived = true;
			bSentCharaData = true;
		}
		else
		{
			for (const auto& CharaData : GameInstance->BattleData.PlayerListP2)
			{
				auto Result = CharaData.LoadSynchronous();
				NetworkMirror.PlayerList.Add(CharaData->GetPrimaryAssetId());
			}
			NetworkPawns[0]->ServerGetBattleData(GameInstance->BattleData, NetworkMirror);
			bSentCharaData = true;
		}
	}
}

void ANightSkyPlayerController::Rematch()
{
	bRematch = true;
	if (const UNightSkyGameInstance* GameInstance = Cast<UNightSkyGameInstance>(GetGameInstance()); GameInstance->
		FighterRunner != Multiplayer)
	{
		ANightSkyGameState::Get(this)->MatchInit();
		bRematch = false;
	}
	else
	{
		int PlayerIndex = Cast<UNightSkyGameInstance>(GetGameInstance())->PlayerIndex;
		TArray<ANetworkPawn*> NetworkPawns;
		for (TActorIterator<ANetworkPawn> It(GetWorld()); It; ++It)
		{
			NetworkPawns.Add(*It);
		}
		if (NetworkPawns.Num() > 1)
		{
			if (PlayerIndex != 0) NetworkPawns[0]->SendRematchToServer();
			else NetworkPawns[1]->bRematchAccepted = true;
		}
	}
}

void ANightSkyPlayerController::PostRematch()
{
	NetworkPawn->bRematchAccepted = false;
	bRematch = false;
	BattleInput->Inputs &= ~INP_Rematch;
}
