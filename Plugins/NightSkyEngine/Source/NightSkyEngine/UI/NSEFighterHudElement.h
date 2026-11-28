// Madrone change: HUD elements a character adds to the battle HUD (stocks, special meters).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NSEFighterHudElement.generated.h"

class APlayerObject;
class UHorizontalBox;
class UTextBlock;
class UTexture2D;

/**
 * Something a character shows on the battle HUD, above its side's super meter (listed in its CHR_ asset's Battle Hud
 * Elements). The HUD makes one per element for the fighter on screen, rebuilt on a tag, and updates it every frame.
 * Presentation only: read the fighter, never change it (the HUD also runs while a rollback resimulates).
 *
 * Blueprint subclasses design their own widget and implement On Update Fighter Hud; Stocks (UNSEStockHudElement) is
 * a ready-made row of icons.
 */
UCLASS(Abstract, Blueprintable)
class NIGHTSKYENGINE_API UNSEFighterHudElement : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The fighter this element shows. */
	UPROPERTY(BlueprintReadOnly, Category="Fighter HUD")
	TObjectPtr<APlayerObject> Fighter;

	/** Player 1's side (left). Player 2's elements are mirrored (they fill from the right edge). */
	UPROPERTY(BlueprintReadOnly, Category="Fighter HUD")
	bool bIsP1 = true;

	/** Called by the HUD after Fighter and bIsP1 are set. */
	virtual void InitElement(APlayerObject* InFighter, bool bInIsP1);
	/** Called by the HUD every frame. */
	virtual void UpdateElement();

	UFUNCTION(BlueprintImplementableEvent, Category="Fighter HUD")
	void OnInitFighterHud();
	UFUNCTION(BlueprintImplementableEvent, Category="Fighter HUD")
	void OnUpdateFighterHud();

	/** Reads one of the fighter's player registers (1-8), 0 when out of range. */
	UFUNCTION(BlueprintPure, Category="Fighter HUD")
	int32 GetPlayerRegister(int32 Register) const;

	/** Reads one of the fighter's extra gauges (ExtraGauges index): value and max. */
	UFUNCTION(BlueprintPure, Category="Fighter HUD")
	void GetExtraGauge(int32 Index, int32& Value, int32& MaxValue) const;
};

UENUM(BlueprintType)
enum class ENSEHudStockSource : uint8
{
	/** One of the fighter's player registers (PlayerReg1-8), e.g. the witch's potion count in PlayerReg1. */
	PlayerRegister,
	/** One of the fighter's extra gauges: stocks = value / (max / sections). */
	ExtraGauge,
};

/**
 * A row of stock icons (full / empty), e.g. the witch's potions. The row has as many slots as the highest count seen
 * this battle (up to Max Slots), so spent stocks show empty; past Max Slots a "x12" count is shown after the row.
 */
UCLASS(Blueprintable)
class NIGHTSKYENGINE_API UNSEStockHudElement : public UNSEFighterHudElement
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks")
	ENSEHudStockSource Source = ENSEHudStockSource::PlayerRegister;

	/** Player register 1-8, or the extra gauge's index. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks", meta=(ClampMin="0"))
	int32 SourceIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks")
	TSoftObjectPtr<UTexture2D> FullIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks")
	TSoftObjectPtr<UTexture2D> EmptyIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks", meta=(ClampMin="1"))
	int32 MaxSlots = 5;

	/** Always show this many slots (0: as many as the highest count seen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks", meta=(ClampMin="0"))
	int32 FixedSlots = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks")
	float IconSize = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stocks")
	float IconSpacing = 2.f;

	virtual void InitElement(APlayerObject* InFighter, bool bInIsP1) override;
	virtual void UpdateElement() override;

	/** The current stock count. */
	UFUNCTION(BlueprintPure, Category="Stocks")
	int32 GetStockCount() const;

protected:
	virtual void NativeOnInitialized() override;

private:
	void Rebuild(int32 Slots);

	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> Row;
	UPROPERTY(Transient) TArray<TObjectPtr<class UBorder>> Icons;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CountText;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedFull;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedEmpty;
	int32 HighestCount = 0;
	int32 ShownSlots = -1;
	int32 ShownCount = -1;
};
