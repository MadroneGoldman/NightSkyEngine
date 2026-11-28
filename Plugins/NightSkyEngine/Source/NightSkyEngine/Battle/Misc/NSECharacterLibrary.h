// Madrone change: Blueprint helpers for character extensions, interaction events and states. Docs:
// Plugins/NightSkyEngine/Docs/10-Character-Systems.md.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NSECharacterLibrary.generated.h"

class APlayerObject;
class UNiagaraSystem;
class UNSECharacterExtension;

UCLASS()
class NIGHTSKYENGINE_API UNSECharacterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The character tag of a fighter (its CHR_ asset's Character Tag, or Character.<CharaName>). Empty if unknown. */
	UFUNCTION(BlueprintPure, Category="NSE|Characters")
	static FGameplayTag GetCharacterTag(const APlayerObject* Fighter);

	/** On screen now: a main player, or an assist that was called and hasn't left. */
	UFUNCTION(BlueprintPure, Category="NSE|Characters")
	static bool IsFighterOnScreen(const APlayerObject* Fighter);

	/** Every fighter in the battle (teams, then the dedicated assists) with that character tag. */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters", meta=(WorldContext="WorldContext"))
	static TArray<APlayerObject*> FindFightersByTag(const UObject* WorldContext, FGameplayTag CharacterTag);

	/** Sets health (clamped to 0..MaxHealth). 0 KOs the fighter on its next update. */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters")
	static void SetFighterHealth(APlayerObject* Fighter, int32 Health);

	/** Enters a state by its name tag (primary state machine). False if the fighter has no such state. */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters")
	static bool EnterState(APlayerObject* Fighter, FGameplayTag StateName);

	/**
	 * Plays a Niagara system on a fighter like a Night Sky particle (rolled back with the battle, mirrored by side):
	 * at the fighter's position plus Offset (cm; X toward its facing, Z up), e.g. (0, 0, 220) above the head. On the
	 * drawn character, nothing is attached to the Manny driver.
	 */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters")
	static void PlayParticleOnFighter(APlayerObject* Fighter, UNiagaraSystem* System, FVector Offset, float Scale = 1.f);

	/** Brings an off-screen fighter (tag partner or assist) on screen at PosX (engine units; the main player's
	 * position when bAtMainPlayer) in State, without tagging or cooldowns: for scripted moments like a victory
	 * walk-in. Call AssistExit / SetOnScreen(false) to take it off again. */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters")
	static bool BringFighterOnScreen(APlayerObject* Fighter, FGameplayTag State, bool bAtMainPlayer = true, int32 PosX = 0);

	/** Shows a battle subtitle (the shared base subtitle element, Narrative's look). Skipped while resimulating and
	 * never shown twice for the same frame. Seconds of real time. */
	UFUNCTION(BlueprintCallable, Category="NSE|Subtitles", meta=(WorldContext="WorldContext"))
	static void ShowBattleSubtitle(const UObject* WorldContext, FText Speaker, FText Line, float Seconds = 3.f, const UObject* SpeakerObject = nullptr);

	/** A fighter's extension of that class, or null. */
	UFUNCTION(BlueprintPure, Category="NSE|Characters", meta=(DeterminesOutputType="Class"))
	static UNSECharacterExtension* GetCharacterExtension(const APlayerObject* Fighter, TSubclassOf<UNSECharacterExtension> Class);

	/** Sets a side's dedicated assist cooldown (frames). */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters", meta=(WorldContext="WorldContext"))
	static void SetAssistCooldown(const UObject* WorldContext, bool bIsP1, int32 Frames);

	/** Tags Fighter (on screen) out to its team member at TeamIndex (1 = the next one), as the tag state does:
	 * cooldowns and CanTagOut apply unless bForce. Returns who came in. */
	UFUNCTION(BlueprintCallable, Category="NSE|Characters")
	static APlayerObject* TagOut(APlayerObject* Fighter, int32 TeamIndex = 1, bool bForce = false);
};
