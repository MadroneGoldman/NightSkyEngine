#pragma once

#include "CoreMinimal.h"
#include "SHealthBar.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "HealthBar.generated.h"

class USlateBrushAsset;

/**
 * The Health bar widget is a simple bar that fills up that can be restyled to fit any number of uses.
 *
 * * No Children
 */
UCLASS()
class NIGHTSKYENGINE_API UHealthBar : public UWidget
{
	GENERATED_UCLASS_BODY()
	
public:
	/** Image to draw */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter = "SetImageFill", Category=Health)
	FSlateBrush ImageFill;
	
	UE_DEPRECATED(5.1, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	/** The Health bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Style" ))
	FHealthBarStyle WidgetStyle = FHealthBarStyle();

	UE_DEPRECATED(5.1, "Direct access to Percent is deprecated. Please use the getter or setter.")
	/** Used to determine the fill position of the Health bar ranging 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Category="Health", meta = (UIMin = "0", UIMax = "1"))
	float HealthPercent;
	
	UE_DEPRECATED(5.1, "Direct access to Percent is deprecated. Please use the getter or setter.")
	/** Used to determine the fill position of the Health bar ranging 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Category="Health", meta = (UIMin = "0", UIMax = "1"))
	float RecoverableHealthPercent;

	UE_DEPRECATED(5.1, "Direct access to BarFillType is deprecated. Please use the getter or setter.")
	/** Defines the direction in which the Health bar fills */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Health")
	TEnumAsByte<EHealthBarFillType::Type> BarFillType;

	UE_DEPRECATED(5.1, "Direct access to BarFillStyle is deprecated. Please use the getter or setter.")
	/** Defines the visual style of the Health bar fill - scale or mask */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Health")
	TEnumAsByte<EHealthBarFillStyle::Type> BarFillStyle;

	UE_DEPRECATED(5.1, "Direct access to BorderPadding is deprecated. Please use the getter or setter.")
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Health")
	FVector2D BorderPadding;

	UE_DEPRECATED(5.1, "Direct access to FillColorAndOpacity is deprecated. Please use the getter or setter.")
	/** Fill Color and Opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Setter, BlueprintSetter="SetFillColorAndOpacity", Category="Appearance")
	FLinearColor FillColorAndOpacity;

	/** */
	UPROPERTY()
	FGetLinearColor FillColorAndOpacityDelegate;

public:
	
	UFUNCTION(BlueprintCallable, Category="Appearance")
	virtual void SetImageFill(const FSlateBrush& InBrush);

	const FSlateBrush& GetImageFill() const;
	
	/** */
	 const FHealthBarStyle& GetWidgetStyle() const;

	/**/
	 void SetWidgetStyle(const FHealthBarStyle& InStyle);

	/** */
	 float GetHealthPercent() const;
	
	/** */
	 float GetRecoverableHealthPercent() const;

	/** Sets the current value of the HealthBar. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetHealthPercent(int index, float InPercent);
	
	/** Sets the current value of the HealthBar. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetRecoverableHealthPercent(int index,float InPercent);
	
	/** */
	 EHealthBarFillType::Type GetBarFillType() const;

	/** */
	 void SetBarFillType(EHealthBarFillType::Type InBarFillType);

	/** */
	 EHealthBarFillStyle::Type GetBarFillStyle() const;

	/** */
	 void SetBarFillStyle(EHealthBarFillStyle::Type InBarFillStyle);

	/** */
	 FVector2D GetBorderPadding() const;

	/** */
	 void SetBorderPadding(FVector2D InBorderPadding);

	/** */
	 FLinearColor GetFillColorAndOpacity() const;

	/** Sets the fill color of the Health bar. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void SetFillColorAndOpacity(FLinearColor InColor);
	
	UFUNCTION()
	void SetMainPlayerIndex(int PlayerIndex);
	
public:

	
	//~ Begin UWidget Interface
	 virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	 virtual const FText GetPaletteCategory() override;
	 virtual void OnCreationFromPalette() override;
	//~ End UWidget Interface
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SHealthBar> MyHealthBar;

	//~ Begin UWidget Interface
	 virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	//void OnHealthChanged(const float Value);
	//void OnRecoverableHealthChanged(const float Value);
	
	void SynchronizeHealthBar();
	
	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, FillColorAndOpacity);
	
private:
	UPROPERTY()
	int MainPlayerIndex = 0;
};
