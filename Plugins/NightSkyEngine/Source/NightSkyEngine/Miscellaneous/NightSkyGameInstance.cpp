// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyGameInstance.h"

#include "NightSkyEditorSettings.h"
#include "NightSkySettingsInfo.h"
#include "ReplayInfo.h"
#include "Kismet/GameplayStatics.h"
#include "NightSkyEngine/Data/PrimaryStageData.h"
#include "NightSkyEngine/Network/NSESessionSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyGameInstance)

void UNightSkyGameInstance::Init()
{
	Super::Init();

#if WITH_EDITOR
	if (auto& InBattleData = UNightSkyEditorSettings::GetConst()->BattleData; InBattleData.bIsValid) 
		BattleData = InBattleData;
#endif
	
	BattleData.bIsValid = true;	
	BattleData.Random = FRandomManager(FDateTime::Now().ToUnixTimestamp());

	SettingsInfo = Cast<UNightSkySettingsInfo>(UGameplayStatics::LoadGameFromSlot("SYSTEM", 0));
	if (!SettingsInfo)
	{
		SettingsInfo = Cast<UNightSkySettingsInfo>(UGameplayStatics::CreateSaveGameObject(UNightSkySettingsInfo::StaticClass()));
		UGameplayStatics::SaveGameToSlot(SettingsInfo, "SYSTEM", 0);
	}

	const FString AntiAliasingCommand = "r.AntiAliasingMethod " + FString::FromInt(SettingsInfo->AntiAliasingMethod);
	const FString GlobalIlluminationCommand = "r.DynamicGlobalIlluminationMethod " + FString::FromInt(SettingsInfo->GlobalIlluminationMethod);

	GetWorld()->Exec(GetWorld(), *AntiAliasingCommand);
	GetWorld()->Exec(GetWorld(), *GlobalIlluminationCommand);

	LoginOnline();
}

void UNightSkyGameInstance::TravelToVSInfo() const
{
	this->GetWorld()->ServerTravel("VSInfo_PL", true);
}

void UNightSkyGameInstance::TravelToBattleMap() const
{
	FString URL = BattleData.Stage->StageURL;
	// Madrone change: training and replays use their own game mode (and so their own game state) when one is set.
	if ((IsTraining || IsReplay) && !TrainingGameMode.IsNull())
	{
		URL += TEXT("?game=") + TrainingGameMode.ToSoftObjectPath().ToString();
	}
	this->GetWorld()->ServerTravel(URL, true);
}

void UNightSkyGameInstance::LoadReplay()
{
	BattleData = CurrentReplay->BattleData;
}

void UNightSkyGameInstance::PlayReplayToGameState(int32 FrameNumber, int32& OutP1Input, int32& OutP2Input) const
{
	if (FrameNumber >= CurrentReplay->LengthInFrames)
	{
		OpenMainMenu(this);
		return;
	}
	OutP1Input = CurrentReplay->InputsP1[FrameNumber];
	OutP2Input = CurrentReplay->InputsP2[FrameNumber];
}

void UNightSkyGameInstance::RecordReplay()
{
	CurrentReplay = Cast<UReplaySaveInfo>(UGameplayStatics::CreateSaveGameObject(UReplaySaveInfo::StaticClass()));
	CurrentReplay->BattleData = BattleData;
	CurrentReplay->Version = BattleVersion;
	CurrentReplay->bIsTraining = IsTraining;
}

void UNightSkyGameInstance::UpdateReplay(int32 InputsP1, int32 InputsP2) const
{
	if (!CurrentReplay) return;
	CurrentReplay->LengthInFrames++;
	CurrentReplay->InputsP1.Add(InputsP1);
	CurrentReplay->InputsP2.Add(InputsP2);
}

void UNightSkyGameInstance::RollbackReplay(int32 FramesToRollback) const
{
	for (int i = 0; i < FramesToRollback; i++)
	{
		if (!CurrentReplay) return;
		CurrentReplay->LengthInFrames--;
		CurrentReplay->InputsP1.Pop();
		CurrentReplay->InputsP2.Pop();
	}
}

void UNightSkyGameInstance::EndRecordReplay() const
{
	if (IsReplay) return;
	FString ReplayName = "REPLAY";
	for (int i = 0; i < MaxReplays; i++)
	{
		ReplayName = "REPLAY";
		ReplayName.AppendInt(i);
		if (!UGameplayStatics::DoesSaveGameExist(ReplayName, 0))
		{
			break;
		}
	}
	UGameplayStatics::SaveGameToSlot(CurrentReplay, ReplayName, 0);
}

