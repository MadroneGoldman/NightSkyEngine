// Madrone change: Baroque Cancel presentation. See BaroqueCancelVisuals.h.

#include "BaroqueCancelVisuals.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/Battle/Script/BaroqueCancelState.h"
#include "NightSkyEngine/Miscellaneous/NightSkyBattleRules.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BaroqueCancelVisuals)

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<float> CVarBaroqueForceDarken(
	TEXT("ns.Baroque.ForceDarken"), 0.f,
	TEXT("Preview: keep the Baroque Cancel screen darkening at this strength (0-1) during battles. 0 = normal."));
#endif

namespace BaroqueVisuals
{
	// Afterimages: the start ghost, the drift trail, a slowed fighter's echoes.
	constexpr float StartOpacity = 0.7f;
	constexpr float StartLife = 0.55f;
	constexpr int32 TrailInterval = 3;
	constexpr float TrailOpacity = 0.4f;
	constexpr float TrailLife = 0.3f;
	constexpr float EchoOpacity = 0.32f;
	constexpr float EchoLife = 0.28f;
	// Darkening: in over about 4 frames, out over about 10.
	constexpr float DarkenInStep = 0.25f;
	constexpr float DarkenOutStep = 0.07f;

	// Where each fighter was at its last slowdown echo (presentation only; one battle at a time).
	static TMap<TObjectKey<APlayerObject>, FIntPoint> EchoPositions;
	static bool MovedSinceLastEcho(const APlayerObject* Player)
	{
		constexpr int32 MinMove = 12000; // about 5 cm
		const FIntPoint Now(Player->PosX, Player->PosY);
		const FIntPoint* Before = EchoPositions.Find(Player);
		EchoPositions.Add(Player, Now);
		return !Before || FMath::Abs(Now.X - Before->X) + FMath::Abs(Now.Y - Before->Y) >= MinMove;
	}

	static int32 Last(const TMap<TObjectKey<APlayerObject>, int32>& Map, const TObjectKey<APlayerObject>& Key)
	{
		const int32* Found = Map.Find(Key);
		return Found ? *Found : INT_MIN;
	}
}

// --------------------------------------------------------------------------------------------------- afterimage actor

ABaroqueAfterimage::ABaroqueAfterimage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	SetActorEnableCollision(false);
}

bool ABaroqueAfterimage::Init(const APlayerObject* Source, UMaterialInterface* InMaterial, const FLinearColor& Color,
                              const float Opacity, const float Life)
{
	if (!IsValid(Source) || !InMaterial)
	{
		Destroy();
		return false;
	}
	LifeSeconds = FMath::Max(0.05f, Life);
	StartOpacity = Opacity;
	Material = UMaterialInstanceDynamic::Create(InMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);

	TArray<USkeletalMeshComponent*> Meshes;
	Source->GetComponents<USkeletalMeshComponent>(Meshes);
	int32 Copied = 0;
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		// Only what's drawn: the hidden driver body (Manny) is left out by these checks.
		if (!Mesh || !Mesh->GetSkinnedAsset() || !Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->bRenderInMainPass) continue;
		// A mesh that follows a leader has no pose of its own: copy the leader's, bone by bone name.
		USkeletalMeshComponent* PoseSource = Mesh;
		if (USkinnedMeshComponent* Leader = Mesh->LeaderPoseComponent.Get())
		{
			if (USkeletalMeshComponent* LeaderMesh = Cast<USkeletalMeshComponent>(Leader)) PoseSource = LeaderMesh;
		}
		if (PoseSource->GetBoneSpaceTransforms().IsEmpty()) continue;

		UPoseableMeshComponent* Copy = NewObject<UPoseableMeshComponent>(this);
		Copy->SetupAttachment(GetRootComponent());
		Copy->SetCastShadow(false);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetSkinnedAssetAndUpdate(Mesh->GetSkinnedAsset());
		Copy->RegisterComponent();
		Copy->SetWorldTransform(Mesh->GetComponentTransform());
		Copy->CopyPoseFromSkeletalComponent(PoseSource);
		for (int32 i = 0; i < Copy->GetNumMaterials(); ++i) Copy->SetMaterial(i, Material);
		++Copied;
	}
	if (Copied == 0)
	{
		Destroy();
		return false;
	}
	return true;
}

void ABaroqueAfterimage::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Left = 1.f - Age / LifeSeconds;
	if (Left <= 0.f)
	{
		Destroy();
		return;
	}
	if (Material) Material->SetScalarParameterValue(TEXT("Opacity"), StartOpacity * Left * Left);
}

// ------------------------------------------------------------------------------------------------------------ visuals

