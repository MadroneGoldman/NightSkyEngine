// Madrone change: character interactions: something happens when two given characters are on screen together in a
// battle. Docs: Plugins/NightSkyEngine/Docs/10-Character-Systems.md, WitchTools Docs/CharacterInteractions.md.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "NightSkyEngine/Battle/Misc/SerializableObj.h"
#include "NSECharacterInteraction.generated.h"

class APlayerObject;
class ANightSkyGameState;
class UNSECharacterInteractionEvent;

/** When a pair is checked. Both characters must be on screen at that moment. */
UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class ENSEInteractionTrigger : uint8
{
	None = 0 UMETA(Hidden),
	/** When a round starts (once the fighters can move: after the intro and the round start countdown). */
	RoundStart = 1 << 0,
	/** Right after a tag (either side), including the switch when a fighter is KO'd in a tag battle. */
	TagIn = 1 << 1,
	/** Right after an assist was called (either side). */
	AssistCall = 1 << 2,
	/** Every frame both are on screen (any way they got there). */
	EveryFrame = 1 << 3,
};
ENUM_CLASS_FLAGS(ENSEInteractionTrigger);

/** Which sides the two characters may be on. */
UENUM(BlueprintType)
enum class ENSEInteractionSides : uint8
{
	/** Same side or opposite sides. */
	Any,
	/** Opposite sides only. */
	Opponents,
	/** The same side only (a fighter and its tag partner or assist). */
	Teammates,
};

/** How often the event may happen. */
UENUM(BlueprintType)
enum class ENSEInteractionRepeat : uint8
{
	OncePerRound,
	OncePerMatch,
	/** Every time a trigger finds them together and the event isn't already running. */
	Unlimited,
};

/**
 * One character pair: when Character A (the subject: the event acts on it) and Character B (the partner) are on
 * screen together at one of the Triggers, an instance of Event runs. Characters are identified by their CHR_ asset's
 * Character Tag (UPrimaryCharaData::GetCharacterTag: Character Tag, or Character.<CharaName> when that tag exists).
 */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FNSECharacterInteractionPair
{
	GENERATED_BODY()

	/** The subject (the event acts on this one). In a CHR_ asset's Character Interactions, empty = that character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(Categories="Character"))
	FGameplayTag CharacterA;

	/** The partner: the character whose presence sets the event off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(Categories="Character"))
	FGameplayTag CharacterB;

	/** What happens. One instance per matching pair of fighters in the match, kept for the whole match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	TSubclassOf<UNSECharacterInteractionEvent> Event;

	/** When the pair is checked (ENSEInteractionTrigger flags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(Bitmask, BitmaskEnum="/Script/NightSkyEngine.ENSEInteractionTrigger"))
	int32 Triggers = static_cast<int32>(ENSEInteractionTrigger::RoundStart | ENSEInteractionTrigger::TagIn | ENSEInteractionTrigger::AssistCall);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	ENSEInteractionSides Sides = ENSEInteractionSides::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	ENSEInteractionRepeat Repeat = ENSEInteractionRepeat::OncePerRound;

	/** Also the other way round: B as the subject when A is on screen (a separate instance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bSymmetric = false;

	/** Lower runs first when two events start on the same frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	int32 Priority = 0;
};

/** A list of character pairs. Add it to Battle Rules > Character Interactions > Interaction Sets (every battle), or to
 * a game state's Character Interaction Data. A CHR_ asset can also list its own pairs (Character Interactions). */
UCLASS(BlueprintType)
class NIGHTSKYENGINE_API UNSECharacterInteractionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interactions")
	TArray<FNSECharacterInteractionPair> Pairs;
};

/**
 * What happens when a pair is met. Made once per match per matching pair of fighters (Subject = the fighter with
 * Character A, Partner = the one with Character B) and reused every time it fires.
 *
 * Runs over frames: Begin on the frame the pair is met, then Update every battle frame (not during a super freeze)
 * until it calls Finish (or the round ends: End with bInterrupted). Frame counts the frames since Begin.
 *
 * Rollback: like character extensions, gameplay state must be in SaveGame properties (Blueprint: SaveGame variables).
 * Frame, bActive and the fire counts are already. No wall-clock time; no random numbers but the battle's.
 * Presentation (subtitles, sounds) must skip resimulated frames (IsResimulating; ShowBattleSubtitle does it itself).
 */
UCLASS(BlueprintType, Blueprintable, Abstract)
class NIGHTSKYENGINE_API UNSECharacterInteractionEvent : public USerializableObj
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	TObjectPtr<ANightSkyGameState> GameState;

	/** The fighter with Character A: the event acts on it. */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	TObjectPtr<APlayerObject> Subject;

	/** The fighter with Character B. */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	TObjectPtr<APlayerObject> Partner;

	/** The pair entry this instance was made from. */
	UPROPERTY(BlueprintReadOnly, Category="Interaction")
	FNSECharacterInteractionPair Pair;

	/** While true, the Subject gets no inputs (player or CPU): the event drives it. Set it in Begin. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category="Interaction")
	bool bHoldSubject = false;

	/** Same for the Partner (a cutscene-like moment where neither fights). */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category="Interaction")
	bool bHoldPartner = false;

	/** Running (between Begin and Finish). */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Interaction")
	bool bActive = false;

	/** Frames since Begin (0 on Begin's frame, 1 on the first Update after it). */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Interaction")
	int32 Frame = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Interaction")
	int32 TimesFiredThisRound = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Interaction")
	int32 TimesFiredThisMatch = 0;

	/** The trigger that started the current run. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Interaction")
	ENSEInteractionTrigger StartedBy = ENSEInteractionTrigger::None;

	/** Ends the event (End is called with bInterrupted false). */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void Finish();

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsResimulating() const;

	/** Whether the event may start now, once the pair is on screen and the repeat rule allows it. Default: both
	 * fighters have health left and the round is being fought. */
	UFUNCTION(BlueprintNativeEvent, Category="Interaction")
	bool CanStart() const;

	/** The frame the pair was met. */
	UFUNCTION(BlueprintNativeEvent, Category="Interaction")
	void Begin();

	/** Every battle frame after Begin while active. */
	UFUNCTION(BlueprintNativeEvent, Category="Interaction")
	void Update();

	/** Finished (bInterrupted false) or cut off by the round ending / match restarting (true). */
	UFUNCTION(BlueprintNativeEvent, Category="Interaction")
	void End(bool bInterrupted);

	/** Engine side: start / step / stop. */
	void StartEvent(ENSEInteractionTrigger Trigger);
	void StepEvent();
	void StopEvent(bool bInterrupted);
	/** Round start: stops a running event and clears the per-round count. */
	void ResetForRound();
};
