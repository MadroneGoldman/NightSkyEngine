// Madrone change: drive the collision viewer (the CollisionData asset editor) from tools, e.g. to save pictures of cels.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Battle/Objects/PlayerObject.h"
#include "CollisionViewerLibrary.generated.h"

class APlayerObject;
class UCollisionData;

/**
 * The collision viewer as a person uses it: open a CollisionData asset, pick the fighter in its dropdown, select a cel in
 * the Cel Browser, and look at the viewport (the fighter posed at that cel, its boxes drawn over it). Give the editor a
 * frame or two after ShowCel before SaveShot: the viewport poses and draws on its own tick.
 */
UCLASS()
class COLLISIONEDITOR_API UCollisionViewerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Opens (or brings up) the viewer for Data with Fighter selected in its dropdown. */
	UFUNCTION(BlueprintCallable, Category = "NightSky|Collision Viewer")
	static bool OpenViewer(UCollisionData* Data, TSubclassOf<APlayerObject> Fighter);

	/** Selects the cel named Cel in the open viewer's Cel Browser. False if the viewer isn't open or has no such cel. */
	UFUNCTION(BlueprintCallable, Category = "NightSky|Collision Viewer")
	static bool ShowCel(UCollisionData* Data, FGameplayTag Cel);

	/** Saves the open viewer's viewport to a PNG, with the cel's boxes drawn or without them. */
	UFUNCTION(BlueprintCallable, Category = "NightSky|Collision Viewer")
	static bool SaveShot(UCollisionData* Data, const FString& PngPath, bool bWithBoxes = true);

	UFUNCTION(BlueprintCallable, Category = "NightSky|Collision Viewer")
	static void CloseViewer(UCollisionData* Data);

	/** For pictures: hides the floor and sky backdrop and aims the light at the fighter from the camera's side. */
	UFUNCTION(BlueprintCallable, Category = "NightSky|Collision Viewer")
	static bool SetPictureLook(UCollisionData* Data, bool bOn);
};
