#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightSkyAssetDescribeLibrary.generated.h"

class UBlueprint;
class UDataTable;

/**
 * Editor-only helpers that dump assets to JSON for migration and review (e.g. porting Narrative Tales dialogue to
 * NarrativePro). Read-only: nothing is modified or saved.
 */
UCLASS()
class NIGHTSKYENGINEEDITOR_API UNightSkyAssetDescribeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Every property of Object (except UObject's own) as JSON text values. Subobjects owned by Object (instanced
	 * conditions/events, dialogue nodes...) are expanded recursively; anything seen before becomes {"_ref": path}.
	 */
	UFUNCTION(BlueprintCallable, Category="NightSky|Describe")
	static FString DescribeObjectJson(UObject* Object, int32 MaxDepth = 12);

	/** Every graph of the Blueprint: nodes (class, title, comment) and pins (default values, links). */
	UFUNCTION(BlueprintCallable, Category="NightSky|Describe")
	static FString DescribeBlueprintGraphsJson(UBlueprint* Blueprint);

	/** The data table's rows as JSON (UDataTable::GetTableAsJSON). */
	UFUNCTION(BlueprintCallable, Category="NightSky|Describe")
	static FString DataTableToJson(UDataTable* Table);
};
