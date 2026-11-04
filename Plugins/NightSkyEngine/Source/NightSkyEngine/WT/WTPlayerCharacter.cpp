// Fill out your copyright notice in the Description page of Project Settings.


#include "WTPlayerCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "AlsCameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Utility/AlsVector.h"
#include "NightSkyWTGameState.h"

// Sets default values
AWTPlayerCharacter::AWTPlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//Als set up
	Camera = CreateDefaultSubobject<UAlsCameraComponent>(FName{TEXTVIEW("Camera")});
	Camera->SetupAttachment(GetMesh());
	Camera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}

// Called when the game starts or when spawned
void AWTPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	GameState = Cast<ANightSkyWTGameState>(ANightSkyGameState::Get(this));
	if (GameState)
		GameState->OnBattleEndDelegate.AddUniqueDynamic(this, &AWTCharacter::EndBattle);
}

// Called every frame
void AWTPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AWTPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	AddWTInputMappingContext();
	auto* EnhancedInput{Cast<UEnhancedInputComponent>(PlayerInputComponent)};
	if (IsValid(EnhancedInput))
	{
		EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLookMouse);
		EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnLookMouse);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLook);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnLook);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnMove);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnMove);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnSprint);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnSprint);
		EnhancedInput->BindAction(WalkAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnWalk);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnCrouch);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnJump);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnAim);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Canceled, this, &ThisClass::Input_OnAim);
		EnhancedInput->BindAction(RagdollAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRagdoll);
		EnhancedInput->BindAction(RotationModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRotationMode);
		EnhancedInput->BindAction(ViewModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnViewMode);
		EnhancedInput->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnSwitchShoulder);
		
	}
}

void AWTPlayerCharacter::StartBattle(AWTCharacter* Opponent)
{
	RemoveWTInputMappingContext();
	
	//Spawn battle player
	UPrimaryCharaData* LoadedCharacterData = CharaData.LoadSynchronous();
	BattlePlayer = GetWorld()->SpawnActor<APlayerObject>(LoadedCharacterData->PlayerClass);
	BattlePlayer->CharacterName = LoadedCharacterData->CharaFriendlyName;
	BattlePlayer->SetActorHiddenInGame(true);
	
	//Spawn opponent battle player
	UPrimaryCharaData* LoadedOppCharacterData = Opponent->CharaData.LoadSynchronous();
	Opponent->BattlePlayer = GetWorld()->SpawnActor<APlayerObject>(LoadedOppCharacterData->PlayerClass);
	Opponent->BattlePlayer->CharacterName = LoadedOppCharacterData->CharaFriendlyName;
	Opponent->BattlePlayer->SetActorHiddenInGame(true);
	
	// Hide WT character and show battle player.
	SetActorHiddenInGame(true);
	BattlePlayer->SetActorHiddenInGame(false);
	Opponent->SetActorHiddenInGame(true);
	Opponent->BattlePlayer->SetActorHiddenInGame(false);
	Opponent->bNPCInBattle = true; 

	APlayerObject* PlayerTagTeammate = nullptr;
	APlayerObject* PlayerAssistTeammate = nullptr;
	TArray<APlayerObject*> OpponentAllies = {};
	
	if (!Allies.IsEmpty())
	{
		//Set tag
		if (CurrentTagAlly > -1 && Allies[CurrentTagAlly].LoadSynchronous() != nullptr && !Allies[CurrentTagAlly].LoadSynchronous()->IsAssist)
		{
			PlayerTagTeammate = GetWorld()->SpawnActor<APlayerObject>(Allies[CurrentTagAlly].LoadSynchronous()->PlayerClass); 
			PlayerTagTeammate->CharacterName = Allies[CurrentTagAlly].LoadSynchronous()->CharaFriendlyName; 
			PlayerTagTeammate->SetActorHiddenInGame(true);
		}
		
		//Set assist
		if (CurrentAssistAlly > -1 && Allies[CurrentAssistAlly].LoadSynchronous() != nullptr && Allies[CurrentAssistAlly].LoadSynchronous()->IsAssist)
		{
			PlayerAssistTeammate = GetWorld()->SpawnActor<APlayerObject>(Allies[CurrentAssistAlly].LoadSynchronous()->PlayerClass); 
			PlayerAssistTeammate->CharacterName = Allies[CurrentAssistAlly].LoadSynchronous()->CharaFriendlyName; 
			PlayerAssistTeammate->SetActorHiddenInGame(true);
		}
	}
	if (!Opponent->Allies.IsEmpty())
	{	for (TSoftObjectPtr<UPrimaryCharaData> OpponentTeammateData : Opponent->Allies )
	{
		UPrimaryCharaData* LoadedOppTeamateData = OpponentTeammateData.LoadSynchronous();
		APlayerObject* OpponentTeammate = nullptr;
		OpponentTeammate = GetWorld()->SpawnActor<APlayerObject>(LoadedOppTeamateData->PlayerClass); 
		OpponentTeammate->CharacterName = LoadedOppTeamateData->CharaFriendlyName; 
		OpponentTeammate->SetActorHiddenInGame(true);
		OpponentAllies.Add(OpponentTeammate);
	}
		
	}
	
	GameState->Init(BattlePlayer, Opponent->BattlePlayer, PlayerTagTeammate, PlayerAssistTeammate, OpponentAllies );
}

