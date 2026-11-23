#include "NightSkyEngineEditor.h"

#include "UI/NSEStyleManager.h"
#include "InputConditionListCustomization.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FNightSkyEngineEditorModule"

void FNightSkyEngineEditorModule::StartupModule()
{
	// Register slate style overrides
	//NSEStyleManager::Initialize();
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FNightSkyEngineEditorModule::OnPostEngineInit);

	// Input Library: load/save a state's input condition lists from/to UInputConditionListAsset
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomPropertyTypeLayout("InputConditionList",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInputConditionListCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
}

void FNightSkyEngineEditorModule::OnPostEngineInit()
{
	// Register slate style overrides (the runtime module already has; it also unregisters it)
	FNSEStyle::Initialize();
}

void FNightSkyEngineEditorModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyEditor->UnregisterCustomPropertyTypeLayout("InputConditionList");
		PropertyEditor->NotifyCustomizationModuleChanged();
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FNightSkyEngineEditorModule, NightSkyEngineEditor)