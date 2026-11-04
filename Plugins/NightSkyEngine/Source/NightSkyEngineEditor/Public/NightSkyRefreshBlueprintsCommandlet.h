#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NightSkyRefreshBlueprintsCommandlet.generated.h"

/**
 * Equivalent of "Refresh All Nodes" in the Blueprint editor, run headlessly: refreshes every node,
 * recompiles and saves. Does not otherwise edit the Blueprints.
 *
 * Usage:
 *   UnrealEditor-Cmd.exe <Project>.uproject -run=NightSkyRefreshBlueprints [-Path=/Game/...] [-Assets=A,B,C] [-DryRun]
 *
 *   -Path=    Content path to scan (default: /Game)
 *   -Assets=  Comma-separated asset names to limit the refresh to
 *   -DryRun   Only list the Blueprints that would be refreshed; nothing is saved
 */
UCLASS()
class UNightSkyRefreshBlueprintsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNightSkyRefreshBlueprintsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
