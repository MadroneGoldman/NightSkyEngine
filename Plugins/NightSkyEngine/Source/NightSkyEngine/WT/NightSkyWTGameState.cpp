// Fill out your copyright notice in the Description page of Project Settings.


#include "NightSkyWTGameState.h"

#include "Algo/BinarySearch.h"
#include "DrawDebugHelpers.h"
#include "NightSkyEngine/Battle/Misc/Globals.h"

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
		// Madrone change: under the synctest runner the opponents are local players (recorded inputs), since NSE's CPU
		// runs outside rollback and a synctest can only pass without it.
		P2->bIsCpu = !GameInstance || GameInstance->FighterRunner != SyncTest;
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
				P2Ally->bIsCpu = !GameInstance || GameInstance->FighterRunner != SyncTest; // Madrone change
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
void ANightSkyWTGameState::SetBattleFloor(const TArray<FVector2D>& PointsCm)
{
	ClearBattleFloor();
	if (PointsCm.Num() < 2) return;
	FloorPoints.Reserve(PointsCm.Num());
	for (const FVector2D& Point : PointsCm)
	{
		FloorPoints.Add(FIntPoint(FMath::RoundToInt32(Point.X * COORD_SCALE), FMath::RoundToInt32(Point.Y * COORD_SCALE)));
	}
}

void ANightSkyWTGameState::ClearBattleFloor()
{
	FloorPoints.Reset();
	CameraFloorOffset = 0.f;
	bCameraFloorOffsetValid = false;
	CameraPitchOffset = 0.f;
	CameraGroundRaise = 0.f;
	CameraGround.Reset();
	CameraGroundNumX = CameraGroundNumY = 0;
}

void ANightSkyWTGameState::SetCameraGround(const TArray<float>& HeightsCm, const int32 NumX, const int32 NumY, const float OriginXCm, const float StepCm)
{
	CameraGround.Reset();
	CameraGroundNumX = CameraGroundNumY = 0;
	if (NumX < 1 || NumY < 1 || StepCm <= 0.f || HeightsCm.Num() != NumX * NumY) return;
	CameraGround = HeightsCm;
	CameraGroundNumX = NumX;
	CameraGroundNumY = NumY;
	CameraGroundOriginX = OriginXCm;
	CameraGroundStep = StepCm;
}

void ANightSkyWTGameState::ShowCameraGround(const float Seconds) const
{
	if (CameraGround.IsEmpty()) return;
	float Low = UE_BIG_NUMBER, High = -UE_BIG_NUMBER;
	for (const float Height : CameraGround)
	{
		if (Height < -1e5f) continue;
		Low = FMath::Min(Low, Height);
		High = FMath::Max(High, Height);
	}
	for (int32 Row = 0; Row < CameraGroundNumY; Row++)
	{
		for (int32 Column = 0; Column < CameraGroundNumX; Column++)
		{
			const float Height = CameraGround[Row * CameraGroundNumX + Column];
			if (Height < -1e5f) continue;
			const float Share = High > Low ? (Height - Low) / (High - Low) : 0.f;
			const FVector Point = BattleSceneTransform.TransformPositionNoScale(FVector(CameraGroundOriginX + Column * CameraGroundStep, Row * CameraGroundStep, Height));
			DrawDebugPoint(GetWorld(), Point, 6.f, FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Share).ToFColor(true), false, Seconds);
		}
	}
	const FVector A = BattleSceneTransform.TransformPositionNoScale(FVector(CameraGroundOriginX, 0, 0));
	const FVector B = BattleSceneTransform.TransformPositionNoScale(FVector(CameraGroundOriginX + (CameraGroundNumX - 1) * CameraGroundStep, 0, 0));
	DrawDebugLine(GetWorld(), A, B, FColor::Magenta, false, Seconds, 0, 4.f);
}

