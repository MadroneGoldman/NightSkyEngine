
#include "HealthBar.h"

#include "NightSkyBattleWidget.h"
#include "SHealthBar.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/DefaultStyleCache.h"
#include "Styling/UMGCoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HealthBar)

#define LOCTEXT_NAMESPACE "UMG"

UHealthBar::UHealthBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	//WidgetStyle = UE::Slate::Private::FDefaultStyleCache::GetRuntime().GetHealthBarStyle();
	//WidgetStyle = FAppStyle::Get().GetWidgetStyle<FHealthBarStyle>("HealthBarStyle");
	
#if WITH_EDITOR 
	if (IsEditorWidget())
	{
	//	WidgetStyle = UE::Slate::Private::FDefaultStyleCache::GetEditor().GetHealthBarStyle();
	}
#endif // WITH_EDITOR

	WidgetStyle.FillImage.TintColor = FLinearColor::White;

	BarFillType = EHealthBarFillType::LeftToRight;
	BarFillStyle = EHealthBarFillStyle::Mask;
	HealthPercent = 0;
	RecoverableHealthPercent = 0;
	FillColorAndOpacity = FLinearColor::White;
	BorderPadding = FVector2D(0, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UHealthBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	
	MyHealthBar.Reset();
}

TSharedRef<SWidget> UHealthBar::RebuildWidget()
{
	MyHealthBar = SNew(SHealthBar);
	return MyHealthBar.ToSharedRef();
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void UHealthBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!MyHealthBar.IsValid())
	{
		return;
	}
	
	SynchronizeHealthBar();
}

void UHealthBar::SynchronizeHealthBar()
{
	TAttribute<FSlateColor> FillColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, FillColorAndOpacity);
	
	MyHealthBar->SetHealthPercent(HealthPercent);
	MyHealthBar->SetRecoverableHealthPercent(RecoverableHealthPercent);
	MyHealthBar->SetFillColorAndOpacity(FillColorAndOpacityBinding);
	MyHealthBar->SetStyle(&WidgetStyle);
	MyHealthBar->SetBarFillType(BarFillType);
	MyHealthBar->SetBarFillStyle(BarFillStyle);
	MyHealthBar->SetBorderPadding(BorderPadding);
	

	FSlateBrush* DynamicMatBrush = new FSlateBrush();
	UMaterialInstanceDynamic* NewDynMaterial = nullptr;
	if (ImageFill.GetResourceObject() != nullptr)
	{
		if (const auto MInstance = Cast<UMaterialInterface>( ImageFill.GetResourceObject()); IsValid(MInstance))
		{
			UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MInstance);

			if ( !DynamicMaterial )
			{
				NewDynMaterial = UMaterialInstanceDynamic::Create(MInstance, this);
				DynamicMatBrush->SetResourceObject(NewDynMaterial);
				NewDynMaterial->SetScalarParameterValue(FName(TEXT("HealthPercent")), 1.0f);
				NewDynMaterial->SetScalarParameterValue(FName(TEXT("RecoverableHealthPercent")), 1.0f);
				
				// mat fills from right to left by default
				if (BarFillType == EHealthBarFillType::LeftToRight)
				{
					NewDynMaterial->SetScalarParameterValue(FName(TEXT("LeftToRight")), 1.0f);
				}
			}
		}
	}
	
	if (DynamicMatBrush->GetResourceObject() != nullptr)
	{
		MyHealthBar->SetDynamicMaterial(NewDynMaterial);
		MyHealthBar->SetFillImage(DynamicMatBrush);
	}
	else
	{
		MyHealthBar->SetFillImage(&ImageFill);
	}
}

void UHealthBar::SetImageFill(const FSlateBrush& InBrush)
{
	if(ImageFill != InBrush)
	{
		ImageFill = InBrush;
		// if (MyHealthBar.IsValid())
		// {
		// 	MyHealthBar->InvalidateImage();
		// }
	}
}

const FSlateBrush& UHealthBar::GetImageFill() const
{
	return ImageFill;

}

const FHealthBarStyle& UHealthBar::GetWidgetStyle() const
{
	return WidgetStyle;
}

void UHealthBar::SetWidgetStyle(const FHealthBarStyle& InStyle)
{
	WidgetStyle = InStyle;
	if (MyHealthBar.IsValid())
	{
		MyHealthBar->SetStyle(&WidgetStyle);
	}
}

float UHealthBar::GetHealthPercent() const
{
	return HealthPercent;
}

float UHealthBar::GetRecoverableHealthPercent() const
{
	return RecoverableHealthPercent;
}

void UHealthBar::SetHealthPercent(int index,float InPercent)
{
	if (index == MainPlayerIndex) //This widget is only used for the main two players
	{
		HealthPercent = InPercent;
		if (MyHealthBar.IsValid())
		{
			MyHealthBar->SetHealthPercent(InPercent);
		}
	}
}

void UHealthBar::SetRecoverableHealthPercent(int index,float InPercent)
{
	if (index == MainPlayerIndex) //This widget is only used for the main two players
	{
		RecoverableHealthPercent = InPercent;
		if (MyHealthBar.IsValid())
		{
			MyHealthBar->SetRecoverableHealthPercent(InPercent);
		}
	}
}

void UHealthBar::SetMainPlayerIndex(int PlayerIndex)
{
	MainPlayerIndex = PlayerIndex;
}

EHealthBarFillType::Type UHealthBar::GetBarFillType() const
{
	return BarFillType;
}

void UHealthBar::SetBarFillType(EHealthBarFillType::Type InBarFillType)
{
	BarFillType = InBarFillType;
	if (MyHealthBar.IsValid())
	{
		MyHealthBar->SetBarFillType(BarFillType);
	}
}

EHealthBarFillStyle::Type UHealthBar::GetBarFillStyle() const
{
	return BarFillStyle;
}

void UHealthBar::SetBarFillStyle(EHealthBarFillStyle::Type InBarFillStyle)
{
	BarFillStyle = InBarFillStyle;
	if (MyHealthBar.IsValid())
	{
		MyHealthBar->SetBarFillStyle(BarFillStyle);
	}
}

FVector2D UHealthBar::GetBorderPadding() const
{
	return BorderPadding;
}

void UHealthBar::SetBorderPadding(FVector2D InBorderPadding)
{
	BorderPadding = InBorderPadding;
	if (MyHealthBar.IsValid())
	{
		MyHealthBar->SetBorderPadding(BorderPadding);
	}
}

FLinearColor UHealthBar::GetFillColorAndOpacity() const
{
	return FillColorAndOpacity;
}

void UHealthBar::SetFillColorAndOpacity(FLinearColor Color)
{
	FillColorAndOpacity = Color;
	if (MyHealthBar.IsValid())
	{
		MyHealthBar->SetFillColorAndOpacity(FillColorAndOpacity);
	}
}

// void UHealthBar::OnHealthChanged(const float Value)
// {
// 	SetHealthPercent(Value);
// }
//
// void UHealthBar::OnRecoverableHealthChanged(const float Value)
// {
// 	SetRecoverableHealthPercent(Value);
// }

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITOR

const FText UHealthBar::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

void UHealthBar::OnCreationFromPalette()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FillColorAndOpacity = FLinearColor(0.f, 0.5f, 1.0f);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

