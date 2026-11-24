#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NightSkyBattleRules.generated.h"

/**
 * Madrone change: gameplay rules that every machine in a match must agree on. They live in the project's config
 * (DefaultGame.ini), which ships with the game, so online peers always read the same values. Don't move them to a
 * per-user config: a different value on one peer desyncs the match.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Battle Rules"))
class NIGHTSKYENGINE_API UNightSkyBattleRules : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNightSkyBattleRules();

	/**
	 * Percent less damage dealt by a move entered with a simple input (E + a direction + a button) instead of its motion.
	 * Applies to its hits, chip damage and armor damage, and to objects (projectiles) it creates.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simple Input", meta = (ClampMin = "0", ClampMax = "100", Units = "Percent"))
	int32 SimpleInputDamageReductionPercent = 10;

	/** Damage after the simple-input reduction (integer maths, the same on every machine). */
	static int32 ScaleSimpleInputDamage(int32 Damage);

	/**
	 * Just Defend (Fatal Fury style): pressing back (any back direction, standing, crouching or in the air) shortly before
	 * a hit arrives, and blocking it, takes no chip damage, heals a little, turns some recoverable health back into
	 * health and shows the Particle.JustDefend effect instead of the guard effect.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Just Defend")
	bool bEnableJustDefend = true;

	/** How many frames before the hit the back press may come (the press frame counts as the first). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Just Defend", meta = (ClampMin = "1", ClampMax = "30"))
	int32 JustDefendWindowFrames = 4;

	/** Health a Just Defend gives back (fighters have 10000 by default). Never above the fighter's max health. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Just Defend", meta = (ClampMin = "0"))
	int32 JustDefendHeal = 100;

	/** Recoverable (grey) health a Just Defend turns back into health, on top of the heal. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Just Defend", meta = (ClampMin = "0"))
	int32 JustDefendRecoverableHeal = 250;

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Night Sky Engine"); }
};
