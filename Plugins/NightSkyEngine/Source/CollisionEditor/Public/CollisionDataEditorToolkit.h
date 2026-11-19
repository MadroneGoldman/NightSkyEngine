#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Data/CollisionData.h"

class APlayerObject;
class FCollisionAnimationPreviewScene;
class SCollisionAnimationViewport;
class SCelAssetTree;


//These aren't used yet
/**
 * The different modes of interaction in the Scene.
 */
enum ESceneInteractionMode
{
	Interaction_CameraMove,
	Interaction_EditCollisionBoxes
};

/**
 * The different modes of editing collision boxes.
 */
enum EBoxEditMode
{
	EditMode_None,
	EditMode_Add,
	EditMode_Move,
	EditMode_Delete
};

/**
 * Toolkit for editing collision data assets for Night Sky.
 */
class FCollisionDataEditorToolkit : public FAssetEditorToolkit
{
public:
	FCollisionDataEditorToolkit();
	virtual ~FCollisionDataEditorToolkit() override;

	// Move some of these to private

	// Initializes the editor with the asset to be edited
	void Initialize(UCollisionData* InCollisionData, const EToolkitMode::Type Mode,
	                const TSharedPtr<IToolkitHost>& InitToolkitHost);

	// FAssetEditorToolkit interface
	virtual FName GetToolkitFName() const override; // Unique name for this toolkit type
	virtual FText GetBaseToolkitName() const override; // Display name for the toolkit
	virtual FText GetToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override; // Prefix for tabs in world-centric mode
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// Tab management
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	TSharedRef<SDockTab> SpawnTab_CollisionDataDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_PlayerObjectSelector(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_ViewportTab(const FSpawnTabArgs& Args);

	// Do I need to initialize a scene?
	void InitializePreviewScene();

	// Accessors
	// Get current animation name
	FText GetSelectedState() const;

	// Madrone change: what the dropdown and the Cel Browser do, for tools (UCollisionViewerLibrary).
	void ShowFighter(UClass* Class) { OnPlayerObjectBPSelected(Class); }
	bool ShowCel(FGameplayTag Cel);
	TSharedPtr<SCollisionAnimationViewport> GetViewportWidget() const { return PreviewViewportWidget; }
	// The boxes are one-frame debug lines drawn on the scene's tick: draws them again, for a picture taken outside it.
	void RedrawBoxes();
	void ClearBoxes();
	// For pictures (the move wiki), and a toggle in the Cel Browser: no floor or sky backdrop, no pivot sprite, and the light
	// aimed at the fighter from the camera's side. Off puts the scene's own light back.
	void SetPictureLook(bool bOn);
	bool IsPictureLook() const { return bPictureLook; }

private:
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SClassPropertyEntryBox> PlayerObjectBPPicker;

	TSharedPtr<FCollisionAnimationPreviewScene> PreviewScene;
	TSharedPtr<SCollisionAnimationViewport> PreviewViewportWidget;

	UCollisionData* CollisionData{};
	TSharedPtr<SCelAssetTree> CelAssetTree;
	FGameplayTag SelectedCel;
	UClass* PlayerObjectClass = nullptr;
	// Madrone change: picture look state, and the scene's own light to put back.
	bool bPictureLook = false;
	bool bSavedLight = false;
	FRotator SavedLightRotation;
	float SavedLightIntensity = 0.f;
	APlayerObject* PlayerObject{};

	void InitializeCelAssetTree();
	void OnCelSelected(int32 CelIndex);
	void OnCollisionFramesChanged();

	void OnPlayerObjectBPSelected(const UClass* Class);
	void InitializePlayerObjectBPPicker();

	FGameplayTag GetCurrentCelName() const;

	void UpdateAnimationPlayback(int32 frame);

protected:
	virtual void OnClose() override;
};
