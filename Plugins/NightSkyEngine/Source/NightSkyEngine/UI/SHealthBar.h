#pragma once

#include "CoreMinimal.h"
#include "NSEStyle.h"
#include "UObject/ObjectMacros.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "SHealthBar.generated.h"

class FPaintArgs;
class FSlateWindowElementList;
class UMaterialInstanceDynamic;

/**
 * SHealthBar Fill Type
 */
UENUM(BlueprintType)
namespace EHealthBarFillType
{
	enum Type : int
	{
		// will fill up from the left side to the right
		LeftToRight,
		// will fill up from the right side to the left side
		RightToLeft,
	};
}

/**
 * SHealthBar Fill Style
 */
UENUM(BlueprintType)
namespace EHealthBarFillStyle
{
	enum Type : int
	{
		// a mask is used to paint the fill image
		Mask,
		// the fill image is scaled to the fill percentage
		Scale,
	};
}

/**
 * A health bar. The fill is usually a material that draws the health itself (FHealthBarStyle's material contract): the
 * bar makes its own dynamic instance of it and keeps the health values on it. Madrone change: owns its brush and
 * material (they leaked, and destroying a bar without a material crashed), takes its style by value (SetBarStyle, so a
 * fight can restyle it), and draws the style's background and frame.
 */
class NIGHTSKYENGINE_API SHealthBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHealthBar)
		: _BarFillType(EHealthBarFillType::LeftToRight)
		, _BarFillStyle(EHealthBarFillStyle::Mask)
		, _BorderPadding(FVector2D(0, 0))
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}

		/** Defines the direction in which the Health bar fills */
		SLATE_ARGUMENT(EHealthBarFillType::Type, BarFillType)

		/** Defines the visual style of the Health bar fill - scale or mask */
		SLATE_ARGUMENT(EHealthBarFillStyle::Type, BarFillStyle)

		/** Border Padding around fill bar */
		SLATE_ARGUMENT(FVector2D, BorderPadding)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                      const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                      int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	/** Health 0..1 (the material's HealthPercent). */
	void SetHealthPercent(float InPercent);

	/** Health + recoverable health 0..1 (the material's RecoverableHealthPercent, plus 1). */
	void SetRecoverableHealthPercent(float InPercent);

	/** The look: InStyle's fill, or FallbackFill when the style has none (the widget's own Image Fill), its background,
	 * frame and material parameters. Makes a new dynamic material and gives it the current health. */
	void SetBarStyle(const FHealthBarStyle& InStyle, const FSlateBrush* FallbackFill);

	void SetBarFillType(EHealthBarFillType::Type InBarFillType);
	void SetBarFillStyle(EHealthBarFillStyle::Type InBarFillStyle);
	void SetBorderPadding(FVector2D InBorderPadding);

private:
	/** Puts the health values, fill direction and the style's parameters on the dynamic material. */
	void PushMaterialParameters();

	FHealthBarStyle Style;
	FSlateBrush FillBrush;
	TStrongObjectPtr<UMaterialInstanceDynamic> FillMaterial;

	float HealthPercent = 1.0f;
	float RecoverableHealthPercent = 1.0f;
	EHealthBarFillType::Type BarFillType = EHealthBarFillType::LeftToRight;
	EHealthBarFillStyle::Type BarFillStyle = EHealthBarFillStyle::Mask;
	FVector2D BorderPadding = FVector2D::ZeroVector;
};
