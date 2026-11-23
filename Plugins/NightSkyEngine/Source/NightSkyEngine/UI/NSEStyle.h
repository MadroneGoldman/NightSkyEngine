#pragma once

#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "NSEStyle.generated.h"

/**
 * One health bar's look (Slate widget style). Madrone change: background and foreground (frame) are drawn, the fill
 * material's parameters can be overridden, and a frame can reach past the bar (FrameMargin).
 *
 * Fill material contract: scalar parameters HealthPercent (0..1), RecoverableHealthPercent (health + recoverable, plus 1)
 * and LeftToRight (1 = fills from the left). Mat_GUI_HealthFluid (/WitchTools/BattleCommon/fantasy_gui_4) follows it.
 */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FHealthBarStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FHealthBarStyle();
	virtual ~FHealthBarStyle() override;

	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }

	static const FHealthBarStyle& GetDefault();

	/** Drawn under the fill when it has an image or material (FrameMargin around the bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush BackgroundImage;
	FHealthBarStyle& SetBackgroundImage(const FSlateBrush& InBackgroundImage) { BackgroundImage = InBackgroundImage; return *this; }

	/** The fill: a material following the contract above, or an image. With no image or material set, the widget's own
	 * Image Fill is used (so a style can only recolour, through the parameters below). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush FillImage;
	FHealthBarStyle& SetFillImage(const FSlateBrush& InFillImage) { FillImage = InFillImage; return *this; }

	/** A frame drawn over the fill when it has an image or material (FrameMargin around the bar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush ForegroundImage;

	/** How far the background and foreground reach past the bar on each side, in slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FMargin FrameMargin;

	/** Scalar parameters set on the fill material (e.g. a glitch amount). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	TMap<FName, float> ScalarParameters;

	/** Vector parameters set on the fill material (Mat_GUI_HealthFluid: FluidColor, FluidGlowColor, InnerCloudsColor,
	 * RecoverableHealthColor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	TMap<FName, FLinearColor> VectorParameters;

	/** Unused by the material bars (the material animates itself); kept for the serialized widget data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool EnableFillAnimation;
	FHealthBarStyle& SetEnableFillAnimation(bool InEnableFillAnimation) { EnableFillAnimation = InEnableFillAnimation; return *this; }

	void UnlinkColors()
	{
		BackgroundImage.UnlinkColors();
		FillImage.UnlinkColors();
		ForegroundImage.UnlinkColors();
	}
};

inline FHealthBarStyle::FHealthBarStyle()
	: EnableFillAnimation(false)
{
}

inline FHealthBarStyle::~FHealthBarStyle() = default;

inline void FHealthBarStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	OutBrushes.Add(&BackgroundImage);
	OutBrushes.Add(&FillImage);
	OutBrushes.Add(&ForegroundImage);
}

inline const FHealthBarStyle& FHealthBarStyle::GetDefault()
{
	static FHealthBarStyle Default;
	return Default;
}

/**
 * Madrone change: the battle HUD's look for one side (both, by default), e.g. a boss fight's. Authored as a Slate
 * Widget Style asset (Content Browser > User Interface > Slate Widget Style, type NSE Battle Hud Style) and chosen per
 * fight by FNSEHudStyleRequest. Anything left at its default keeps the HUD Blueprint's own look.
 */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FNSEBattleHudStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override
	{
		MainHealthBar.GetResources(OutBrushes);
		TeammateHealthBar.GetResources(OutBrushes);
	}

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }

	static const FNSEBattleHudStyle& GetDefault()
	{
		static FNSEBattleHudStyle Default;
		return Default;
	}

	/** For the HUD Blueprint (OnBattleHudStyleChanged): e.g. "Boss" to show extra art. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FName ThemeId;

	/** The fighter on screen's bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FHealthBarStyle MainHealthBar;

	/** The partners' small bars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FHealthBarStyle TeammateHealthBar;

	/** Scales the main bar (a bigger bar for a boss). 1 = the Blueprint's size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Layout)
	FVector2D MainBarScale = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Names, meta=(InlineEditConditionToggle))
	bool bOverrideNameFont = false;

	/** The fighter names' font (P1Name / P2Name and the partner names). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Names, meta=(EditCondition="bOverrideNameFont"))
	FSlateFontInfo NameFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Names, meta=(InlineEditConditionToggle))
	bool bOverrideNameColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Names, meta=(EditCondition="bOverrideNameColor"))
	FSlateColor NameColor = FSlateColor(FLinearColor::White);
};

/** The container that makes FNSEBattleHudStyle a Slate Widget Style asset type. */
UCLASS(BlueprintType, hidecategories=Object, meta=(DisplayName="NSE Battle Hud Style"))
class NIGHTSKYENGINE_API UNSEBattleHudWidgetStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category=Appearance, meta=(ShowOnlyInnerProperties))
	FNSEBattleHudStyle HudStyle;

	virtual const FSlateWidgetStyle* const GetStyle() const override { return static_cast<const FSlateWidgetStyle*>(&HudStyle); }
};

/** Which sides a battle HUD style restyles. */
UENUM(BlueprintType)
enum class ENSEHudStyleScope : uint8
{
	/** Both sides' bars (the default: a boss fight's HUD). */
	WholeHud,
	/** Only the side of the fighter that asks for it. */
	OwnSide,
};

/**
 * Madrone change: a fighter's (or a fight's) request for a battle HUD style. Set on a character (CHR_ asset, Battle Hud
 * Style), an overworld encounter (WitchNPCDefinition / WitchBattle component) or the game state (override / default).
 * Every frame the HUD shows, per side, the highest Priority request that applies; ties go to player 2. Presentation
 * only: never rollback state.
 */
USTRUCT(BlueprintType)
struct NIGHTSKYENGINE_API FNSEHudStyleRequest
{
	GENERATED_BODY()

	/** A Slate Widget Style asset of type NSE Battle Hud Style. Empty: no request. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD", meta=(AllowedClasses="/Script/SlateCore.SlateWidgetStyleAsset"))
	TSoftObjectPtr<USlateWidgetStyleAsset> Style;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	ENSEHudStyleScope Scope = ENSEHudStyleScope::WholeHud;

	/** Only while this fighter is the one on screen (follows tags). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HUD")
	bool bOnlyWhileOnScreen = false;

	bool IsSet() const { return !Style.IsNull(); }
};
