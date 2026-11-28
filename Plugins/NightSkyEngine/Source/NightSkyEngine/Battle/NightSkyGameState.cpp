// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyGameState.h"
#include "NightSkyEngine/Battle/Misc/BaroqueCancelVisuals.h" // Madrone change
#include "NightSkyEngine/Miscellaneous/NightSkyBattleRules.h" // Madrone change
#include "Components/PostProcessComponent.h" // Madrone change
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "NightSkyPlayerController.h"
#include "Actors/ParticleManager.h"
#include "Camera/CameraActor.h"
#include "CineCameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "FighterRunners/FighterReplayRunner.h"
#include "FighterRunners/FighterSynctestRunner.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/Globals.h"
#include "Misc/NightSkyBlueprintFunctionLibrary.h"
#include "NightSkyEngine/Battle/CPU/NightSkyAIController.h"
#include "NightSkyEngine/Data/BattleExtensionData.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "NightSkyEngine/Data/SubroutineData.h"
#include "NightSkyEngine/Network/FighterRunners.h"
#include "NightSkyEngine/Miscellaneous/NightSkyGameInstance.h"
#include "NightSkyEngine/Miscellaneous/Logging.h"
#include "NightSkyEngine/UI/NightSkyBattleHudActor.h"
#include "NightSkyEngine/UI/NightSkyBattleWidget.h"
#include "NightSkyEngine/UI/NSEStyleManager.h"
#include "NightSkyEngine/UI/NSEBattleSubtitleWidget.h" // Madrone change
#include "NightSkyEngine/Battle/Misc/NSECharacterLibrary.h" // Madrone change
#include "Blueprint/UserWidget.h" // Madrone change

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyGameState)

// Sets default values
ANightSkyGameState::ANightSkyGameState()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SequenceCameraActorClass = ACineCameraActor::StaticClass();
}

ANightSkyGameState* ANightSkyGameState::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return nullptr;
	if (const auto WorldGameState = Cast<ANightSkyGameState>(World->GetGameState())) return WorldGameState;
	// A battle spawned next to another game state (see bIsSubGameState). There is at most one at a time.
	for (TActorIterator<ANightSkyGameState> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed()) return *It;
	}
	return nullptr;
}

void ANightSkyGameState::PostInitializeComponents()
{
	// AGameStateBase::PostInitializeComponents makes this the world's game state. When the world already has one
	// (e.g. Narrative's game state in an overworld), the battle runs as a sub game state and the world keeps its own,
	// so systems that cast GetWorld()->GetGameState() to their own class keep working during the battle.
	UWorld* World = GetWorld();
	AGameStateBase* PreviousGameState = World ? World->GetGameState() : nullptr;
	Super::PostInitializeComponents();
	if (World && IsValid(PreviousGameState) && PreviousGameState != this)
	{
		World->SetGameState(PreviousGameState);
		bIsSubGameState = true;
	}
}

// Called when the game starts or when spawned
void ANightSkyGameState::BeginPlay()
{
	Super::BeginPlay();

	FActorSpawnParameters SpawnParameters;
	ParticleManager = GetWorld()->SpawnActor<AParticleManager>();
	AudioManager = GetWorld()->SpawnActor<AAudioManager>();
	GameInstance = Cast<UNightSkyGameInstance>(GetGameInstance());
	CameraActor = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
	CameraActor->GetCameraComponent()->SetFieldOfView(54);
	SequenceCameraActor = GetWorld()->SpawnActor<ACineCameraActor>(SequenceCameraActorClass);
	SequenceActor = GetWorld()->SpawnActor<ALevelSequenceActor>(ALevelSequenceActor::StaticClass());

	UpdateCamera();

	BaroqueVisuals = NewObject<UBaroqueCancelVisuals>(this); // Madrone change
	ApplyBattleExposureLock(); // Madrone change

	Init();
}

// Madrone change: auto exposure off for the battle.
void ANightSkyGameState::ApplyBattleExposureLock()
{
	const UNightSkyBattleRules* Rules = GetDefault<UNightSkyBattleRules>();
	if (!Rules->bDisableAutoExposureInBattle || !GetWorld() || IsValid(ExposureLockActor)) return;
	// Its own visible actor: a post-process component only applies when its owner renders (the game state is hidden).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ExposureLockActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!ExposureLockActor) return;
	UPostProcessComponent* PostProcess = NewObject<UPostProcessComponent>(ExposureLockActor, TEXT("BattleExposureLock"));
	ExposureLockActor->SetRootComponent(PostProcess);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 20000.f; // above every level volume and the Baroque darkening
	FPostProcessSettings& S = PostProcess->Settings;
	if (Rules->bUseFixedBattleExposure)
	{
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = Rules->FixedBattleExposureEV100;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = Rules->FixedBattleExposureEV100;
	}
	else
	{
		// Hold the exposure where the overworld left it. The engine clamps speeds to at least 0.001 f-stops a second
		// (about a tenth of a stop over a whole match); 0 would mean "jump straight to the target", not frozen.
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 0.001f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 0.001f;
	}
	PostProcess->RegisterComponent();
}

void ANightSkyGameState::RemoveBattleExposureLock()
{
	if (IsValid(ExposureLockActor)) ExposureLockActor->Destroy();
	ExposureLockActor = nullptr;
}

void ANightSkyGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	RemoveBattleExposureLock(); // Madrone change
	if (IsValid(BattleSubtitleWidget)) BattleSubtitleWidget->RemoveFromParent(); // Madrone change
	if (BaroqueVisuals) BaroqueVisuals->Reset(); // Madrone change: fighters' stencil back, screen not dark
	GameInstance->EndRecordReplay();
}

void ANightSkyGameState::Init()
{
	if (IsValid(BattleExtensionData))
	{
		for (auto Extension : BattleExtensionData->ExtensionArray)
		{
			BattleExtensions.Add(NewObject<UBattleExtension>(this, Extension));
			BattleExtensions.Last()->Parent = this;
			BattleExtensionNames.Add(BattleExtensions.Last()->Name);
		}
	}

	// An unpicked slot (null) would spawn a bare APlayerObject with no states, which crashes on its first update.
	// Drop those (and their colours) as long as the side keeps at least one fighter.
	for (const bool bP1 : {true, false})
	{
		auto& List = bP1 ? GameInstance->BattleData.PlayerListP1 : GameInstance->BattleData.PlayerListP2;
		auto& Colors = bP1 ? GameInstance->BattleData.ColorIndicesP1 : GameInstance->BattleData.ColorIndicesP2;
		const bool bAnyPicked = List.ContainsByPredicate([](const TSoftObjectPtr<UPrimaryCharaData>& Entry) { return !Entry.IsNull(); });
		for (int i = List.Num() - 1; bAnyPicked && i >= 0; i--)
		{
			if (!List[i].IsNull()) continue;
			UE_LOG(LogNightSkyEngine, Warning, TEXT("Battle init: dropping empty %s slot %d"), bP1 ? TEXT("P1") : TEXT("P2"), i);
			List.RemoveAt(i);
			if (Colors.IsValidIndex(i)) Colors.RemoveAt(i);
		}
	}

	// Madrone change: a character that always fights with a partner (CHR_ Default Tag Partner) gets it as its tag
	// partner when it's alone on its side, and the battle becomes a tag battle (Boy Mom and Golden Boy).
	for (const bool bP1 : {true, false})
	{
		auto& List = bP1 ? GameInstance->BattleData.PlayerListP1 : GameInstance->BattleData.PlayerListP2;
		auto& Colors = bP1 ? GameInstance->BattleData.ColorIndicesP1 : GameInstance->BattleData.ColorIndicesP2;
		if (List.Num() != 1) continue;
		const UPrimaryCharaData* Only = List[0].LoadSynchronous();
		if (!Only || Only->DefaultTagPartner.IsNull()) continue;
		List.Add(Only->DefaultTagPartner);
		while (Colors.Num() < List.Num()) Colors.Add(1);
		GameInstance->BattleData.BattleFormat = EBattleFormat::Tag;
		UE_LOG(LogNightSkyEngine, Log, TEXT("Battle init: %s gets its default tag partner %s (tag battle)"), *Only->GetName(),
			*Only->DefaultTagPartner.ToString());
	}

	// Madrone change: a battle level started with no fighter on a side (Play In Editor straight into a stage, without
	// character select) crashed the editor in MatchInit. Say what's missing and don't start the match.
	if (GameInstance->BattleData.PlayerListP1.IsEmpty() || GameInstance->BattleData.PlayerListP2.IsEmpty())
	{
		UE_LOG(LogNightSkyEngine, Error, TEXT("Battle init: no fighters (P1 %d, P2 %d). Pick them on character select, or for Play In "
			"Editor fill Project Settings > Night Sky Engine Editor Settings > Battle Data (with Is Valid on)."),
			GameInstance->BattleData.PlayerListP1.Num(), GameInstance->BattleData.PlayerListP2.Num());
		return;
	}

	for (int i = 0; i < GameInstance->BattleData.PlayerListP1.Num(); i++)
	{
		[[maybe_unused]] auto Unused = GameInstance->BattleData.PlayerListP1[i].LoadSynchronous();

		APlayerObject* SpawnedPlayer;
		if (const auto Player = GameInstance->BattleData.PlayerListP1[i]; Player != nullptr)
		{
			Players.Add(GetWorld()->SpawnActor<APlayerObject>(Player->PlayerClass,
			                                                  BattleSceneTransform));
			SpawnedPlayer = Players.Last();
			// Madrone change: the HUD shows CharacterName; only assists were given theirs
			SpawnedPlayer->CharacterName = Player->CharaFriendlyName.IsEmpty() ? FText::FromName(Player->CharaName) : Player->CharaFriendlyName;
			SpawnedPlayer->HudStyleRequest = Player->BattleHudStyle; // Madrone change
			SpawnedPlayer->CharaData = Player.Get(); // Madrone change
			SpawnedPlayer->ColorIndex = 1;
			if (GameInstance->BattleData.ColorIndicesP1.Num() > i)
				SpawnedPlayer->ColorIndex = GameInstance->BattleData.ColorIndicesP1[i];
		}
		else
		{
			Players.Add(GetWorld()->SpawnActor<APlayerObject>(APlayerObject::StaticClass(), BattleSceneTransform));
			SpawnedPlayer = Players.Last();
		}
		SortedObjects.Add(SpawnedPlayer);
		SpawnedPlayer->GameState = this;
		SpawnedPlayer->AddTickPrerequisiteActor(this);
	}

	for (int i = 0; i < GameInstance->BattleData.PlayerListP2.Num(); i++)
	{
		[[maybe_unused]] auto Unused = GameInstance->BattleData.PlayerListP2[i].LoadSynchronous();

		APlayerObject* SpawnedPlayer;
		if (const auto Player = GameInstance->BattleData.PlayerListP2[i]; Player != nullptr)
		{
			Players.Add(GetWorld()->SpawnActor<APlayerObject>(Player->PlayerClass,
			                                                  BattleSceneTransform));
			SpawnedPlayer = Players.Last();
			// Madrone change: the HUD shows CharacterName; only assists were given theirs
			SpawnedPlayer->CharacterName = Player->CharaFriendlyName.IsEmpty() ? FText::FromName(Player->CharaName) : Player->CharaFriendlyName;
			SpawnedPlayer->HudStyleRequest = Player->BattleHudStyle; // Madrone change
			SpawnedPlayer->CharaData = Player.Get(); // Madrone change
			SpawnedPlayer->ColorIndex = 1;
			if (GameInstance->BattleData.ColorIndicesP2.Num() > i)
				SpawnedPlayer->ColorIndex = GameInstance->BattleData.ColorIndicesP2[i];
			for (int j = 0; j < GameInstance->BattleData.PlayerListP1.Num(); j++)
			{
				if (IsValid(GameInstance->BattleData.PlayerListP1[j].Get()))
				{
					if (SpawnedPlayer->IsA(GameInstance->BattleData.PlayerListP1[j]->PlayerClass))
					{
						if (SpawnedPlayer->ColorIndex == Players[j]->ColorIndex)
						{
							if (SpawnedPlayer->ColorIndex > 1)
								SpawnedPlayer->ColorIndex = 1;
							else
								SpawnedPlayer->ColorIndex = 2;
						}
						break;
					}
				}
			}
		}
		else
		{
			Players.Add(GetWorld()->SpawnActor<APlayerObject>(APlayerObject::StaticClass(), BattleSceneTransform));
			SpawnedPlayer = Players.Last();
		}
		SortedObjects.Add(SpawnedPlayer);
		SpawnedPlayer->GameState = this;
		SpawnedPlayer->AddTickPrerequisiteActor(this);

		// Madrone change: never a CPU under the synctest runner (as NightSkyWTGameState already does for overworld battles).
		// The AI controller reads the battle and sets Inputs outside rollback (UpdateGameState skips CPU->Update while
		// resimulating), so a CPU P2 desyncs every synctest frame once the round starts, whatever the fighters. An
		// Is CPU Battle left on by a test (game instance defaults edited from Python) made every stage synctest fail.
		if (GameInstance->IsCPUBattle && !GameInstance->IsTraining && GameInstance->FighterRunner != SyncTest)
		{
			SpawnedPlayer->SpawnDefaultController();
			SpawnedPlayer->bIsCpu = true;
		}
	}
	
	//NickTour Assists
	{
		auto P1AssistData = GameInstance->BattleData.P1Assist.LoadSynchronous();
		auto P2AssistData = GameInstance->BattleData.P2Assist.LoadSynchronous();
		
		if (P1AssistData != nullptr &&  P1AssistData->PlayerClass != nullptr)
		{
			APlayerObject* NewP1Assist = GetWorld()->SpawnActor<APlayerObject>(P1AssistData->PlayerClass); 
			NewP1Assist->CharacterName = P1AssistData->CharaFriendlyName; 
			NewP1Assist->HudStyleRequest = P1AssistData->BattleHudStyle; // Madrone change
			NewP1Assist->CharaData = P1AssistData; // Madrone change
			NewP1Assist->SetActorHiddenInGame(true);
		
			P1Assist = NewP1Assist;
			NewP1Assist->SetActorTransform(BattleSceneTransform);
			SortedObjects.Add(NewP1Assist);
			NewP1Assist->InitPlayer();
			NewP1Assist->GameState = this;
			NewP1Assist->AddTickPrerequisiteActor(this);
			NewP1Assist->IsActive = false;
		}
		if (P2AssistData != nullptr &&  P2AssistData->PlayerClass != nullptr)
		{
			APlayerObject* NewP2Assist = GetWorld()->SpawnActor<APlayerObject>(P2AssistData->PlayerClass); 
			NewP2Assist->CharacterName = P2AssistData->CharaFriendlyName; 
			NewP2Assist->HudStyleRequest = P2AssistData->BattleHudStyle; // Madrone change
			NewP2Assist->CharaData = P2AssistData; // Madrone change
			NewP2Assist->SetActorHiddenInGame(true);
		
			P2Assist = NewP2Assist;
			NewP2Assist->PlayerIndex = 1; // Madrone change: MatchInit only sets it for Players; 0 made it count as P1's side
			NewP2Assist->SetActorTransform(BattleSceneTransform);
			SortedObjects.Add(NewP2Assist);
			NewP2Assist->InitPlayer();
			NewP2Assist->GameState = this;
			NewP2Assist->AddTickPrerequisiteActor(this);
			NewP2Assist->IsActive = false;
		}
		
		
	}

	for (int i = 0; i < MaxBattleObjects; i++)
	{
		Objects.Add(GetWorld()->SpawnActor<ABattleObject>(BattleObjectClass, BattleSceneTransform));
		Objects[i]->GameState = this;
		Objects[i]->AddTickPrerequisiteActor(this);
		SortedObjects.Add(Objects.Last());
	}
	ObjectSpawnOrder = SortedObjects; // Madrone change

	MatchInit();
	HUDInit();
}

// Madrone change: the fighter has a state for its IntroName. The template characters' IntroName (Manny.Stand) names no
// state, so every battle start logged "Could not find state"; without the jump the intro ends the same way, at once.
static bool HasIntroState(const APlayerObject* Player)
{
	return Player && Player->IntroName.IsValid() && Player->PrimaryStateMachine.StateNames.Contains(Player->IntroName);
}

void ANightSkyGameState::PlayIntros()
{
	if (GameInstance->IsTraining)
	{
		BattleState.BattlePhase = EBattlePhase::Battle;
		GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
		GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);
		bHudRoundInitPending = true; // Madrone change: no intro end to run the HUD's round init (see UpdateHUD)
		return;
	}
	BattleState.BattlePhase = EBattlePhase::Intro;
	BattleState.CurrentIntroSide = INT_P1;
	if (HasIntroState(GetMainPlayer(true))) GetMainPlayer(true)->JumpToStatePrimary(GetMainPlayer(true)->IntroName); // Madrone change
	BattleHudVisibility(false);
}

// Madrone change: an assist still out when the round ended went into the next one active and on screen
void ANightSkyGameState::ResetAssistsForRound()
{
	for (APlayerObject* Assist : {P1Assist.Get(), P2Assist.Get()})
	{
		if (!IsValid(Assist)) continue;
		Assist->RoundInit(true);
		Assist->SetOnScreen(false);
		Assist->IsActive = false;
	}
}

