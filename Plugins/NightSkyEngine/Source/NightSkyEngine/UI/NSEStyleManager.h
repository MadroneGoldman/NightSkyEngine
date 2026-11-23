#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"
#include "UObject/GCObject.h"

struct FNSEBattleHudStyle;
class USlateWidgetStyleAsset;

/**
 * Madrone change: Night Sky's Slate style set ("NightSkyEngineStyle"). Holds the battle HUD styles in use, keyed by
 * their Slate Widget Style asset's path, and keeps those assets (and the materials they name) loaded while it does.
 * Registered when the module starts.
 */
class NIGHTSKYENGINE_API FNSEStyle : public FSlateStyleSet, public FGCObject
{
public:
	static void Initialize();
	static void Shutdown();
	/** The style set, or null before Initialize / after Shutdown. */
	static FNSEStyle* Get();

	/** Puts the asset's battle HUD style in the set (again, so edits made in the editor show) and returns it, or null
	 * when the asset isn't an NSE Battle Hud Style. The returned style lives until the asset is registered again: copy
	 * what you keep. */
	static const FNSEBattleHudStyle* RegisterHudStyle(USlateWidgetStyleAsset* Asset);

	FNSEStyle();

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FNSEStyle"); }

private:
	TArray<TObjectPtr<USlateWidgetStyleAsset>> HudStyleAssets;
	static TSharedPtr<FNSEStyle> Instance;
};
