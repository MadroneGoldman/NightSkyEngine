// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParticleManager.generated.h"

class UNiagaraComponent;
class ABattleObject;

USTRUCT()
struct FBattleParticle
{
	GENERATED_BODY()
	
	FBattleParticle()
	{
		NiagaraComponent = nullptr;
		ParticleOwner = nullptr;
	}
	
	FBattleParticle(UNiagaraComponent* InNiagaraComponent, ABattleObject* InOwner)
		: NiagaraComponent(InNiagaraComponent), ParticleOwner(InOwner) {}
	
	UPROPERTY()
	UNiagaraComponent* NiagaraComponent;
	UPROPERTY()
	ABattleObject* ParticleOwner;
};

UCLASS()
class NIGHTSKYENGINE_API AParticleManager : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FBattleParticle> BattleParticles;

	// Sets default values for this actor's properties
	AParticleManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void UpdateParticles();
	void PauseParticles();
	/** Madrone change: destroys every battle particle. Only UpdateParticles ages them (desired age mode), so whatever was
	 * alive when a battle ended stayed frozen in the world. */
	void ClearParticles();
	void RollbackParticles(int RollbackFrames);
};
