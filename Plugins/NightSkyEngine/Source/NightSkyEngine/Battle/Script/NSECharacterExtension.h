// Madrone change: per-character unique battle systems (character extensions). Docs: Plugins/NightSkyEngine/Docs/
// 10-Character-Systems.md, WitchTools Docs/CharacterInteractions.md.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NightSkyEngine/Battle/Misc/SerializableObj.h"
#include "NSECharacterExtension.generated.h"

class APlayerObject;
class ANightSkyGameState;

/** Madrone change: a cooldown (frames) at or above this never ends this round; the HUD shows it as never ready. */
constexpr int32 NSECooldownForever = 1000000;

/**
 * Madrone change: one extra health bar a character extension adds to its side's teammate bars (next to the real tag
 * partners' bars), for teammates that aren't real fighters (thrown sons, summons). Presentation only: the HUD reads it
 * every frame from GetVirtualTeammateBars, so build it from rollback state (SaveGame variables), never keep it.
 */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FNSETeammateBar
{
	GENERATED_BODY()

	/** Shown at all (false: the bar is collapsed, e.g. a son who has left the screen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar")
	bool bVisible = true;

	/** Health, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar", meta=(ClampMin="0", ClampMax="1"))
	float Health = 1.f;

	/** Health + recoverable (grey) health, 0..1. Below Health counts as Health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar", meta=(ClampMin="0", ClampMax="1"))
	float RecoverableHealth = 1.f;

	/** Recolours the bar (the fill material's FluidColor / FluidGlowColor and the fill tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar")
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar", meta=(EditCondition="bOverrideColor"))
	FLinearColor Color = FLinearColor(1.f, 0.75f, 0.1f, 1.f);

	/** For tooling and tests only (not drawn by the stock HUD). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bar")
	FText Name;

	bool operator==(const FNSETeammateBar& Other) const
	{
		return bVisible == Other.bVisible && Health == Other.Health && RecoverableHealth == Other.RecoverableHealth
			&& bOverrideColor == Other.bOverrideColor && Color == Other.Color;
	}
};

/**
 * Madrone change: a character's own battle system ("unique mechanic"), made per fighter from its CHR_ asset's
 * Character Extensions list (or the fighter Blueprint's), and kept for the whole match.
 *
 * Rollback: everything that matters for gameplay must be in SaveGame properties (Blueprint: tick "SaveGame" in the
 * variable's advanced details). They're saved and loaded with the fighter every frame. Other properties are config or
 * presentation. Never use wall-clock time or random numbers that aren't the battle's (BattleState.RandomManager).
 * Presentation (sounds, subtitles, widgets) must skip resimulated frames: check IsResimulating().
 *
 * Every hook is a BlueprintNativeEvent: override in C++ (_Implementation) or in a Blueprint child.
 */
UCLASS(BlueprintType, Blueprintable, Abstract)
class NIGHTSKYENGINE_API UNSECharacterExtension : public USerializableObj
{
	GENERATED_BODY()

public:
	/** The fighter this extension belongs to. Set before MatchInit. */
	UPROPERTY(BlueprintReadOnly, Category="Character Extension")
	TObjectPtr<APlayerObject> Owner;

	/** The battle's game state. Set before MatchInit. */
	UPROPERTY(BlueprintReadOnly, Category="Character Extension")
	TObjectPtr<ANightSkyGameState> GameState;

	/** Optional name, for GetCharacterExtensionByName. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Extension")
	FGameplayTag Name;

	/** True while the frame being run is a rollback resimulation: skip sounds, subtitles, widgets. */
	UFUNCTION(BlueprintPure, Category="Character Extension")
	bool IsResimulating() const;

	/** The fighter on screen for the owner's side. */
	UFUNCTION(BlueprintPure, Category="Character Extension")
	APlayerObject* GetMainPlayer() const;

	/** Whether the owner's side is player 1. */
	UFUNCTION(BlueprintPure, Category="Character Extension")
	bool IsP1() const;

	// ------------------------------------------------------------------------------------------------------ lifecycle

	/** Once when the match starts (after every fighter and extension exists, before the first RoundInit). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension")
	void MatchInit();

	/** At the start of every round (after the fighters were reset and placed). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension")
	void RoundInit();

	/** Every battle frame (not during a super freeze), on screen or not, before the fighters update. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension")
	void Update();

	// ------------------------------------------------------------------------------------------------- assists, tags

	/** Called on the extensions of the fighter who called the assist (its side's main player) and of the assist
	 * itself, right after the assist came on screen. Caller: the side's main player. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Assist")
	void OnAssistCalled(APlayerObject* Caller, APlayerObject* Assist);

	/** Whether Caller's side may call Assist now (asked of the caller's and the assist's extensions; any false = no).
	 * Cooldowns are checked separately, before this. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Assist")
	bool CanCallAssist(APlayerObject* Caller, APlayerObject* Assist) const;

	/** The cooldown (frames) the owner gets when it, as an assist or tag partner, is put on cooldown (SetTeamCooldown).
	 * Return Frames unchanged for the default; a huge number (e.g. 1000000) never comes off cooldown this round. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Assist")
	int32 ModifyCooldown(int32 Frames) const;

	/** The owner just came on screen in a tag (From left). Also for the forced switch when a teammate is KO'd. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Tag")
	void OnTaggedIn(APlayerObject* From);

	/** The owner just left the screen in a tag (To came in). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Tag")
	void OnTaggedOut(APlayerObject* To);

	/** Whether the owner (on screen) may tag out to To. Not asked for a forced switch (a KO'd fighter replaced). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Tag")
	bool CanTagOut(APlayerObject* To) const;

	/** Whether the owner (off screen) may be tagged in by From. Not asked for a forced switch. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Tag")
	bool CanBeTaggedIn(APlayerObject* From) const;

	// ------------------------------------------------------------------------------------------------------- victory

	/** Asked of every extension on the winning side (the winner's first, then its teammates' and assist's) when the
	 * winner would enter its round win (bMatchWin false) or match win state. The first valid tag returned is the state
	 * the WINNER enters instead (it must be in the winner's state list); empty keeps the default. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Victory")
	FGameplayTag GetVictoryState(APlayerObject* Winner, APlayerObject* Loser, bool bMatchWin) const;

	/** Every extension on both sides, right after the winner entered its win state. An assist or teammate can come on
	 * screen here (UNSECharacterLibrary::BringFighterOnScreen) for a longer victory sequence. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Victory")
	void OnVictory(APlayerObject* Winner, APlayerObject* Loser, bool bMatchWin);

	/** While any extension (either side) returns true, the match doesn't end and the next round doesn't start (a
	 * button press or the CPU's RoundEndFlag waits): a victory sequence that must play out. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|Victory")
	bool IsVictorySequenceRunning() const;

	// ------------------------------------------------------------------------------------------------------------ HUD

	/** Extra teammate bars for the owner's side, after the real tag partners' bars (HUD only, every frame). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|HUD")
	void GetVirtualTeammateBars(TArray<FNSETeammateBar>& OutBars) const;

	/** Recolours the owner's own teammate bar (when it's a tag partner off screen), e.g. Golden Boy's gold bar.
	 * Return true and set OutColor to recolour. */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|HUD")
	bool GetOwnBarColor(FLinearColor& OutColor) const;

	// ------------------------------------------------------------------------------------------------------------ CPU

	/** Asked every CPU decision while the owner is CPU controlled and on screen: a valid state tag makes the CPU enter
	 * that state now (if it can), instead of choosing. E.g. a tag out state. CPU decisions run outside rollback (NSE). */
	UFUNCTION(BlueprintNativeEvent, Category="Character Extension|CPU")
	FGameplayTag GetCPUForcedState();
};
