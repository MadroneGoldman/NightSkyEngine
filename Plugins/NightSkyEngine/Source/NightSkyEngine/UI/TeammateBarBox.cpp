// Fill out your copyright notice in the Description page of Project Settings.


#include "TeammateBarBox.h"
#include "Widgets/Layout/SBox.h" // Madrone change
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TeammateBarBox)

UTeammateBarBox::UTeammateBarBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BarFillType = EHealthBarFillType::LeftToRight;
}

void UTeammateBarBox::SetHealthPercent(int index,float InPercent)
{
	//input is index of whole team but we only need the teammate index
	if (index == 0){return;}
	index --;
	
	if (TeammateHealthBars.IsValidIndex(index) && TeammateHealthBars[index].IsValid())
	{
		TeammateHealthBars[index]->SetHealthPercent(InPercent);
	}
}

void UTeammateBarBox::SetRecoverableHealthPercent(int index,float InPercent)
{
	//input is index of whole team but we only need the teammate index
	if (index == 0){return;}
	index --;
	
	if (TeammateHealthBars.IsValidIndex(index) && TeammateHealthBars[index].IsValid())
	{
		TeammateHealthBars[index]->SetRecoverableHealthPercent(InPercent);
	}
}

void UTeammateBarBox::AddHealthbars(int NewCount)
{
	if ( MyVerticalBox.IsValid())
	{
		TeammateHealthBars.Empty();
		MyVerticalBox->ClearChildren();
		TeammateCount = NewCount;
		
		//Spacer at the top
		MyVerticalBox->AddSlot().FillHeight(1.5f)
			[
				SNew(SSpacer)
			];
		
		for (int i = 0; i < TeammateCount; i++)
		{
			TSharedPtr<SHealthBar> CurrentTeammateHealthBar;
			MyVerticalBox->AddSlot()
			[
				SAssignNew(CurrentTeammateHealthBar, SHealthBar)
				.BarFillType(BarFillType)
			];
			ApplyStyle(*CurrentTeammateHealthBar);
			TeammateHealthBars.Add(CurrentTeammateHealthBar);
		}
		// Madrone change: the character-extension bars are made again by the next SetCharacterBars
		VirtualHealthBars.Empty();
		VirtualBarColors.Empty();
		VirtualBarVisible.Empty();
		RealBarColors.Empty();
		RealBarNames.Empty();
		VirtualBarNames.Empty();
		VirtualBarCells.Empty();
		bCharacterLayout = false;
	}
}

// Madrone change: a recoloured bar: the style's fill colours and the fill tint
void UTeammateBarBox::ApplyStyle(SHealthBar& Bar, const TOptional<FLinearColor>& Color) const
{
	if (!Color.IsSet())
	{
		ApplyStyle(Bar);
		return;
	}
	FHealthBarStyle Style = StyleOverride.IsSet() ? StyleOverride.GetValue() : FHealthBarStyle::GetDefault();
	const FLinearColor C = Color.GetValue();
	Style.VectorParameters.Add(TEXT("FluidColor"), C);
	Style.VectorParameters.Add(TEXT("FluidGlowColor"), FLinearColor(FMath::Min(C.R * 1.4f + 0.1f, 1.f), FMath::Min(C.G * 1.4f + 0.1f, 1.f), FMath::Min(C.B * 1.4f + 0.1f, 1.f), 1.f));
	Style.VectorParameters.Add(TEXT("InnerCloudsColor"), C * 0.8f);
	FSlateBrush Fill = (Style.FillImage.DrawAs != ESlateBrushDrawType::NoDrawType && Style.FillImage.GetResourceObject()) ? Style.FillImage : ImageFill;
	Fill.TintColor = FSlateColor(C);
	Style.FillImage = Fill;
	Bar.SetBarFillType(BarFillType);
	Bar.SetBarStyle(Style, &ImageFill);
}

void UTeammateBarBox::ApplyStyle(SHealthBar& Bar) const
{
	// Madrone change: the Slate bar owns its brush and dynamic material (this leaked one brush per bar per round).
	Bar.SetBarFillType(BarFillType);
	Bar.SetBarStyle(StyleOverride.IsSet() ? StyleOverride.GetValue() : FHealthBarStyle::GetDefault(), &ImageFill);
}

