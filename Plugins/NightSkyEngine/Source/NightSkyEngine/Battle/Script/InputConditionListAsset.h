// Reusable input condition lists for states (Input Library).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightSkyEngine/Battle/Script/State.h"
#include "InputConditionListAsset.generated.h"

/**
 * One reusable input condition list, the same type a state's Input Condition Lists holds.
 * Load it into a state, or save a state's list into one, by right-clicking the list in the state's
 * class defaults. The shared library lives in /NightSkyEngine/InputLibrary.
 */
UCLASS(BlueprintType)
class NIGHTSKYENGINE_API UInputConditionListAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FInputConditionList InputConditionList;

	/** Numpad notation of the list (e.g. "236A", "[4]6B"), refreshed on save. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	FString Notation;

	/** What it's for, and which states it was taken from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (MultiLine = true))
	FString Description;

	/**
	 * Numpad notation for an input condition list: directions as numpad digits (5 = neutral, 6 = forward),
	 * buttons as letters, * = any input, [x] = held, ~x = released, x! = strict. Conditions are joined with " + ".
	 */
	UFUNCTION(BlueprintPure, Category = "NightSky|Input")
	static FString DescribeInputConditionList(const FInputConditionList& List);

	/** Whether two lists are the same field for field. */
	UFUNCTION(BlueprintPure, Category = "NightSky|Input")
	static bool InputConditionListsMatch(const FInputConditionList& A, const FInputConditionList& B);

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
