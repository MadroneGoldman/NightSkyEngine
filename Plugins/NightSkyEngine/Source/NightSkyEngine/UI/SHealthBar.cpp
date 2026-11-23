#include "SHealthBar.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHealthBar)

namespace NSEHealthBar
{
	static const FName HealthParam(TEXT("HealthPercent"));
	static const FName RecoverableParam(TEXT("RecoverableHealthPercent"));
	static const FName DirectionParam(TEXT("LeftToRight"));
	// The fluid material reads recoverable health with 1 added (its own convention).
	constexpr float RecoverableOffset = 1.0f;

	static bool HasArt(const FSlateBrush& Brush)
	{
		return Brush.DrawAs != ESlateBrushDrawType::NoDrawType && Brush.GetResourceObject() != nullptr;
	}

	/** The bar's geometry grown by Margin on each side (frames reach past the bar). */
	static FPaintGeometry Grown(const FGeometry& Geometry, const FMargin& Margin)
	{
		const FVector2D Size = Geometry.GetLocalSize() + FVector2D(Margin.Left + Margin.Right, Margin.Top + Margin.Bottom);
		return Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D(-Margin.Left, -Margin.Top)));
	}
}

void SHealthBar::Construct(const FArguments& InArgs)
{
	BarFillType = InArgs._BarFillType;
	BarFillStyle = InArgs._BarFillStyle;
	BorderPadding = InArgs._BorderPadding;
	SetCanTick(false);
}

void SHealthBar::SetHealthPercent(const float InPercent)
{
	if (HealthPercent != InPercent)
	{
		HealthPercent = InPercent;
		if (FillMaterial.IsValid())
		{
			FillMaterial->SetScalarParameterValue(NSEHealthBar::HealthParam, HealthPercent);
		}
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetRecoverableHealthPercent(const float InPercent)
{
	if (RecoverableHealthPercent != InPercent)
	{
		RecoverableHealthPercent = InPercent;
		if (FillMaterial.IsValid())
		{
			FillMaterial->SetScalarParameterValue(NSEHealthBar::RecoverableParam, RecoverableHealthPercent + NSEHealthBar::RecoverableOffset);
		}
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetBarStyle(const FHealthBarStyle& InStyle, const FSlateBrush* FallbackFill)
{
	Style = InStyle;
	FillBrush = NSEHealthBar::HasArt(Style.FillImage) || !FallbackFill ? Style.FillImage : *FallbackFill;
	FillMaterial.Reset();
	if (UMaterialInterface* Material = Cast<UMaterialInterface>(FillBrush.GetResourceObject()))
	{
		// Each bar gets its own instance: the health lives on it.
		FillMaterial.Reset(UMaterialInstanceDynamic::Create(Material, GetTransientPackage()));
		FillBrush.SetResourceObject(FillMaterial.Get());
		PushMaterialParameters();
	}
	Invalidate(EInvalidateWidget::Paint);
}

void SHealthBar::PushMaterialParameters()
{
	if (!FillMaterial.IsValid())
	{
		return;
	}
	for (const TPair<FName, float>& Param : Style.ScalarParameters)
	{
		FillMaterial->SetScalarParameterValue(Param.Key, Param.Value);
	}
	for (const TPair<FName, FLinearColor>& Param : Style.VectorParameters)
	{
		FillMaterial->SetVectorParameterValue(Param.Key, Param.Value);
	}
	// The material fills from the right by default.
	FillMaterial->SetScalarParameterValue(NSEHealthBar::DirectionParam, BarFillType == EHealthBarFillType::LeftToRight ? 1.0f : 0.0f);
	FillMaterial->SetScalarParameterValue(NSEHealthBar::HealthParam, HealthPercent);
	FillMaterial->SetScalarParameterValue(NSEHealthBar::RecoverableParam, RecoverableHealthPercent + NSEHealthBar::RecoverableOffset);
}

void SHealthBar::SetBarFillType(const EHealthBarFillType::Type InBarFillType)
{
	if (BarFillType != InBarFillType)
	{
		BarFillType = InBarFillType;
		PushMaterialParameters();
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetBarFillStyle(const EHealthBarFillStyle::Type InBarFillStyle)
{
	if (BarFillStyle != InBarFillStyle)
	{
		BarFillStyle = InBarFillStyle;
		Invalidate(EInvalidateWidget::Paint);
	}
}

void SHealthBar::SetBorderPadding(const FVector2D InBorderPadding)
{
	if (BorderPadding != InBorderPadding)
	{
		BorderPadding = InBorderPadding;
		Invalidate(EInvalidateWidget::Paint);
	}
}

// Returns false if the clipping zone area is zero in which case we should skip drawing
static bool PushTransformedClip(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, FVector2D InsetPadding, FVector2D HealthOrigin, FSlateRect Health)
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

int32 SHealthBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = LayerId;
	const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	if (NSEHealthBar::HasArt(Style.BackgroundImage))
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer++, NSEHealthBar::Grown(AllottedGeometry, Style.FrameMargin),
			&Style.BackgroundImage, DrawEffects, Tint * Style.BackgroundImage.GetTint(InWidgetStyle));
	}

	if (FillBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
	{
		EHealthBarFillType::Type FillType = BarFillType;
		if (GSlateFlowDirection == EFlowDirection::RightToLeft)
		{
			FillType = FillType == EHealthBarFillType::LeftToRight ? EHealthBarFillType::RightToLeft : EHealthBarFillType::LeftToRight;
		}
		// The material draws the health: the clip only trims the border padding.
		const bool bClipped = FillType == EHealthBarFillType::RightToLeft
			? PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPadding, FVector2D(1, 0), FSlateRect(1, 0, 0, 1))
			: PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPadding, FVector2D(0, 0), FSlateRect(0, 0, 1, 1));
		if (bClipped)
		{
			FSlateDrawElement::MakeBox(OutDrawElements, Layer++, AllottedGeometry.ToPaintGeometry(), &FillBrush, DrawEffects,
				Tint * FillBrush.GetTint(InWidgetStyle));
			OutDrawElements.PopClip();
		}
	}

	if (NSEHealthBar::HasArt(Style.ForegroundImage))
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer++, NSEHealthBar::Grown(AllottedGeometry, Style.FrameMargin),
			&Style.ForegroundImage, DrawEffects, Tint * Style.ForegroundImage.GetTint(InWidgetStyle));
	}

	return FMath::Max(LayerId, Layer - 1);
}

FVector2D SHealthBar::ComputeDesiredSize(float) const
{
	return FVector2D(0.0f, 0.0f);
}