void UNightSkyGameInstance::PlayReplayFromBP(FString ReplayName)
{
	FighterRunner = Multiplayer;
	IsReplay = true;
	CurrentReplay = Cast<UReplaySaveInfo>(UGameplayStatics::LoadGameFromSlot(ReplayName, 0));
	IsTraining = CurrentReplay->bIsTraining;
	LoadReplay();
}

void UNightSkyGameInstance::FindReplays()
{
	ReplayList.Empty();
	for (int i = 0; i < MaxReplays; i++)
	{
		FString ReplayName = "REPLAY";
		ReplayName.AppendInt(i);
		if (!UGameplayStatics::DoesSaveGameExist(ReplayName, 0))
		{
			continue;
		}
		ReplayList.Add(Cast<UReplaySaveInfo>(UGameplayStatics::LoadGameFromSlot(ReplayName, 0)));
		ReplayList.Last()->ReplayIndex = i;
		if (ReplayList.Last()->Version != BattleVersion)
		{
			ReplayList.Pop();
			UGameplayStatics::DeleteGameInSlot(ReplayName, 0);
		}
	}
	BP_OnFindReplaysComplete(ReplayList);
}

void UNightSkyGameInstance::DeleteReplay(const FString& ReplayName)
{
	if (UGameplayStatics::DoesSaveGameExist(ReplayName, 0))
	{
		UGameplayStatics::DeleteGameInSlot(ReplayName, 0);
	}
	FindReplays();
}

void UNightSkyGameInstance::BP_OnFindReplaysComplete_Implementation(const TArray<UReplaySaveInfo*>& AllReplays)
{
	// The replay menu lists them: call OnFindReplays(Array) on every open widget that has it.
	static const FName FuncName(TEXT("OnFindReplays"));
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->GetWorld() != GetWorld() || Widget->HasAnyFlags(RF_ClassDefaultObject))
			continue;
		UFunction* Func = Widget->FindFunction(FuncName);
		if (!Func || Func->NumParms != 1)
			continue;
		const FArrayProperty* Param = CastField<FArrayProperty>(Func->PropertyLink);
		if (!Param || !Param->Inner->IsA<FObjectPropertyBase>())
			continue;
		TArray<UReplaySaveInfo*> Copy = AllReplays;
		Widget->ProcessEvent(Func, &Copy);
	}
}

void UNightSkyGameInstance::InitPlayerList()
{
	BattleData.PlayerListP1.Empty();
	BattleData.PlayerListP1.SetNum(TeamSizeP1);
	BattleData.PlayerListP2.Empty();
	BattleData.PlayerListP2.SetNum(TeamSizeP2);
	BattleData.ColorIndicesP1.Empty();
	BattleData.ColorIndicesP1.SetNum(TeamSizeP1);
	BattleData.ColorIndicesP2.Empty();
	BattleData.ColorIndicesP2.SetNum(TeamSizeP2);
}

void UNightSkyGameInstance::HostSession()
{
	if (!IsHost)
		return;
	if (UNSESessionSubsystem* Sessions = GetSubsystem<UNSESessionSubsystem>())
		Sessions->CreateSession(2, false);
}

void UNightSkyGameInstance::LoginOnline()
{
	if (!Online::GetSubsystem(GetWorld()))
		return;
	if (UNSESessionSubsystem* Sessions = GetSubsystem<UNSESessionSubsystem>())
		Sessions->Login();
}

void UNightSkyGameInstance::OpenMainMenu(const UObject* WorldContextObject)
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject);
	const UNightSkyGameInstance* NSGI = Cast<UNightSkyGameInstance>(GI);
	if (NSGI && !NSGI->MainMenuMap.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContextObject, NSGI->MainMenuMap);
		return;
	}
	UGameplayStatics::OpenLevel(WorldContextObject, FName(TEXT("MainMenu_PL")));
}

void UNightSkyGameInstance::OpenStory(const UObject* WorldContextObject)
{
	const UNightSkyGameInstance* NSGI = Cast<UNightSkyGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (NSGI && !NSGI->StoryMap.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(WorldContextObject, NSGI->StoryMap);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("OpenStory: the game instance has no StoryMap set"));
}