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

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Night Sky Engine"); }
};