void UTeammateBarBox::SetStyleOverride(const FHealthBarStyle& InStyle)
{
	StyleOverride = InStyle;
	for (const TSharedPtr<SHealthBar>& Bar : TeammateHealthBars)
	{
		if (Bar.IsValid()) ApplyStyle(*Bar);
	}
	// Madrone change: recoloured and virtual bars are restyled on the next SetCharacterBars
	RealBarColors.Empty();
	for (int32 i = 0; i < VirtualHealthBars.Num(); i++) if (VirtualHealthBars[i].IsValid()) { ApplyStyle(*VirtualHealthBars[i]); VirtualBarColors[i].Reset(); }
}

void UTeammateBarBox::ClearStyleOverride()
{
	if (!StyleOverride.IsSet()) return;
	StyleOverride.Reset();
	for (const TSharedPtr<SHealthBar>& Bar : TeammateHealthBars)
	{
		if (Bar.IsValid()) ApplyStyle(*Bar);
	}
}

// Madrone change: character-extension bars
void UTeammateBarBox::SetCharacterBars(const TArray<FNSETeammateBar>& Virtual, const TArray<FNSETeammateBar>& RealOverrides)
{
	if (!MyVerticalBox.IsValid()) return;
	// Real teammates: recolour when asked, back to the style when not.
	RealBarColors.SetNum(TeammateHealthBars.Num());
	for (int32 i = 0; i < TeammateHealthBars.Num(); i++)
	{
		TOptional<FLinearColor> Want;
		if (RealOverrides.IsValidIndex(i) && RealOverrides[i].bOverrideColor) Want = RealOverrides[i].Color;
		if (Want != RealBarColors[i] && TeammateHealthBars[i].IsValid())
		{
			RealBarColors[i] = Want;
			ApplyStyle(*TeammateHealthBars[i], Want);
		}
	}
	// Virtual bars: added after the real ones as needed (never removed mid-fight, collapsed instead).
	bool bRebuild = false;
	while (VirtualHealthBars.Num() < Virtual.Num())
	{
		TSharedPtr<SHealthBar> Bar = SNew(SHealthBar).BarFillType(BarFillType);
		ApplyStyle(*Bar);
		VirtualHealthBars.Add(Bar);
		VirtualBarColors.AddDefaulted();
		VirtualBarVisible.Add(true);
		bRebuild = true;
	}
	if (bRebuild) RebuildCharacterLayout();
	if (bCharacterLayout)
	{
		for (int32 i = 0; i < RealBarNames.Num(); i++)
		{
			if (RealBarNames[i].IsValid() && RealOverrides.IsValidIndex(i)) RealBarNames[i]->SetText(RealOverrides[i].Name);
		}
		for (int32 i = 0; i < VirtualBarNames.Num(); i++)
		{
			if (VirtualBarNames[i].IsValid() && Virtual.IsValidIndex(i)) VirtualBarNames[i]->SetText(Virtual[i].Name);
		}
	}
	for (int32 i = 0; i < VirtualHealthBars.Num(); i++)
	{
		const TSharedPtr<SHealthBar>& Bar = VirtualHealthBars[i];
		if (!Bar.IsValid()) continue;
		const bool bVisible = Virtual.IsValidIndex(i) && Virtual[i].bVisible;
		if (bVisible != VirtualBarVisible[i])
		{
			VirtualBarVisible[i] = bVisible;
			// Hidden, not collapsed: the grid keeps its place
			const TSharedPtr<SWidget>& Cell = VirtualBarCells.IsValidIndex(i) ? VirtualBarCells[i] : TSharedPtr<SWidget>();
			if (Cell.IsValid()) Cell->SetVisibility(bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden);
			else Bar->SetVisibility(bVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden);
		}
		if (!Virtual.IsValidIndex(i)) continue;
		TOptional<FLinearColor> Want;
		if (Virtual[i].bOverrideColor) Want = Virtual[i].Color;
		if (Want != VirtualBarColors[i])
		{
			VirtualBarColors[i] = Want;
			ApplyStyle(*Bar, Want);
		}
		Bar->SetHealthPercent(FMath::Clamp(Virtual[i].Health, 0.f, 1.f));
		Bar->SetRecoverableHealthPercent(FMath::Clamp(FMath::Max(Virtual[i].RecoverableHealth, Virtual[i].Health), 0.f, 1.f));
	}
}