void ANightSkyGameState::RoundInit()
{
	BattleState.PauseTimer = false;
	BattleState.RandomManager.Reseed(BattleState.RandomManager.Rand() + BattleState.RoundCount);
	BattleState.RoundCount++;

	BattleState.SuperFreezeSelfDuration = 0;
	BattleState.SuperFreezeDuration = 0;
	BattleState.SuperFreezeCaller = nullptr;
	BattleState.BattlePhase = EBattlePhase::Battle;
	BattleState.FadeTimer = BattleState.MaxFadeTimer;

	if (BattleState.BattleFormat == EBattleFormat::Rounds || BattleState.RoundCount == 1)
	{
		BattleState.ScreenData = Cast<ANightSkyGameState>(GetClass()->GetDefaultObject())->BattleState.ScreenData;
		ApplyStageWalls(BattleState.ScreenData); // Madrone change

		if (!GameInstance->IsTraining)
			BattleState.TimeUntilRoundStart = BattleState.MaxTimeUntilRoundStart;
		for (int i = 0; i < MaxBattleObjects; i++)
			Objects[i]->ResetObject();

		for (const auto Player : Players)
		{
			Player->RoundInit(true);
			if (!Player->IsMainPlayer()) Player->SetOnScreen(false);
		}

		ResetAssistsForRound(); // Madrone change
		GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
		GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);

		// Madrone change: the main players, not the ones the match started with: after a tag in round 1, round 2 put the
		// first fighter back on screen next to the one in control (and its partner then counted as on screen, so no tag).
		GetMainPlayer(true)->PlayerFlags |= PLF_IsOnScreen;
		GetMainPlayer(false)->PlayerFlags |= PLF_IsOnScreen;

		BattleState.MaxMeter[0] = GetMainPlayer(true)->MaxMeter;
		BattleState.MaxMeter[1] = GetMainPlayer(false)->MaxMeter;

		BattleState.RoundTimer = GameInstance->BattleData.StartRoundTimer * 60;
		BattleState.bHUDVisible = true;

		UpdateCamera();
		CallBattleExtension(BattleExtension_RoundInit);
		OnRoundInit.Broadcast(this); //Madrone change
	}
	else if (BattleState.BattleFormat == EBattleFormat::Tag)
	{
		const bool IsP1 = GetMainPlayer(true)->CurrentHealth == 0;
		GetMainPlayer(IsP1)->SetOnScreen(false);
		const auto NewPosX = GetMainPlayer(IsP1)->PosX;
		const auto NewDirection = GetMainPlayer(IsP1)->Direction;
		if (!SwitchMainPlayer(GetMainPlayer(IsP1), 1, true)) return;
		GetMainPlayer(IsP1)->PosX = NewPosX;
		GetMainPlayer(IsP1)->Direction = NewDirection;
		GetMainPlayer(IsP1)->PosY = 0;
		GetMainPlayer(IsP1)->JumpToStatePrimary(State_Universal_TagIn);

		GetMainPlayer(true)->PlayerFlags &= ~PLF_RoundWinInputLock;
		GetMainPlayer(false)->PlayerFlags &= ~PLF_RoundWinInputLock;

		for (const auto& Player : Players) Player->RoundWinTimer = 120;
		// Madrone change: the KO'd fighter's KO object (ST_CommonKO*) hid the HUD for the KO; every other format shows it
		// again here, the tag one didn't, so the HUD stayed gone for the rest of the fight after a teammate went down.
		BattleState.bHUDVisible = true;
	}
	else if (BattleState.BattleFormat == EBattleFormat::NickTour) //Madrone change this whole round init block
	{
		BattleState.ScreenData = Cast<ANightSkyGameState>(GetClass()->GetDefaultObject())->BattleState.ScreenData;
		ApplyStageWalls(BattleState.ScreenData); // Madrone change

		const bool IsP1 = GetMainPlayer(true)->CurrentHealth == 0;
		GetMainPlayer(IsP1)->SetOnScreen(false);
		SwitchMainPlayer(GetMainPlayer(IsP1), 1, true);

		if (!GameInstance->IsTraining)
			BattleState.TimeUntilRoundStart = BattleState.MaxTimeUntilRoundStart;
		for (int i = 0; i < MaxBattleObjects; i++)
			Objects[i]->ResetObject();

		for (const auto Player : Players)
		{
			if (Player->CurrentHealth != 0)
			{
				Player->RoundInit(true);
			}
			
			if (!Player->IsMainPlayer()) Player->SetOnScreen(false);
		}

		ResetAssistsForRound(); // Madrone change
		GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
		GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);

		GetMainPlayer(true)->PlayerFlags = PLF_IsOnScreen;
		GetMainPlayer(false)->PlayerFlags = PLF_IsOnScreen;

		BattleState.MaxMeter[0] = GetMainPlayer(true)->MaxMeter;
		BattleState.MaxMeter[1] = GetMainPlayer(false)->MaxMeter;

		BattleState.RoundTimer = GameInstance->BattleData.StartRoundTimer * 60;
		BattleState.bHUDVisible = true;

		UpdateCamera();
		CallBattleExtension(BattleExtension_RoundInit);
		OnRoundInit.Broadcast(this); //Madrone change
	}
	else
	{
		BattleState.ScreenData = Cast<ANightSkyGameState>(GetClass()->GetDefaultObject())->BattleState.ScreenData;
		ApplyStageWalls(BattleState.ScreenData); // Madrone change

		const bool IsP1 = GetMainPlayer(true)->CurrentHealth == 0;
		GetMainPlayer(IsP1)->SetOnScreen(false);
		SwitchMainPlayer(GetMainPlayer(IsP1), 1, true);

		if (!GameInstance->IsTraining)
			BattleState.TimeUntilRoundStart = BattleState.MaxTimeUntilRoundStart;
		for (int i = 0; i < MaxBattleObjects; i++)
			Objects[i]->ResetObject();

		for (const auto Player : Players)
		{
			Player->RoundInit(true);
			if (!Player->IsMainPlayer()) Player->SetOnScreen(false);
		}

		ResetAssistsForRound(); // Madrone change
		GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
		GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);

		GetMainPlayer(true)->PlayerFlags |= PLF_IsOnScreen;
		GetMainPlayer(false)->PlayerFlags |= PLF_IsOnScreen;

		BattleState.MaxMeter[0] = GetMainPlayer(true)->MaxMeter;
		BattleState.MaxMeter[1] = GetMainPlayer(false)->MaxMeter;

		BattleState.RoundTimer = GameInstance->BattleData.StartRoundTimer * 60;
		BattleState.bHUDVisible = true;

		UpdateCamera();
		CallBattleExtension(BattleExtension_RoundInit);
		OnRoundInit.Broadcast(this); //Madrone change
	}

	BattleState.ScreenData.TargetObjects.AddUnique(GetMainPlayer(true));
	BattleState.ScreenData.TargetObjects.AddUnique(GetMainPlayer(false));
	AssignEnemy();
	RoundInitCharacterSystems(); // Madrone change
}

void ANightSkyGameState::AssignEnemy()
{
	for (int i = 0; i < Players.Num(); i++)
	{
		if (i < BattleState.TeamData[0].TeamCount)
		{
			Players[i]->Enemy = GetMainPlayer(false);
		}
		else
		{
			Players[i]->Enemy = GetMainPlayer(true);
		}
	}
}

void ANightSkyGameState::UpdateLocalInput()
{
	LocalInputs[0] = GetLocalInputs(0);
	LocalInputs[1] = GetLocalInputs(1);
}

// Called every frame
void ANightSkyGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!FighterRunner) return; // Madrone change: no match was started (see Init)
	FighterRunner->Update(DeltaTime);
}

void ANightSkyGameState::MatchInit()
{
	if (!FighterRunner)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();

		switch (GameInstance->FighterRunner)
		{
		case LocalPlay:
			FighterRunner = GetWorld()->SpawnActor<AFighterLocalRunner>(SpawnParameters);
			break;
		case Multiplayer:
			if (GameInstance->IsReplay)
				FighterRunner = GetWorld()->SpawnActor<AFighterReplayRunner>(SpawnParameters);
			else
				FighterRunner = GetWorld()->SpawnActor<AFighterMultiplayerRunner>(SpawnParameters);
			break;
		case SyncTest:
			FighterRunner = GetWorld()->SpawnActor<AFighterSynctestRunner>(SpawnParameters);
			break;
		default:
			FighterRunner = GetWorld()->SpawnActor<AFighterLocalRunner>(SpawnParameters);
			break;
		}
	}

	BattleState = Cast<ANightSkyGameState>(GetClass()->GetDefaultObject())->BattleState;

	BattleState.RandomManager = GameInstance->BattleData.Random;
	BattleState.TeamData[0].TeamCount = GameInstance->BattleData.PlayerListP1.Num();
	BattleState.TeamData[0].CooldownTimer.Init(0, GameInstance->BattleData.PlayerListP1.Num()); // Madrone change: was appended to every match
	BattleState.TeamData[1].TeamCount = GameInstance->BattleData.PlayerListP2.Num();
	BattleState.TeamData[1].CooldownTimer.Init(0, GameInstance->BattleData.PlayerListP2.Num());
	BattleState.GaugeP1.Empty();
	BattleState.GaugeP2.Empty();
	BattleState.GaugeP1.AddDefaulted(BattleState.MaxGauge.Num());
	BattleState.GaugeP2.AddDefaulted(BattleState.MaxGauge.Num());

	CallBattleExtension(BattleExtension_MatchInit);

	for (int i = 0; i < Players.Num(); i++)
	{
		Players[i]->PlayerIndex = i >= BattleState.TeamData[0].TeamCount;
		Players[i]->TeamIndex = i >= BattleState.TeamData[0].TeamCount ? 0 : 1;
		Players[i]->PlayerFlags &= ~PLF_IsOnScreen;
		Players[i]->ObjNumber = i + MaxBattleObjects;
		Players[i]->CallSubroutine(Subroutine_Cmn_MatchInit);
		Players[i]->CallSubroutine(Subroutine_MatchInit);
	}
	for (int i = 0; i < MaxBattleObjects; i++)
	{
		Objects[i]->ObjNumber = i;
	}
	for (int i = SortedObjects.Num() - 1; i >= 0; i--)
	{
		SetDrawPriorityFront(SortedObjects[i]);
	}

	BattleState.MainPlayer[0] = Players[0];
	BattleState.MainPlayer[0]->PlayerFlags |= PLF_IsOnScreen;
	BattleState.MainPlayer[1] = Players[BattleState.TeamData[0].TeamCount];
	BattleState.MainPlayer[1]->PlayerFlags |= PLF_IsOnScreen;
	BattleState.BattleFormat = GameInstance->BattleData.BattleFormat;
	BattleState.MaxTimeUntilRoundStart = GameInstance->BattleData.TimeUntilRoundStart;
	BattleState.MaxRoundCount = GameInstance->BattleData.RoundCount;
	BattleState.RoundTimer = GameInstance->BattleData.StartRoundTimer * 60;

	PlayMusic(GameInstance->BattleData.MusicName);
	BattleHudActor->BottomWidget->PlayFadeOutAnim();
	InitCharacterSystems(); // Madrone change: character extensions and interactions
	RoundInit();
	PlayIntros();
}

void ANightSkyGameState::MatchInitWt(int Team1Count, int Team2Count) //Madrone change
{
	if (!FighterRunner)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();

		switch (GameInstance->FighterRunner)
		{
		case LocalPlay:
			FighterRunner = GetWorld()->SpawnActor<AFighterLocalRunner>(SpawnParameters);
			break;
		case Multiplayer:
			if (GameInstance->IsReplay)
				FighterRunner = GetWorld()->SpawnActor<AFighterReplayRunner>(SpawnParameters);
			else
				FighterRunner = GetWorld()->SpawnActor<AFighterMultiplayerRunner>(SpawnParameters);
			break;
		case SyncTest:
			FighterRunner = GetWorld()->SpawnActor<AFighterSynctestRunner>(SpawnParameters);
			break;
		default:
			FighterRunner = GetWorld()->SpawnActor<AFighterLocalRunner>(SpawnParameters);
			break;
		}
	}

	BattleState = Cast<ANightSkyGameState>(GetClass()->GetDefaultObject())->BattleState;

	BattleState.RandomManager = GameInstance->BattleData.Random;
	
	BattleState.TeamData[0].TeamCount = Team1Count;
	BattleState.TeamData[1].TeamCount = Team2Count;
	// Madrone change: reset, not appended to: the overworld game state is reused, so it kept the last fights' timers.
	BattleState.TeamData[0].CooldownTimer.Init(0, BattleState.TeamData[0].TeamCount);
	BattleState.TeamData[1].CooldownTimer.Init(0, BattleState.TeamData[1].TeamCount);
	BattleState.GaugeP1.Empty();
	BattleState.GaugeP2.Empty();
	BattleState.GaugeP1.AddDefaulted(BattleState.MaxGauge.Num());
	BattleState.GaugeP2.AddDefaulted(BattleState.MaxGauge.Num());

	CallBattleExtension(BattleExtension_MatchInit);

	for (int i = 0; i < Players.Num(); i++)
	{
		Players[i]->PlayerIndex = i >= BattleState.TeamData[0].TeamCount;
		//Players[i]->TeamIndex = i >= BattleState.TeamData[0].TeamCount ? i - BattleState.TeamData[0].TeamCount : i;
		Players[i]->TeamIndex = i >= BattleState.TeamData[0].TeamCount ? 1 : 0;
		Players[i]->PlayerFlags &= ~PLF_IsOnScreen;
		Players[i]->ObjNumber = i + MaxBattleObjects;
		Players[i]->CallSubroutine(Subroutine_Cmn_MatchInit);
		Players[i]->CallSubroutine(Subroutine_MatchInit);
	}
	for (int i = 0; i < MaxBattleObjects; i++)
	{
		Objects[i]->ObjNumber = i;
	}
	for (int i = SortedObjects.Num() - 1; i >= 0; i--)
	{
		SetDrawPriorityFront(SortedObjects[i]);
	}

	BattleState.MainPlayer[0] = Players[0];
	BattleState.MainPlayer[0]->PlayerFlags |= PLF_IsOnScreen;
	BattleState.MainPlayer[1] = Players[BattleState.TeamData[0].TeamCount];
	BattleState.MainPlayer[1]->PlayerFlags |= PLF_IsOnScreen;
	BattleState.BattleFormat = GameInstance->BattleData.BattleFormat;
	// Madrone change: an overworld fight with a tag partner on either side (the player's tag slot, an NPC's allies, a CHR_
	// Default Tag Partner such as Boy Mom's Golden Boy) is a Tag battle: CanTag refuses every tag outside the Tag format,
	// and the game instance keeps whatever format the menu last used (Rounds by default). Dedicated assists (P1Assist /
	// P2Assist) work in every format.
	if (Team1Count > 1 || Team2Count > 1) BattleState.BattleFormat = EBattleFormat::Tag;
	BattleState.MaxTimeUntilRoundStart = GameInstance->BattleData.TimeUntilRoundStart;
	BattleState.MaxRoundCount = GameInstance->BattleData.RoundCount;
	BattleState.RoundTimer = GameInstance->BattleData.StartRoundTimer * 60;

	PlayMusic(GameInstance->BattleData.MusicName);
	BattleHudActor->BottomWidget->PlayFadeOutAnim();
	InitCharacterSystems(); // Madrone change: character extensions and interactions
	RoundInit();
	PlayIntros();
}

void ANightSkyGameState::UpdateGameState(int32 Input1, int32 Input2, bool bShouldResimulate)
{
	if ((Input1 & INP_Rematch) == INP_Rematch && (Input2 & INP_Rematch) == INP_Rematch)
	{
		MatchInit();
		ANightSkyPlayerController* Controller1 = Cast<ANightSkyPlayerController>(
			UGameplayStatics::GetPlayerController(GetWorld(), 0));
		ANightSkyPlayerController* Controller2 = Cast<ANightSkyPlayerController>(
			UGameplayStatics::GetPlayerController(GetWorld(), 1));
		if (Controller1) Controller1->PostRematch();
		if (Controller2) Controller2->PostRematch();
	}
	if (Input1 & INP_ResetTraining || Input2 & INP_ResetTraining)
	{
		ResetTraining();
	}

	if (bShouldResimulate == false && bIsResimulating == true) RollbackStartAudio(BattleState.FrameNumber);
	bIsResimulating = bShouldResimulate;
	LocalFrame++;

	ParticleManager->UpdateParticles();
	UpdateScreen();

	AssignEnemy();

	if (BattleState.BattlePhase == EBattlePhase::Intro)
	{
		if (BattleState.CurrentIntroSide == INT_P1)
		{
			if (GetMainPlayer(true)->IntroEndFlag)
			{
				GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
				SequenceActor->GetSequencePlayer()->Stop();
				BattleState.CurrentSequenceTime = -1;
				BattleState.IsPlayingSequence = false;
			}
			if (GetMainPlayer(true)->GetCurrentStateName(StateMachine_Primary) != GetMainPlayer(true)->IntroName)
			{
				// Madrone change: only into an intro state the fighter has (most have none: IntroName is a placeholder).
				if (HasIntroState(GetMainPlayer(false))) GetMainPlayer(false)->JumpToStatePrimary(GetMainPlayer(false)->IntroName);
				BattleState.CurrentIntroSide = INT_P2;
			}
		}
		else if (BattleState.CurrentIntroSide == INT_P2)
		{
			bool bIsP1 = false;
			if (GetMainPlayer(false)->bIsCpu)
			{
				bIsP1 = true;
			}
			if (GetMainPlayer(bIsP1)->IntroEndFlag)
			{
				GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);
				SequenceActor->GetSequencePlayer()->Stop();
				BattleState.CurrentSequenceTime = -1;
				BattleState.IsPlayingSequence = false;
			}
			if (GetMainPlayer(false)->GetCurrentStateName(StateMachine_Primary) != GetMainPlayer(false)->IntroName)
			{
				GetMainPlayer(true)->JumpToStatePrimary(State_Universal_Stand);
				GetMainPlayer(false)->JumpToStatePrimary(State_Universal_Stand);
				BattleState.CurrentIntroSide = INT_None;
				BattleState.BattlePhase = EBattlePhase::Battle;
				BattleHudVisibility(true);
				BattleHudActor->BottomWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount); //Nick change
				BattleHudActor->TopWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount);
				BattleState.FadeTimer = BattleState.MaxFadeTimer;
			}
		}
	}
	else
	{
		if (!GameInstance->IsTraining && !BattleState.PauseTimer)
		{
			if (BattleState.TimeUntilRoundStart > 0)
				BattleState.TimeUntilRoundStart--;
			else
				BattleState.RoundTimer--;
		}
	}

	if (BattleState.RoundTimer < 0)
		BattleState.RoundTimer = 0;
	BattleState.FrameNumber++;

	if (!BattleState.SuperFreezeDuration)
	{
		CallBattleExtension(BattleExtension_Update);
		for (int i = 0; i < 2; i++)
		{
			for (auto& Cooldown : BattleState.TeamData[i].CooldownTimer)
			{
				if (Cooldown == 0) continue;
				if (GameInstance->IsTraining && !GetMainPlayer(i == 0)->ComboCounter) Cooldown = 0;
				else Cooldown--;
			}
			// Madrone change: the dedicated assist's cooldown, same rules
			if (int32& AssistCooldown = BattleState.AssistCooldownTimer[i]; AssistCooldown > 0)
			{
				if (GameInstance->IsTraining && !GetMainPlayer(i == 0)->ComboCounter) AssistCooldown = 0;
				else AssistCooldown--;
			}
		}
	}

	if (BattleState.CurrentSequenceTime != -1)
		BattleState.CurrentSequenceTime++;

	SortObjects();

	if (!BattleState.MainPlayer[0]->bIsCpu) BattleState.MainPlayer[0]->Inputs = Input1;
	else if (const auto CPU = Cast<ANightSkyAIController>(BattleState.MainPlayer[0]->Controller); CPU && !bShouldResimulate)
		CPU->Update();
	if (!BattleState.MainPlayer[1]->bIsCpu) BattleState.MainPlayer[1]->Inputs = Input2;
	else if (const auto CPU = Cast<ANightSkyAIController>(BattleState.MainPlayer[1]->Controller); CPU && !bShouldResimulate)
		CPU->Update();
	OnFighterInputs(bShouldResimulate); // Madrone change: training dummy, swap, record / playback (subclass)
	if (!BattleState.SuperFreezeDuration) UpdateCharacterSystems(); // Madrone change: extensions, interactions

	for (int i = 0; i < SortedObjects.Num(); i++)
	{
		if (i == BattleState.ActiveObjectCount)
			break;
		if (((BattleState.SuperFreezeSelfDuration && SortedObjects[i] == BattleState.SuperFreezeCaller)
				|| (BattleState.SuperFreezeDuration && SortedObjects[i] != BattleState.SuperFreezeCaller))
			&& (SortedObjects[i]->MiscFlags & MISC_IgnoreSuperFreeze) == 0)
		{
			if (SortedObjects[i]->IsPlayer)
			{
				if (SortedObjects[i]->Player->PlayerFlags & PLF_IsStunned)
					SortedObjects[i]->Player->HandleBufferedState();
				SortedObjects[i]->GetBoxes();
				SortedObjects[i]->Player->HandleFlipInput();
				SortedObjects[i]->Player->StoredInputBuffer.Update(SortedObjects[i]->Player->Inputs, true);

				// Handle state transitions
				SortedObjects[i]->Player->HandleStateMachine(
					true, SortedObjects[i]->Player->GetStateMachine(StateMachine_Primary));
				for (auto& StateMachine : SortedObjects[i]->Player->SubStateMachines)
				{
					SortedObjects[i]->Player->HandleStateMachine(false, StateMachine);
				}
			}
			SortedObjects[i]->PositionLinkUpdate();
			if (!SortedObjects[i]->IsPlayer && SortedObjects[i]->MiscFlags & MISC_DeactivateOnNextUpdate)
			{
				SortedObjects[i]->ResetObject();
			}
			continue;
		}
		SortedObjects[i]->Update();
	}

	SortObjects();
	HandleHitCollision();

	if (BattleState.SuperFreezeSelfDuration == 1)
	{
		BattleState.SuperFreezeCaller->TriggerEvent(EVT_SuperFreezeEnd, StateMachine_Primary);
	}
	if (BattleState.SuperFreezeDuration == 1)
	{
		for (int i = 0; i < SortedObjects.Num(); i++)
		{
			SortedObjects[i]->TriggerEvent(EVT_SuperFreezeEnd, StateMachine_Primary);
		}

		BattleState.PauseTimer = false;
		BattleState.SuperFreezeCaller = nullptr;
	}

	if (BattleState.SuperFreezeSelfDuration) BattleState.SuperFreezeSelfDuration--;
	if (BattleState.SuperFreezeDuration) BattleState.SuperFreezeDuration--;

	GetMainPlayer(true)->HandleEndCombo();
	GetMainPlayer(false)->HandleEndCombo();

	SetScreenBounds();
	HandlePushCollision();
	SetScreenBounds();
	if (!BattleState.SuperFreezeDuration)
	{
		HandleRoundWin();
	}

	// these aren't strictly game state related, but tying them to game state update makes things better	
	if (GameInstance->FighterRunner == Multiplayer && !GameInstance->IsReplay)
	{
		GameInstance->UpdateReplay(Input1, Input2);
	}

	CollisionView();

	const auto [network, timesync] = GetNetworkStats();
	NetworkStats.Ping = network.ping;
	const int32 LocalFramesBehind = timesync.local_frames_behind;
	const int32 RemoteFramesBehind = timesync.remote_frames_behind;

	if (LocalFramesBehind < 0 && RemoteFramesBehind < 0)
	{
		NetworkStats.RollbackFrames = abs(abs(LocalFramesBehind) - abs(RemoteFramesBehind));
	}
	else if (LocalFramesBehind > 0 && RemoteFramesBehind > 0)
	{
		NetworkStats.RollbackFrames = 0;
	}
	else
	{
		NetworkStats.RollbackFrames = abs(LocalFramesBehind) + abs(RemoteFramesBehind);
	}

	for (int i = 0; i < 2; i++)
	{
		if (BattleState.Meter[i] > BattleState.MaxMeter[i])
			BattleState.Meter[i] = BattleState.MaxMeter[i];
		if (BattleState.Meter[i] < 0)
			BattleState.Meter[i] = 0;
	}

	for (int i = 0; i < BattleState.MaxGauge.Num(); i++)
	{
		if (BattleState.GaugeP1[i] > BattleState.MaxGauge[i])
			BattleState.GaugeP1[i] = BattleState.MaxGauge[i];
		if (BattleState.GaugeP1[i] < 0)
			BattleState.GaugeP1[i] = 0;
		if (BattleState.GaugeP2[i] > BattleState.MaxGauge[i])
			BattleState.GaugeP2[i] = BattleState.MaxGauge[i];
		if (BattleState.GaugeP2[i] < 0)
			BattleState.GaugeP2[i] = 0;
	}

	OnBattleFrameEnd(bShouldResimulate); // Madrone change: training frame bar, input history (subclass)

	ParticleManager->PauseParticles();
	UpdateVisuals(bShouldResimulate);
	if (!bShouldResimulate && BaroqueVisuals) BaroqueVisuals->Update(this); // Madrone change
	UpdateCamera();
	UpdateHUD();
	ManageAudio();
}

