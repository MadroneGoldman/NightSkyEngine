// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NightSkyEngine/Network/FighterRunners.h"
#include "GameplayTagContainer.h"
#include "NightSkyEngine/Battle/Misc/RandomManager.h"
#include "Engine/GameInstance.h"
#include "NightSkyGameInstance.generated.h"

class UPrimaryStageData;
class UPrimaryCharaData;
class UNightSkySettingsInfo;
class USoundData;
constexpr int32 MaxReplays = 999;

class APlayerObject;
class UReplaySaveInfo;
class AGameModeBase;
enum class EBattleFormat : uint8;

USTRUCT(BlueprintType)
struct FBattleData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	bool bIsValid = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="NickTourOnly")
	TSoftObjectPtr<UPrimaryCharaData> P1Assist;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="NickTourOnly")
	TSoftObjectPtr<UPrimaryCharaData> P2Assist;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<TSoftObjectPtr<UPrimaryCharaData>> PlayerListP1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<TSoftObjectPtr<UPrimaryCharaData>> PlayerListP2;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ColorIndicesP1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<int32> ColorIndicesP2;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EBattleFormat BattleFormat{};
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TimeUntilRoundStart = 180;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RoundCount = 2;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StartRoundTimer = 99;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UPrimaryStageData* Stage = nullptr;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameplayTag MusicName;
	UPROPERTY()
	FRandomManager Random;
};

/**
 * 
 */
UCLASS()
class NIGHTSKYENGINE_API UNightSkyGameInstance : public UGameInstance
{
	GENERATED_BODY()

	UPROPERTY()
	UReplaySaveInfo* CurrentReplay;
	
	virtual void Init() override;

protected:
	/** Called when FindReplays finishes. By default it hands the list to every open widget with an OnFindReplays(Array) function (WBP_ReplayMenu). */
	UFUNCTION(BlueprintNativeEvent, Category = "Replays")
	void BP_OnFindReplaysComplete(const TArray<UReplaySaveInfo*> &AllReplays);

public:
	/** Menus: this player hosts the match (local play, or the online host). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Menu")
	bool IsHost = true;
	/** Menus: how many fighters each side picks on character select. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Menu")
	int32 TeamSizeP1 = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Menu")
	int32 TeamSizeP2 = 1;
	/** The level the main menu's Story button opens. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Menu")
	TSoftObjectPtr<UWorld> StoryMap;
	/** The project's main menu level: menus, replays and online errors return here. Empty = NSE's MainMenu_PL. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Menu")
	TSoftObjectPtr<UWorld> MainMenuMap;

	/** Sizes BattleData's player and colour lists to TeamSizeP1/P2, emptied. */
	UFUNCTION(BlueprintCallable, Category="Menu")
	void InitPlayerList();
	/** Creates the online session when this player is the host. */
	UFUNCTION(BlueprintCallable, Category="Online")
	void HostSession();
	/** Logs in to the online subsystem, if there is one. */
	UFUNCTION(BlueprintCallable, Category="Online")
	void LoginOnline();

	/** Opens the game instance's MainMenuMap (NSE's MainMenu_PL when it is empty). */
	UFUNCTION(BlueprintCallable, Category="Menu", meta=(WorldContext="WorldContextObject"))
	static void OpenMainMenu(const UObject* WorldContextObject);
	/** Opens the game instance's StoryMap. */
	UFUNCTION(BlueprintCallable, Category="Menu", meta=(WorldContext="WorldContextObject"))
	static void OpenStory(const UObject* WorldContextObject);

	bool FinishedLoadingForNetworkBattle = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString GameVersion;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BattleVersion;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UNightSkySettingsInfo> SettingsInfo;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundData> AnnouncerData;
	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundData> MusicData;

	UPROPERTY()
	TArray<UReplaySaveInfo*> ReplayList;
	UPROPERTY(BlueprintReadWrite)
	int PlayerIndex;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FBattleData BattleData;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool IsTraining = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool IsCPUBattle = false;
	UPROPERTY(BlueprintReadWrite)
	bool IsReplay = false;
	/** Madrone change: the game mode for training and replays (its game state can show a frame bar, input history...).
	 * Added to the stage URL as ?game=. Empty: the stage's own game mode, as for every other battle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Training")
	TSoftClassPtr<AGameModeBase> TrainingGameMode;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TEnumAsByte<EFighterRunners> FighterRunner;	

	UFUNCTION(BlueprintCallable)
	void TravelToVSInfo() const;
	void TravelToBattleMap() const;

	void LoadReplay();
	void PlayReplayToGameState(int32 FrameNumber, int32& OutP1Input, int32& OutP2Input) const;
	
	void RecordReplay();
	void UpdateReplay(int32 InputsP1, int32 InputsP2) const;
	void RollbackReplay(int32 FramesToRollback) const;
	void EndRecordReplay() const;
	
	/** Start playback for a previously recorded Replay, from blueprint */   
	UFUNCTION(BlueprintCallable, Category = "Replays")   
	void PlayReplayFromBP(FString ReplayName);   
  
	/** Start looking for/finding replays on the hard drive */  
	UFUNCTION(BlueprintCallable, Category = "Replays")  
	void FindReplays(); 
  
	/** Delete a previously recorded replay */
	UFUNCTION(BlueprintCallable, Category = "Replays")
	void DeleteReplay(const FString &ReplayName);
};
