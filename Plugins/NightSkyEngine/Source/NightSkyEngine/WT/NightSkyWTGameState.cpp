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
	float Target = GetFloorHeightAt(BattleState.ScreenData.FinalScreenX * 1000) / COORD_SCALE;
	// ...and stay above the ground beneath the camera, which is off to the side of the fight's line.
	const FVector Top = CameraLocation + FVector(0, 0, Target + MaxCameraGroundRaise + CameraGroundClearance);
	const FVector Bottom = CameraLocation + FVector(0, 0, Target - 500.f);
	FHitResult Hit;
	const FCollisionObjectQueryParams Static(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, Static, FCollisionQueryParams(TEXT("WitchBattleCamera"), false)) && !Hit.bStartPenetrating)
	{
		Target += FMath::Clamp(Hit.ImpactPoint.Z + CameraGroundClearance - (CameraLocation.Z + Target), 0.f, MaxCameraGroundRaise);
	}
	CameraFloorOffset = bCameraFloorOffsetValid ? FMath::FInterpTo(CameraFloorOffset, Target, GetWorld()->GetDeltaSeconds(), CameraFloorFollowSpeed) : Target;
	bCameraFloorOffsetValid = true;
	return CameraFloorOffset;
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