bool ANightSkyGameState::ShouldCounterHit(const APlayerObject* Defender) const
{
	return Defender && (Defender->AttackFlags & ATK_IsAttacking) != 0;
}

void ANightSkyGameState::SetScreenCorners()
{
	const auto ScreenData = &BattleState.ScreenData;

	if (ScreenData->TargetObjects.Num() == 0)
	{
		ScreenData->ObjTop = 0;
		ScreenData->ObjBottom = 0;
		ScreenData->HigherObjBottom = 0;
		ScreenData->ObjLeft = 0;
		ScreenData->ObjRight = 0;
		ScreenData->ObjLength = 0;
		ScreenData->ObjHeight = 0;

		return;
	}

	bool bIsFirst = true;

	for (const auto Target : ScreenData->TargetObjects)
	{
		if (Target == nullptr) break;

		Target->CalculatePushbox();

		auto TargetL = Target->PosX - 85000;
		auto TargetR = Target->PosX + 85000;

		if (bIsFirst)
		{
			ScreenData->ObjTop = (Target->T - Target->GroundHeight) / 1000;
			ScreenData->ObjBottom = (Target->B - Target->GroundHeight) / 1000;
			ScreenData->HigherObjBottom = (Target->B - Target->GroundHeight) / 1000;
			ScreenData->ObjLeft = TargetL / 1000;
			ScreenData->ObjRight = TargetR / 1000;

			bIsFirst = false;
		}
		else
		{
			if (Target->T - Target->GroundHeight > ScreenData->ObjTop * 1000) ScreenData->ObjTop = (Target->T - Target->GroundHeight) / 1000;
			if (Target->B - Target->GroundHeight < ScreenData->ObjBottom * 1000) ScreenData->ObjBottom = (Target->B - Target->GroundHeight) / 1000;
			if (Target->B - Target->GroundHeight > ScreenData->HigherObjBottom * 1000) ScreenData->HigherObjBottom = (Target->B - Target->GroundHeight) / 1000;
			if (TargetL < ScreenData->ObjLeft * 1000) ScreenData->ObjLeft = TargetL / 1000;
			if (TargetR > ScreenData->ObjRight * 1000) ScreenData->ObjRight = TargetR / 1000;
		}
	}

	ScreenData->ObjLength = ScreenData->ObjRight - ScreenData->ObjLeft;
	ScreenData->ObjHeight = ScreenData->ObjTop - ScreenData->ObjBottom;
	ScreenData->ObjDistanceY = ScreenData->HigherObjBottom - ScreenData->ObjBottom;
}

void ANightSkyGameState::UpdateScreen()
{
	const auto ScreenData = &BattleState.ScreenData;

	ScreenData->MaxZoomOutWidth = ScreenData->DefaultMaxWidth;
	ScreenData->ZoomOutBeginX = ScreenData->DefaultWidth;

	SetScreenCorners();

	if ((ScreenData->Flags & SCR_Lock) == 0)
	{
		if ((ScreenData->Flags & SCR_LockXPos) == 0)
			ScreenData->TargetCenterX = ScreenData->ObjLeft + ScreenData->
				ObjLength / 2;

		if ((ScreenData->Flags & SCR_LockWidth) == 0)
		{
			auto XWidth = ScreenData->ObjLength + ScreenData->ZoomOutBeginX / 4;
			auto YWidth = FMath::Max(ScreenData->ObjDistanceY - ScreenData->ZoomOutBeginY, 0) + ScreenData->
				ZoomOutBeginX;
			auto HWidth = FMath::Max(ScreenData->ObjHeight - ScreenData->ZoomOutBeginH, 0) + ScreenData->ZoomOutBeginX;

			ScreenData->TargetWidth = FMath::Max(XWidth, FMath::Max(YWidth, HWidth));
			if (ScreenData->TargetWidth < ScreenData->ZoomOutBeginX)
				ScreenData->TargetWidth = ScreenData->
					ZoomOutBeginX;
			if (ScreenData->TargetWidth > ScreenData->MaxZoomOutWidth)
				ScreenData->TargetWidth = ScreenData->
					MaxZoomOutWidth;
		}

		if ((ScreenData->Flags & SCR_LockYPos) == 0)
		{
			int TargetOffsetY;

			auto Ratio = ScreenData->DefaultWidth * 1000 / ScreenData->TargetWidth;

			if (Ratio * ScreenData->ObjBottom / 1000 <= ScreenData->TargetOffsetAirYPos ||
				Ratio * (ScreenData->HigherObjBottom - ScreenData->ObjBottom) / 1000 >= ScreenData->
				TargetOffsetAirYDist)
			{
				TargetOffsetY = FMath::Min(ScreenData->TargetOffsetY + ScreenData->TargetOffsetLandYAdd,
				                           ScreenData->TargetOffsetLandYMax);
			}
			else
			{
				TargetOffsetY = FMath::Max(ScreenData->TargetOffsetY - ScreenData->TargetOffsetAirYAdd,
				                           ScreenData->TargetOffsetAirYMax);
			}

			ScreenData->TargetOffsetY = TargetOffsetY;

			Ratio = Ratio * ScreenData->HigherObjBottom / 1000 - Ratio * TargetOffsetY / 1000;
			ScreenData->TargetCenterY = FMath::Clamp(Ratio, 0, ScreenData->StageBoundsTop);
		}
	}

	const auto Width = ScreenData->TargetWidth - ScreenData->ScreenWorldWidth;
	ScreenData->WidthVelocity = Width / 14 - 1;
	ScreenData->ScreenWorldWidth += ScreenData->WidthVelocity;

	auto ScreenXSpeedFrame = 10;
	auto ScreenXSpeed = 50;

	ScreenData->CenterXVelocity = (ScreenData->TargetCenterX - ScreenData->ScreenWorldCenterX) / ScreenXSpeedFrame;

	if (ScreenData->CenterXVelocity > ScreenXSpeed) ScreenData->CenterXVelocity = ScreenXSpeed;
	else if (ScreenData->CenterXVelocity < -ScreenXSpeed) ScreenData->CenterXVelocity = -ScreenXSpeed;

	if (ScreenData->TargetCenterX - ScreenData->ScreenWorldCenterX > 0)
	{
		ScreenData->CenterXVelocity++;
	}
	else if (ScreenData->TargetCenterX - ScreenData->ScreenWorldCenterX < 0)
	{
		ScreenData->CenterXVelocity--;
	}

	ScreenData->ScreenWorldCenterX += ScreenData->CenterXVelocity;

	auto CenterYVelocity = ScreenData->TargetCenterY - ScreenData->ScreenWorldCenterY > 0 ? 3000 : 1000;
	auto ScreenYSpeed = 250;

	if (ScreenData->TargetCenterY - ScreenData->ScreenWorldCenterY <= 0) CenterYVelocity = 3000;

	ScreenData->CenterYVelocity = (ScreenData->TargetCenterY - ScreenData->ScreenWorldCenterY) * 1000 / CenterYVelocity;

	if (ScreenData->CenterYVelocity > ScreenYSpeed) ScreenData->CenterYVelocity = ScreenYSpeed;
	else if (ScreenData->CenterYVelocity < -ScreenYSpeed) ScreenData->CenterYVelocity = -ScreenYSpeed;

	if (ScreenData->TargetCenterY - ScreenData->ScreenWorldCenterY > 0)
	{
		ScreenData->CenterYVelocity++;
	}
	else if (ScreenData->TargetCenterY - ScreenData->ScreenWorldCenterY < 0)
	{
		ScreenData->CenterYVelocity--;
	}

	ScreenData->ScreenWorldCenterY += ScreenData->CenterYVelocity;

	ScreenData->bTouchingWorldSide = false;

	if (ScreenData->ScreenWorldCenterX + ScreenData->ScreenWorldWidth / 2 >= ScreenData->StageBoundsRight)
	{
		ScreenData->ScreenWorldCenterX = ScreenData->StageBoundsRight - ScreenData->ScreenWorldWidth / 2;
		ScreenData->bTouchingWorldSide = true;
	}

	if (ScreenData->ScreenWorldCenterX - ScreenData->ScreenWorldWidth / 2 <= ScreenData->StageBoundsLeft)
	{
		ScreenData->ScreenWorldCenterX = ScreenData->StageBoundsLeft + ScreenData->ScreenWorldWidth / 2;
		ScreenData->bTouchingWorldSide = true;
	}

	ScreenData->ScreenBoundsLeft = ScreenData->ScreenWorldCenterX - ScreenData->ScreenWorldWidth / 2;
	ScreenData->ScreenBoundsRight = ScreenData->ScreenWorldCenterX + ScreenData->ScreenWorldWidth / 2;

	if (ScreenData->Flags & SCR_DisableScreenSides)
	{
		ScreenData->ScreenBoundsLeft = ScreenData->StageBoundsLeft;
		ScreenData->ScreenBoundsRight = ScreenData->StageBoundsRight;
	}

	// Madrone change: the stage's edges are walls even when the stage is narrower than the screen (WT walls). Normally the
	// screen is already inside them, so this changes nothing there.
	ScreenData->ScreenBoundsLeft = FMath::Max(ScreenData->ScreenBoundsLeft, ScreenData->StageBoundsLeft);
	ScreenData->ScreenBoundsRight = FMath::Min(ScreenData->ScreenBoundsRight, ScreenData->StageBoundsRight);

	ScreenData->ScreenBoundsTop = 106432 / (ScreenData->ZoomOutBeginX * 1000 / ScreenData->ScreenWorldWidth) +
		368000 / (ScreenData->ZoomOutBeginX * 1000 / ScreenData->ScreenWorldWidth) + ScreenData->ScreenWorldCenterY;

	ScreenData->FinalScreenX = ScreenData->ScreenWorldCenterX;
	ScreenData->FinalScreenY = ScreenData->ScreenWorldCenterY;
	ScreenData->FinalScreenWidth = ScreenData->ScreenWorldWidth;

	if (ScreenData->FinalScreenY >= ScreenData->StageBoundsTop - 106)
		ScreenData->FinalScreenY = ScreenData->
			StageBoundsTop - 106;

	ScreenData->ScreenYZoom = CameraYOffset * (1
		- static_cast<float>(ScreenData->FinalScreenWidth) / static_cast<float>(ScreenData->DefaultWidth));
}

void ANightSkyGameState::UpdateGameState()
{
	RemoteFrame++;
	UpdateLocalInput();
	UpdateGameState(LocalInputs[0], LocalInputs[1], false);
}

void ANightSkyGameState::SortObjects()
{
	// Madrone change: active objects first, each group in spawn order. The swap sort below kept whatever order the objects
	// were in, so after a rollback load (which ran it from a later frame's order) hits could be handled in a different
	// order than when the frame first ran: a wall prop broken by one hit before or after the hit that pinned a fighter to it.
	if (ObjectSpawnOrder.Num() == SortedObjects.Num())
	{
		int32 Write = Players.Num();
		for (int i = Players.Num(); i < ObjectSpawnOrder.Num(); i++)
		{
			if (ObjectSpawnOrder[i]->IsActive) SortedObjects[Write++] = ObjectSpawnOrder[i];
		}
		BattleState.ActiveObjectCount = Write;
		for (int i = Players.Num(); i < ObjectSpawnOrder.Num(); i++)
		{
			if (!ObjectSpawnOrder[i]->IsActive) SortedObjects[Write++] = ObjectSpawnOrder[i];
		}
		return;
	}

	BattleState.ActiveObjectCount = Players.Num();
	for (int i = Players.Num(); i < SortedObjects.Num(); i++)
	{
		for (int j = i + 1; j < SortedObjects.Num(); j++)
		{
			if (SortedObjects[j]->IsActive && !SortedObjects[i]->IsActive)
			{
				ABattleObject* Temp = SortedObjects[i];
				SortedObjects[i] = SortedObjects[j];
				SortedObjects[j] = Temp;
			}
		}
		if (SortedObjects[i]->IsActive)
		{
			BattleState.ActiveObjectCount++;
		}
	}
}

void ANightSkyGameState::HandlePushCollision() const
{
	for (int i = 0; i < Players.Num(); i++)
	{
		for (int j = 0; j < Players.Num(); j++)
		{
			if (Players[i]->PlayerIndex != Players[j]->PlayerIndex && Players[i]->PlayerFlags & PLF_IsOnScreen &&
				Players[j]->PlayerFlags & PLF_IsOnScreen)
			{
				Players[i]->HandlePushCollision(Players[j]);
			}
		}
	}
	// Madrone change: on-screen fighters and neutral objects (battle props) with push collision push each other.
	for (int i = 0; i < SortedObjects.Num() && i < BattleState.ActiveObjectCount; i++)
	{
		ABattleObject* Object = SortedObjects[i];
		if (Object->IsPlayer || !Object->bIsNeutral || !(Object->MiscFlags & MISC_PushCollisionActive)) continue;
		// Walls hold fighters in SetScreenBounds, by their own rules (height above the fighter's floor, any state).
		if (Object->bActsAsWall) continue;
		for (APlayerObject* Fighter : Players)
		{
			if (Fighter->PlayerFlags & PLF_IsOnScreen) Fighter->HandlePushCollision(Object);
		}
	}
}

void ANightSkyGameState::HandleHitCollision() const
{
	for (int i = 0; i < SortedObjects.Num(); i++)
	{
		if (i == BattleState.ActiveObjectCount)
			break;
		for (int j = 0; j < SortedObjects.Num(); j++)
		{
			if (j == BattleState.ActiveObjectCount)
				break;
			// Madrone change: a neutral object (a battle prop) hits and is hit by everything but itself, its owner included,
			// and is on screen whether or not the player that owns it is. Pairs without one are unchanged.
			const bool bNeutralPair = (SortedObjects[i]->bIsNeutral || SortedObjects[j]->bIsNeutral) && SortedObjects[i] != SortedObjects[j];
			const bool bOnScreenI = SortedObjects[i]->bIsNeutral || SortedObjects[i]->Player->PlayerFlags & PLF_IsOnScreen;
			const bool bOnScreenJ = SortedObjects[j]->bIsNeutral || SortedObjects[j]->Player->PlayerFlags & PLF_IsOnScreen;
			if ( (SortedObjects[i]->Player->PlayerIndex != SortedObjects[j]->Player->PlayerIndex || bNeutralPair)
				&& bOnScreenI
				&& bOnScreenJ)
			{
				SortedObjects[i]->HandleCustomCollision_PreHit(SortedObjects[j]);
				SortedObjects[i]->HandleClashCollision(SortedObjects[j]);
				SortedObjects[i]->HandleHitCollision(SortedObjects[j]);
				SortedObjects[i]->HandleCustomCollision_PostHit(SortedObjects[j]);
			}
		}
	}
}

