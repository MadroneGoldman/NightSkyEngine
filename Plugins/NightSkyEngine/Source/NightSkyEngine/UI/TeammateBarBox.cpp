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
	
	if ((TeammateHealthBars.Num() == TeammateCount) && TeammateHealthBars[index].IsValid())
	{
		TeammateHealthBars[index]->SetHealthPercent(InPercent);
	}
}

void UTeammateBarBox::SetRecoverableHealthPercent(int index,float InPercent)
{
	//input is index of whole team but we only need the teammate index
	if (index == 0){return;}
	index --;
	
	if ((TeammateHealthBars.Num() == TeammateCount) && TeammateHealthBars[index].IsValid())
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
		MyVerticalBox->AddSlot().FillHeight(1.5f)//.VAlign(VAlign_Bottom)
			[
				SNew(SSpacer)
			];
		
		for (int i = 0; i < TeammateCount; i++)
		{
			TSharedPtr<SHealthBar> CurrentTeammateHealthBar;
			MyVerticalBox->AddSlot()//.MinHeight(7).MaxHeight(23.3)//.VAlign(VAlign_Bottom)
			[
				//SNew(SHealthBar)
				SAssignNew(CurrentTeammateHealthBar, SHealthBar)
				
				//SNew(STextBlock)
				//.Text(FText::FromString("Header Text"))
			];
			
			MakeDynamicFillImage(CurrentTeammateHealthBar);
			TeammateHealthBars.Add(CurrentTeammateHealthBar);
		}
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

void UTeammateBarBox::MakeDynamicFillImage(TSharedPtr<SHealthBar>& TeammateHealthBar)
{
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
		TeammateHealthBar->SetDynamicMaterial(NewDynMaterial);
		TeammateHealthBar->SetFillImage(DynamicMatBrush);
	}
	else
	{
		TeammateHealthBar->SetFillImage(&ImageFill);
	}
}
