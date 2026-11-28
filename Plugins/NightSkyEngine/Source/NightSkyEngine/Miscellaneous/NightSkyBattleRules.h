#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Materials/MaterialInterface.h"
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
	 * Proximity guard: holding back near an active attack puts the fighter in its guard state before the hit arrives.
	 * Off (the default, user's choice 2026-11-27): a fighter only blocks when the hit lands, so the guard state (and its
	 * sound) is entered once per blocked hit, never for attacks that whiff or are still out of reach.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Blocking")
	bool bProximityGuard = false;

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

	/**
	 * Baroque Cancel (a Guilty Gear -Strive- Roman Cancel, UBaroqueCancelState): recoverable health it costs. It can't be
	 * started without any; with less than this it takes what's left.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel", meta = (ClampMin = "0"))
	int32 BaroqueCancelCost = 2000;

	/** How far the Baroque Cancel shockwave reaches (Night Sky units, about 2326 per cm), across and up. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel", meta = (ClampMin = "0"))
	int32 BaroqueShockwaveRadius = 650000;

	/** A Baroque Cancel pressed in a normal's first frames (that hasn't hit) counts as if the normal never started: Blue,
	 * or Red while the opponent is still in hit or block stun from the move before. A+C+E pressed a frame or two apart
	 * fires the first button's normal before the three complete, which would make every sloppy press a Purple. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel", meta = (ClampMin = "0"))
	int32 BaroqueKaraFrames = 3;

	/** Presentation only (not rollback state, safe to differ between machines): Baroque Cancel looks. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel Visuals")
	bool bBaroqueVisuals = true;

	/** Post-process material that darkens the screen during the freeze, except the fighters (custom stencil). Scalar
	 * parameters Darkness, FighterStencil; vector Tint. Built by witch_tools_battle.baroque_cancel.build_materials(). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel Visuals")
	TSoftObjectPtr<UMaterialInterface> BaroqueScreenDarkenMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/WitchTools/BattleCommon/NightSkyEngine/CharacterAssets/Common/Particles/Materials/M_WitchBaroqueScreenDarken.M_WitchBaroqueScreenDarken")));

	/** Translucent material for afterimages (the spot the cancel started, the drift trail, a slowed fighter's echoes).
	 * Parameters Color (vector), Opacity (scalar). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel Visuals")
	TSoftObjectPtr<UMaterialInterface> BaroqueAfterimageMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/WitchTools/BattleCommon/NightSkyEngine/CharacterAssets/Common/Particles/Materials/M_WitchBaroqueAfterimage.M_WitchBaroqueAfterimage")));

	/** Custom stencil value the fighters are drawn with while the screen is dark (so the darkening skips them). Night Sky's
	 * particles use 2, the datamosh mask 77, Pepper's ghost silhouettes 201. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel Visuals", meta = (ClampMin = "1", ClampMax = "255"))
	int32 BaroqueFighterStencil = 3;

	/** A slowed fighter leaves a fading echo of itself every this many frames (0 = none). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Baroque Cancel Visuals", meta = (ClampMin = "0"))
	int32 SlowdownEchoInterval = 5;

	/**
	 * CPU: chance in percent, per decision, at CPU Difficulty 100 (scaled down with difficulty), that a CPU Baroque
	 * Cancels when it's worth it: Red after its attack hit (to extend the combo), Yellow (half as often) while blocking.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CPUBaroqueChance = 30;

	/**
	 * CPU: how far away (engine units, about 2326 per cm) the enemy must be for a projectile move (CPU Data > Projectile)
	 * to get the far-away bonus. The move's own CPU Data X/Y range must also contain the enemy.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUFarProjectileDistance = 600000;

	/**
	 * CPU: average weight a projectile gets from far away (a random half to one and a half times this). Walking and
	 * dashing in score about 150 there, an in-range attack about 75: higher throws more fireballs. 0 turns it off.
	 * Measured keeping away for 20 s from Dark Knight: 0 = 5 fireballs, 40 = 22 (mixed with dashing in), 100 = 56 (nonstop).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUFarProjectileBonus = 40;

	/** CPU: average extra weight for crouching attacks (entry stance Crouching) against a grounded enemy. 0 turns it off. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUCrouchAttackBonus = 25;

	/**
	 * CPU: average extra weight for the attack that beats how the enemy is standing: lows (CPU Data > Block Type Low)
	 * against a standing enemy, overheads (High) against a crouching one, blocking or not. Jumps toward a crouching enemy
	 * at jump-in range get it too, so the CPU jumps in with an air overhead. 0 turns it off.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUMixupBonus = 50;

	/**
	 * CPU: average extra weight for cancelling an attack into one of its cancel options, at CPU Difficulty 100 (scaled
	 * down with the fighter's difficulty; 0 difficulty gets none). The difficulty also shortens the CPU's wait between
	 * decisions while it's attacking, from 7 frames down to every frame, so it catches short cancel windows.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUCancelBonus = 150;

	/**
	 * CPU: weight a fully patient (100) or fully impatient (0) fighter moves between waiting / backing off / backdashing
	 * and approaching / attacking in neutral. A fighter's CPU Patience 50 changes nothing; 0 turns it off for everyone.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "CPU", meta = (ClampMin = "0"))
	int32 CPUPatienceStrength = 60;

	/**
	 * Turns auto exposure (eye adaptation) off for the whole battle: the picture keeps the brightness it had when the
	 * battle started, so supers, darkening, flashes and camera moves don't make it brighten or dim. Presentation only.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bDisableAutoExposureInBattle = true;

	/**
	 * With auto exposure off: lock to this fixed exposure (EV100) instead of the brightness at battle start.
	 * Leave off to keep the battle as bright as the place it started in.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bDisableAutoExposureInBattle"))
	bool bUseFixedBattleExposure = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bDisableAutoExposureInBattle && bUseFixedBattleExposure"))
	float FixedBattleExposureEV100 = 10.f;

	/**
	 * Madrone change: character pairs checked in every battle (when both characters are on screen together, their event
	 * runs). CHR_ assets and game states can add their own. Docs: NSE Docs/10-Character-Systems.md.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Interactions")
	TArray<TSoftObjectPtr<class UNSECharacterInteractionData>> InteractionSets;

	/** Madrone change: the battle subtitle element (a UNSEBattleSubtitleWidget child). Presentation only. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Interactions")
	TSoftClassPtr<class UNSEBattleSubtitleWidget> BattleSubtitleWidget = TSoftClassPtr<class UNSEBattleSubtitleWidget>(FSoftObjectPath(
		TEXT("/WitchTools/BattleCommon/NightSkyEngine/UI/Battle/WBP_BattleSubtitle.WBP_BattleSubtitle_C")));

	/** Madrone change: the subtitle element's viewport Z order. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Interactions")
	int32 BattleSubtitleZOrder = 60;

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Night Sky Engine"); }
};
