// Fill out your copyright notice in the Description page of Project Settings.


#include "TeammateBarBox.h"

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
	}
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

void UTeammateBarBox::ReleaseSlateResources(bool bReleaseChildren)
{
	for (int i = 0; i < TeammateHealthBars.Num(); i++)
	{
		TeammateHealthBars[i].Reset();
	}
	TeammateHealthBars.Empty();
	
	Super::ReleaseSlateResources(bReleaseChildren);
}
