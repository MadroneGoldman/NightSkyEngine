// Madrone change: the bottom battle HUD (super meters) with the health bar's shader and characters' own HUD elements.

#pragma once

#include "CoreMinimal.h"
#include "NightSkyBattleWidget.h"
#include "NickBattleHudBottom.generated.h"

class UHealthBar;
class UHorizontalBox;
class UNSEFighterHudElement;
class APlayerObject;

/**
 * Bottom battle HUD. Draws each side's super meter with the health bar's fluid shader (a blue Health Bar in place of
 * the Blueprint's P1MeterBar / P2MeterBar progress bars, which stay in the layout but hidden, so the meters keep the
 * place and size designed for them), and puts the fighter on screen's Battle Hud Elements (CHR_ asset) in a row above
 * its meter. Presentation only.
 */
UCLASS()
class NIGHTSKYENGINE_API UNickBattleHudBottom : public UNightSkyBattleWidget
{
	GENERATED_BODY()

public:
	/** Draw the super meters with Super Meter Material instead of the progress bars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Super Meter")
	bool bShaderSuperMeter = true;

	/** A Health Bar material (HealthPercent etc.): the blue fluid meter by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Super Meter")
	TSoftObjectPtr<UMaterialInterface> SuperMeterMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/WitchTools/BattleCommon/fantasy_gui_4/materials/glassball/MI_SuperMeter_Fluid.MI_SuperMeter_Fluid")));

	/** Gap between the top of a meter and the fighter's HUD elements above it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fighter HUD")
	float ElementGap = 6.f;

	virtual void RoundInitImplementation(int P1TeamSize, int P2TeamSize) override;
	virtual void OnMainPlayersChanged() override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildSuperMeters();
	void RefreshFighterElements(bool bForce);
	const class ANightSkyGameState* FindGameState() const;

	UPROPERTY(Transient) TObjectPtr<UHealthBar> SuperBar[2];
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ElementRow[2];
	UPROPERTY(Transient) TArray<TObjectPtr<UNSEFighterHudElement>> Elements0;
	UPROPERTY(Transient) TArray<TObjectPtr<UNSEFighterHudElement>> Elements1;
	TWeakObjectPtr<const APlayerObject> ShownFighter[2];
	mutable TWeakObjectPtr<const class ANightSkyGameState> CachedGameState;
	float ShownMeter[2] = {-1.f, -1.f};
};
