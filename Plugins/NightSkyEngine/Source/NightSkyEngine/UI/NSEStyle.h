#pragma once

#include "Styling/SlateWidgetStyle.h"
#include "NSEStyle.generated.h"

#define UE_API

USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FHealthBarStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	UE_API FHealthBarStyle();

	UE_API virtual ~FHealthBarStyle();

	UE_API virtual void GetResources( TArray< const FSlateBrush* >& OutBrushes ) const override;
	
	static UE_API const FName TypeName;
	UE_API virtual const FName GetTypeName() const override { return TypeName; };

	static UE_API const FHealthBarStyle& GetDefault();

	/** Background image to use for the Health bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush BackgroundImage;
	FHealthBarStyle& SetBackgroundImage( const FSlateBrush& InBackgroundImage ){ BackgroundImage = InBackgroundImage; return *this; }

	/** Foreground image to use for the Health bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush FillImage;
	FHealthBarStyle& SetFillImage( const FSlateBrush& InFillImage ){ FillImage = InFillImage; return *this; }

	/** Enables a simple animation on the fill image to give the appearance that Health has not stalled. Disable this if you have a custom material which animates itself. 
	 * This requires a pattern in your material or texture to give the appearance of movement.  A solid color will show no movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool EnableFillAnimation;
	FHealthBarStyle& SetEnableFillAnimation(bool InEnableFillAnimation) { EnableFillAnimation = InEnableFillAnimation; return *this; }

	/**
	* Unlinks all colors in this style.
	* @see FSlateColor::Unlink
	*/
	void UnlinkColors()
	{
		BackgroundImage.UnlinkColors();
		FillImage.UnlinkColors();
	}
};

inline FHealthBarStyle::FHealthBarStyle()
	: EnableFillAnimation(false)
{
}

inline FHealthBarStyle::~FHealthBarStyle() = default;

inline void FHealthBarStyle::GetResources( TArray< const FSlateBrush* >& OutBrushes ) const
{
	OutBrushes.Add( &BackgroundImage );
	OutBrushes.Add( &FillImage );
}

inline const FName FHealthBarStyle::TypeName( TEXT("FHealthBarStyle") );

inline const FHealthBarStyle& FHealthBarStyle::GetDefault()
{
	static FHealthBarStyle Default;
	return Default;
}

#undef UE_API