void ANightSkyGameState::UpdateVisuals(bool bShouldResimulate) const
{
	for (int i = 0; i < BattleState.ActiveObjectCount; i++)
	{
		SortedObjects[i]->UpdateVisuals();
		if (!bShouldResimulate)
			SortedObjects[i]->UpdateVisualsNoRollback();
	}
}

void ANightSkyGameState::HandleRoundWin()
{
	if (BattleState.BattlePhase == EBattlePhase::EndScreen) return;

	if (GetMainPlayer(true)->CurrentHealth > 0 && GetMainPlayer(false)->CurrentHealth <= 0)
	{
		if ((GetMainPlayer(true)->PlayerFlags & PLF_RoundWinInputLock) == 0)
			BattleState.P1RoundsWon++;
		GetMainPlayer(true)->RoundWinTimer--;
		GetMainPlayer(true)->PlayerFlags |= PLF_RoundWinInputLock;
		if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
			BattleState.BattlePhase = EBattlePhase::RoundEnd;
		BattleState.PauseTimer = true;
		if (GetMainPlayer(true)->RoundWinTimer == 0 && !IsTagBattle())
		{
			EnterWinState(true, State_Universal_RoundWin, false); // Madrone change: character extensions may override
		}
		NextRoundTransition(true);
	}
	else if (GetMainPlayer(false)->CurrentHealth > 0 && GetMainPlayer(true)->CurrentHealth <= 0)
	{
		if ((GetMainPlayer(false)->PlayerFlags & PLF_RoundWinInputLock) == 0)
			BattleState.P2RoundsWon++;
		GetMainPlayer(false)->RoundWinTimer--;
		GetMainPlayer(false)->PlayerFlags |= PLF_RoundWinInputLock;
		if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
			BattleState.BattlePhase = EBattlePhase::RoundEnd;
		BattleState.PauseTimer = true;
		if (GetMainPlayer(false)->RoundWinTimer == 0 && !IsTagBattle())
		{
			EnterWinState(false, State_Universal_RoundWin, false); // Madrone change: character extensions may override
		}
		NextRoundTransition(false);
	}
	else if (GetMainPlayer(true)->CurrentHealth <= 0 && GetMainPlayer(false)->CurrentHealth <= 0)
	{
		if ((GetMainPlayer(true)->PlayerFlags & PLF_RoundWinInputLock) == 0)
		{
			BattleState.P1RoundsWon++;
			BattleState.P2RoundsWon++;
		}
		GetMainPlayer(true)->PlayerFlags |= PLF_RoundWinInputLock;
		GetMainPlayer(false)->PlayerFlags |= PLF_RoundWinInputLock;
		GetMainPlayer(true)->RoundWinTimer--;
		if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
			BattleState.BattlePhase = EBattlePhase::RoundEnd;
		BattleState.PauseTimer = true;
		NextRoundTransition(true);
	}
	else if (BattleState.RoundTimer <= 0)
	{
		if (GetMainPlayer(true)->CurrentHealth > GetMainPlayer(false)->CurrentHealth)
		{
			if ((GetMainPlayer(true)->PlayerFlags & PLF_RoundWinInputLock) == 0)
				BattleState.P1RoundsWon++;
			GetMainPlayer(true)->RoundWinTimer--;
			GetMainPlayer(true)->PlayerFlags |= PLF_RoundWinInputLock;
			GetMainPlayer(false)->PlayerFlags |= PLF_RoundWinInputLock;
			if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
				BattleState.BattlePhase = EBattlePhase::RoundEnd;
			BattleState.PauseTimer = true;
			if (GetMainPlayer(true)->RoundWinTimer == 0 && !IsTagBattle())
			{
				EnterWinState(true, State_Universal_RoundWin, false); // Madrone change: character extensions may override
				GetMainPlayer(false)->JumpToStatePrimary(State_Universal_RoundLose);
			}
			NextRoundTransition(true);
		}
		else if (GetMainPlayer(false)->CurrentHealth > GetMainPlayer(true)->CurrentHealth)
		{
			if ((GetMainPlayer(false)->PlayerFlags & PLF_RoundWinInputLock) == 0)
				BattleState.P2RoundsWon++;
			GetMainPlayer(false)->RoundWinTimer--;
			GetMainPlayer(true)->PlayerFlags |= PLF_RoundWinInputLock;
			GetMainPlayer(false)->PlayerFlags |= PLF_RoundWinInputLock;
			if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
				BattleState.BattlePhase = EBattlePhase::RoundEnd;
			BattleState.PauseTimer = true;
			if (GetMainPlayer(false)->RoundWinTimer == 0 && !IsTagBattle())
			{
				EnterWinState(false, State_Universal_RoundWin, false); // Madrone change: character extensions may override
				GetMainPlayer(true)->JumpToStatePrimary(State_Universal_RoundLose);
			}
			NextRoundTransition(false);
		}
		else if (GetMainPlayer(true)->CurrentHealth == GetMainPlayer(false)->CurrentHealth)
		{
			if ((GetMainPlayer(true)->PlayerFlags & PLF_RoundWinInputLock) == 0)
			{
				BattleState.P1RoundsWon++;
				BattleState.P2RoundsWon++;
			}
			GetMainPlayer(true)->PlayerFlags |= PLF_RoundWinInputLock;
			GetMainPlayer(false)->PlayerFlags |= PLF_RoundWinInputLock;
			if (BattleState.BattlePhase < EBattlePhase::RoundEnd)
				BattleState.BattlePhase = EBattlePhase::RoundEnd;
			GetMainPlayer(true)->RoundWinTimer--;
			BattleState.PauseTimer = true;
			if (GetMainPlayer(true)->RoundWinTimer == 0 && !IsTagBattle())
			{
				GetMainPlayer(true)->JumpToStatePrimary(State_Universal_RoundLose);
				GetMainPlayer(false)->JumpToStatePrimary(State_Universal_RoundLose);
			}
			NextRoundTransition(true);
		}
	}
}

void ANightSkyGameState::NextRoundTransition(bool bIsP1)
{
	if (GetMainPlayer(bIsP1)->RoundWinTimer <= 0)
	{
		if (IsTagBattle())
		{
			if (!HandleMatchWin())
			{
				RoundInit();
			}
		}
		else if (!HandleMatchWin() && GetMainPlayer(bIsP1)->RoundEndFlag == true && !IsVictorySequenceRunning()) // Madrone change
		{
			if (BattleState.BattlePhase != EBattlePhase::Fade)
			{
				BattleHudActor->BottomWidget->PlayFadeAnim();
				BattleState.BattlePhase = EBattlePhase::Fade;
			}
			BattleState.FadeTimer--;
			if (BattleState.FadeTimer <= 0)
			{
				RoundInit();
				BattleHudActor->BottomWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount); //Nick change
				BattleHudActor->TopWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount);
			}
		}
	}
}