const UBaroqueCancelState* UBaroqueCancelVisuals::FindBaroqueState(const APlayerObject* Player)
{
	if (!IsValid(Player)) return nullptr;
	if (const UBaroqueCancelState* Current = Cast<UBaroqueCancelState>(Player->PrimaryStateMachine.CurrentState)) return Current;
	for (const UState* State : Player->PrimaryStateMachine.States)
	{
		if (const UBaroqueCancelState* Baroque = Cast<UBaroqueCancelState>(State)) return Baroque;
	}
	return nullptr;
}

void UBaroqueCancelVisuals::Update(ANightSkyGameState* GameState)
{
	const UNightSkyBattleRules* Rules = GetDefault<UNightSkyBattleRules>();
	if (!GameState || !Rules->bBaroqueVisuals)
	{
		Reset();
		return;
	}
	if (!AfterimageMaterial) AfterimageMaterial = Rules->BaroqueAfterimageMaterial.LoadSynchronous();

	const int32 FrameNumber = GameState->BattleState.FrameNumber;
	float DarkTarget = 0.f;
	FLinearColor Tint = DarkTint;

	for (APlayerObject* Player : GameState->Players)
	{
		if (!IsValid(Player) || !Player->IsOnScreen()) continue;
		const TObjectKey<APlayerObject> Key(Player);

		if (const UBaroqueCancelState* Baroque = Cast<UBaroqueCancelState>(Player->PrimaryStateMachine.CurrentState))
		{
			const FBaroqueCancelKind Kind = Baroque->GetKind(Player->GetBaroqueCancelType());
			const int32 Frame = Player->ActionTime;
			// Where it started: one ghost per cancel.
			if (const int32 Start = FrameNumber - Frame; BaroqueVisuals::Last(LastStartFrame, Key) != Start)
			{
				LastStartFrame.Add(Key, Start);
				SpawnAfterimage(Player, Kind.Color, BaroqueVisuals::StartOpacity, BaroqueVisuals::StartLife);
			}
			// The drift trail.
			if (Player->GetBaroqueDrift() != EBaroqueDrift::None && Frame > 0 && Frame <= Baroque->DriftFrames
				&& Frame % BaroqueVisuals::TrailInterval == 0 && BaroqueVisuals::Last(LastTrailFrame, Key) != FrameNumber)
			{
				LastTrailFrame.Add(Key, FrameNumber);
				SpawnAfterimage(Player, Kind.Color, BaroqueVisuals::TrailOpacity, BaroqueVisuals::TrailLife);
			}
			// Dark while this fighter's freeze runs.
			if (GameState->BattleState.SuperFreezeDuration > 0 && GameState->BattleState.SuperFreezeCaller == Player
				&& Kind.ScreenDarken > DarkTarget)
			{
				DarkTarget = Kind.ScreenDarken;
				Tint = Kind.Color;
			}
		}

		// Slowed: echoes in the colour of the cancel that slowed it.
		// (Only while it moves: an echo on a fighter standing still sits right on it.)
		if (Rules->SlowdownEchoInterval > 0 && Player->GetSlowdownTimer() > 0
			&& FrameNumber % Rules->SlowdownEchoInterval == 0 && BaroqueVisuals::Last(LastEchoFrame, Key) != FrameNumber
			&& BaroqueVisuals::MovedSinceLastEcho(Player))
		{
			LastEchoFrame.Add(Key, FrameNumber);
			const UBaroqueCancelState* Source = FindBaroqueState(Player->Enemy);
			if (!Source) Source = FindBaroqueState(Player);
			const FLinearColor Color = Source ? Source->GetKind(Player->GetSlowdownType()).Color : FLinearColor(0.15f, 0.45f, 1.f);
			SpawnAfterimage(Player, Color, BaroqueVisuals::EchoOpacity, BaroqueVisuals::EchoLife);
		}
	}

#if !UE_BUILD_SHIPPING
	if (const float Forced = CVarBaroqueForceDarken.GetValueOnGameThread(); Forced > 0.f)
	{
		DarkTarget = FMath::Min(Forced, 1.f);
		Tint = FLinearColor(0.15f, 0.45f, 1.f);
	}
#endif
	UpdateDarkness(GameState, DarkTarget, Tint);
}

