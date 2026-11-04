#include "NightSkyBattleInputComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "NightSkyEngine/Battle/Misc/Bitflags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(NightSkyBattleInputComponent)

UNightSkyBattleInputComponent::UNightSkyBattleInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNightSkyBattleInputComponent::BindActions(UEnhancedInputComponent* Input)
{
	if (!Input) return;

	const auto BindPair = [this, Input](const UInputAction* Press, const UInputAction* Release, const int32 Bit)
	{
		if (IsValid(Press)) Input->BindAction(Press, ETriggerEvent::Triggered, this, &ThisClass::HandlePress, Bit);
		if (IsValid(Release)) Input->BindAction(Release, ETriggerEvent::Triggered, this, &ThisClass::HandleRelease, Bit);
	};
	const FBattleInputActions& A = InputActions;
	BindPair(A.PressUp, A.ReleaseUp, INP_Up);
	BindPair(A.PressDown, A.ReleaseDown, INP_Down);
	BindPair(A.PressLeft, A.ReleaseLeft, INP_Left);
	BindPair(A.PressRight, A.ReleaseRight, INP_Right);
	BindPair(A.PressA, A.ReleaseA, INP_A);
	BindPair(A.PressB, A.ReleaseB, INP_B);
	BindPair(A.PressC, A.ReleaseC, INP_C);
	BindPair(A.PressD, A.ReleaseD, INP_D);
	BindPair(A.PressE, A.ReleaseE, INP_E);
	BindPair(A.PressF, A.ReleaseF, INP_F);
	BindPair(A.PressG, A.ReleaseG, INP_G);
	BindPair(A.PressH, A.ReleaseH, INP_H);
	if (IsValid(A.PauseGame)) Input->BindAction(A.PauseGame.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandlePause);
	if (IsValid(A.ResetTraining)) Input->BindAction(A.ResetTraining.Get(), ETriggerEvent::Triggered, this, &ThisClass::HandleResetTraining);
}

APlayerController* UNightSkyBattleInputComponent::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UNightSkyBattleInputComponent::AddMapping()
{
	const APlayerController* PC = GetOwningPlayerController();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || InputMapping.IsNull()) return;
	if (UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		FModifyContextOptions Options;
		Options.bNotifyUserSettings = true;
		InputSystem->AddMappingContext(InputMapping.LoadSynchronous(), MappingPriority, Options);
	}
}

void UNightSkyBattleInputComponent::RemoveMapping()
{
	const APlayerController* PC = GetOwningPlayerController();
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	if (!LocalPlayer || InputMapping.IsNull()) return;
	if (UEnhancedInputLocalPlayerSubsystem* InputSystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		if (UInputMappingContext* Context = InputMapping.Get())
		{
			InputSystem->RemoveMappingContext(Context);
		}
	}
}

void UNightSkyBattleInputComponent::SetBattleInputEnabled(const bool bEnabled)
{
	bBattleInputEnabled = bEnabled;
	if (!bEnabled) ClearHeldInputs();
}

void UNightSkyBattleInputComponent::ClearHeldInputs()
{
	Inputs &= (INP_Rematch | INP_ResetTraining);
}

UNightSkyBattleInputComponent* UNightSkyBattleInputComponent::FindOn(const AActor* Controller)
{
	return Controller ? Controller->FindComponentByClass<UNightSkyBattleInputComponent>() : nullptr;
}

void UNightSkyBattleInputComponent::HandlePress(const int32 Bit)
{
	if (bBattleInputEnabled) Inputs |= Bit;
}

void UNightSkyBattleInputComponent::HandleRelease(const int32 Bit)
{
	// Releases always clear, so a key let go while input was disabled can't stay held.
	Inputs &= ~Bit;
}

void UNightSkyBattleInputComponent::HandlePause()
{
	if (bBattleInputEnabled) OnPauseGame.Broadcast();
}

void UNightSkyBattleInputComponent::HandleResetTraining()
{
	if (bBattleInputEnabled) Inputs |= INP_ResetTraining;
}
