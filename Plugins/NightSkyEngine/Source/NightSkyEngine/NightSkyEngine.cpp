// Copyright Epic Games, Inc. All Rights Reserved.

#include "NightSkyEngine.h"
#include "Modules/ModuleManager.h"
#include "UI/NSEStyleManager.h"

#define LOCTEXT_NAMESPACE "FNightSkyEngineModule"

void FNightSkyEngineModule::StartupModule()
{
	// Madrone change: the style set holds no file brushes, so it can register now (also in commandlets, which never
	// reach post engine init in time for the HUD).
	FNSEStyle::Initialize();
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FNightSkyEngineModule::OnPostEngineInit);
}


void FNightSkyEngineModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	// Unregister slate style overrides
	FNSEStyle::Shutdown();
}

void FNightSkyEngineModule::OnPostEngineInit()
{
	// Register slate style overrides (does nothing when StartupModule already did)
	FNSEStyle::Initialize();
}

#undef LOCTEXT_NAMESPACE

//IMPLEMENT_MODULE(FDefaultModuleImpl, NightSkyEngine);
IMPLEMENT_MODULE(FNightSkyEngineModule, NightSkyEngine)
