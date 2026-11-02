#include "NightSkyEngineEditor.h"

#include "UI/NSEStyleManager.h"

#define LOCTEXT_NAMESPACE "FNightSkyEngineEditorModule"

void FNightSkyEngineEditorModule::StartupModule()
{
	// Register slate style overrides
	//NSEStyleManager::Initialize();
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FNightSkyEngineEditorModule::OnPostEngineInit);
}

void FNightSkyEngineEditorModule::OnPostEngineInit()
{
	// Register slate style overrides
	NSEStyleManager::Initialize();
}

void FNightSkyEngineEditorModule::ShutdownModule()
{
	// Unregister slate style overrides
	NSEStyleManager::Shutdown();
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FNightSkyEngineEditorModule, NightSkyEngineEditor)