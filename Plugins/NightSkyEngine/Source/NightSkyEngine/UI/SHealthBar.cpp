#include "SHealthBar.h"
#include "Rendering/DrawElements.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHealthBar)


void SHealthBar::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	Style = InArgs._Style;

	SetHealthPercent(InArgs._HealthPercent);
	SetRecoverableHealthPercent(InArgs._RecoverableHealthPercent);
	BarFillType = InArgs._BarFillType;
	BarFillStyle = InArgs._BarFillStyle;
	
	FillColorAndOpacity = InArgs._FillColorAndOpacity;
	BorderPadding = InArgs._BorderPadding;

	CurrentTickRate = 0.0f;
	MinimumTickRate = InArgs._RefreshRate;
	
	FillImage = InArgs._FillImage;
	BackgroundImage = InArgs._BackgroundImage;
	
	SetCanTick(false);
	
}

void SHealthBar::SetHealthPercent(TAttribute< TOptional<float> > InPercent)
{
	if ( !HealthPercent.IdenticalTo(InPercent) )
	{
		HealthPercent = InPercent;
		if (FillImage && FillImage->GetResourceObject() != nullptr)
		{
			if (const auto MIDynamic = Cast<UMaterialInstanceDynamic>(FillImage->GetResourceObject()); IsValid(MIDynamic))
			{
				MIDynamic->SetScalarParameterValue(FName(TEXT("HealthPercent")), HealthPercent.Get().GetValue());
			}
		}
		
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SHealthBar::SetRecoverableHealthPercent(TAttribute< TOptional<float> > InPercent)
{
	if ( !RecoverableHealthPercent.IdenticalTo(InPercent) )
	{
		RecoverableHealthPercent = InPercent;
		if (FillImage && FillImage->GetResourceObject() != nullptr)
		{
			if (const auto MIDynamic = Cast<UMaterialInstanceDynamic>(FillImage->GetResourceObject()); IsValid(MIDynamic))
			{
				MIDynamic->SetScalarParameterValue(FName(TEXT("RecoverableHealthPercent")), RecoverableHealthPercent.Get().GetValue() + 1.0f);
			}
		}
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SHealthBar::SetStyle(const FHealthBarStyle* InStyle)
{
	Style = InStyle;

	if (Style == nullptr)
	{
		Style = new FHealthBarStyle();
		//FArguments Defaults;
		//Style = Defaults._Style;
	}

	check(Style);
	
	Invalidate(EInvalidateWidget::Layout);
}

void SHealthBar::SetBarFillType(EHealthBarFillType::Type InBarFillType)
{
	if(BarFillType != InBarFillType)
	{
		BarFillType = InBarFillType;
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetBarFillStyle(EHealthBarFillStyle::Type InBarFillStyle)
{
	if (BarFillStyle != InBarFillStyle)
	{
		BarFillStyle = InBarFillStyle;
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetFillColorAndOpacity(TAttribute< FSlateColor > InFillColorAndOpacity)
{
	if(!FillColorAndOpacity.IdenticalTo(InFillColorAndOpacity))
	{
		FillColorAndOpacity = InFillColorAndOpacity;
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetBorderPadding(TAttribute< FVector2D > InBorderPadding)
{
	if(!BorderPadding.IdenticalTo(InBorderPadding))
	{
		BorderPadding = InBorderPadding;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SHealthBar::SetBackgroundImage(const FSlateBrush* InBackgroundImage)
{
	if(BackgroundImage != InBackgroundImage)
	{
		BackgroundImage = InBackgroundImage;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SHealthBar::SetFillImage(FSlateBrush* InFillImage)
{
	if(InFillImage && FillImage != InFillImage)
	{
		FillImage = InFillImage;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SHealthBar::SetDynamicMaterial(UMaterialInstanceDynamic* DynamicMaterial)
{
	DynMaterial = DynamicMaterial;
	DynMaterial->AddToRoot();
	
}

SHealthBar::~SHealthBar()
{
	DynMaterial->RemoveFromRoot();
}

const FSlateBrush* SHealthBar::GetBackgroundImage() const
{
	return BackgroundImage ? BackgroundImage : &Style->BackgroundImage;
}

FSlateBrush* SHealthBar::GetFillImage() const
{
	return FillImage;
	//return FillImage ? FillImage : &Style->FillImage;
}

// Returns false if the clipping zone area is zero in which case we should skip drawing
bool PushTransformedClip(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, FVector2D InsetPadding, FVector2D HealthOrigin, FSlateRect Health)
{
	const FSlateRenderTransform& Transform = AllottedGeometry.GetAccumulatedRenderTransform();

	const FVector2D MaxSize = AllottedGeometry.GetLocalSize() - ( InsetPadding * 2.0f );

	const FSlateClippingZone ClippingZone(Transform.TransformPoint(InsetPadding + (HealthOrigin - FVector2D(Health.Left, Health.Top)) * MaxSize),
		Transform.TransformPoint(InsetPadding + FVector2D(HealthOrigin.X + Health.Right, HealthOrigin.Y - Health.Top) * MaxSize),
		Transform.TransformPoint(InsetPadding + FVector2D(HealthOrigin.X - Health.Left, HealthOrigin.Y + Health.Bottom) * MaxSize),
		Transform.TransformPoint(InsetPadding + (HealthOrigin + FVector2D(Health.Right, Health.Bottom)) * MaxSize));

	if (ClippingZone.HasZeroArea())
	{
		return false;
	}

	OutDrawElements.PushClip(ClippingZone);
	return true;
}

int32 SHealthBar::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Used to track the layer ID we will return.
	int32 RetLayerId = LayerId;

	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	FSlateBrush* CurrentFillImage = GetFillImage();

	TOptional<float> HealthFraction = HealthPercent.Get();	
	FVector2D BorderPaddingRef = BorderPadding.Get();

	// Background image stuff not being used right now
	/*
	const FSlateBrush* CurrentBackgroundImage = GetBackgroundImage();
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		CurrentBackgroundImage,
		DrawEffects
		//InWidgetStyle.GetColorAndOpacityTint() * CurrentBackgroundImage->GetTint( InWidgetStyle )
	);
	*/
	
	if( HealthFraction.IsSet() )
	{
		//const FLinearColor FillColorAndOpacitySRGB = FLinearColor(1.0f, 0.0f, 0.0f, 0.3f);

		EHealthBarFillType::Type ComputedBarFillType = BarFillType;
		if (GSlateFlowDirection == EFlowDirection::RightToLeft)
		{
			switch (ComputedBarFillType)
			{
			case EHealthBarFillType::LeftToRight:
				ComputedBarFillType = EHealthBarFillType::RightToLeft;
				break;
			case EHealthBarFillType::RightToLeft:
				ComputedBarFillType = EHealthBarFillType::LeftToRight;
				break;
			}
		}

		bool bScaleWithFillPerc = BarFillStyle == EHealthBarFillStyle::Scale;

		//const float ClampedFraction = FMath::Clamp(HealthFraction.GetValue(), 0.0f, 1.0f);
		const float ClampedFraction = 1.0f; //TODO: fix this its not being used (put fill bar outside of clip zone, use clip for something i guess )
		switch (ComputedBarFillType)
		{
			case EHealthBarFillType::RightToLeft:
			{
				if (PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(1, 0), FSlateRect(ClampedFraction, 0, 0, 1)))
				{
					// Draw Fill
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						RetLayerId++,
						AllottedGeometry.ToPaintGeometry(), //PaintRect,
						CurrentFillImage,
						DrawEffects//,
						//FillColorAndOpacitySRGB
					);

					OutDrawElements.PopClip();
				}
				break;
			}
			case EHealthBarFillType::LeftToRight:
			default:
			{
				if (PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0, 0), FSlateRect(0, 0, ClampedFraction, 1)))
				{
					// Draw fill
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						RetLayerId++,
						AllottedGeometry.ToPaintGeometry(), //PaintRect,
						CurrentFillImage,
						DrawEffects//,
						//FillColorAndOpacitySRGB
					);
					OutDrawElements.PopClip();
				}


				break;
			}
		}
	}
	
	//return RetLayerId - 1; crashes sometimes
	return RetLayerId > 1 ? RetLayerId - 1 : 1; 
}

FVector2D SHealthBar::ComputeDesiredSize( float ) const
{
	return FVector2D(0.0f, 0.0f);
}

bool SHealthBar::ComputeVolatility() const
{
	return SLeafWidget::ComputeVolatility() || HealthPercent.IsBound() || RecoverableHealthPercent.IsBound() || FillColorAndOpacity.IsBound() || BorderPadding.IsBound();
}

void SHealthBar::SetActiveTimerTickRate(float TickRate)
{
	if (CurrentTickRate != TickRate || !ActiveTimerHandle.IsValid())
	{
		CurrentTickRate = TickRate;

		TSharedPtr<FActiveTimerHandle> SharedActiveTimerHandle = ActiveTimerHandle.Pin();
		if (SharedActiveTimerHandle.IsValid())
		{
			UnRegisterActiveTimer(SharedActiveTimerHandle.ToSharedRef());
		}
	}
}

EActiveTimerReturnType SHealthBar::ActiveTick(double InCurrentTime, float InDeltaTime)
{
	
	TOptional<float> PercentFraction = HealthPercent.Get();
	if (PercentFraction.IsSet() && !Style->EnableFillAnimation)
	{
		SetActiveTimerTickRate(MinimumTickRate);
	}
	else
	{
		SetActiveTimerTickRate(0.0f);
	}

	Invalidate(EInvalidateWidget::Paint);

	return EActiveTimerReturnType::Continue;
}