void AWTPlayerCharacter::EndBattle()
{
	SetActorHiddenInGame(false);
	AddWTInputMappingContext();
		
	//DestroyBattleActor
	BattlePlayer->SetActorHiddenInGame(true);
	BattlePlayer->Destroy();
	
	EndBattle_BP(GameState->BattleState.CurrentWinSide);	
}

void AWTPlayerCharacter::AddWTInputMappingContext()
{
	auto* NewPlayer{Cast<APlayerController>(GetController())};
	if (IsValid(NewPlayer))
	{
		NewPlayer->InputYawScale_DEPRECATED = 1.0f;
		NewPlayer->InputPitchScale_DEPRECATED = 1.0f;
		NewPlayer->InputRollScale_DEPRECATED = 1.0f;

		auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(NewPlayer->GetLocalPlayer())};
		if (IsValid(InputSubsystem))
		{
			FModifyContextOptions Options;
			Options.bNotifyUserSettings = true;

			InputSubsystem->AddMappingContext(InputMappingContext, 0, Options);
		}
	}
}

void AWTPlayerCharacter::RemoveWTInputMappingContext()
{
	auto* NewPlayer{Cast<APlayerController>(GetController())};
	if (IsValid(NewPlayer))
	{
		auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(NewPlayer->GetLocalPlayer())};
		if (IsValid(InputSubsystem))
		{
			InputSubsystem->RemoveMappingContext(InputMappingContext);
		}
	}
}

//Als stuff
void AWTPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
}

void AWTPlayerCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	if (Camera->IsActive())
	{
		Camera->GetViewInfo(ViewInfo);
		return;
	}

	Super::CalcCamera(DeltaTime, ViewInfo);
}

