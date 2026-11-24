// Madrone change: Baroque Cancel, a universal Roman Cancel (Guilty Gear -Strive-) paid with recoverable health.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "State.h"
#include "BaroqueCancelState.generated.h"

// The four kinds' cels (each character maps them to an animation in its Collision Data; a T-pose until it has its own)
// and their effects in the common Particle Data.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cel_BaroqueCancel_Blue);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cel_BaroqueCancel_Red);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cel_BaroqueCancel_Purple);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cel_BaroqueCancel_Yellow);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Particle_BaroqueCancel_Blue);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Particle_BaroqueCancel_Red);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Particle_BaroqueCancel_Purple);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Particle_BaroqueCancel_Yellow);

/** Which Baroque Cancel a fighter gets, from what it was doing when it pressed it (like Strive's Roman Cancel colours). */
UENUM(BlueprintType)
enum class EBaroqueCancelType : uint8
{
	/** Neutral or moving. Long slowdown that lasts through hits and blocks. */
	Blue,
	/** During an attack that made contact (hit or block). On hit the shockwave launches the opponent. */
	Red,
	/** During an attack that hasn't made contact. Short slowdown. */
	Purple,
	/** While blocking. Guard-crushes an opponent who isn't blocking and pushes them away. */
	Yellow,
};

/** One kind's numbers. Frames are battle frames (60 a second). The defaults are Strive's. */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FBaroqueCancelKind
{
	GENERATED_BODY()

	/** The pose (a cel in the character's Collision Data). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	FGameplayTag Cel;

	/** The effect (a Particle Data entry: the character's own, else the common one). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	FGameplayTag Particle;

	/** The fighter's colour flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	FLinearColor Color = FLinearColor(1, 0.1f, 0.1f, 1);

	/** Everyone else stands still this long from the press; the fighter doesn't. The shockwave goes off when it ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 SuperFreezeFrames = 20;

	/** The whole state, press to recovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 1))
	int32 TotalFrames = 45;

	/** An opponent the shockwave reaches runs at half speed this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 SlowdownFrames = 39;

	/** The slowdown lasts through hits and blocks (Blue). Otherwise either fighter's next hit or block ends it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	bool bPersistentSlowdown = false;

	/** Frames (from the press) during which an attack cancels the Baroque Cancel ("Roman Cancel Cancel"). -1 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	int32 CancelWindowStart = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	int32 CancelWindowEnd = 13;

	/** Strike invulnerable from the press this long (Yellow: until the super freeze ends). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 InvulnerableFrames = 0;
};

/**
 * Madrone change: Baroque Cancel, Strive's Roman Cancel paid with recoverable health (Battle Rules > Baroque Cancel).
 * Cancels whatever the fighter is doing; the kind comes from what that was (APlayerObject::GetBaroqueCancelType). Each
 * kind has its own pose, effect and numbers, set here: a character swaps them with a Blueprint child of this state in
 * its state list. Everything is worked out from the frame in the state, so it replays the same after a rollback.
 */
UCLASS(Blueprintable)
class NIGHTSKYENGINE_API UBaroqueCancelState : public UState
{
	GENERATED_BODY()

public:
	UBaroqueCancelState();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel")
	FBaroqueCancelKind Blue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel")
	FBaroqueCancelKind Red;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel")
	FBaroqueCancelKind Purple;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel")
	FBaroqueCancelKind Yellow;

	/** Red on hit: how long the launched opponent can't tech (Strive: 55 for a neutral Red). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	int32 RedLaunchUntech = 55;

	/** Red on hit: the launch's speed away and up (Night Sky units per frame). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	int32 RedLaunchSpeedX = 1500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	int32 RedLaunchSpeedY = 30000;

	/** Red on block: the opponent stays in blockstun at least this long (Strive: 12). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	int32 RedBlockstun = 12;

	/** Yellow on an opponent who isn't blocking: guard crush, frames after the slowdown (Strive: 43 with the slowdown). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yellow Guard Crush")
	int32 YellowGuardCrushFrames = 33;

	/** Yellow: how hard the opponent is pushed away (a hit's ground pushback; Strive pushes to about round-start distance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Yellow Guard Crush")
	int32 YellowPushback = 60000;

	/** The kind's numbers. */
	UFUNCTION(BlueprintPure, Category = "Baroque Cancel")
	FBaroqueCancelKind GetKind(EBaroqueCancelType Type) const;

	virtual void CallExec() override;

private:
	void Shockwave(class APlayerObject* Self, EBaroqueCancelType Type, const FBaroqueCancelKind& Kind) const;
};
