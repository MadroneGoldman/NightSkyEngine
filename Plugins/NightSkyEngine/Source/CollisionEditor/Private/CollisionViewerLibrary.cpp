// Madrone change: drive the collision viewer from tools.

#include "CollisionViewerLibrary.h"

#include "CollisionDataEditorToolkit.h"
#include "Data/CollisionData.h"
#include "Editor.h"
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Viewport/CollisionAnimationViewport.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CollisionViewerLibrary)

namespace
{
	FCollisionDataEditorToolkit* FindViewer(UCollisionData* Data)
	{
		if (!Data || !GEditor) return nullptr;
		UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		IAssetEditorInstance* Instance = Editors ? Editors->FindEditorForAsset(Data, false) : nullptr;
		if (!Instance || Instance->GetEditorName() != FName("CollisionDataEditor")) return nullptr;
		return static_cast<FCollisionDataEditorToolkit*>(Instance);
	}
}

bool UCollisionViewerLibrary::OpenViewer(UCollisionData* Data, TSubclassOf<APlayerObject> Fighter)
{
	if (!Data || !Fighter || !GEditor) return false;
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Data);
	FCollisionDataEditorToolkit* Viewer = FindViewer(Data);
	if (!Viewer) return false;
	// Its viewport only gets a size (and draws) once its tab is on screen.
	Viewer->BringToolkitToFront();
	if (const TSharedPtr<FTabManager> Tabs = Viewer->GetTabManager()) Tabs->TryInvokeTab(FTabId("ViewportTab"));
	Viewer->ShowFighter(Fighter);
	return true;
}

bool UCollisionViewerLibrary::ShowCel(UCollisionData* Data, const FGameplayTag Cel)
{
	FCollisionDataEditorToolkit* Viewer = FindViewer(Data);
	return Viewer && Viewer->ShowCel(Cel);
}

bool UCollisionViewerLibrary::SaveShot(UCollisionData* Data, const FString& PngPath, const bool bWithBoxes)
{
	FCollisionDataEditorToolkit* Viewer = FindViewer(Data);
	const TSharedPtr<SCollisionAnimationViewport> Widget = Viewer ? Viewer->GetViewportWidget() : nullptr;
	const TSharedPtr<FSceneViewport> Viewport = Widget ? Widget->GetSceneViewport() : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("Collision viewer SaveShot: %s"), !Viewer ? TEXT("the viewer isn't open") : !Widget ? TEXT("no viewport widget") : TEXT("the viewport isn't made yet (its tab hasn't been shown)"));
		return false;
	}
	if (const TSharedPtr<FEditorViewportClient> Client = Widget->GetViewportClient()) Client->Invalidate();
	if (bWithBoxes) Viewer->RedrawBoxes();
	else Viewer->ClearBoxes();
	Viewport->Draw(false);
	FlushRenderingCommands();
	TArray<FColor> Pixels;
	const FIntPoint Size = Viewport->GetSizeXY();
	if (Size.X <= 0 || Size.Y <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Collision viewer SaveShot: the viewport has no size (%d x %d): is its tab visible?"), Size.X, Size.Y);
		return false;
	}
	if (!Viewport->ReadPixels(Pixels) || Pixels.Num() != Size.X * Size.Y)
	{
		UE_LOG(LogTemp, Warning, TEXT("Collision viewer SaveShot: couldn't read the viewport's pixels (%d of %d)"), Pixels.Num(), Size.X * Size.Y);
		return false;
	}
	for (FColor& Pixel : Pixels) Pixel.A = 255;
	return FImageUtils::SaveImageByExtension(*PngPath, FImageView(Pixels.GetData(), Size.X, Size.Y));
}

bool UCollisionViewerLibrary::SetPictureLook(UCollisionData* Data, const bool bOn)
{
	FCollisionDataEditorToolkit* Viewer = FindViewer(Data);
	if (!Viewer) return false;
	Viewer->SetPictureLook(bOn);
	return true;
}

void UCollisionViewerLibrary::CloseViewer(UCollisionData* Data)
{
	if (Data && GEditor) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Data);
}
