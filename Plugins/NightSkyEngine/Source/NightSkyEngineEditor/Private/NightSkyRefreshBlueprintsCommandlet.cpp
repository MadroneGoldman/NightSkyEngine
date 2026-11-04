#include "NightSkyRefreshBlueprintsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogNSRefreshBlueprints, Log, All);

UNightSkyRefreshBlueprintsCommandlet::UNightSkyRefreshBlueprintsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UNightSkyRefreshBlueprintsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	const bool bDryRun = Switches.Contains(TEXT("DryRun"));
	FString RootPath = ParamVals.FindRef(TEXT("Path"));
	if (RootPath.IsEmpty())
	{
		RootPath = TEXT("/Game");
	}

	TArray<FString> AssetNames;
	ParamVals.FindRef(TEXT("Assets")).ParseIntoArray(AssetNames, TEXT(","), true);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.SearchAllAssets(true);

	FARFilter Filter;
	Filter.PackagePaths.Add(*RootPath);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	if (AssetNames.Num() > 0)
	{
		Assets.RemoveAll([&AssetNames](const FAssetData& Asset)
		{
			return !AssetNames.Contains(Asset.AssetName.ToString());
		});
	}

	UE_LOG(LogNSRefreshBlueprints, Display, TEXT("Refreshing %d Blueprints under %s%s"), Assets.Num(), *RootPath,
	       bDryRun ? TEXT(" (dry run)") : TEXT(""));

	int32 NumSaved = 0;
	int32 NumFailed = 0;

	for (const FAssetData& Asset : Assets)
	{
		UE_LOG(LogNSRefreshBlueprints, Display, TEXT("  %s"), *Asset.GetObjectPathString());
		if (bDryRun)
		{
			continue;
		}

		UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		if (!Blueprint)
		{
			continue;
		}

		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);

		UPackage* Package = Blueprint->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		if (UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs))
		{
			++NumSaved;
		}
		else
		{
			++NumFailed;
			UE_LOG(LogNSRefreshBlueprints, Error, TEXT("Failed to save %s"), *Filename);
		}
	}

	UE_LOG(LogNSRefreshBlueprints, Display, TEXT("Done: %d saved, %d failed."), NumSaved, NumFailed);
	return NumFailed > 0 ? 1 : 0;
}