void AWTPlayerCharacter::Input_OnLookMouse(const FInputActionValue& ActionValue)
{
	const FVector2f Value{ActionValue.Get<FVector2D>()};

	AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
	AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void AWTPlayerCharacter::Input_OnLook(const FInputActionValue& ActionValue)
{
	const FVector2f Value{ActionValue.Get<FVector2D>()};

	AddControllerPitchInput(Value.Y * LookUpRate);
	AddControllerYawInput(Value.X * LookRightRate);
}

void AWTPlayerCharacter::Input_OnMove(const FInputActionValue& ActionValue)
{
	const auto Value{UAlsVector::ClampMagnitude012D(ActionValue.Get<FVector2D>())};

	auto ViewRotation{GetViewState().Rotation};

	if (IsValid(GetController()))
	{
		// Use exact camera rotation instead of target rotation whenever possible.

		FVector ViewLocation;
		GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}

	const auto ForwardDirection{UAlsVector::AngleToDirectionXY(UE_REAL_TO_FLOAT(ViewRotation.Yaw))};
	const auto RightDirection{UAlsVector::PerpendicularCounterClockwiseXY(ForwardDirection)};

	AddMovementInput(ForwardDirection * Value.Y + RightDirection * Value.X);
}

void AWTPlayerCharacter::Input_OnSprint(const FInputActionValue& ActionValue)
{
	SetDesiredGait(ActionValue.Get<bool>() ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void AWTPlayerCharacter::Input_OnWalk()
{
	if (GetDesiredGait() == AlsGaitTags::Walking)
	{
		SetDesiredGait(AlsGaitTags::Running);
	}
	else if (GetDesiredGait() == AlsGaitTags::Running)
	{
		SetDesiredGait(AlsGaitTags::Walking);
	}
}

void AWTPlayerCharacter::Input_OnCrouch()
{
	if (GetDesiredStance() == AlsStanceTags::Standing)
	{
		SetDesiredStance(AlsStanceTags::Crouching);
	}
	else if (GetDesiredStance() == AlsStanceTags::Crouching)
	{
		SetDesiredStance(AlsStanceTags::Standing);
	}
}

void AWTPlayerCharacter::Input_OnJump(const FInputActionValue& ActionValue)
{
	if (ActionValue.Get<bool>())
	{
		if (StopRagdolling())
		{
			return;
		}

		if (StartMantling())
		{
			return;
		}

		if (GetStance() == AlsStanceTags::Crouching)
		{
			SetDesiredStance(AlsStanceTags::Standing);
			return;
		}

		Jump();
	}
	else
	{
		StopJumping();
	}
}

void AWTPlayerCharacter::Input_OnAim(const FInputActionValue& ActionValue)
{
	SetDesiredAiming(ActionValue.Get<bool>());
}

void AWTPlayerCharacter::Input_OnRagdoll()
{
	if (!StopRagdolling())
	{
		StartRagdolling();
	}
}

void AWTPlayerCharacter::Input_OnRotationMode()
{
	SetDesiredRotationMode(GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
		                       ? AlsRotationModeTags::ViewDirection
		                       : AlsRotationModeTags::VelocityDirection);
}

void AWTPlayerCharacter::Input_OnViewMode()
{
	SetViewMode(GetViewMode() == AlsViewModeTags::ThirdPerson ? AlsViewModeTags::FirstPerson : AlsViewModeTags::ThirdPerson);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void AWTPlayerCharacter::Input_OnSwitchShoulder()
{
	Camera->SetRightShoulder(!Camera->IsRightShoulder());
}

void AWTPlayerCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& Unused, float& VerticalLocation)
{
	if (Camera->IsActive())
	{
		Camera->DisplayDebug(Canvas, DisplayInfo, VerticalLocation);
	}

	Super::DisplayDebug(Canvas, DisplayInfo, Unused, VerticalLocation);
}

// Function to get mirrored socket transform
FTransform AWTPlayerCharacter::GetMirroredSocketWorldTransform(USkeletalMeshComponent* SkeletalMesh, FName SocketName)
{
	if (!SkeletalMesh) return FTransform::Identity;

	// Get the local transform of the original socket relative to the skeleton
	FTransform SocketLocalTransform = SkeletalMesh->GetSocketTransform(SocketName, RTS_Component);

	// Mirror across the X axis (usually forward is X, adjust as needed)
	FVector MirroredLocation = SocketLocalTransform.GetLocation();
	MirroredLocation.Y *= -1;  // Flip Y for left-right mirror

	FQuat MirroredRotation = SocketLocalTransform.GetRotation(); 
	// Flip the Yaw for mirroring (basic approach)
	FRotator Rotator = MirroredRotation.Rotator();
	Rotator.Yaw *= -1;
	MirroredRotation = Rotator.Quaternion();

	FTransform MirroredTransform(MirroredRotation, MirroredLocation, SocketLocalTransform.GetScale3D());

	// Convert to world space
	FTransform WorldTransform = MirroredTransform * SkeletalMesh->GetComponentTransform();
	return WorldTransform;
}

// Returns true if success; OutWorldLocation/OutWorldRotation are set. Madrone change
bool AWTPlayerCharacter::MirrorSocketToOtherSide(USkeletalMeshComponent* SkelComp, FName _SocketName, FVector& OutWorldLocation, FRotator& OutWorldRotation)
{
	if (!SkelComp) return false;

	// 1) Get socket transform in component (local) space
	FTransform SocketLocalTransform;
	if (SkelComp->DoesSocketExist(_SocketName))
	{
		SocketLocalTransform = SkelComp->GetSocketTransform(_SocketName, RTS_Component);
	}
	else
	{
		return false;
	}

	// 2) Build a reflection matrix that mirrors Y (left/right) in component space
	const FMatrix Mirror = FScaleMatrix(FVector(1.0f, -1.0f, 1.0f));

	// 3) Convert socket local transform to matrix, apply mirror on both sides
	FMatrix LocalMat = SocketLocalTransform.ToMatrixWithScale();
	FMatrix MirroredLocalMat = Mirror * LocalMat * Mirror;

	// 4) Extract mirrored local translation and rotation
	FVector MirroredLocalLocation = MirroredLocalMat.GetOrigin();
	FQuat MirroredLocalQuat = FQuat(MirroredLocalMat);

	// 5) Convert mirrored local transform to world space using component transform
	const FTransform ComponentTransform = SkelComp->GetComponentTransform();
	OutWorldLocation = ComponentTransform.TransformPosition(MirroredLocalLocation);
	FQuat WorldQuat = ComponentTransform.GetRotation() * MirroredLocalQuat;
	OutWorldRotation = WorldQuat.Rotator();

	return true;
}