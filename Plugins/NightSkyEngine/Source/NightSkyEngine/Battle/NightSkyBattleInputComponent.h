// NSE battle input as a component, so any player controller can feed a battle (not only ANightSkyPlayerController).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NightSkyBattleInputComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class APlayerController;

USTRUCT()
struct FBattleInputActions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressUp;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseUp;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressDown;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseDown;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressLeft;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseLeft;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressRight;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseRight;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressA;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseA;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressB;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseB;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressC;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseC;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressD;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseD;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressE;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseE;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressF;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseF;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressG;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseG;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PressH;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ReleaseH;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> PauseGame;
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UInputAction> ResetTraining;

	bool HasAnyAction() const { return PressUp || PressA || PauseGame; }
};

DECLARE_MULTICAST_DELEGATE(FOnNightSkyBattleInputEvent);

/**
 * Collects the battle input bits (INP_* in Bitflags.h) for one local player from the NSE Enhanced Input actions.
 * Lives on a player controller. ANightSkyGameState::GetLocalInputs reads it from whatever controller the local
 * player has, so a game's own controller (e.g. a Narrative one) can run NSE battles by adding this component.
 *
 * SetBattleInputEnabled(false) ignores presses and clears held bits, so exploration input never leaks into a battle
 * and no direction stays held when a battle ends.
 */
UCLASS(ClassGroup=(NightSky), meta=(BlueprintSpawnableComponent))
class NIGHTSKYENGINE_API UNightSkyBattleInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightSkyBattleInputComponent();

	/** Battle mapping context (IMC_Battle). Added by AddMapping, removed by RemoveMapping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	/** Priority of InputMapping. Put it above exploration contexts when both are added. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditAnywhere, Category="Input")
	FBattleInputActions InputActions;

	/** Current INP_* bits for this player. */
	UPROPERTY(BlueprintReadOnly, Category="Input")
	int32 Inputs = 0;

	UPROPERTY(BlueprintReadOnly, Category="Input")
	bool bBattleInputEnabled = true;

	/** Fired by the PauseGame action (only while battle input is enabled). */
	FOnNightSkyBattleInputEvent OnPauseGame;

	/** Binds InputActions on Input. Call from the owning controller's SetupInputComponent. */
	void BindActions(UEnhancedInputComponent* Input);

	/** Adds InputMapping to the owning controller's local player (with user settings notification, so remaps apply). */
	UFUNCTION(BlueprintCallable, Category="NightSky|Input")
	void AddMapping();

	UFUNCTION(BlueprintCallable, Category="NightSky|Input")
	void RemoveMapping();

	/** Enables or disables battle input. Disabling also clears the held bits. */
	UFUNCTION(BlueprintCallable, Category="NightSky|Input")
	void SetBattleInputEnabled(bool bEnabled);

	/** Clears every held bit (directions and buttons) but keeps the hidden ones (rematch, training reset). */
	UFUNCTION(BlueprintCallable, Category="NightSky|Input")
	void ClearHeldInputs();

	/** The battle input component on Controller, if any. */
	static UNightSkyBattleInputComponent* FindOn(const AActor* Controller);

private:
	APlayerController* GetOwningPlayerController() const;
	void HandlePress(int32 Bit);
	void HandleRelease(int32 Bit);
	void HandlePause();
	void HandleResetTraining();
};