bool ANightSkyGameState::HandleMatchWin()
{
	if (BattleState.CurrentWinSide != WIN_None)
	{
		if (BattleState.BattlePhase == EBattlePhase::EndScreen) return true;
		if (BattleState.CurrentWinSide == WIN_P2)
		{
			if (GetMainPlayer(false)->RoundEndFlag == true && !IsVictorySequenceRunning()) // Madrone change
			{
				EndMatch();
			}
		}
		else
		{
			if (GetMainPlayer(true)->RoundEndFlag == true && !IsVictorySequenceRunning()) // Madrone change
			{
				EndMatch();
			}
		}
		return true;
	}
	switch (BattleState.BattleFormat)
	{
	case EBattleFormat::Rounds:
		{
			if (BattleState.P1RoundsWon >= BattleState.MaxRoundCount && BattleState.P2RoundsWon < BattleState.
				P1RoundsWon)
			{
				EnterWinState(true, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P1;
				return true;
			}
			if (BattleState.P2RoundsWon >= BattleState.MaxRoundCount && BattleState.P1RoundsWon < BattleState.
				P2RoundsWon)
			{
				EnterWinState(false, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P2;
				return true;
			}
			if (BattleState.P1RoundsWon > BattleState.MaxRoundCount && BattleState.P2RoundsWon > BattleState.
				MaxRoundCount)
			{
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_Draw;
				return true;
			}
		}
		return false;
	case EBattleFormat::Tag:
	case EBattleFormat::KOF:
		{
			// Madrone change: a side wins when every fighter of the other side is down and it still has one standing. It
			// compared the two sides' KO counts instead, so after one KO each (Golden Boy swooning, then the player's solo
			// fighter going down) nobody won and RoundInit ran again every frame for the rest of the fight.
			if (BattleState.P1RoundsWon >= BattleState.TeamData[1].TeamCount && BattleState.P2RoundsWon < BattleState.
				TeamData[0].TeamCount)
			{
				EnterWinState(true, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P1;
				return true;
			}
			if (BattleState.P2RoundsWon >= BattleState.TeamData[0].TeamCount && BattleState.P1RoundsWon < BattleState.
				TeamData[1].TeamCount)
			{
				EnterWinState(false, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P2;
				return true;
			}
			if (BattleState.P1RoundsWon >= BattleState.TeamData[1].TeamCount && BattleState.P2RoundsWon >= BattleState.
				TeamData[0].TeamCount)
			{
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_Draw;
				return true;
			}
		}
		return false;
	case EBattleFormat::NickTour:
		{
			if (BattleState.P1RoundsWon >= BattleState.TeamData[1].TeamCount && BattleState.P2RoundsWon <= BattleState.
				P1RoundsWon)
			{
				EnterWinState(true, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P1;
				return true;
			}
			if (BattleState.P2RoundsWon >= BattleState.TeamData[0].TeamCount && BattleState.P1RoundsWon <= BattleState.
				P2RoundsWon)
			{
				EnterWinState(false, State_Universal_MatchWin, true); // Madrone change
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_P2;
				return true;
			}
			if (BattleState.P1RoundsWon >= BattleState.TeamData[1].TeamCount && BattleState.P2RoundsWon >= BattleState.
				TeamData[0].TeamCount)
			{
				GameInstance->EndRecordReplay();
				BattleState.CurrentWinSide = WIN_Draw;
				return true;
			}
		}
		return false;
	default:
		return false;
	}
}

void ANightSkyGameState::CollisionView() const
{
	if (bViewCollision)
	{
		for (int i = 0; i < BattleState.ActiveObjectCount; i++)
		{
			SortedObjects[i]->CollisionView();
		}
	}
}

int32 ANightSkyGameState::CreateChecksum()
{
	Checksum = 0;

	for (const auto Player : GetRollbackPlayers()) // Madrone change: with the assists
	{
		Checksum += Player->ActionTime ^ Player->ActionTime << 16;
		Checksum += Player->PosX ^ Player->PosX << 16;
		Checksum += Player->PosY ^ Player->PosY << 16;
		Checksum += Player->CurrentHealth ^ Player->CurrentHealth << 16;
	}

	Checksum += BattleState.Meter[0] ^ BattleState.Meter[0] << 16;
	Checksum += BattleState.Meter[1] ^ BattleState.Meter[1] << 16;

	for (const auto Gauge : BattleState.GaugeP1)
	{
		Checksum += Gauge ^ Gauge << 16;
	}
	for (const auto Gauge : BattleState.GaugeP2)
	{
		Checksum += Gauge ^ Gauge << 16;
	}

	return Checksum;
}

FGGPONetworkStats ANightSkyGameState::GetNetworkStats() const
{
	FGGPONetworkStats Stats{};
	if (AFighterMultiplayerRunner* Runner = Cast<AFighterMultiplayerRunner>(FighterRunner))
	{
		if (Runner->Players[0]->type == GGPO_PLAYERTYPE_REMOTE)
			GGPONet::ggpo_get_network_stats(Runner->ggpo, Runner->PlayerHandles[0], &Stats);
		else
			GGPONet::ggpo_get_network_stats(Runner->ggpo, Runner->PlayerHandles[1], &Stats);
		return Stats;
	}
	return Stats;
}

void ANightSkyGameState::ResetTraining()
{
	if (GameInstance->IsTraining)
	{
		BattleState.RoundCount = 0;
		RoundInit();
	}
	
	for (int32 Index = 0; Index < 2; Index++)
	{
		if (UNightSkyBattleInputComponent* BattleInput = UNightSkyBattleInputComponent::FindOn(
			UGameplayStatics::GetPlayerController(GetWorld(), Index)))
		{
			BattleInput->Inputs &= ~INP_ResetTraining;
		}
	}
}

// Madrone change: walls in the middle of the stage (wall battle props).
namespace
{
	bool IsLiveObjectWall(const ABattleObject* Wall)
	{
		return Wall && !Wall->IsPlayer && Wall->IsActive && Wall->bActsAsWall && (Wall->MiscFlags & MISC_PushCollisionActive);
	}

	// Below the wall's top. Heights are above each one's own floor, so on a slope the wall is as tall from either side.
	bool IsBelowWallTop(const APlayerObject* Player, const ABattleObject* Wall)
	{
		return Player->B - Player->GroundHeight < Wall->T - Wall->PosY;
	}
}

ABattleObject* ANightSkyGameState::GetObjectWallAgainst(const APlayerObject* Player, const int32 Reach) const
{
	if (!Player) return nullptr;
	for (int i = 0; i < BattleState.ActiveObjectCount && i < SortedObjects.Num(); i++)
	{
		ABattleObject* Wall = SortedObjects[i];
		if (!IsLiveObjectWall(Wall)) continue;
		// Flush counts.
		if (Player->R < Wall->L - Reach || Player->L > Wall->R + Reach || !IsBelowWallTop(Player, Wall)) continue;
		// Like a stage edge, a wall is behind the fighter: one between them and their opponent isn't one they're pinned to.
		const bool bWallOnRight = Wall->PosX > Player->PosX;
		const APlayerObject* Enemy = Player->Enemy;
		if (Enemy && Enemy->PosX != Player->PosX && (Enemy->PosX > Player->PosX) == bWallOnRight) continue;
		return Wall;
	}
	return nullptr;
}

void ANightSkyGameState::PushOutOfObjectWalls(APlayerObject* Player) const
{
	if (!Player) return;
	for (int i = 0; i < BattleState.ActiveObjectCount && i < SortedObjects.Num(); i++)
	{
		const ABattleObject* Wall = SortedObjects[i];
		if (!IsLiveObjectWall(Wall)) continue;
		Player->CalculatePushbox();
		if (Player->R <= Wall->L || Player->L >= Wall->R || !IsBelowWallTop(Player, Wall)) continue;
		// Out the way they came in: fighters are put back every frame, so they never get past the middle.
		if (Player->PosX < Wall->PosX || (Player->PosX == Wall->PosX && Player->PlayerIndex == 0))
			Player->PosX -= Player->R - Wall->L;
		else
			Player->PosX += Wall->R - Player->L;
		Player->CalculatePushbox();
	}
}

void ANightSkyGameState::SetScreenBounds() const
{
	for (int i = 0; i < BattleState.ActiveObjectCount; i++)
	{
		if (SortedObjects[i] != nullptr)
		{
			if (SortedObjects[i]->MiscFlags & MISC_WallCollisionActive)
			{
				const auto ScreenData = &BattleState.ScreenData;

				if (const auto Player = Cast<APlayerObject>(SortedObjects[i]))
				{
					if (!(Player->PlayerFlags & PLF_IsOnScreen)) continue;
					Player->PlayerFlags |= PLF_TouchingWall;
					Player->WallTouchTimer++;
				}

				// Madrone change: walls in the middle of the stage hold fighters like the edges do (the edges win).
				if (const auto Player = Cast<APlayerObject>(SortedObjects[i])) PushOutOfObjectWalls(Player);

				SortedObjects[i]->CalculatePushbox();

				if (SortedObjects[i]->PosX + 85000 >= ScreenData->ScreenBoundsRight * 1000)
				{
					SortedObjects[i]->PosX = ScreenData->ScreenBoundsRight * 1000 - 85000;
				}
				else if (SortedObjects[i]->PosX - 85000 <= ScreenData->ScreenBoundsLeft * 1000)
				{
					SortedObjects[i]->PosX = ScreenData->ScreenBoundsLeft * 1000 + 85000;
				}
				else
				{
					// Madrone change: walls in the middle of the stage (wall battle props) count too.
					const auto Player = Cast<APlayerObject>(SortedObjects[i]);
					if (Player && !GetObjectWallAgainst(Player))
					{
						Player->PlayerFlags &= ~PLF_TouchingWall;
						Player->WallTouchTimer = 0;
					}
				}

				if (SortedObjects[i]->PosY >= ScreenData->ScreenBoundsTop * 1000 + SortedObjects[i]->GroundHeight)
				{
					SortedObjects[i]->PosY = ScreenData->ScreenBoundsTop * 1000 + SortedObjects[i]->GroundHeight;
				}
			}
		}
	}
}

void ANightSkyGameState::StartSuperFreeze(int32 Duration, int32 SelfDuration, ABattleObject* CallingObject)
{
	BattleState.SuperFreezeDuration = Duration;
	BattleState.SuperFreezeSelfDuration = SelfDuration;
	BattleState.SuperFreezeCaller = CallingObject;
	BattleState.PauseTimer = true;
}

ABattleObject* ANightSkyGameState::AddBattleObject(
	const UState* InState,
	int PosX,
	int PosY,
	EObjDir Dir,
	int32 ObjectStateIndex,
	bool bIsCommonState,
	APlayerObject* Parent,
	bool bIsNeutral) const
{
	for (int i = 0; i < MaxBattleObjects; i++)
	{
		if (!Objects[i]->IsActive)
		{
			Objects[i]->ObjectState = DuplicateObject(InState, Objects[i]);
			Objects[i]->ObjectStateName = InState->Name;
			Objects[i]->ObjectState->Parent = Objects[i];
			Objects[i]->IsActive = true;
			Objects[i]->Direction = Dir;
			Objects[i]->Player = Parent;
			Objects[i]->PosX = PosX;
			Objects[i]->PosY = PosY;
			Objects[i]->ObjectStateIndex = ObjectStateIndex;
			Objects[i]->bIsCommonState = bIsCommonState;
			Objects[i]->bIsNeutral = bIsNeutral;
			Objects[i]->InitObject();
			// Madrone change: a projectile from a simple-input move deals the reduced damage too
			Objects[i]->bSimpleInput = Parent && Parent->bSimpleInput;
			return Objects[i];
		}
	}
	return nullptr;
}

void ANightSkyGameState::UpdateCamera()
{
	if (bIsResimulating) return;
	if (CameraActor != nullptr)
	{
		const auto ScreenData = &BattleState.ScreenData;

		BattleState.CameraPosition = BattleSceneTransform.GetRotation().RotateVector(
				FVector(ScreenData->FinalScreenX * OBJ_SCALE, ScreenData->FinalScreenWidth * OBJ_SCALE,
				        ScreenData->FinalScreenY * OBJ_SCALE - ScreenData->ScreenYZoom + CameraYOffset)) + BattleSceneTransform.
			GetLocation();
		BattleState.CameraPosition.Z += GetCameraFloorOffset(BattleState.CameraPosition);
		FRotator CameraRotation = BattleSceneTransform.GetRotation().Rotator();
		CameraRotation.Yaw -= 90;
		CameraRotation.Pitch -= CameraYaw;
		CameraRotation.Pitch += GetCameraPitchOffset(); // Madrone change
		if (BattleState.CurrentSequenceTime == -1)
		{
			BattleState.OrthoBlendActive = 1;
			const FVector SequenceCameraLocation = BattleSceneTransform.GetRotation().RotateVector(
				FVector(0, 1080, 175)) + BattleSceneTransform.GetLocation();
			SequenceCameraActor->SetActorLocation(SequenceCameraLocation);
			if (const auto PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0); IsValid(
				PlayerController))
			{
				PlayerController->SetViewTargetWithBlend(CameraActor);
			}
			CameraActor->SetActorLocation(BattleState.CameraPosition);
			CameraActor->SetActorRotation(CameraRotation);
		}
		else
		{
			if (BattleState.CurrentSequenceTime >= SequenceActor->GetSequencePlayer()->GetEndTime().Time)
			{
				StopLevelSequence();
				return;
			}
			// Madrone change: scrub through each frame since the last update (a slow render frame covers several battle
			// frames), so event keys on every frame fire, once. After a rollback (time went back) or a long gap: jump.
			const int32 SequenceTime = BattleState.CurrentSequenceTime;
			const int32 FirstTime = LastScrubbedSequenceTime >= 0 && LastScrubbedSequenceTime < SequenceTime
				&& SequenceTime - LastScrubbedSequenceTime <= 30 ? LastScrubbedSequenceTime + 1 : SequenceTime;
			if (LastScrubbedSequenceTime != SequenceTime)
			{
				for (int32 Time = FirstTime; Time <= SequenceTime; ++Time)
				{
					SequenceActor->GetSequencePlayer()->SetPlaybackPosition(
						FMovieSceneSequencePlaybackParams(FFrameTime(Time), EUpdatePositionMethod::Scrub));
				}
				LastScrubbedSequenceTime = SequenceTime;
			}
			const FVector SequenceTargetVector = FVector(SequenceTarget->PosX / COORD_SCALE,
			                                             SequenceTarget->PosZ / COORD_SCALE,
			                                             SequenceTarget->PosY / COORD_SCALE);

			FVector NewCamLocation = SequenceCameraActor->GetActorLocation();
			NewCamLocation.Y = NewCamLocation.Y + SequenceTargetVector.Y;
			NewCamLocation.Z = NewCamLocation.Z + SequenceTargetVector.Z;

			if (SequenceTarget->Direction == DIR_Left)
			{
				NewCamLocation.X = -NewCamLocation.X + SequenceTargetVector.X;
				auto NewCamRotation = SequenceCameraActor->GetActorForwardVector();
				NewCamRotation.X *= -1;
				SequenceCameraActor->SetActorRotation(UKismetMathLibrary::MakeRotFromX(NewCamRotation));
			}
			else
			{
				NewCamLocation.X = NewCamLocation.X + SequenceTargetVector.X;
			}
			
			APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
			if (CameraManager->PendingViewTarget.Target == CameraActor)
			{
				CameraActor->SetActorLocation(BattleState.CameraPosition);
				CameraActor->SetActorRotation(CameraRotation);
				BattleState.OrthoBlendActive = 1 - (CameraManager->BlendTimeToGo / CameraManager->BlendParams.BlendTime);
			}
			else if (CameraManager->PendingViewTarget.Target == SequenceCameraActor)
			{
				BattleState.OrthoBlendActive = CameraManager->BlendTimeToGo / CameraManager->BlendParams.BlendTime;
			}
			else if (CameraManager->ViewTarget.Target == CameraActor)
			{
				CameraActor->SetActorLocation(BattleState.CameraPosition);
				CameraActor->SetActorRotation(CameraRotation);
				BattleState.OrthoBlendActive = 1;
			}
			else
			{
				BattleState.OrthoBlendActive = 0;
			}

			SequenceCameraActor->SetActorLocation(
				BattleSceneTransform.GetRotation().RotateVector(NewCamLocation) + BattleSceneTransform.GetLocation()
				+ FVector(0, 0, SequenceTarget ? GetFloorHeightAt(SequenceTarget->PosX) / COORD_SCALE : 0.f));

			const FRotator SequenceRotation = BattleSceneTransform.GetRotation().Rotator() + SequenceCameraActor->
				GetActorRotation();

			SequenceCameraActor->SetActorRotation(SequenceRotation);
			AvoidSequenceCameraCollision(); // Madrone change: never in the ground or behind a wall
		}
	}
	bIsPlayingSequence = BattleState.IsPlayingSequence;
}

// Madrone change: sequence camera collision. Docs: WitchTools Docs/BattleCamera.md (Sequence cameras).
FVector ANightSkyGameState::ResolveCameraCollision(const UObject* WorldContextObject, const FVector& Focus,
                                                   const FVector& Desired, const float ProbeRadius,
                                                   const float GroundClearance, const TArray<AActor*>& Ignore)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return Desired;
	FCollisionQueryParams Params(TEXT("NSECameraCollision"), false);
	Params.AddIgnoredActors(Ignore);
	const FCollisionObjectQueryParams Static(ECC_WorldStatic);
	FVector Result = Desired;

	// 1. Anything solid between the focus and the camera: stop in front of it.
	FHitResult Hit;
	if (World->SweepSingleByObjectType(Hit, Focus, Desired, FQuat::Identity, Static,
	                                   FCollisionShape::MakeSphere(FMath::Max(1.f, ProbeRadius)), Params))
	{
		Result = Hit.bStartPenetrating ? Focus : Hit.Location;
	}

	// 2. Keep clear of the ground under the camera (a slope can rise right under the end of the sweep).
	if (GroundClearance > 0.f)
	{
		const FVector Top = Result + FVector(0, 0, FMath::Max(300.f, GroundClearance * 4.f));
		const FVector Bottom = Result - FVector(0, 0, GroundClearance);
		if (World->LineTraceSingleByObjectType(Hit, Top, Bottom, Static, Params) && !Hit.bStartPenetrating)
		{
			Result.Z = FMath::Max(Result.Z, Hit.ImpactPoint.Z + GroundClearance);
		}
	}
	return Result;
}

void ANightSkyGameState::AvoidSequenceCameraCollision()
{
	if (!bSequenceCameraAvoidCollision || !SequenceCameraActor || !IsValid(SequenceTarget))
	{
		SequenceCameraPull = FVector::ZeroVector;
		return;
	}
	const FVector Desired = SequenceCameraActor->GetActorLocation();
	const FVector Focus = SequenceTarget->GetActorLocation() + BattleSceneTransform.GetRotation().GetUpVector() * SequenceCameraFocusHeight;
	TArray<AActor*> Ignore;
	for (APlayerObject* Player : Players) if (IsValid(Player)) Ignore.Add(Player);
	Ignore.Add(SequenceCameraActor);
	if (CameraActor) Ignore.Add(CameraActor);
	const FVector Pull = ResolveCameraCollision(this, Focus, Desired, SequenceCameraProbeRadius, SequenceCameraGroundClearance, Ignore) - Desired;

	// In at once, back out smoothly.
	if (Pull.SizeSquared() >= SequenceCameraPull.SizeSquared())
	{
		SequenceCameraPull = Pull;
	}
	else
	{
		SequenceCameraPull = FMath::VInterpTo(SequenceCameraPull, Pull, GetWorld()->GetDeltaSeconds(), SequenceCameraReleaseSpeed);
		// Easing out must not leave it in the ground: re-check where the eased camera would be.
		const FVector Eased = Desired + SequenceCameraPull;
		SequenceCameraPull = ResolveCameraCollision(this, Focus, Eased, SequenceCameraProbeRadius, SequenceCameraGroundClearance, Ignore) - Desired;
	}
	SequenceCameraActor->SetActorLocation(Desired + SequenceCameraPull);
}

void ANightSkyGameState::PlayLevelSequence(APlayerObject* Target, APlayerObject* Enemy, ULevelSequence* Sequence)
{
	if (SequenceActor != nullptr)
	{
		SequenceActor->GetSequencePlayer()->Stop();
		SequenceActor->SetSequence(Sequence);
		for (int i = 0; i < Sequence->GetMovieScene()->GetPossessableCount(); i++)
		{
			auto MovieSceneBinding = Sequence->GetMovieScene()->GetPossessable(i);
			if (!MovieSceneBinding.GetName().Equals("Target"))
			{
				continue;
			}

			FMovieSceneObjectBindingID BindingId = FMovieSceneObjectBindingID(MovieSceneBinding.GetGuid());
			SequenceActor->SetBinding(BindingId, TArray<AActor*>{Target});

			break;
		}
		for (int i = 0; i < Sequence->GetMovieScene()->GetPossessableCount(); i++)
		{
			auto MovieSceneBinding = Sequence->GetMovieScene()->GetPossessable(i);
			if (!MovieSceneBinding.GetName().Equals("Enemy"))
			{
				continue;
			}

			FMovieSceneObjectBindingID BindingId = FMovieSceneObjectBindingID(MovieSceneBinding.GetGuid());
			SequenceActor->SetBinding(BindingId, TArray<AActor*>{Enemy});

			break;
		}
		for (int i = 0; i < Sequence->GetMovieScene()->GetPossessableCount(); i++)
		{
			auto MovieSceneBinding = Sequence->GetMovieScene()->GetPossessable(i);
			if (!MovieSceneBinding.GetName().Equals("CameraActor"))
			{
				continue;
			}

			FMovieSceneObjectBindingID BindingId = FMovieSceneObjectBindingID(MovieSceneBinding.GetGuid());
			SequenceActor->SetBinding(BindingId, TArray<AActor*>{CameraActor});

			break;
		}
		for (int i = 0; i < Sequence->GetMovieScene()->GetPossessableCount(); i++)
		{
			auto MovieSceneBinding = Sequence->GetMovieScene()->GetPossessable(i);
			if (!MovieSceneBinding.GetName().Equals("SequenceCameraActor"))
			{
				continue;
			}

			FMovieSceneObjectBindingID BindingId = FMovieSceneObjectBindingID(MovieSceneBinding.GetGuid());
			SequenceActor->SetBinding(BindingId, TArray<AActor*>{SequenceCameraActor});

			break;
		}
		SequenceTarget = Target;
		SequenceEnemy = Enemy;
		LastScrubbedSequenceTime = -1;
		SequenceCameraPull = FVector::ZeroVector; // Madrone change
		BattleState.CurrentSequenceTime = 0;
		BattleState.IsPlayingSequence = true;
	}
}

void ANightSkyGameState::StopLevelSequence()
{
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	
	if (CameraManager != nullptr)
	{
		CameraActor->SetActorLocation(CameraManager->ViewTarget.POV.Location);
		CameraActor->SetActorRotation(CameraManager->ViewTarget.POV.Rotation);
	}
	
	SequenceActor->GetSequencePlayer()->Stop();
	LastScrubbedSequenceTime = -1;
	BattleState.CurrentSequenceTime = -1;
	BattleState.IsPlayingSequence = false;
}

void ANightSkyGameState::CameraShake(const TSubclassOf<UCameraShakeBase>& Pattern, float Scale) const
{
	if (Pattern)
	{
		const auto PlayerCameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
		PlayerCameraManager->StopAllCameraShakes();
		PlayerCameraManager->StartCameraShake(Pattern, Scale);
	}
}

void ANightSkyGameState::HUDInit() const
{
	// Madrone change: a new fight (or HUD) gets its HUD style applied again.
	for (int32 Side = 0; Side < 2; Side++)
	{
		AppliedHudStyle[Side] = nullptr;
		bHudStyleApplied[Side] = false;
		HudMainPlayer[Side] = nullptr;
	}
	if (BattleHudActor != nullptr)
	{
		if (BattleHudActor->TopWidget != nullptr)
		{
			BattleHudActor->TopWidget->P1Health.Init(0, BattleState.TeamData[0].TeamCount);
			BattleHudActor->TopWidget->P2Health.Init(0, BattleState.TeamData[1].TeamCount);
			BattleHudActor->TopWidget->P1RecoverableHealth.Init(0, BattleState.TeamData[0].TeamCount);
			BattleHudActor->TopWidget->P2RecoverableHealth.Init(0, BattleState.TeamData[1].TeamCount);
		}
	}
}

USlateWidgetStyleAsset* ANightSkyGameState::ResolveHudStyle(const bool bIsP1)
{
	if (HudStyleOverride.IsSet())
	{
		return HudStyleOverride.Style.LoadSynchronous();
	}
	// Player 2's fighters first, so a tie goes to player 2; within a side, the fighter on screen first (GetTeam order).
	TArray<APlayerObject*> Fighters = GetTeam(false);
	if (P2Assist) Fighters.Add(P2Assist);
	Fighters.Append(GetTeam(true));
	if (P1Assist) Fighters.Add(P1Assist);
	const FNSEHudStyleRequest* Best = nullptr;
	for (const APlayerObject* Fighter : Fighters)
	{
		if (!Fighter || !Fighter->HudStyleRequest.IsSet()) continue;
		const FNSEHudStyleRequest& Request = Fighter->HudStyleRequest;
		const bool bFighterP1 = Fighter->PlayerIndex == 0;
		if (Request.Scope == ENSEHudStyleScope::OwnSide && bFighterP1 != bIsP1) continue;
		if (Request.bOnlyWhileOnScreen && Fighter != GetMainPlayer(bFighterP1)) continue;
		if (!Best || Request.Priority > Best->Priority) Best = &Request;
	}
	if (!Best && DefaultHudStyle.IsSet())
	{
		Best = &DefaultHudStyle;
	}
	return Best ? Best->Style.LoadSynchronous() : nullptr;
}

void ANightSkyGameState::UpdateHudStyles()
{
	UNightSkyBattleWidget* Top = BattleHudActor ? BattleHudActor->TopWidget : nullptr;
	if (!Top || BattleState.MainPlayer[0] == nullptr || BattleState.MainPlayer[1] == nullptr) return;
	if (HudMainPlayer[0] != BattleState.MainPlayer[0] || HudMainPlayer[1] != BattleState.MainPlayer[1])
	{
		HudMainPlayer[0] = BattleState.MainPlayer[0];
		HudMainPlayer[1] = BattleState.MainPlayer[1];
		Top->OnMainPlayersChanged();
	}
	for (int32 Side = 0; Side < 2; Side++)
	{
		USlateWidgetStyleAsset* Asset = ResolveHudStyle(Side == 0);
		if (bHudStyleApplied[Side] && AppliedHudStyle[Side].Get() == Asset) continue;
		bHudStyleApplied[Side] = true;
		AppliedHudStyle[Side] = Asset;
		Top->ApplyBattleHudStyle(Side == 0, Asset ? FNSEStyle::RegisterHudStyle(Asset) : nullptr, Asset);
	}
}

void ANightSkyGameState::UpdateHUD()
{
	if (bIsResimulating) return;
	// Madrone change: training's HUD round init (teammate health bars, names), once the widgets are built
	if (bHudRoundInitPending && BattleHudActor && BattleHudActor->TopWidget && BattleHudActor->BottomWidget
		&& BattleHudActor->TopWidget->GetCachedWidget().IsValid())
	{
		bHudRoundInitPending = false;
		BattleHudActor->BottomWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount);
		BattleHudActor->TopWidget->RoundInitImplementation(BattleState.TeamData[0].TeamCount, BattleState.TeamData[1].TeamCount);
	}
	UpdateHudStyles(); // Madrone change
	if (BattleState.bHUDVisible)
	{
		BattleHudActor->TopWidget->SetVisibility(ESlateVisibility::Visible);
		BattleHudActor->BottomWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		BattleHudActor->TopWidget->SetVisibility(ESlateVisibility::Hidden);
		BattleHudActor->BottomWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	if (BattleHudActor != nullptr)
	{
		if (BattleHudActor->TopWidget != nullptr)
		{
			//Madrone change: sized to the teams every time they change, not only when empty. The overworld HUD lives on
			// between fights, so a tag fight after a 1v1 indexed past the end (crash entering the battle).
			if (BattleHudActor->TopWidget->P1Cooldowns.Num() != BattleState.TeamData[0].TeamCount)
				BattleHudActor->TopWidget->P1Cooldowns.SetNum(
					BattleState.TeamData[0].TeamCount);
			if (BattleHudActor->TopWidget->P2Cooldowns.Num() != BattleState.TeamData[1].TeamCount)
				BattleHudActor->TopWidget->P2Cooldowns.SetNum(
					BattleState.TeamData[1].TeamCount);
			
			BattleHudActor->TopWidget->P1AssistCooldown = BattleState.AssistCooldownTimer[0]; // Madrone change
			BattleHudActor->TopWidget->P2AssistCooldown = BattleState.AssistCooldownTimer[1];
			for (int i = 0; i < BattleState.TeamData[0].TeamCount; i++)
			{
				//Tag cooldown timers
				BattleHudActor->TopWidget->P1Cooldowns[i] = BattleState.TeamData[0].CooldownTimer[i];
				
				//BattleHudActor->TopWidget->P1Health[i] = static_cast<float>(GetTeam(true)[i]->CurrentHealth) /
				//	static_cast<float>(GetTeam(true)[i]->MaxHealth);
				BattleHudActor->TopWidget->UpdateAndNotifyP1Health(i, static_cast<float>(GetTeam(true)[i]->CurrentHealth) /
					static_cast<float>(GetTeam(true)[i]->MaxHealth));
				//BattleHudActor->TopWidget->P1RecoverableHealth[i] = static_cast<float>(GetTeam(true)[i]->CurrentHealth +
					//GetTeam(true)[i]->RecoverableHealth) / static_cast<float>(GetTeam(true)[i]->MaxHealth);
				BattleHudActor->TopWidget->UpdateAndNotifyP1RecoverableHealth(i, static_cast<float>(GetTeam(true)[i]->CurrentHealth +
					GetTeam(true)[i]->RecoverableHealth) / static_cast<float>(GetTeam(true)[i]->MaxHealth));
				
			}
			for (int i = 0; i < BattleState.TeamData[1].TeamCount; i++)
			{
				//Tag cooldown timers
				BattleHudActor->TopWidget->P2Cooldowns[i] = BattleState.TeamData[1].CooldownTimer[i];
				
				//BattleHudActor->TopWidget->P2Health[i] = static_cast<float>(GetTeam(false)[i]->CurrentHealth) /
				//	static_cast<float>(GetTeam(false)[i]->MaxHealth);
				//BattleHudActor->TopWidget->P2RecoverableHealth[i] = static_cast<float>(GetTeam(false)[i]->CurrentHealth
				//	+ GetTeam(false)[i]->RecoverableHealth) / static_cast<float>(GetTeam(false)[i]->MaxHealth);
				
				BattleHudActor->TopWidget->UpdateAndNotifyP2Health(i, static_cast<float>(GetTeam(false)[i]->CurrentHealth) /
					static_cast<float>(GetTeam(false)[i]->MaxHealth));
				BattleHudActor->TopWidget->UpdateAndNotifyP2RecoverableHealth(i, static_cast<float>(GetTeam(false)[i]->CurrentHealth +
					GetTeam(false)[i]->RecoverableHealth) / static_cast<float>(GetTeam(false)[i]->MaxHealth));
			}
			// Madrone change: character extensions' teammate bars (virtual teammates, recoloured partners)
			for (const bool bSideP1 : {true, false})
			{
				TArray<FNSETeammateBar> VirtualBars, RealOverrides;
				GatherTeammateBars(bSideP1, VirtualBars, RealOverrides);
				BattleHudActor->TopWidget->SetCharacterTeammateBars(bSideP1, VirtualBars, RealOverrides);
			}
			BattleHudActor->TopWidget->P1RoundsWon = BattleState.P1RoundsWon;
			BattleHudActor->TopWidget->P2RoundsWon = BattleState.P2RoundsWon;
			BattleHudActor->TopWidget->Timer = static_cast<float>(BattleState.RoundTimer / 60);
			BattleHudActor->TopWidget->P1ComboCounter = BattleState.MainPlayer[0]->ComboCounter;
			BattleHudActor->TopWidget->P2ComboCounter = BattleState.MainPlayer[1]->ComboCounter;
			BattleHudActor->TopWidget->Ping = NetworkStats.Ping;
			BattleHudActor->TopWidget->RollbackFrames = NetworkStats.RollbackFrames;

			if (BattleHudActor->TopWidget->P1Gauge.IsEmpty())
				BattleHudActor->TopWidget->P1Gauge.SetNum(
					BattleState.MaxGauge.Num());
			if (BattleHudActor->TopWidget->P2Gauge.IsEmpty())
				BattleHudActor->TopWidget->P2Gauge.SetNum(
					BattleState.MaxGauge.Num());

			for (int j = 0; j < BattleState.MaxGauge.Num(); j++)
			{
				BattleHudActor->TopWidget->P1Gauge[j] = static_cast<float>(BattleState.GaugeP1[j]) / BattleState.
					MaxGauge[j];
				BattleHudActor->TopWidget->P2Gauge[j] = static_cast<float>(BattleState.GaugeP2[j]) / BattleState.
					MaxGauge[j];
			}
		}
		if (BattleHudActor->BottomWidget != nullptr)
		{
			BattleHudActor->BottomWidget->P1Meter = static_cast<float>(BattleState.Meter[0]) / 10000;
			BattleHudActor->BottomWidget->P2Meter = static_cast<float>(BattleState.Meter[1]) / 10000;

			if (BattleHudActor->BottomWidget->P1Gauge.IsEmpty())
				BattleHudActor->BottomWidget->P1Gauge.SetNum(
					BattleState.MaxGauge.Num());
			if (BattleHudActor->BottomWidget->P2Gauge.IsEmpty())
				BattleHudActor->BottomWidget->P2Gauge.SetNum(
					BattleState.MaxGauge.Num());

			for (int j = 0; j < BattleState.MaxGauge.Num(); j++)
			{
				BattleHudActor->BottomWidget->P1Gauge[j] = static_cast<float>(BattleState.GaugeP1[j]) / BattleState.
					MaxGauge[j];
				BattleHudActor->BottomWidget->P2Gauge[j] = static_cast<float>(BattleState.GaugeP2[j]) / BattleState.
					MaxGauge[j];
			}
		}
		UpdateHUD_BP();
	}

	UpdateHUDAnimations_BP();

	BattleHudActor->TopWidget->PlayStandardAnimations();
	BattleHudActor->TopWidget->SetAnimationRollbackData();
	BattleHudActor->BottomWidget->PlayStandardAnimations();
	BattleHudActor->BottomWidget->SetAnimationRollbackData();

	for (auto& [Anim, Time, bPlaying] : BattleHudActor->TopWidget->WidgetAnimationRollback)
	{
		bPlaying = BattleHudActor->TopWidget->IsAnimationPlaying(Anim);
		Time = BattleHudActor->TopWidget->GetAnimationCurrentTime(Anim);
	}

	for (auto& [Anim, Time, bPlaying] : BattleHudActor->BottomWidget->WidgetAnimationRollback)
	{
		bPlaying = BattleHudActor->BottomWidget->IsAnimationPlaying(Anim);
		Time = BattleHudActor->BottomWidget->GetAnimationCurrentTime(Anim);
	}
}

void ANightSkyGameState::SetDrawPriorityFront(ABattleObject* InObject) const
{
	if (InObject->IsPlayer)
	{
		for (int i = 0; i < Players.Num(); i++)
		{
			if (SortedObjects[i] == InObject) continue;
			if (SortedObjects[i]->DrawPriority <= InObject->DrawPriority)
				SortedObjects[i]->DrawPriority++;
			SortedObjects[i]->DrawPriority = FMath::Min(SortedObjects[i]->DrawPriority, MaxDrawPriority);
		}
		InObject->DrawPriority = 0;
		return;
	}
	for (int i = Players.Num(); i < BattleState.ActiveObjectCount; i++)
	{
		if (SortedObjects[i] == InObject) continue;
		if (SortedObjects[i]->DrawPriority <= InObject->DrawPriority)
			SortedObjects[i]->DrawPriority++;
		SortedObjects[i]->DrawPriority = FMath::Min(SortedObjects[i]->DrawPriority, MaxDrawPriority);
	}
	InObject->DrawPriority = 0;
}

void ANightSkyGameState::SetDrawPriorityBack(ABattleObject* InObject) const
{
	if (InObject->IsPlayer)
	{
		for (int i = 0; i < Players.Num(); i++)
		{
			if (SortedObjects[i] == InObject) continue;
			if (SortedObjects[i]->DrawPriority > InObject->DrawPriority)
				SortedObjects[i]->DrawPriority--;
		}
		InObject->DrawPriority = Players.Num();
		return;
	}
	for (int i = Players.Num(); i < BattleState.ActiveObjectCount; i++)
	{
		if (SortedObjects[i] == InObject) continue;
		if (SortedObjects[i]->DrawPriority > InObject->DrawPriority)
			SortedObjects[i]->DrawPriority--;
		InObject->DrawPriority = Players.Num();
	}
}

APlayerObject* ANightSkyGameState::SwitchMainPlayer(APlayerObject* InPlayer, const int TeamIndex, const bool bForce,
                                                    const bool bEvenOnScreen)
{
	if (TeamIndex == 0) return nullptr;
	const bool IsP1 = InPlayer->PlayerIndex == 0;
	if (BattleState.BattleFormat != EBattleFormat::Tag && TeamIndex >= BattleState.TeamData[IsP1 == false].TeamCount)
		return nullptr;
	// Madrone change (agent C): a Tag battle can have a one-fighter side now (CHR_ DefaultTagPartner fills only one side);
	// its tag out state asked for team index 1 and crashed on CooldownTimer / GetTeam out of bounds.
	if (TeamIndex >= BattleState.TeamData[IsP1 == false].TeamCount || TeamIndex >= GetTeam(IsP1).Num()
		|| !BattleState.TeamData[IsP1 == false].CooldownTimer.IsValidIndex(TeamIndex))
		return nullptr;
	if (BattleState.TeamData[IsP1 == false].CooldownTimer[TeamIndex] > 0 && !bForce) return nullptr;

	// Madrone change: only the fighter on screen can switch out. A tag out state calls this every frame it runs, and the
	// next frame the one who just left (now team index 1) found itself at index 1 and made itself main again, off screen.
	if (InPlayer != GetMainPlayer(IsP1)) return nullptr;
	const auto NewPlayer = GetTeam(IsP1)[TeamIndex];
	if (NewPlayer == InPlayer) return nullptr;
	if (NewPlayer->CurrentHealth == 0) return nullptr;
	if (NewPlayer->PlayerFlags & PLF_IsOnScreen)
	{
		if (!bEvenOnScreen) return nullptr;
	}
	// Madrone change: character extensions may refuse a tag (not the forced switch after a KO)
	if (!bForce && !CanTagByExtensions(InPlayer, NewPlayer)) return nullptr;
	NewPlayer->TeamIndex = 0;
	InPlayer->TeamIndex = TeamIndex;
	BattleState.ScreenData.TargetObjects.Remove(InPlayer);
	BattleState.ScreenData.TargetObjects.Add(NewPlayer);
	NewPlayer->JumpToStatePrimary(State_Universal_TagIn);
	if (!(NewPlayer->PlayerFlags & PLF_IsOnScreen))
	{
		// Madrone change: comes in from the edge of the screen behind the fighter leaving (was: on top of it), facing the
		// same way; the tag in state carries it forward.
		// The side away from the opponent (not the leaving fighter's facing: it can face away after a cross-up).
		const FScreenData& Screen = BattleState.ScreenData;
		const APlayerObject* Opponent = GetMainPlayer(!IsP1);
		const bool bFromLeft = Opponent && Opponent->PosX != InPlayer->PosX ? InPlayer->PosX < Opponent->PosX : InPlayer->Direction == DIR_Right;
		NewPlayer->PosX = bFromLeft ? Screen.ScreenBoundsLeft * 1000 : Screen.ScreenBoundsRight * 1000;
		NewPlayer->PosY = InPlayer->PosY;
		NewPlayer->Direction = InPlayer->Direction;
	}
	// Madrone change: the one who left can't be tagged back in for TagCooldown frames (nothing set a tag cooldown before)
	{
		int32& Cooldown = BattleState.TeamData[IsP1 == false].CooldownTimer[TeamIndex];
		Cooldown = ModifyCooldownByExtensions(InPlayer, FMath::Max(Cooldown, BattleState.TagCooldown)); // Madrone change: extensions
	}
	NewPlayer->SetOnScreen(true);
	NewPlayer->CalculatePushbox();
	NewPlayer->ComboCounter = InPlayer->ComboCounter;
	NewPlayer->ComboTimer = InPlayer->ComboTimer;
	NewPlayer->StoredInputBuffer = InPlayer->StoredInputBuffer;
	for (const auto EnemyPlayer : GetTeam(!IsP1))
	{
		EnemyPlayer->Enemy = NewPlayer;
	}
	// Madrone change: and the other side's dedicated assist, which kept targeting the fighter who tagged out
	if (APlayerObject* EnemyAssist = IsP1 ? P2Assist.Get() : P1Assist.Get(); IsValid(EnemyAssist)) EnemyAssist->Enemy = NewPlayer;
	BattleState.MainPlayer[!IsP1] = NewPlayer;
	SetDrawPriorityFront(NewPlayer);
	BattleHudActor->TopWidget->PlayTagAnim(IsP1);
	BattleHudActor->TopWidget->SetMainPlayerIndex(IsP1, NewPlayer->PlayerIndex);
	// Madrone change: character extensions and interactions
	for (UNSECharacterExtension* Extension : InPlayer->CharacterExtensions) if (Extension) Extension->OnTaggedOut(NewPlayer);
	for (UNSECharacterExtension* Extension : NewPlayer->CharacterExtensions) if (Extension) Extension->OnTaggedIn(InPlayer);
	QueueInteractionTrigger(ENSEInteractionTrigger::TagIn);
	return NewPlayer;
}

APlayerObject* ANightSkyGameState::CallAssist(const bool IsP1, const int AssistIndex, const FGameplayTag AssistName)
{
	APlayerObject* NewPlayer;
	// Madrone change: index 0 calls the side's dedicated assist (P1Assist / P2Assist, outside the team: WT battles) in
	// every format, not only NickTour; it has its own cooldown.
	if (BattleState.BattleFormat == EBattleFormat::NickTour || (AssistIndex == 0 && (IsP1 ? P1Assist : P2Assist) != nullptr))
	{
		if (IsP1)
		{
			NewPlayer = P1Assist;	
		}
		else
		{
			NewPlayer = P2Assist;	
		}
		if (NewPlayer == nullptr) return nullptr;
		if (BattleState.AssistCooldownTimer[IsP1 ? 0 : 1] > 0) return nullptr;
	}
	else
	{
		if (AssistIndex == 0) return nullptr;
		if (BattleState.BattleFormat != EBattleFormat::Tag && AssistIndex >= BattleState.TeamData[IsP1 == false].TeamCount)
			return nullptr;
		// Madrone change (agent C): one-fighter side in a Tag battle (see SwitchMainPlayer)
		if (AssistIndex >= BattleState.TeamData[IsP1 == false].TeamCount || AssistIndex >= GetTeam(IsP1).Num()
			|| !BattleState.TeamData[IsP1 == 0].CooldownTimer.IsValidIndex(AssistIndex))
			return nullptr;
		if (BattleState.TeamData[IsP1 == 0].CooldownTimer[AssistIndex] > 0) return nullptr;

		NewPlayer = GetTeam(IsP1)[AssistIndex];
	}
	
	
	if (NewPlayer->CurrentHealth == 0) return nullptr;
	if (NewPlayer->PlayerFlags & PLF_IsOnScreen) return nullptr;
	if (!NewPlayer->PrimaryStateMachine.StateNames.Contains(AssistName)) return nullptr;
	if (!CanCallAssistByExtensions(GetMainPlayer(IsP1), NewPlayer)) return nullptr; // Madrone change
	

	NewPlayer->Enemy = GetTeam(!IsP1)[0];

	NewPlayer->SetOnScreen(true);
	NewPlayer->PosX = GetMainPlayer(IsP1)->PosX;
	NewPlayer->FaceOpponent();
	NewPlayer->JumpToStatePrimary(AssistName);
	NewPlayer->IsActive = true;
	SetDrawPriorityBack(NewPlayer);
	//Zoom out screen
	//BattleState.ScreenData.TargetObjects.Add(NewPlayer);
	BattleState.ScreenData.DefaultMaxWidth += 150;
	BattleState.ScreenData.DefaultWidth += 150;
	
	BattleHudActor->TopWidget->PlayAssistAnim(IsP1);
	// Madrone change: character extensions and interactions
	{
		APlayerObject* Caller = GetMainPlayer(IsP1);
		if (Caller) for (UNSECharacterExtension* Extension : Caller->CharacterExtensions) if (Extension) Extension->OnAssistCalled(Caller, NewPlayer);
		if (NewPlayer != Caller) for (UNSECharacterExtension* Extension : NewPlayer->CharacterExtensions) if (Extension) Extension->OnAssistCalled(Caller, NewPlayer);
		QueueInteractionTrigger(ENSEInteractionTrigger::AssistCall);
	}
	return NewPlayer;
}

void ANightSkyGameState::SetTeamCooldown(const bool IsP1, const int TeamIndex, const int Cooldown)
{
	// Madrone change: the teammate's character extensions may change it
	const TArray<APlayerObject*> Team = GetTeam(IsP1);
	BattleState.TeamData[IsP1 == 0].CooldownTimer[TeamIndex] = Team.IsValidIndex(TeamIndex) ? ModifyCooldownByExtensions(Team[TeamIndex], Cooldown) : Cooldown;
}

bool ANightSkyGameState::CanTag(const APlayerObject* InPlayer, int TeamIndex) const
{
	if (InPlayer->PlayerFlags & PLF_RoundWinInputLock) return false;

	if (TeamIndex == 0) return false;
	const bool IsP1 = InPlayer->PlayerIndex == 0;
	if (BattleState.BattleFormat != EBattleFormat::Tag || TeamIndex >= BattleState.TeamData[IsP1 == false].TeamCount)
		return false;

	if (BattleState.TeamData[IsP1 == 0].CooldownTimer[TeamIndex] > 0) return false;

	const auto NewPlayer = GetTeam(IsP1)[TeamIndex];
	if (NewPlayer->CurrentHealth == 0) return false;
	if (NewPlayer->PlayerFlags & PLF_IsOnScreen) return false;
	if (!CanTagByExtensions(InPlayer, NewPlayer)) return false; // Madrone change

	return true;
}

int ANightSkyGameState::GetLocalInputs(int Index) const
{
	// Any controller with a battle input component works (ANightSkyPlayerController always has one).
	if (const UNightSkyBattleInputComponent* BattleInput = UNightSkyBattleInputComponent::FindOn(
		UGameplayStatics::GetPlayerController(GetWorld(), Index)))
	{
		return BattleInput->Inputs;
	}
	return 0;
}

void ANightSkyGameState::SetOtherChecksum(uint32 RemoteChecksum, int32 InFrame)
{
	OtherChecksum = RemoteChecksum;
	OtherChecksumFrame = InFrame;
}

void ANightSkyGameState::PlayAnnouncerVoice(const FGameplayTag Name)
{
	if (!IsValid(GameInstance))
		return;
	if (GameInstance->AnnouncerData != nullptr)
	{
		for (FSoundStruct SoundStruct : GameInstance->AnnouncerData->SoundDatas)
		{
			if (SoundStruct.Name == Name)
			{
				PlayAnnouncerVoice(SoundStruct.SoundWave, SoundStruct.MaxDuration);
				break;
			}
		}
	}
}

TArray<APlayerObject*> ANightSkyGameState::GetTeam(bool IsP1) const
{
	if (IsP1)
	{
		TArray<APlayerObject*> PlayerObjects;
		for (int i = 0; i < BattleState.TeamData[0].TeamCount; i++)
		{
			PlayerObjects.Add(Players[i]);
			for (int j = 0; j < PlayerObjects.Num() - 1; j++)
			{
				if (PlayerObjects[i]->TeamIndex < PlayerObjects[j]->TeamIndex)
				{
					PlayerObjects.Swap(i, j);
				}
			}
		}
		return PlayerObjects;
	}
	TArray<APlayerObject*> PlayerObjects;
	for (int i = BattleState.TeamData[0].TeamCount; i < Players.Num(); i++)
	{
		PlayerObjects.Add(Players[i]);
		for (int j = 0; j < PlayerObjects.Num() - 1; j++)
		{
			if (PlayerObjects[i - BattleState.TeamData[0].TeamCount]->TeamIndex < PlayerObjects[j]->TeamIndex)
			{
				PlayerObjects.Swap(i - BattleState.TeamData[0].TeamCount, j);
			}
		}
	}
	return PlayerObjects;
}

APlayerObject* ANightSkyGameState::GetMainPlayer(bool IsP1) const
{
	if (IsP1) return BattleState.MainPlayer[0];
	return BattleState.MainPlayer[1];
}

void ANightSkyGameState::CallBattleExtension(FGameplayTag Name)
{
	if (BattleExtensionNames.Find(FGameplayTag(Name)) != INDEX_NONE)
	{
		BattleExtensions[BattleExtensionNames.Find(FGameplayTag(Name))]->Exec();
	}
}

int32 ANightSkyGameState::GetGauge(bool IsP1, int32 GaugeIndex) const
{
	if (GaugeIndex < BattleState.MaxGauge.Num())
	{
		if (IsP1) return BattleState.GaugeP1[GaugeIndex];
		return BattleState.GaugeP2[GaugeIndex];
	}

	return -1;
}

void ANightSkyGameState::SetGauge(bool IsP1, int32 GaugeIndex, int32 Value)
{
	if (GaugeIndex < BattleState.MaxGauge.Num())
	{
		if (IsP1) BattleState.GaugeP1[GaugeIndex] = Value;
		else BattleState.GaugeP2[GaugeIndex] = Value;
	}
}

void ANightSkyGameState::UseGauge(bool IsP1, int32 GaugeIndex, int32 Value)
{
	if (GaugeIndex < BattleState.MaxGauge.Num())
	{
		if (IsP1) BattleState.GaugeP1[GaugeIndex] -= Value;
		else BattleState.GaugeP2[GaugeIndex] -= Value;
	}
}

EScreenFlag ANightSkyGameState::GetScreenFlags() const
{
	return BattleState.ScreenData.Flags;
}

void ANightSkyGameState::SetScreenFlags(int32 InFlags)
{
	BattleState.ScreenData.Flags = (EScreenFlag)InFlags;
}

bool ANightSkyGameState::IsTagBattle() const
{
	return BattleState.BattleFormat == EBattleFormat::Tag;
}

int32 ANightSkyGameState::GetTeamCount(const bool bIsP1) const
{
	return BattleState.TeamData[bIsP1 == false].TeamCount;
}

bool ANightSkyGameState::GetPaused() const
{
	return bPauseGame;
}

void ANightSkyGameState::SetPaused(bool bPause)
{
	bPauseGame = bPause;
	if (GameInstance->FighterRunner == LocalPlay)
	{
		UGameplayStatics::SetGamePaused(this, bPause);
	}
}

void ANightSkyGameState::ScreenPosToWorldPos(const int32 X, const int32 Y, int32& OutX, int32& OutY) const
{
	const auto ScreenYSize = BattleState.ScreenData.FinalScreenWidth * 720 / 1280;

	OutX = BattleState.ScreenData.FinalScreenX * 1000 + BattleState.ScreenData.FinalScreenWidth * X;
	OutY = BattleState.ScreenData.FinalScreenY * 1000 + ScreenYSize * Y;
}

void ANightSkyGameState::WorldPosToScreenPos(int32 X, int32 Y, int32& OutX, int32& OutY) const
{
	const auto ScreenYSize = BattleState.ScreenData.FinalScreenWidth * 720 / 1280;

	OutX = (X - BattleState.ScreenData.FinalScreenX * 1000) * 20 / (BattleState.ScreenData.FinalScreenWidth * 9);
	OutY = (Y - BattleState.ScreenData.FinalScreenY * 1000) * 100 / (ScreenYSize * 72);
}

void ANightSkyGameState::BattleHudVisibility(bool Visible)
{
	BattleState.bHUDVisible = Visible;
}

void ANightSkyGameState::PlayCommonAudio(USoundBase* InSoundWave, float MaxDuration)
{
	for (int i = 0; i < CommonAudioChannelCount; i++)
	{
		if (BattleState.CommonAudioChannels[i].Finished)
		{
			BattleState.CommonAudioChannels[i].SoundWave = InSoundWave;
			BattleState.CommonAudioChannels[i].StartingFrame = BattleState.FrameNumber;
			BattleState.CommonAudioChannels[i].MaxDuration = MaxDuration;
			BattleState.CommonAudioChannels[i].Finished = false;
			if (!bIsResimulating)
			{
				AudioManager->CommonAudioPlayers[i]->SetSound(InSoundWave);
				AudioManager->CommonAudioPlayers[i]->Play();
			}
			return;
		}
	}
}

void ANightSkyGameState::PlayCharaAudio(USoundBase* InSoundWave, float MaxDuration)
{
	for (int i = 0; i < CharaAudioChannelCount; i++)
	{
		if (BattleState.CharaAudioChannels[i].Finished)
		{
			BattleState.CharaAudioChannels[i].SoundWave = InSoundWave;
			BattleState.CharaAudioChannels[i].StartingFrame = BattleState.FrameNumber;
			BattleState.CharaAudioChannels[i].MaxDuration = MaxDuration;
			BattleState.CharaAudioChannels[i].Finished = false;
			if (!bIsResimulating)
			{
				AudioManager->CharaAudioPlayers[i]->SetSound(InSoundWave);
				AudioManager->CharaAudioPlayers[i]->Play();
			}
			return;
		}
	}
}

void ANightSkyGameState::PlayVoiceLine(USoundBase* InSoundWave, float MaxDuration, int Player)
{
	BattleState.CharaVoiceChannels[Player].SoundWave = InSoundWave;
	BattleState.CharaVoiceChannels[Player].StartingFrame = BattleState.FrameNumber;
	BattleState.CharaVoiceChannels[Player].MaxDuration = MaxDuration;
	BattleState.CharaVoiceChannels[Player].Finished = false;
	if (!bIsResimulating)
	{
		AudioManager->CharaVoicePlayers[Player]->SetSound(InSoundWave);
		AudioManager->CharaVoicePlayers[Player]->Play();
	}
}

void ANightSkyGameState::PlayAnnouncerVoice(USoundBase* InSoundWave, float MaxDuration)
{
	BattleState.AnnouncerVoiceChannel.SoundWave = InSoundWave;
	BattleState.AnnouncerVoiceChannel.StartingFrame = BattleState.FrameNumber;
	BattleState.AnnouncerVoiceChannel.MaxDuration = MaxDuration;
	BattleState.AnnouncerVoiceChannel.Finished = false;
	if (!bIsResimulating)
	{
		AudioManager->AnnouncerVoicePlayer->SetSound(InSoundWave);
		AudioManager->AnnouncerVoicePlayer->Play();
	}
}

void ANightSkyGameState::PlayMusic(const FGameplayTag Name)
{
	if (!IsValid(GameInstance))
		return;
	if (GameInstance->MusicData != nullptr)
	{
		for (FSoundStruct SoundStruct : GameInstance->MusicData->SoundDatas)
		{
			if (SoundStruct.Name == Name)
			{
				PlayMusic(SoundStruct.SoundWave, SoundStruct.MaxDuration);
				break;
			}
		}
	}
}

void ANightSkyGameState::PlayMusic(USoundBase* InSoundWave, float MaxDuration)
{
	BattleState.MusicChannel.SoundWave = InSoundWave;
	BattleState.MusicChannel.StartingFrame = BattleState.FrameNumber;
	BattleState.MusicChannel.MaxDuration = MaxDuration;
	if (!bIsResimulating)
	{
		AudioManager->MusicPlayer->SetSound(InSoundWave);
		AudioManager->MusicPlayer->Play();
	}
}

void ANightSkyGameState::ManageAudio()
{
	for (int i = 0; i < CommonAudioChannelCount; i++)
	{
		const int CurrentAudioTime = BattleState.FrameNumber - BattleState.CommonAudioChannels[i].StartingFrame;
		if (!BattleState.CommonAudioChannels[i].Finished && static_cast<int>(BattleState.CommonAudioChannels[i].
			MaxDuration * 60) < CurrentAudioTime)
		{
			BattleState.CommonAudioChannels[i].Finished = true;
			BattleState.CommonAudioChannels[i].SoundWave = nullptr;
			if (bIsResimulating) continue;
			AudioManager->CommonAudioPlayers[i]->Stop();
			AudioManager->CommonAudioPlayers[i]->SetSound(nullptr);
		}
	}
	for (int i = 0; i < CharaAudioChannelCount; i++)
	{
		const int CurrentAudioTime = BattleState.FrameNumber - BattleState.CharaAudioChannels[i].StartingFrame;
		if (!BattleState.CharaAudioChannels[i].Finished && static_cast<int>(BattleState.CharaAudioChannels[i].
			MaxDuration * 60) < CurrentAudioTime)
		{
			BattleState.CharaAudioChannels[i].Finished = true;
			BattleState.CharaAudioChannels[i].SoundWave = nullptr;
			if (bIsResimulating) continue;
			AudioManager->CharaAudioPlayers[i]->Stop();
			AudioManager->CharaAudioPlayers[i]->SetSound(nullptr);
		}
	}
	for (int i = 0; i < CharaVoiceChannelCount; i++)
	{
		const int CurrentAudioTime = BattleState.FrameNumber - BattleState.CharaVoiceChannels[i].StartingFrame;
		if (!BattleState.CharaVoiceChannels[i].Finished && static_cast<int>(BattleState.CharaVoiceChannels[i].
			MaxDuration * 60) < CurrentAudioTime)
		{
			BattleState.CharaVoiceChannels[i].Finished = true;
			BattleState.CharaVoiceChannels[i].SoundWave = nullptr;
			if (bIsResimulating) continue;
			AudioManager->CharaVoicePlayers[i]->Stop();
			AudioManager->CharaVoicePlayers[i]->SetSound(nullptr);
		}
	}
	{
		const int CurrentAudioTime = BattleState.FrameNumber - BattleState.AnnouncerVoiceChannel.StartingFrame;
		if (!BattleState.AnnouncerVoiceChannel.Finished && static_cast<int>(BattleState.AnnouncerVoiceChannel.
			MaxDuration * 60) < CurrentAudioTime)
		{
			BattleState.AnnouncerVoiceChannel.Finished = true;
			BattleState.AnnouncerVoiceChannel.SoundWave = nullptr;
			if (!bIsResimulating)
			{
				AudioManager->AnnouncerVoicePlayer->Stop();
				AudioManager->AnnouncerVoicePlayer->SetSound(nullptr);
			}
		}
	}
	{
		const int CurrentAudioTime = BattleState.FrameNumber - BattleState.MusicChannel.StartingFrame;
		if (static_cast<int>(BattleState.MusicChannel.MaxDuration * 60) < CurrentAudioTime)
		{
			PlayMusic(BattleState.MusicChannel.SoundWave, BattleState.MusicChannel.MaxDuration);
		}
	}
}

void ANightSkyGameState::RollbackStartAudio(int32 InFrame)
{
	ManageAudio();

	for (int i = 0; i < CommonAudioChannelCount; i++)
	{
		if (BattleState.CommonAudioChannels[i].Finished) continue;
		if (BattleState.CommonAudioChannels[i].SoundWave == AudioManager->CommonAudioPlayers[i]->GetSound()) continue;

		AudioManager->CommonAudioPlayers[i]->Stop();
		AudioManager->CommonAudioPlayers[i]->SetSound(BattleState.CommonAudioChannels[i].SoundWave);
		const float CurrentAudioTime = static_cast<float>(InFrame - BattleState.CommonAudioChannels[i].StartingFrame) /
			60.f;
		if (BattleState.CommonAudioChannels[i].MaxDuration > CurrentAudioTime)
			AudioManager->CommonAudioPlayers[i]->Play(CurrentAudioTime);
	}
	for (int i = 0; i < CharaAudioChannelCount; i++)
	{
		if (BattleState.CharaAudioChannels[i].Finished) continue;
		if (BattleState.CharaAudioChannels[i].SoundWave == AudioManager->CharaAudioPlayers[i]->GetSound()) continue;

		AudioManager->CharaAudioPlayers[i]->Stop();
		AudioManager->CharaAudioPlayers[i]->SetSound(BattleState.CharaAudioChannels[i].SoundWave);
		const float CurrentAudioTime = static_cast<float>(InFrame - BattleState.CharaAudioChannels[i].StartingFrame) /
			60.f;
		if (BattleState.CharaAudioChannels[i].MaxDuration > CurrentAudioTime)
			AudioManager->CharaAudioPlayers[i]->Play(CurrentAudioTime);
	}
	for (int i = 0; i < CharaVoiceChannelCount; i++)
	{
		if (BattleState.CharaVoiceChannels[i].Finished) continue;
		if (BattleState.CharaVoiceChannels[i].SoundWave == AudioManager->CharaVoicePlayers[i]->GetSound()) continue;

		AudioManager->CharaVoicePlayers[i]->Stop();
		AudioManager->CharaVoicePlayers[i]->SetSound(BattleState.CharaVoiceChannels[i].SoundWave);
		const float CurrentAudioTime = static_cast<float>(InFrame - BattleState.CharaVoiceChannels[i].StartingFrame) /
			60.f;
		if (BattleState.CharaVoiceChannels[i].MaxDuration > CurrentAudioTime)
			AudioManager->CharaVoicePlayers[i]->Play(CurrentAudioTime);
	}
	if (!BattleState.AnnouncerVoiceChannel.Finished && BattleState.AnnouncerVoiceChannel.SoundWave != AudioManager->
		AnnouncerVoicePlayer->GetSound())
	{
		AudioManager->AnnouncerVoicePlayer->Stop();
		AudioManager->AnnouncerVoicePlayer->SetSound(BattleState.AnnouncerVoiceChannel.SoundWave);
		const float CurrentAudioTime = static_cast<float>(InFrame - BattleState.AnnouncerVoiceChannel.StartingFrame) /
			60.f;
		if (BattleState.AnnouncerVoiceChannel.MaxDuration > CurrentAudioTime)
			AudioManager->AnnouncerVoicePlayer->Play(CurrentAudioTime);
	}
	if (BattleState.MusicChannel.SoundWave != AudioManager->MusicPlayer->GetSound())
	{
		AudioManager->MusicPlayer->Stop();
		AudioManager->MusicPlayer->SetSound(BattleState.MusicChannel.SoundWave);
		const float CurrentAudioTime = static_cast<float>(InFrame - BattleState.MusicChannel.StartingFrame) / 60.f;
		if (BattleState.MusicChannel.MaxDuration > CurrentAudioTime)
			AudioManager->MusicPlayer->Play(CurrentAudioTime);
	}
}

void ANightSkyGameState::SaveGameState(TArray<uint8>& RollbackBuffer, int32* InChecksum)
{
	{
		TArray<uint8> BattleStateBuffer;
		BattleStateBuffer.AddUninitialized(SizeOfBattleState);
		FMemory::Memcpy(BattleStateBuffer.GetData(), &BattleState.BattleStateSync, SizeOfBattleState);
		RollbackBuffer.Append(BattleStateBuffer);
	}
	
	{
		TArray<uint8> BattleStateDataBuffer;
		SaveForRollback(BattleStateDataBuffer);
		RollbackBuffer.Append(BattleStateDataBuffer);
	}

	for (int i = 0; i < BattleExtensions.Num(); i++)
	{
		TArray<uint8> ExtensionBuffer;
		BattleExtensions[i]->SaveForRollback(ExtensionBuffer);
		RollbackBuffer.Append(ExtensionBuffer);
	}
	// Madrone change: character interaction events
	for (UNSECharacterInteractionEvent* Event : InteractionEvents)
	{
		TArray<uint8> EventBuffer;
		Event->SaveForRollback(EventBuffer);
		RollbackBuffer.Append(EventBuffer);
	}

	for (int i = 0; i < MaxBattleObjects; i++)
	{
		if (Objects[i]->IsActive)
		{
			RollbackBuffer.Add(true);
		
			{
				TArray<uint8> ObjBuffer;
				ObjBuffer.AddUninitialized(SizeOfBattleObject);
				Objects[i]->SaveForRollback(ObjBuffer.GetData());
				RollbackBuffer.Append(ObjBuffer);
			}
			
			{
				TArray<uint8> StateBuffer;
				Objects[i]->ObjectState->SaveForRollback(StateBuffer);
				RollbackBuffer.Append(StateBuffer);
			}

			// Madrone change: the object's SaveGame properties, as SaveForRollbackBP saves a player's. They weren't saved for
			// objects, so ObjectsToIgnoreHitsFrom, Boxes, LinkedActor, AnimStructs... kept a later frame's values after a
			// rollback load: an object hit just before the rollback ignored the same hit when the frame was re-run.
			{
				TArray<uint8> PropertyBuffer;
				UNightSkyBlueprintFunctionLibrary::SerializeBin(Objects[i], PropertyBuffer);
				RollbackBuffer.Append(PropertyBuffer);
			}
		}
		else
		{
			RollbackBuffer.Add(false);
		}
	}

	// Madrone change: the dedicated assists too (they're not in Players): their state, position and IsActive were never
	// restored, so after a rollback an assist kept a later frame's state while the battle state (its cooldown) went back.
	for (APlayerObject* Player : GetRollbackPlayers())
	{
		{
			TArray<uint8> ObjBuffer;
			ObjBuffer.AddUninitialized(SizeOfBattleObject);
			Player->SaveForRollback(ObjBuffer.GetData());
			RollbackBuffer.Append(ObjBuffer);
		}
		
		{
			TArray<uint8> CharBuffer;
			CharBuffer.AddUninitialized(SizeOfPlayerObject);
			Player->SaveForRollbackPlayer(CharBuffer.GetData());
			RollbackBuffer.Append(CharBuffer);
		}
		
		{
			TArray<uint8> CharBPBuffer;
			Player->SaveForRollbackBP(CharBPBuffer);
			RollbackBuffer.Append(CharBPBuffer);
		}
		
		{
			TArray<uint8> StateBuffer;
			Player->PrimaryStateMachine.CurrentState->SaveForRollback(StateBuffer);
			RollbackBuffer.Append(StateBuffer);
		}
		// Madrone change: the fighter's character extensions
		for (UNSECharacterExtension* Extension : Player->CharacterExtensions)
		{
			TArray<uint8> ExtensionBuffer;
			Extension->SaveForRollback(ExtensionBuffer);
			RollbackBuffer.Append(ExtensionBuffer);
		}
	}
	
	{
		TArray<uint8> TopWidgetBuffer;
		BattleHudActor->TopWidget->SaveForRollback(TopWidgetBuffer);
		RollbackBuffer.Append(TopWidgetBuffer);
	}
	
	{
		TArray<uint8> BottomWidget;
		BattleHudActor->BottomWidget->SaveForRollback(BottomWidget);
		RollbackBuffer.Append(BottomWidget);
	}

	*InChecksum = CreateChecksum();
}

void ANightSkyGameState::LoadGameState(const TArray<uint8>& RollbackBuffer)
{
	const int CurrentFrame = BattleState.FrameNumber;
	
	int64 DataIdx = 0;
	
	FMemory::Memcpy(&BattleState.BattleStateSync, RollbackBuffer.GetData(), SizeOfBattleState);
	DataIdx += SizeOfBattleState;
	
	auto GetRollbackBufferView = [RollbackBuffer](int64 DataIdx)
	{
		return MakeArrayView(RollbackBuffer.GetData() + DataIdx, RollbackBuffer.Num() - DataIdx);
	};
	
	DataIdx += LoadForRollback(GetRollbackBufferView(DataIdx));
	
	for (int i = 0; i < BattleExtensions.Num(); i++)
	{
		DataIdx += BattleExtensions[i]->LoadForRollback(GetRollbackBufferView(DataIdx));
	}
	for (UNSECharacterInteractionEvent* Event : InteractionEvents) // Madrone change
	{
		DataIdx += Event->LoadForRollback(GetRollbackBufferView(DataIdx));
	}
	for (int i = 0; i < MaxBattleObjects; i++)
	{
		if (RollbackBuffer[DataIdx])
		{
			DataIdx++;
			Objects[i]->LoadForRollback(GetRollbackBufferView(DataIdx).GetData());
			DataIdx += SizeOfBattleObject;
			DataIdx += Objects[i]->ObjectState->LoadForRollback(GetRollbackBufferView(DataIdx));
			DataIdx += UNightSkyBlueprintFunctionLibrary::DeserializeBin(Objects[i], GetRollbackBufferView(DataIdx)); // Madrone change
		}
		else
		{
			DataIdx++;
			if (Objects[i]->IsActive)
				Objects[i]->ResetObject();
		}
	}
	for (APlayerObject* Player : GetRollbackPlayers()) // Madrone change: with the assists
	{
		Player->LoadForRollback(GetRollbackBufferView(DataIdx).GetData());
		DataIdx += SizeOfBattleObject;
		Player->LoadForRollbackPlayer(GetRollbackBufferView(DataIdx).GetData());
		DataIdx += SizeOfPlayerObject;
		DataIdx += Player->LoadForRollbackBP(GetRollbackBufferView(DataIdx));
		DataIdx += Player->PrimaryStateMachine.CurrentState->LoadForRollback(GetRollbackBufferView(DataIdx));
		for (UNSECharacterExtension* Extension : Player->CharacterExtensions) // Madrone change
		{
			DataIdx += Extension->LoadForRollback(GetRollbackBufferView(DataIdx));
		}
	}
	
	SortObjects();
	ParticleManager->RollbackParticles(CurrentFrame - BattleState.FrameNumber);

	DataIdx += BattleHudActor->TopWidget->LoadForRollback(GetRollbackBufferView(DataIdx));
	DataIdx += BattleHudActor->BottomWidget->LoadForRollback(GetRollbackBufferView(DataIdx));
	
	checkf(DataIdx == RollbackBuffer.Num(), TEXT("Rollback buffer not fully consumed!"))

	BattleHudActor->TopWidget->RollbackAnimations();
	BattleHudActor->BottomWidget->RollbackAnimations();

	if (!FighterRunner->IsA(AFighterSynctestRunner::StaticClass()))
		GameInstance->RollbackReplay(CurrentFrame - BattleState.FrameNumber);
}

TArray<APlayerObject*, TInlineAllocator<8>> ANightSkyGameState::GetRollbackPlayers() const
{
	TArray<APlayerObject*, TInlineAllocator<8>> Result(Players);
	// The assists are set once per battle (Init) and only cleared after it, so the list (and the rollback buffer's
	// layout) doesn't change during one. Their actors live until the overworld destroys them after the battle.
	for (APlayerObject* Assist : {P1Assist.Get(), P2Assist.Get()})
	{
		if (Assist && !Result.Contains(Assist)) Result.Add(Assist);
	}
	return Result;
}

void ANightSkyGameState::SaveForRollback(TArray<uint8>& Data)
{
	UNightSkyBlueprintFunctionLibrary::SerializeBin(this, Data);
}

int64 ANightSkyGameState::LoadForRollback(const TArrayView<const uint8>& InBytes)
{
	return UNightSkyBlueprintFunctionLibrary::DeserializeBin(this, InBytes);
}

void ANightSkyGameState::EndMatch()
{
	EndMatch_BP();
	BattleState.BattlePhase = EBattlePhase::EndScreen;
}

TArray<FText> ANightSkyGameState::GetPlayerNames()
{
	TArray<FText> PlayerNames;
	
	for (int i = 0; i < Players.Num(); i++)
	 {
	 	PlayerNames.Add(Players[i]->CharacterName);
	 }
	return PlayerNames;
}


// ------------------------------------------------------------------------------------ Madrone change: character systems
// Character extensions (per fighter) and character interactions (pairs of characters on screen together). Docs:
// Plugins/NightSkyEngine/Docs/10-Character-Systems.md, WitchTools Docs/CharacterInteractions.md.

static bool SamePair(const FNSECharacterInteractionPair& A, const FNSECharacterInteractionPair& B)
{
	return A.CharacterA == B.CharacterA && A.CharacterB == B.CharacterB && A.Event == B.Event;
}

void ANightSkyGameState::InitCharacterSystems()
{
	for (UNSECharacterInteractionEvent* Event : InteractionEvents)
	{
		if (Event && Event->bActive) Event->StopEvent(true);
	}
	InteractionEvents.Empty();
	InteractionPairs.Empty();
	InteractionPairsByCharacter.Empty();
	PendingInteractionTriggers = 0;
	ShownSubtitleKeys.Empty();
	SubtitleLog.Empty();
	HideBattleSubtitle();

	const TArray<APlayerObject*, TInlineAllocator<8>> Fighters = GetRollbackPlayers();

	// Extensions: the CHR_ asset's, then the fighter Blueprint's, one instance per class.
	for (APlayerObject* Fighter : Fighters)
	{
		if (!IsValid(Fighter)) continue;
		Fighter->CharacterExtensions.Empty();
		TArray<TSubclassOf<UNSECharacterExtension>> Classes;
		if (Fighter->CharaData) for (const auto& Class : Fighter->CharaData->CharacterExtensions) Classes.AddUnique(Class);
		for (const auto& Class : Fighter->CharacterExtensionClasses) Classes.AddUnique(Class);
		for (const TSubclassOf<UNSECharacterExtension>& Class : Classes)
		{
			if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) continue;
			UNSECharacterExtension* Extension = NewObject<UNSECharacterExtension>(Fighter, Class);
			Extension->Owner = Fighter;
			Extension->GameState = this;
			Fighter->CharacterExtensions.Add(Extension);
		}
	}

	// Pairs: Battle Rules' sets, this game state's, then every fighter's CHR_ list (Character A empty = that character).
	auto AddPair = [this](FNSECharacterInteractionPair Pair, const FGameplayTag& Self)
	{
		if (!Pair.CharacterA.IsValid()) Pair.CharacterA = Self;
		if (!Pair.CharacterA.IsValid() || !Pair.CharacterB.IsValid() || !Pair.Event) return;
		if (InteractionPairs.ContainsByPredicate([&Pair](const FNSECharacterInteractionPair& Other) { return SamePair(Pair, Other); })) return;
		InteractionPairs.Add(Pair);
	};
	for (const TSoftObjectPtr<UNSECharacterInteractionData>& Set : GetDefault<UNightSkyBattleRules>()->InteractionSets)
	{
		if (const UNSECharacterInteractionData* Data = Set.LoadSynchronous())
			for (const auto& Pair : Data->Pairs) AddPair(Pair, FGameplayTag());
	}
	for (const UNSECharacterInteractionData* Data : CharacterInteractionData)
	{
		if (Data) for (const auto& Pair : Data->Pairs) AddPair(Pair, FGameplayTag());
	}
	for (const APlayerObject* Fighter : Fighters)
	{
		if (IsValid(Fighter) && Fighter->CharaData)
			for (const auto& Pair : Fighter->CharaData->CharacterInteractions) AddPair(Pair, Fighter->GetCharacterTag());
	}
	InteractionPairs.StableSort([](const FNSECharacterInteractionPair& A, const FNSECharacterInteractionPair& B) { return A.Priority < B.Priority; });
	for (int32 i = 0; i < InteractionPairs.Num(); i++)
	{
		InteractionPairsByCharacter.FindOrAdd(InteractionPairs[i].CharacterA).AddUnique(i);
		InteractionPairsByCharacter.FindOrAdd(InteractionPairs[i].CharacterB).AddUnique(i);
	}

	// Events: one per (subject fighter, partner fighter) that matches a pair, in pair then fighter order.
	auto SidesMatch = [](const FNSECharacterInteractionPair& Pair, const APlayerObject* A, const APlayerObject* B)
	{
		const bool bSameSide = A->PlayerIndex == B->PlayerIndex;
		return Pair.Sides == ENSEInteractionSides::Any || (Pair.Sides == ENSEInteractionSides::Teammates) == bSameSide;
	};
	auto AddEvents = [&](const FNSECharacterInteractionPair& Pair, const FGameplayTag& SubjectTag, const FGameplayTag& PartnerTag)
	{
		for (APlayerObject* Subject : Fighters)
		{
			if (!IsValid(Subject) || !Subject->GetCharacterTag().MatchesTagExact(SubjectTag)) continue;
			for (APlayerObject* Partner : Fighters)
			{
				if (!IsValid(Partner) || Partner == Subject || !Partner->GetCharacterTag().MatchesTagExact(PartnerTag)) continue;
				if (!SidesMatch(Pair, Subject, Partner)) continue;
				UNSECharacterInteractionEvent* Event = NewObject<UNSECharacterInteractionEvent>(this, Pair.Event);
				Event->GameState = this;
				Event->Subject = Subject;
				Event->Partner = Partner;
				Event->Pair = Pair;
				InteractionEvents.Add(Event);
				UE_LOG(LogNightSkyEngine, Log, TEXT("Character interaction: %s (%s) with %s (%s) -> %s"), *Subject->GetName(),
					*SubjectTag.ToString(), *Partner->GetName(), *PartnerTag.ToString(), *Pair.Event->GetName());
			}
		}
	};
	for (const FNSECharacterInteractionPair& Pair : InteractionPairs)
	{
		if (Pair.Event->HasAnyClassFlags(CLASS_Abstract)) continue;
		AddEvents(Pair, Pair.CharacterA, Pair.CharacterB);
		if (Pair.bSymmetric && Pair.CharacterA != Pair.CharacterB) AddEvents(Pair, Pair.CharacterB, Pair.CharacterA);
	}

	ForEachCharacterExtension([](UNSECharacterExtension* Extension) { Extension->MatchInit(); });
}

void ANightSkyGameState::RoundInitCharacterSystems()
{
	for (UNSECharacterInteractionEvent* Event : InteractionEvents)
	{
		if (Event) Event->ResetForRound();
	}
	ForEachCharacterExtension([](UNSECharacterExtension* Extension) { Extension->RoundInit(); });
	QueueInteractionTrigger(ENSEInteractionTrigger::RoundStart);
}

void ANightSkyGameState::UpdateCharacterSystems()
{
	ForEachCharacterExtension([](UNSECharacterExtension* Extension) { Extension->Update(); });

	// Running events first, so one started this frame gets its Begin now and its first Update next frame.
	for (UNSECharacterInteractionEvent* Event : InteractionEvents)
	{
		if (Event && Event->bActive) Event->StepEvent();
	}

	// Triggers wait until the fighters can move (after the intro and the round start countdown).
	if (BattleState.BattlePhase == EBattlePhase::Battle && BattleState.TimeUntilRoundStart <= 0)
	{
		const int32 Triggers = PendingInteractionTriggers | static_cast<int32>(ENSEInteractionTrigger::EveryFrame);
		PendingInteractionTriggers = 0;
		for (UNSECharacterInteractionEvent* Event : InteractionEvents)
		{
			if (!Event || Event->bActive) continue;
			const int32 Matching = Event->Pair.Triggers & Triggers;
			if (Matching == 0) continue;
			if (Event->Pair.Repeat == ENSEInteractionRepeat::OncePerRound && Event->TimesFiredThisRound > 0) continue;
			if (Event->Pair.Repeat == ENSEInteractionRepeat::OncePerMatch && Event->TimesFiredThisMatch > 0) continue;
			if (!UNSECharacterLibrary::IsFighterOnScreen(Event->Subject) || !UNSECharacterLibrary::IsFighterOnScreen(Event->Partner)) continue;
			if (!Event->CanStart()) continue;
			const ENSEInteractionTrigger Trigger = static_cast<ENSEInteractionTrigger>(Matching & -Matching); // lowest flag
			UE_LOG(LogNightSkyEngine, Log, TEXT("Character interaction starts (frame %d, round %d): %s, subject %s, partner %s"),
				BattleState.FrameNumber, BattleState.RoundCount, *Event->GetClass()->GetName(), *Event->Subject->GetName(), *Event->Partner->GetName());
			Event->StartEvent(Trigger);
		}
	}

	for (APlayerObject* Fighter : GetRollbackPlayers())
	{
		if (IsFighterHeld(Fighter)) Fighter->Inputs = INP_Neutral;
	}
}

void ANightSkyGameState::ForEachCharacterExtension(TFunctionRef<void(UNSECharacterExtension*)> Func) const
{
	for (const APlayerObject* Fighter : GetRollbackPlayers())
	{
		if (!IsValid(Fighter)) continue;
		for (UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
		{
			if (Extension) Func(Extension);
		}
	}
}

bool ANightSkyGameState::IsFighterHeld(const APlayerObject* Fighter) const
{
	if (!Fighter) return false;
	for (const UNSECharacterInteractionEvent* Event : InteractionEvents)
	{
		if (Event && Event->bActive && ((Event->bHoldSubject && Event->Subject == Fighter) || (Event->bHoldPartner && Event->Partner == Fighter))) return true;
	}
	return false;
}

FGameplayTag ANightSkyGameState::GetCPUForcedState(APlayerObject* Fighter) const
{
	if (!Fighter) return FGameplayTag();
	for (UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
	{
		if (!Extension) continue;
		const FGameplayTag State = Extension->GetCPUForcedState();
		if (State.IsValid()) return State;
	}
	return FGameplayTag();
}

void ANightSkyGameState::EnterWinState(const bool bIsP1, const FGameplayTag Default, const bool bMatchWin)
{
	APlayerObject* Winner = GetMainPlayer(bIsP1);
	APlayerObject* Loser = GetMainPlayer(!bIsP1);
	if (!Winner) return;
	// The winner's extensions first, then its teammates', then its side's assist.
	TArray<APlayerObject*, TInlineAllocator<8>> Side;
	Side.Add(Winner);
	for (APlayerObject* Member : GetTeam(bIsP1)) Side.AddUnique(Member);
	if (APlayerObject* Assist = bIsP1 ? P1Assist.Get() : P2Assist.Get(); IsValid(Assist)) Side.AddUnique(Assist);
	FGameplayTag State = Default;
	for (const APlayerObject* Fighter : Side)
	{
		bool bFound = false;
		for (const UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
		{
			if (!Extension) continue;
			const FGameplayTag Override = Extension->GetVictoryState(Winner, Loser, bMatchWin);
			if (Override.IsValid() && Winner->PrimaryStateMachine.StateNames.Contains(Override))
			{
				State = Override;
				bFound = true;
				break;
			}
		}
		if (bFound) break;
	}
	Winner->JumpToStatePrimary(State);
	ForEachCharacterExtension([Winner, Loser, bMatchWin](UNSECharacterExtension* Extension) { Extension->OnVictory(Winner, Loser, bMatchWin); });
}

bool ANightSkyGameState::IsVictorySequenceRunning() const
{
	bool bRunning = false;
	ForEachCharacterExtension([&bRunning](UNSECharacterExtension* Extension) { bRunning |= Extension->IsVictorySequenceRunning(); });
	return bRunning;
}

int32 ANightSkyGameState::ModifyCooldownByExtensions(const APlayerObject* Fighter, int32 Frames) const
{
	if (!Fighter) return Frames;
	for (const UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
	{
		if (Extension) Frames = Extension->ModifyCooldown(Frames);
	}
	return Frames;
}

bool ANightSkyGameState::CanTagByExtensions(const APlayerObject* Out, const APlayerObject* In) const
{
	if (Out)
	{
		for (const UNSECharacterExtension* Extension : Out->CharacterExtensions)
		{
			if (Extension && !Extension->CanTagOut(const_cast<APlayerObject*>(In))) return false;
		}
	}
	if (In)
	{
		for (const UNSECharacterExtension* Extension : In->CharacterExtensions)
		{
			if (Extension && !Extension->CanBeTaggedIn(const_cast<APlayerObject*>(Out))) return false;
		}
	}
	return true;
}

bool ANightSkyGameState::CanCallAssistByExtensions(const APlayerObject* Caller, const APlayerObject* Assist) const
{
	for (const APlayerObject* Fighter : {Caller, Assist})
	{
		if (!Fighter) continue;
		for (const UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
		{
			if (Extension && !Extension->CanCallAssist(const_cast<APlayerObject*>(Caller), const_cast<APlayerObject*>(Assist))) return false;
		}
	}
	return true;
}

void ANightSkyGameState::GatherTeammateBars(const bool bIsP1, TArray<FNSETeammateBar>& OutVirtual, TArray<FNSETeammateBar>& OutRealOverrides) const
{
	OutVirtual.Reset();
	OutRealOverrides.Reset();
	if (Players.IsEmpty()) return;
	const TArray<APlayerObject*> Team = GetTeam(bIsP1);
	for (int32 i = 1; i < Team.Num(); i++)
	{
		FNSETeammateBar& Bar = OutRealOverrides.AddDefaulted_GetRef();
		if (!Team[i]) continue;
		Bar.Name = Team[i]->CharacterName;
		for (const UNSECharacterExtension* Extension : Team[i]->CharacterExtensions)
		{
			FLinearColor Color;
			if (Extension && Extension->GetOwnBarColor(Color))
			{
				Bar.bOverrideColor = true;
				Bar.Color = Color;
				break;
			}
		}
	}
	TArray<const APlayerObject*, TInlineAllocator<8>> Side;
	for (const APlayerObject* Member : Team) Side.Add(Member);
	if (const APlayerObject* Assist = bIsP1 ? P1Assist.Get() : P2Assist.Get(); IsValid(Assist)) Side.AddUnique(Assist);
	for (const APlayerObject* Fighter : Side)
	{
		if (!Fighter) continue;
		for (const UNSECharacterExtension* Extension : Fighter->CharacterExtensions)
		{
			if (!Extension) continue;
			TArray<FNSETeammateBar> Bars;
			Extension->GetVirtualTeammateBars(Bars);
			OutVirtual.Append(Bars);
		}
	}
}

UNSEBattleSubtitleWidget* ANightSkyGameState::GetBattleSubtitleWidget()
{
	if (IsValid(BattleSubtitleWidget)) return BattleSubtitleWidget;
	UClass* Class = GetDefault<UNightSkyBattleRules>()->BattleSubtitleWidget.LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogNightSkyEngine, Warning, TEXT("Battle subtitles: no Battle Subtitle Widget (Battle Rules > Character Interactions)"));
		return nullptr;
	}
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	BattleSubtitleWidget = Controller ? CreateWidget<UNSEBattleSubtitleWidget>(Controller, Class) : CreateWidget<UNSEBattleSubtitleWidget>(GetWorld(), Class);
	if (BattleSubtitleWidget) BattleSubtitleWidget->AddToViewport(GetDefault<UNightSkyBattleRules>()->BattleSubtitleZOrder);
	return BattleSubtitleWidget;
}

void ANightSkyGameState::ShowBattleSubtitle(const FText Speaker, const FText Line, const float Seconds)
{
	if (bIsResimulating) return;
	// A frame re-run after a rollback load (not resimulated: the newest frame) mustn't show the line again.
	const uint32 Key = HashCombine(HashCombine(GetTypeHash(BattleState.RoundCount), GetTypeHash(BattleState.FrameNumber)),
		HashCombine(GetTypeHash(Line.ToString()), GetTypeHash(Speaker.ToString())));
	if (ShownSubtitleKeys.Contains(Key)) return;
	ShownSubtitleKeys.Add(Key);
	SubtitleLog.Add(Speaker.IsEmpty() ? Line.ToString() : Speaker.ToString() + TEXT(": ") + Line.ToString());
	UE_LOG(LogNightSkyEngine, Log, TEXT("Battle subtitle (frame %d): %s: %s"), BattleState.FrameNumber, *Speaker.ToString(), *Line.ToString());
	if (UNSEBattleSubtitleWidget* Widget = GetBattleSubtitleWidget()) Widget->ShowSubtitle(Speaker, Line, Seconds);
}

void ANightSkyGameState::HideBattleSubtitle()
{
	if (IsValid(BattleSubtitleWidget)) BattleSubtitleWidget->HideSubtitle();
}