float ANightSkyWTGameState::CameraRaiseOverGround(const float CameraX, const float CameraDepth, const float CameraHeight) const
{
	// Presentation only, so floats are fine here: the camera isn't part of the fight's state (and NSE doesn't move it while
	// re-running frames after a rollback).
	if (CameraGround.IsEmpty() || CameraDepth <= 1.f) return 0.f;
	// NSE's battle camera: 54 degrees across. Half its width at a distance is that distance x tan(27).
	static const float HalfViewTan = FMath::Tan(FMath::DegreesToRadians(27.f));
	const float MinDepth = 100.f; // nearer the line than this is the floor the fighters stand on
	const float NearReach = 100.f; // around the camera itself, it keeps Clearance above the ground
	float Needed = -UE_BIG_NUMBER;
	const int32 LastRow = FMath::Min(CameraGroundNumY - 1, FMath::CeilToInt((CameraDepth + NearReach) / CameraGroundStep));
	for (int32 Row = FMath::CeilToInt(MinDepth / CameraGroundStep); Row <= LastRow; Row++)
	{
		const float Depth = Row * CameraGroundStep;
		const float Ahead = CameraDepth - Depth; // distance from the camera, towards the line
		const float Reach = Ahead > 0.f ? HalfViewTan * Ahead : NearReach;
		const int32 First = FMath::Max(0, FMath::FloorToInt((CameraX - Reach - CameraGroundOriginX) / CameraGroundStep));
		const int32 Last = FMath::Min(CameraGroundNumX - 1, FMath::CeilToInt((CameraX + Reach - CameraGroundOriginX) / CameraGroundStep));
		for (int32 Column = First; Column <= Last; Column++)
		{
			const float Ground = CameraGround[Row * CameraGroundNumX + Column];
			if (Ground < -1e5f) continue;
			const float X = CameraGroundOriginX + Column * CameraGroundStep;
			if (FMath::Abs(Ahead) <= NearReach && FMath::Abs(X - CameraX) <= NearReach)
			{
				Needed = FMath::Max(Needed, Ground + CameraGroundClearance); // the camera itself stays above the ground
			}
			if (Ahead <= 0.f) continue;
			// The sight line from the camera through this spot reaches the fight's line at TargetX: the ground here must stay
			// under the line from the camera to CameraSightHeight above the floor there.
			const float Share = Depth / CameraDepth; // 0 at the fight's line, 1 at the camera
			const float TargetX = CameraX + (X - CameraX) / (1.f - Share);
			const float Target = GetFloorHeightAt(FMath::RoundToInt32(TargetX * COORD_SCALE)) / COORD_SCALE + CameraSightHeight;
			Needed = FMath::Max(Needed, Target + (Ground - Target) / Share);
		}
	}
	return FMath::Clamp(Needed - CameraHeight, 0.f, MaxCameraGroundRaise);
}

TArray<FVector2D> ANightSkyWTGameState::GetBattleFloorPoints() const
{
	TArray<FVector2D> Points;
	Points.Reserve(FloorPoints.Num());
	for (const FIntPoint& Point : FloorPoints) Points.Add(FVector2D(Point.X / COORD_SCALE, Point.Y / COORD_SCALE));
	return Points;
}

int32 ANightSkyWTGameState::FloorHeightOnLine(const int32 InPosX) const
{
	if (InPosX <= FloorPoints[0].X) return FloorPoints[0].Y;
	if (InPosX >= FloorPoints.Last().X) return FloorPoints.Last().Y;
	const int32 Next = Algo::UpperBoundBy(FloorPoints, InPosX, &FIntPoint::X);
	const FIntPoint& A = FloorPoints[Next - 1];
	const FIntPoint& B = FloorPoints[Next];
	// Integers only: a fight has to play out the same way on every machine.
	return A.Y + static_cast<int32>(static_cast<int64>(B.Y - A.Y) * (InPosX - A.X) / (B.X - A.X));
}

