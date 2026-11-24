// Madrone change: Baroque Cancel presentation (Guilty Gear -Strive- Roman Cancel look). Not rollback state: everything here
// is worked out each rendered frame from the battle state, so a rollback never needs it saved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"
#include "BaroqueCancelVisuals.generated.h"

class APlayerObject;
class ANightSkyGameState;
class UBaroqueCancelState;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPostProcessComponent;
class UPrimitiveComponent;

/**
 * A frozen, coloured copy of a fighter's drawn meshes that fades out: where a Baroque Cancel started (Strive's "I wasn't
 * actually there" ghost), the drift trail, and the echoes a slowed fighter leaves. Copies every visible skeletal mesh (the
 * drawn Visual mesh and what it wears), never the hidden driver body.
 */
UCLASS(NotPlaceable, Transient)
class NIGHTSKYENGINE_API ABaroqueAfterimage : public AActor
{
	GENERATED_BODY()

public:
	ABaroqueAfterimage();

	/** Copies Source's pose now. Life in seconds. False if there was nothing to copy (the actor destroys itself). */
	bool Init(const APlayerObject* Source, UMaterialInterface* Material, const FLinearColor& Color, float Opacity, float Life);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	float Age = 0.f;
	float LifeSeconds = 0.5f;
	float StartOpacity = 0.6f;
};

/**
 * Madrone change: the Baroque Cancel's look, driven by ANightSkyGameState after each frame it shows (not resimulated ones):
 * - the screen darkens during the super freeze, tinted with the cancel's colour; the fighters stay lit (they're drawn into
 *   the custom stencil buffer while it's dark, and the post-process material skips those pixels);
 * - an afterimage stays where the cancel started, and a drift leaves a trail of them;
 * - a slowed fighter leaves fading echoes in the colour of the cancel that slowed it.
 * Settings: Project Settings > Night Sky Engine > Battle Rules > Baroque Cancel Visuals.
 */
UCLASS(Transient)
class NIGHTSKYENGINE_API UBaroqueCancelVisuals : public UObject
{
	GENERATED_BODY()

public:
	void Update(ANightSkyGameState* GameState);

	/** Screen back to normal, fighters' stencil put back (end of the battle). */
	void Reset();

	/** The Baroque Cancel state in a fighter's state list (for its colours), or null. */
	static const UBaroqueCancelState* FindBaroqueState(const APlayerObject* Player);

	/** How dark the screen is right now (0-1). For tests. */
	float GetDarkness() const { return Darkness; }

private:
	void UpdateDarkness(ANightSkyGameState* GameState, float Target, const FLinearColor& Tint);
	void MarkFighters(const ANightSkyGameState* GameState);
	void UnmarkFighters();
	void SpawnAfterimage(const APlayerObject* Player, const FLinearColor& Color, float Opacity, float Life) const;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DarkenMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AfterimageMaterial;

	float Darkness = 0.f;
	FLinearColor DarkTint = FLinearColor::White;

	/** Per fighter: the battle frame its last Baroque Cancel started (one start ghost each), and the last frame a trail
	 * ghost or a slowdown echo was made (once per battle frame, however often it's shown). */
	TMap<TObjectKey<APlayerObject>, int32> LastStartFrame;
	TMap<TObjectKey<APlayerObject>, int32> LastTrailFrame;
	TMap<TObjectKey<APlayerObject>, int32> LastEchoFrame;

	struct FMarked
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		bool bRenderedCustomDepth = false;
		int32 StencilValue = 0;
	};
	TArray<FMarked> Marked;
};
