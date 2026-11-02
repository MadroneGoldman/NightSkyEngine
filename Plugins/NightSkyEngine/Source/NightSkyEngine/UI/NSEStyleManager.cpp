#include "NSEStyleManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "SlateOptMacros.h"
#include "Styling/SlateStyle.h"
#include "NSEStyle.h"
#include "Styling/SlateStyleMacros.h"

TSharedPtr< FSlateStyleSet > NSEStyleManager::StyleSet = nullptr;

void NSEStyleManager::Initialize()
{
	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}
	
	StyleSet = MakeShareable(new FSlateStyleSet("NightSkeEngineStyle"));
	//StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	//StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	
	//StyleSet->SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("NightSkyEngine/UI"));
	//StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	
	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Register custom style instance
	//const FHealthBarStyle HealthBarStyleInstance = FHealthBarStyle();
	
	//StyleSet->Set("HealthBarStyle", FHealthBarStyle());
	
	StyleSet->Set( "HealthBarStyle", FHealthBarStyle()
			//.SetBackgroundImage( BOX_BRUSH( "Common/ProgressBar_Background", FMargin(5.f/12.f) ) )
			//.SetFillImage( BOX_BRUSH( "Common/ProgressBar_Fill", FMargin(5.f/12.f), FLinearColor( 1.0f, 0.22f, 0.0f )  ) )
			);
	
	// Selection color for the active row should be ??? to align with the editor viewport.
	// const FHealthBarStyle& HealthBarStyleInstance = FAppStyle::Get().GetWidgetStyle<FHealthBarStyle>("HealthBarStyle");
	//
	// StyleSet->Set("HealthBarStyle",
	// 	FHealthBarStyle(HealthBarStyleInstance)
	// 	//.EnableFillAnimation(false)
	// 	);
	
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void NSEStyleManager::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}
