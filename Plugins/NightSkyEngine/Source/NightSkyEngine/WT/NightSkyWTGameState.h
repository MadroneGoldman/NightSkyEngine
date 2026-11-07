// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyWTGameState.generated.h"

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleEndDelegate);

UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBattleStartDelegate);  //madrone change

UCLASS()
class NIGHTSKYENGINE_API ANightSkyWTGameState : public ANightSkyGameState
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ANightSkyWTGameState();

	UPROPERTY(BlueprintReadWrite)
	bool bIsMatchEnd;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsBattling;

	UPROPERTY()
	FOnBattleEndDelegate OnBattleEndDelegate;
	
	UPROPERTY()
	FOnBattleStartDelegate OnBattleStartDelegate; //madrone change

	/** Experimental, WT battles only: a floor that follows the ground under the fight's line. Points are (cm along the
	 * line, cm above BattleSceneTransform), sorted by position. Fewer than 2 points means a flat floor. */
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void SetBattleFloor(const TArray<FVector2D>& PointsCm);
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void ClearBattleFloor();
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	TArray<FVector2D> GetBattleFloorPoints() const;

	/** Most the camera rises to stay above the ground beneath it (cm). */
	UPROPERTY(EditAnywhere, Category = "Battle Floor")
	float MaxCameraGroundRaise = 200.f;
	/** How far above the ground beneath it the camera stays (cm). */
	UPROPERTY(EditAnywhere, Category = "Battle Floor")
	float CameraGroundClearance = 60.f;
	/** How quickly the camera follows the floor's height. */
	UPROPERTY(EditAnywhere, Category = "Battle Floor")
	float CameraFloorFollowSpeed = 4.f;

	virtual bool HasBattleFloor() const override { return FloorPoints.Num() >= 2; }
	virtual int32 GetFloorHeightAt(int32 InPosX) const override;
	virtual float GetCameraFloorOffset(const FVector& CameraLocation) override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
public:
	void Init(APlayerObject* P1, APlayerObject* P2, APlayerObject* P1_TagTeammate = nullptr, APlayerObject* P1_AssistTeammate = nullptr, TArray<APlayerObject*> P2Allies = {});

private:
	/** Position, height, in the game's own units. Never changes during a fight, so rollback doesn't need it. */
	TArray<FIntPoint> FloorPoints;
	float CameraFloorOffset = 0.f;
	bool bCameraFloorOffsetValid = false;

	int32 FloorHeightOnLine(int32 InPosX) const;
	void DrawBattleFloor() const;

public:
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
