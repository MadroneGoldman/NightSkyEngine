#pragma once

#include "CoreMinimal.h"
#include "NSEStyle.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "SHealthBar.generated.h"

#define UE_API

class FActiveTimerHandle;
class FPaintArgs;
class FSlateWindowElementList;

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
		// will scale up from the midpoint to the outer edges both vertically and horizontally
		//FillFromCenter,
		// will fill up from the centerline to the outer edges horizontally
		//FillFromCenterHorizontal,
		// will fill up from the centerline to the outer edges vertically
		//FillFromCenterVertical,
		// will fill up from the top to the the bottom
		//TopToBottom,
		// will fill up from the bottom to the the top
		//BottomToTop,
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

/** A Health bar widget.*/
class NIGHTSKYENGINE_API SHealthBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHealthBar)
			: _Style(  new FHealthBarStyle() )
			//: _Style( &FAppStyle::Get().GetWidgetStyle<FHealthBarStyle>("HealthBarStyle") )
			  , _BarFillType(EHealthBarFillType::LeftToRight)
			  , _BarFillStyle(EHealthBarFillStyle::Mask)
			  , _HealthPercent(TOptional<float>(1.0f))
			  , _RecoverableHealthPercent(TOptional<float>(1.0f))
			  , _FillColorAndOpacity(FLinearColor::White)
			  , _BorderPadding(FVector2D(0, 0))
			  , _BackgroundImage(nullptr)
			  , _FillImage(nullptr)
			  , _RefreshRate(2.0f)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		/** Style used for the Health bar */
		SLATE_STYLE_ARGUMENT(FHealthBarStyle, Style)

		/** Defines the direction in which the Health bar fills */
		SLATE_ARGUMENT(EHealthBarFillType::Type, BarFillType)

		/** Defines the visual style of the Health bar fill - scale or mask */
		SLATE_ARGUMENT(EHealthBarFillStyle::Type, BarFillStyle)

		/** Used to determine the fill position of the Health bar ranging 0..1 */
		SLATE_ATTRIBUTE(TOptional<float>, HealthPercent)

		/** Used to determine the fill position of the Health bar ranging 0..1 */
		SLATE_ATTRIBUTE(TOptional<float>, RecoverableHealthPercent)

		/** Fill Color and Opacity */
		SLATE_ATTRIBUTE(FSlateColor, FillColorAndOpacity)

		/** Border Padding around fill bar */
		SLATE_ATTRIBUTE(FVector2D, BorderPadding)

		/** The brush to use as the background of the Health bar */
		SLATE_ARGUMENT(const FSlateBrush*, BackgroundImage)

		/** The brush to use as the fill image */
		SLATE_ARGUMENT(FSlateBrush*, FillImage)
		//const FSlateBrush* _FillImage;

		/*WidgetArgsType& FillImage(const FSlateBrush* InArg)
		{
			if (const auto MInstance = Cast<UMaterialInstance>( InArg->GetResourceObject()); IsValid(MInstance))
			{
				UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(MInstance, InArg->GetResourceObject());
				FSlateBrush* NewFillImage = new FSlateBrush();
				NewFillImage->SetResourceObject(DynMaterial);
				
				_FillImage = NewFillImage;
			}
			else{_FillImage = InArg;}
			
			return static_cast<WidgetArgsType*>(this)->Me();
		}*/

		/** Rate at which this widget is ticked when sleeping in seconds */
		SLATE_ARGUMENT(float, RefreshRate)

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	UE_API void Construct(const FArguments& InArgs);

	UE_API virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                             const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                             int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UE_API virtual FVector2D ComputeDesiredSize(float) const override;
	UE_API virtual bool ComputeVolatility() const override;

	/** See attribute Health */
	UE_API void SetHealthPercent(TAttribute<TOptional<float>> InPercent);

	/** See attribute RecoverableHealth */
	UE_API void SetRecoverableHealthPercent(TAttribute<TOptional<float>> InPercent);

	/** See attribute Style */
	UE_API void SetStyle(const FHealthBarStyle* InStyle);

	/** See attribute BarFillType */
	UE_API void SetBarFillType(EHealthBarFillType::Type InBarFillType);

	/** See attribute BarFillStyle */
	UE_API void SetBarFillStyle(EHealthBarFillStyle::Type InBarFillStyle);

	/** See attribute SetFillColorAndOpacity */
	UE_API void SetFillColorAndOpacity(TAttribute<FSlateColor> InFillColorAndOpacity);

	/** See attribute BorderPadding */
	UE_API void SetBorderPadding(TAttribute<FVector2D> InBorderPadding);

	/** See attribute BackgroundImage */
	UE_API void SetBackgroundImage(const FSlateBrush* InBackgroundImage);

	/** See attribute FillImage */
	UE_API void SetFillImage(FSlateBrush* InFillImage);
	
	/** See attribute FillImage */
	void SetDynamicMaterial(UMaterialInstanceDynamic* DynamicMaterial);

	// Override the destructor
	virtual ~SHealthBar() override;
	
private:
	/** Controls the speed at which the widget is ticked when in slate sleep mode */
	void SetActiveTimerTickRate(float TickRate);

	/** Widgets active tick */
	EActiveTimerReturnType ActiveTick(double InCurrentTime, float InDeltaTime);

	/** Gets the current background image. */
	const FSlateBrush* GetBackgroundImage() const;
	
	/** Gets the current fill image */
	FSlateBrush* GetFillImage() const;

private:
	/** The style of the Health bar */
	const FHealthBarStyle* Style;

	/** The text displayed over the Health bar */
	TAttribute<TOptional<float>> HealthPercent = 1.0f;

	/** The text displayed over the Health bar */
	TAttribute<TOptional<float>> RecoverableHealthPercent = 1.0f;;

	EHealthBarFillType::Type BarFillType;

	EHealthBarFillStyle::Type BarFillStyle;

	/** Background image to use for the Health bar */
	const FSlateBrush* BackgroundImage;

	/** Foreground image to use for the Health bar */
	UPROPERTY()
	FSlateBrush* FillImage;
	
	/** Material to use as FillImage resource object */
	UPROPERTY()
	UMaterialInstanceDynamic* DynMaterial;

	/** Fill Color and Opacity */
	TAttribute<FSlateColor> FillColorAndOpacity;

	/** Border Padding */
	TAttribute<FVector2D> BorderPadding;

	/** Reference to the widgets current active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	/** Rate at which the widget is currently ticked when slate sleep mode is active */
	float CurrentTickRate;

	/** The slowest that this widget can tick when in slate sleep mode */
	float MinimumTickRate;
};

#undef UE_API