void UBaroqueCancelVisuals::UpdateDarkness(ANightSkyGameState* GameState, const float Target, const FLinearColor& Tint)
{
	if (Target > 0.f) DarkTint = Tint;
	Darkness = Target > Darkness
		           ? FMath::Min(Target, Darkness + BaroqueVisuals::DarkenInStep)
		           : FMath::Max(Target, Darkness - BaroqueVisuals::DarkenOutStep);

	if (Darkness <= 0.001f)
	{
		Darkness = 0.f;
		if (PostProcess) PostProcess->bEnabled = false;
		UnmarkFighters();
		return;
	}

	const UNightSkyBattleRules* Rules = GetDefault<UNightSkyBattleRules>();
	if (!PostProcess)
	{
		UMaterialInterface* Base = Rules->BaroqueScreenDarkenMaterial.LoadSynchronous();
		if (!Base)
		{
			UE_LOG(LogTemp, Warning, TEXT("Baroque Cancel: no screen darkening material (Battle Rules > Baroque Screen Darken Material)"));
			return;
		}
		// Its own visible actor: a post-process component only applies when it ShouldRender(), which needs a visible
		// owner, and the game state is a hidden info actor.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AActor* Holder = GameState->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Holder) return;
		PostProcess = NewObject<UPostProcessComponent>(Holder, TEXT("BaroqueScreenDarken"));
		Holder->SetRootComponent(PostProcess);
		PostProcess->bUnbound = true;
		PostProcess->Priority = 10000.f;
		PostProcess->RegisterComponent();
		DarkenMaterial = UMaterialInstanceDynamic::Create(Base, PostProcess);
		PostProcess->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, DarkenMaterial));
		// The darkening runs before exposure is measured: auto exposure would brighten the picture back up within a
		// few frames. Hold the exposure where it is while the screen is dark: a very slow speed (0 is NOT frozen, the
		// engine jumps straight to the target then, which blew the picture out white once the darkening ended).
		PostProcess->Settings.bOverride_AutoExposureSpeedUp = true;
		PostProcess->Settings.AutoExposureSpeedUp = 0.01f;
		PostProcess->Settings.bOverride_AutoExposureSpeedDown = true;
		PostProcess->Settings.AutoExposureSpeedDown = 0.01f;
	}
	// The stencil buffer is only drawn with r.CustomDepth 3 (custom depth with stencil).
	if (IConsoleVariable* CustomDepth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth")); CustomDepth && CustomDepth->GetInt() < 3)
	{
		CustomDepth->Set(3, ECVF_SetByCode);
	}
	PostProcess->bEnabled = true;
	DarkenMaterial->SetScalarParameterValue(TEXT("Darkness"), Darkness);
	DarkenMaterial->SetScalarParameterValue(TEXT("FighterStencil"), Rules->BaroqueFighterStencil);
	DarkenMaterial->SetVectorParameterValue(TEXT("Tint"), DarkTint);
	MarkFighters(GameState);
}

void UBaroqueCancelVisuals::MarkFighters(const ANightSkyGameState* GameState)
{
	const int32 Stencil = GetDefault<UNightSkyBattleRules>()->BaroqueFighterStencil;
	TArray<AActor*> Actors;
	for (APlayerObject* Player : GameState->Players)
	{
		if (!IsValid(Player)) continue;
		Actors.Add(Player);
		Player->GetAttachedActors(Actors, false, true);
	}
	for (const AActor* Actor : Actors)
	{
		TArray<UMeshComponent*> Meshes;
		Actor->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh->bRenderCustomDepth && Mesh->CustomDepthStencilValue == Stencil) continue;
			if (!Marked.ContainsByPredicate([Mesh](const FMarked& M) { return M.Component.Get() == Mesh; }))
			{
				Marked.Add({Mesh, Mesh->bRenderCustomDepth != 0, Mesh->CustomDepthStencilValue});
			}
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(Stencil);
		}
	}
}

void UBaroqueCancelVisuals::UnmarkFighters()
{
	for (const FMarked& M : Marked)
	{
		if (UPrimitiveComponent* Component = M.Component.Get())
		{
			Component->SetRenderCustomDepth(M.bRenderedCustomDepth);
			Component->SetCustomDepthStencilValue(M.StencilValue);
		}
	}
	Marked.Reset();
}

void UBaroqueCancelVisuals::SpawnAfterimage(const APlayerObject* Player, const FLinearColor& Color, const float Opacity,
                                           const float Life) const
{
	if (!AfterimageMaterial || !Player->GetWorld()) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABaroqueAfterimage* Ghost = Player->GetWorld()->SpawnActor<ABaroqueAfterimage>(Player->GetActorLocation(), FRotator::ZeroRotator, Params))
	{
		Ghost->Init(Player, AfterimageMaterial, Color, Opacity, Life);
	}
}

void UBaroqueCancelVisuals::Reset()
{
	Darkness = 0.f;
	if (PostProcess) PostProcess->bEnabled = false;
	UnmarkFighters();
}
