// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyWTGameState.h"

#include "Camera/CameraActor.h"
#include "NightSkyEngine/Battle/Actors/ParticleManager.h"
#include "NightSkyEngine/Battle/FighterRunners/FighterLocalRunner.h"
#include "NightSkyEngine/Miscellaneous/NightSkyGameInstance.h"
#include "CineCameraActor.h"
#include "Camera/CameraComponent.h"
#include "LevelSequenceActor.h"
#include "NightSkyEngine/Data/BattleExtensionData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyWTGameState)

// Sets default values
ANightSkyWTGameState::ANightSkyWTGameState()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	BattleSceneTransform = FTransform();
	bIsBattling = false;
	bIsMatchEnd = false;
}

// Called when the game starts or when spawned
void ANightSkyWTGameState::BeginPlay()
{
	AActor::BeginPlay();

	FActorSpawnParameters SpawnParameters;
	ParticleManager = GetWorld()->SpawnActor<AParticleManager>();
	AudioManager = GetWorld()->SpawnActor<AAudioManager>();
	GameInstance = Cast<UNightSkyGameInstance>(GetGameInstance());
	CameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
	CameraActor->GetCameraComponent()->SetFieldOfView(54);
	SequenceCameraActor = GetWorld()->SpawnActor<ACineCameraActor>(ACineCameraActor::StaticClass());
	SequenceActor = GetWorld()->SpawnActor<ALevelSequenceActor>(ALevelSequenceActor::StaticClass());
}

void ANightSkyWTGameState::Init(APlayerObject* P1, APlayerObject* P2,APlayerObject* P1_TagTeammate, APlayerObject* P1_AssistTeammate, TArray<APlayerObject*> P2Allies)
{
	bIsBattling = true;

	if (IsValid(BattleExtensionData))
	{
		for (auto Extension : BattleExtensionData->ExtensionArray)
		{
			BattleExtensions.Add(NewObject<UBattleExtension>(this, Extension));
			BattleExtensions.Last()->Parent = this;
			BattleExtensionNames.Add(BattleExtensions.Last()->Name);
		}
	}

	int TeamCount1 = 1;
	int TeamCount2 = 1;
	{
		Players.Add(P1);
		P1->SetActorTransform(BattleSceneTransform);
		P1->PlayerFlags |= PLF_IsOnScreen;
		SortedObjects.Add(P1);
		P1->InitPlayer();
		P1->GameState = this;
		P1->AddTickPrerequisiteActor(this);

		if ( P1_TagTeammate != nullptr )
		{
			TeamCount1 = 2;

			Players.Add(P1_TagTeammate);
			P1_TagTeammate->SetActorTransform(BattleSceneTransform);
			SortedObjects.Add(P1_TagTeammate);
			P1_TagTeammate->InitPlayer();
			P1_TagTeammate->GameState = this;
			P1_TagTeammate->AddTickPrerequisiteActor(this);
		}
		if ( P1_AssistTeammate != nullptr )
		{
			P1Assist = P1_AssistTeammate;
			P1_AssistTeammate->SetActorTransform(BattleSceneTransform);
			SortedObjects.Add(P1_AssistTeammate);
			P1_AssistTeammate->InitPlayer();
			P1_AssistTeammate->GameState = this;
			P1_AssistTeammate->AddTickPrerequisiteActor(this);
		}
	}
	{
		Players.Add(P2);
		P2->SetActorTransform(BattleSceneTransform);
		P2->PlayerFlags |= PLF_IsOnScreen;
		SortedObjects.Add(P2);
		P2->bIsCpu = true;
		P2->SpawnDefaultController();
		P2->InitPlayer();
		P2->GameState = this;
		P2->AddTickPrerequisiteActor(this);

		if ( !P2Allies.IsEmpty() )
		{
			for (auto P2Ally : P2Allies)
			{
				if (P2Ally == nullptr){continue;}
				
				Players.Add(P2Ally);
				P2Ally->SetActorTransform(BattleSceneTransform);
				SortedObjects.Add(P2Ally);
				P2Ally->bIsCpu = true;
				P2Ally->SpawnDefaultController();
				P2Ally->InitPlayer();
				P2Ally->GameState = this;
				P2Ally->AddTickPrerequisiteActor(this);
				TeamCount2 += 1;
			}
		}
	}

	for (int i = 0; i < MaxBattleObjects; i++)
	{
		Objects.Add(GetWorld()->SpawnActor<ABattleObject>(ABattleObject::StaticClass(), BattleSceneTransform));
		Objects[i]->GameState = this;
		Objects[i]->AddTickPrerequisiteActor(this);
		SortedObjects.Add(Objects.Last());
	}
	
	UpdateCamera();
	
	OnBattleStartDelegate.Broadcast();
	MatchInitWt(TeamCount1, TeamCount2);
	HUDInit();
}

// Called every frame
void ANightSkyWTGameState::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	
	if (bIsBattling && IsValid(FighterRunner))
		FighterRunner->Update(DeltaTime);

	if (bIsMatchEnd)
	{
		bIsBattling = false;
		bIsMatchEnd = false;
		// Init spawns MaxBattleObjects pooled objects for every battle; destroy them, or every WT battle leaks them.
		for (ABattleObject* Object : Objects)
		{
			if (IsValid(Object)) Object->Destroy();
		}
		Objects.Empty();
		Players.Empty();
		SortedObjects.Empty();
		OnBattleEndDelegate.Broadcast();
	}
}