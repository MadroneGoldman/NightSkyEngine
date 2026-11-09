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

	/** Madrone change. The ground between the fight's line and the camera, measured once at battle start (by the caller,
	 * which knows what counts as ground): heights in cm above BattleSceneTransform, NumX along the line from OriginXCm and
	 * NumY away from it towards the camera from 0, StepCm apart. Below -1e5: nothing there. The camera rises over it. */
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void SetCameraGround(const TArray<float>& HeightsCm, int32 NumX, int32 NumY, float OriginXCm, float StepCm);

	/** Madrone change, for debugging: draws the measured camera ground as points for Seconds (green low, red high; the
	 * fight's line in magenta). */
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void ShowCameraGround(float Seconds = 10.f) const;

	/** Most the camera rises to keep the ground out of the way (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Camera")
	float MaxCameraGroundRaise = 300.f;
	/** How far above the ground around it the camera stays (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Camera")
	float CameraGroundClearance = 60.f;
	/** How much of the fighters, from their feet up, the ground between them and the camera must never hide (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Camera")
	float CameraSightHeight = 40.f;
	/** When the camera rises over the ground, how much it tilts down to keep the fight framed: 1 = looks at the same
	 * spot as before rising, 0 = only rises (the fight sinks on screen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Camera")
	float CameraRaiseTilt = 1.f;
	/** How quickly the camera follows the floor's height and its rise over the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle Camera")
	float CameraFloorFollowSpeed = 4.f;
	/** The camera's rise over the ground right now, after smoothing (cm). For tests. */
	UPROPERTY(BlueprintReadOnly, Category = "Battle Camera")
	float CameraGroundRaise = 0.f;

	/** Madrone change. Walls from the level (actors tagged NSE_Wall, found by the caller at battle start): cm along the
	 * fight's line from BattleSceneTransform, left (negative) and right (positive). They narrow the stage: fighters and the
	 * camera stop there. bHasLeft / bHasRight false keeps that side's normal stage edge. Kept for every round. */
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void SetStageWalls(bool bHasLeft, float LeftCm, bool bHasRight, float RightCm);
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	void ClearStageWalls();
	/** The stage edges in effect, cm along the line (the level's walls or the normal stage edges). */
	UFUNCTION(BlueprintCallable, Category = "Battle Floor")
	FVector2D GetStageWallsCm() const;

	virtual void ApplyStageWalls(FScreenData& Screen) const override;

	virtual bool HasBattleFloor() const override { return FloorPoints.Num() >= 2; }
	virtual int32 GetFloorHeightAt(int32 InPosX) const override;
	virtual float GetCameraFloorOffset(const FVector& CameraLocation) override;
	virtual float GetCameraPitchOffset() const override { return CameraPitchOffset; }
	
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
	float CameraPitchOffset = 0.f;
	TArray<float> CameraGround;
	int32 CameraGroundNumX = 0;
	int32 CameraGroundNumY = 0;
	float CameraGroundOriginX = 0.f;
	float CameraGroundStep = 50.f;

	/** How much the camera has to rise so the ground stays out of the way, from CameraGround (cm). */
	float CameraRaiseOverGround(float CameraX, float CameraDepth, float CameraHeight) const;

	/** Stage edges from the level's walls, in ScreenData units (cm / OBJ_SCALE). Never change during a fight. */
	bool bHasWallLeft = false;
	bool bHasWallRight = false;
	int32 WallLeft = 0;
	int32 WallRight = 0;

	int32 FloorHeightOnLine(int32 InPosX) const;
	void DrawBattleFloor() const;
	void DrawStageWalls() const;

public:
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
