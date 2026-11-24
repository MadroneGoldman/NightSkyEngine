// Madrone change: Baroque Cancel, a universal Roman Cancel (Guilty Gear -Strive-) paid with recoverable health.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "State.h"
#include "NightSkyEngine/Battle/Misc/Bitflags.h"
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

/** Drift: a dash in a cardinal direction just before the Baroque Cancel moves the fighter that way during the freeze
 * (Strive's Dash Input Roman Cancel). Not for Yellow. */
UENUM(BlueprintType)
enum class EBaroqueDrift : uint8
{
	None,
	Forward,
	Back,
	Up,
	Down,
};

/** How a Red shockwave launches after a drift in one direction, compared with the neutral launch. */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FBaroqueDriftLaunch
{
	GENERATED_BODY()

	FBaroqueDriftLaunch() = default;
	FBaroqueDriftLaunch(const int32 InUntech, const int32 InX, const int32 InY) : Untech(InUntech), SpeedXPercent(InX), SpeedYPercent(InY) {}

	/** How long the launched opponent can't tech. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	int32 Untech = 55;

	/** Percent of the neutral launch's speed away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	int32 SpeedXPercent = 100;

	/** Percent of the neutral launch's speed up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	int32 SpeedYPercent = 100;
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

	/** Super freeze: everyone else stands still this long from the press (Strive: Blue 20, Red 25, Purple 20, Yellow 12);
	 * the fighter doesn't (it drifts). When it ends the shockwave goes off and the fighter can act. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 SuperFreezeFrames = 20;

	/** The whole state, press to recovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 1))
	int32 TotalFrames = 45;

	/** Slowdown, separate from (and after) the super freeze: an opponent the shockwave reaches runs at half speed this
	 * long (Strive: Blue 59, Red 39, Purple 19, Yellow 10). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 SlowdownFrames = 39;

	/** The slowdown lasts through hits and blocks (Blue). Otherwise either fighter's next hit or block ends it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	bool bPersistentSlowdown = false;

	/** The fighter can act (attack, move, block, jump) from the first frame after the super freeze: the state ends there.
	 * Off = it plays out to Total Frames (Yellow's recovery). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baroque Cancel")
	bool bActionableAfterFreeze = true;

	/** How dark the screen goes during the super freeze (0 = not at all, 1 = black). The fighters stay lit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (ClampMin = 0, ClampMax = 1))
	float ScreenDarken = 0.6f;

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

	/** Drift: the dash button (F on every fighter; any direction, or neutral for forward). A double tap (66, 44, 88, 22)
	 * works too. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (Bitmask, BitmaskEnum = "/Script/NightSkyEngine.EInputFlags"))
	int32 DriftDashButton = INP_F;

	/** Drift: how many frames before the press the dash still counts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = 1))
	int32 DriftInputFrames = 8;

	/** Drift: how many frames the fighter moves (from the press; it slows to a stop). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift", meta = (ClampMin = 1))
	int32 DriftFrames = 16;

	/** Drift: starting speed sideways and up / down (Night Sky units per frame; a forward dash starts at 13000). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift")
	int32 DriftSpeedX = 24000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drift")
	int32 DriftSpeedY = 20000;

	/** Red's launch after a drift (Strive: forward lower and farther, back higher and closer, up much higher, down much
	 * lower; untech Up 68 > Back 62 > Neutral 55 > Forward 48 > Down 42). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	FBaroqueDriftLaunch RedLaunchForward = {48, 200, 70};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	FBaroqueDriftLaunch RedLaunchBack = {62, 50, 120};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	FBaroqueDriftLaunch RedLaunchUp = {68, 40, 145};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Red Launch")
	FBaroqueDriftLaunch RedLaunchDown = {42, 40, 50};

	/** After a Baroque Cancel ends, frames before another can start (G or A+C+E mashed at the end of the freeze would
	 * otherwise start a second one at once). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel", meta = (ClampMin = 0))
	int32 RelockFrames = 10;

	/** The kind's numbers. */
	UFUNCTION(BlueprintPure, Category = "Baroque Cancel")
	FBaroqueCancelKind GetKind(EBaroqueCancelType Type) const;

	virtual void CallExec() override;

private:
	void Shockwave(class APlayerObject* Self, EBaroqueCancelType Type, const FBaroqueCancelKind& Kind) const;
};
