#include "NSEStyleManager.h"

#include "NSEStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateWidgetStyleAsset.h"

TSharedPtr<FNSEStyle> FNSEStyle::Instance = nullptr;

const FName FHealthBarStyle::TypeName(TEXT("FHealthBarStyle"));
const FName FNSEBattleHudStyle::TypeName(TEXT("FNSEBattleHudStyle"));

FNSEStyle::FNSEStyle()
	: FSlateStyleSet(TEXT("NightSkyEngineStyle"))
{
	// No file brushes: the looks come from assets. "HealthBarStyle" is the plain default (the widgets' own settings).
	Set("HealthBarStyle", FHealthBarStyle());
}

void FNSEStyle::Initialize()
{
	if (Instance.IsValid())
	{
		return;
	}
	Instance = MakeShared<FNSEStyle>();
	FSlateStyleRegistry::RegisterSlateStyle(*Instance);
}

void FNSEStyle::Shutdown()
{
	if (Instance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Instance);
		Instance.Reset();
	}
}

FNSEStyle* FNSEStyle::Get()
{
	return Instance.Get();
}

const FNSEBattleHudStyle* FNSEStyle::RegisterHudStyle(USlateWidgetStyleAsset* Asset)
{
	if (!Instance.IsValid() || !Asset)
	{
		return nullptr;
	}
	const FNSEBattleHudStyle* Source = Asset->GetStyle<FNSEBattleHudStyle>();
	if (!Source)
	{
		UE_LOG(LogTemp, Warning, TEXT("NightSkyEngineStyle: %s is not an NSE Battle Hud Style"), *Asset->GetPathName());
		return nullptr;
	}
	const FName Key(*Asset->GetPathName());
	Instance->Set(Key, *Source);
	Instance->HudStyleAssets.AddUnique(Asset);
	return &Instance->GetWidgetStyle<FNSEBattleHudStyle>(Key);
}

void FNSEStyle::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(HudStyleAssets);
}
