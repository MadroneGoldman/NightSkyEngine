// Copyright Epic Games, Inc. All Rights Reserved.

#include "NightSkyEngine.h"
#include "Modules/ModuleManager.h"
#include "UI/NSEStyleManager.h"

#define LOCTEXT_NAMESPACE "FNightSkyEngineModule"

void FNightSkyEngineModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FNightSkyEngineModule::OnPostEngineInit);
}


void FNightSkyEngineModule::ShutdownModule()
{
	// Unregister slate style overrides
	NSEStyleManager::Shutdown();
}

void FNightSkyEngineModule::OnPostEngineInit()
{
	// Register slate style overrides
	NSEStyleManager::Initialize();
}

#undef LOCTEXT_NAMESPACE
    
//IMPLEMENT_MODULE(FDefaultModuleImpl, NightSkyEngine);
IMPLEMENT_MODULE(FNightSkyEngineModule, NightSkyEngine)