TSharedRef<SWidget> UTeammateBarBox::MakeCharacterCell(const TSharedPtr<SHealthBar>& Bar, TSharedPtr<STextBlock>& OutName, const int32 NameSize) const
{
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", NameSize);
	Font.OutlineSettings.OutlineSize = 1;
	const TSharedRef<SWidget> Name = SAssignNew(OutName, STextBlock).Font(Font).ColorAndOpacity(FLinearColor::White);
	const bool bLeftToRight = BarFillType == EHealthBarFillType::LeftToRight;
	TSharedRef<SHorizontalBox> Cell = SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible);
	if (bLeftToRight) Cell->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 3, 0)[Name];
	Cell->AddSlot().FillWidth(1.f).Padding(0, NameSize < CharacterBarNameSize ? 0.f : 1.f)[Bar.ToSharedRef()];
	if (!bLeftToRight) Cell->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0, 0, 0)[Name];
	return Cell;
}

void UTeammateBarBox::RebuildCharacterLayout()
{
	if (!MyVerticalBox.IsValid()) return;
	bCharacterLayout = true;
	MyVerticalBox->ClearChildren();
	RealBarNames.SetNum(TeammateHealthBars.Num());
	VirtualBarNames.SetNum(VirtualHealthBars.Num());
	VirtualBarCells.SetNum(VirtualHealthBars.Num());
	TArray<TSharedRef<SWidget>> Cells;
	// One squashed vertical list above the fighter's bar (the user's call, 2026-11-26: no extra columns). Past four bars
	// the rows get thinner, and the names smaller to fit them.
	int32 Count = VirtualHealthBars.Num();
	for (const TSharedPtr<SHealthBar>& Bar : TeammateHealthBars) Count += Bar.IsValid() ? 1 : 0;
	const int32 NameSize = Count > 4 ? FMath::Max(5, CharacterBarNameSize * 4 / Count) : CharacterBarNameSize;
	for (int32 i = 0; i < TeammateHealthBars.Num(); i++)
	{
		if (TeammateHealthBars[i].IsValid()) Cells.Add(MakeCharacterCell(TeammateHealthBars[i], RealBarNames[i], NameSize));
	}
	for (int32 i = 0; i < VirtualHealthBars.Num(); i++)
	{
		const TSharedRef<SWidget> Cell = MakeCharacterCell(VirtualHealthBars[i], VirtualBarNames[i], NameSize);
		VirtualBarCells[i] = Cell;
		if (!VirtualBarVisible[i]) Cell->SetVisibility(EVisibility::Hidden);
		Cells.Add(Cell);
	}
	for (const TSharedRef<SWidget>& Cell : Cells)
	{
		MyVerticalBox->AddSlot().FillHeight(1.f)[Cell];
	}
}

int32 UTeammateBarBox::GetVisibleVirtualBarCount() const
{
	int32 Count = 0;
	for (const bool bVisible : VirtualBarVisible) Count += bVisible ? 1 : 0;
	return Count;
}

void UTeammateBarBox::ReleaseSlateResources(bool bReleaseChildren)
{
	for (int i = 0; i < TeammateHealthBars.Num(); i++)
	{
		TeammateHealthBars[i].Reset();
	}
	TeammateHealthBars.Empty();
	VirtualHealthBars.Empty(); // Madrone change
	RealBarNames.Empty();
	VirtualBarNames.Empty();
	VirtualBarCells.Empty();
	VirtualBarColors.Empty();
	VirtualBarVisible.Empty();
	RealBarColors.Empty();
	
	Super::ReleaseSlateResources(bReleaseChildren);
}