int32 ANightSkyWTGameState::GetFloorHeightAt(const int32 InPosX) const
{
	if (!HasBattleFloor()) return 0;
	// Fighters stay upright, so stand on the highest ground under their feet, not just under their middle. Small, or
	// they float over steep slopes: this lifts them by the slope x this distance.
	static const int32 FootReach = FMath::RoundToInt32(15.f * COORD_SCALE);
	return FMath::Max3(FloorHeightOnLine(InPosX - FootReach), FloorHeightOnLine(InPosX), FloorHeightOnLine(InPosX + FootReach));
}

float ANightSkyWTGameState::GetCameraFloorOffset(const FVector& CameraLocation)
{
	if (!HasBattleFloor()) return 0.f;
	// Follow the floor under what's on screen...
	const float Floor = GetFloorHeightAt(BattleState.ScreenData.FinalScreenX * 1000) / COORD_SCALE;
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	float Raise = 0.f;
	const FVector Local = BattleSceneTransform.GetRotation().UnrotateVector(CameraLocation - BattleSceneTransform.GetLocation());
	if (!CameraGround.IsEmpty())
	{
		// ...and rise as much as the ground between the camera and the fight needs (measured at battle start).
		Raise = CameraRaiseOverGround(Local.X, Local.Y, Local.Z + Floor);
	}
	else
	{
		// No ground measured (no landscape): the camera's behaviour from before the ground survey, unchanged: stay above
		// the ground straight beneath it, at most 200 cm up, no tilt.
		constexpr float MaxRaise = 200.f;
		const FVector Top = CameraLocation + FVector(0, 0, Floor + MaxRaise + CameraGroundClearance);
		const FVector Bottom = CameraLocation + FVector(0, 0, Floor - 500.f);
		FHitResult Hit;
		const FCollisionObjectQueryParams Static(ECC_WorldStatic);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, Static, FCollisionQueryParams(TEXT("WitchBattleCamera"), false)) && !Hit.bStartPenetrating)
		{
			Raise = FMath::Clamp(Hit.ImpactPoint.Z + CameraGroundClearance - (CameraLocation.Z + Floor), 0.f, MaxRaise);
		}
	}
	if (bCameraFloorOffsetValid)
	{
		CameraFloorOffset = FMath::FInterpTo(CameraFloorOffset, Floor, DeltaSeconds, CameraFloorFollowSpeed);
		CameraGroundRaise = FMath::FInterpTo(CameraGroundRaise, Raise, DeltaSeconds, CameraFloorFollowSpeed);
	}
	else
	{
		CameraFloorOffset = Floor;
		CameraGroundRaise = Raise;
	}
	bCameraFloorOffsetValid = true;
	// Tilt down by the angle the rise added, so the camera keeps looking where it did.
	CameraPitchOffset = !CameraGround.IsEmpty() && Local.Y > 1.f ? -FMath::RadiansToDegrees(FMath::Atan2(CameraGroundRaise, Local.Y)) * CameraRaiseTilt : 0.f;
	return CameraFloorOffset + CameraGroundRaise;
}

void ANightSkyWTGameState::DrawBattleFloor() const
{
	const FTransform& Scene = BattleSceneTransform;
	for (int32 i = 1; i < FloorPoints.Num(); i++)
	{
		const FVector A = Scene.GetRotation().RotateVector(FVector(FloorPoints[i - 1].X / COORD_SCALE, 0, FloorPoints[i - 1].Y / COORD_SCALE)) + Scene.GetLocation();
		const FVector B = Scene.GetRotation().RotateVector(FVector(FloorPoints[i].X / COORD_SCALE, 0, FloorPoints[i].Y / COORD_SCALE)) + Scene.GetLocation();
		DrawDebugLine(GetWorld(), A, B, FColor::Magenta, false, -1.f, 255, 3.f);
	}
}

void ANightSkyWTGameState::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);

	if (bViewCollision && HasBattleFloor()) DrawBattleFloor();
	
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