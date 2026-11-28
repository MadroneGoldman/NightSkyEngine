// Madrone change: HUD elements a character adds to the battle HUD.

#include "NSEFighterHudElement.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NSEFighterHudElement)

void UNSEFighterHudElement::InitElement(APlayerObject* InFighter, const bool bInIsP1)
{
	Fighter = InFighter;
	bIsP1 = bInIsP1;
	OnInitFighterHud();
}

void UNSEFighterHudElement::UpdateElement()
{
	OnUpdateFighterHud();
}

int32 UNSEFighterHudElement::GetPlayerRegister(const int32 Register) const
{
	if (!IsValid(Fighter)) return 0;
	switch (Register)
	{
	case 1: return Fighter->PlayerReg1;
	case 2: return Fighter->PlayerReg2;
	case 3: return Fighter->PlayerReg3;
	case 4: return Fighter->PlayerReg4;
	case 5: return Fighter->PlayerReg5;
	case 6: return Fighter->PlayerReg6;
	case 7: return Fighter->PlayerReg7;
	case 8: return Fighter->PlayerReg8;
	default: return 0;
	}
}

void UNSEFighterHudElement::GetExtraGauge(const int32 Index, int32& Value, int32& MaxValue) const
{
	Value = 0;
	MaxValue = 0;
	if (!IsValid(Fighter) || !Fighter->ExtraGauges.IsValidIndex(Index)) return;
	Value = Fighter->ExtraGauges[Index].Value;
	MaxValue = Fighter->ExtraGauges[Index].MaxValue;
}

// ---------------------------------------------------------------------------------------------------------- Stocks

void UNSEStockHudElement::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) return;
	// a Blueprint with its own design keeps it; an empty panel at its root (a new Widget Blueprint's canvas) gets the row
	UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
	if (WidgetTree->RootWidget && (!RootPanel || RootPanel->GetChildrenCount() > 0)) return;
	Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StockRow"));
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (RootPanel) RootPanel->AddChild(Row);
	else WidgetTree->RootWidget = Row;
}

void UNSEStockHudElement::InitElement(APlayerObject* InFighter, const bool bInIsP1)
{
	// Widgets made at runtime were seen without their Blueprint's icon defaults (the class default object had them):
	// fall back to the class defaults.
	const UNSEStockHudElement* Defaults = GetClass()->GetDefaultObject<UNSEStockHudElement>();
	if (FullIcon.IsNull() && Defaults) FullIcon = Defaults->FullIcon;
	if (EmptyIcon.IsNull() && Defaults) EmptyIcon = Defaults->EmptyIcon;
	LoadedFull = FullIcon.LoadSynchronous();
	LoadedEmpty = EmptyIcon.LoadSynchronous();
	HighestCount = 0;
	ShownSlots = -1;
	ShownCount = -1;
	Super::InitElement(InFighter, bInIsP1);
	UpdateElement();
}

int32 UNSEStockHudElement::GetStockCount() const
{
	if (Source == ENSEHudStockSource::ExtraGauge)
	{
		if (!IsValid(Fighter) || !Fighter->ExtraGauges.IsValidIndex(SourceIndex)) return 0;
		const FExtraGauge& Gauge = Fighter->ExtraGauges[SourceIndex];
		const int32 Sections = FMath::Max(Gauge.Sections, 1);
		const int32 PerStock = FMath::Max(Gauge.MaxValue / Sections, 1);
		return FMath::Max(Gauge.Value / PerStock, 0);
	}
	return FMath::Max(GetPlayerRegister(SourceIndex), 0);
}

void UNSEStockHudElement::Rebuild(const int32 Slots)
{
	if (!Row || !WidgetTree) return;
	Row->ClearChildren();
	Icons.Reset();
	CountText = nullptr;
	for (int32 i = 0; i < Slots; i++)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(IconSize);
		Size->SetHeightOverride(IconSize);
		// Borders, not images: runtime-made UImages don't draw in the world-space battle HUD.
		UBorder* Icon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Icon->SetPadding(FMargin(0.f));
		Size->AddChild(Icon);
		UHorizontalBoxSlot* BoxSlot = Row->AddChildToHorizontalBox(Size);
		BoxSlot->SetPadding(FMargin(IconSpacing * 0.5f, 0.f));
		BoxSlot->SetVerticalAlignment(VAlign_Center);
		Icons.Add(Icon);
	}
	CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = CountText->GetFont();
	Font.Size = FMath::RoundToInt(IconSize * 0.6f);
	Font.OutlineSettings.OutlineSize = 2;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	CountText->SetFont(Font);
	CountText->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* TextSlot = bIsP1 ? Row->AddChildToHorizontalBox(CountText) : Cast<UHorizontalBoxSlot>(Row->InsertChildAt(0, CountText));
	if (TextSlot)
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(4.f, 0.f));
	}
	ShownSlots = Slots;
	ShownCount = -1;
}

void UNSEStockHudElement::UpdateElement()
{
	const int32 Count = GetStockCount();
	HighestCount = FMath::Max(HighestCount, Count);
	const int32 Slots = FixedSlots > 0 ? FixedSlots : FMath::Min(HighestCount, MaxSlots);
	if (Slots != ShownSlots) Rebuild(Slots);
	if (Count != ShownCount)
	{
		ShownCount = Count;
		for (int32 i = 0; i < Icons.Num(); i++)
		{
			// player 2 fills from the right edge inwards (the mirror of player 1)
			const int32 FromOuterEdge = bIsP1 ? i : Icons.Num() - 1 - i;
			UTexture2D* Texture = FromOuterEdge < Count ? LoadedFull.Get() : LoadedEmpty.Get();
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(IconSize, IconSize);
			Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
			Icons[i]->SetBrush(Brush);
			Icons[i]->SetBrushColor(FLinearColor::White);
		}
		if (CountText)
		{
			const bool bShowCount = Count > Icons.Num();
			CountText->SetText(FText::Format(NSLOCTEXT("NSE", "StockCount", "x{0}"), Count));
			CountText->SetVisibility(bShowCount ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	Super::UpdateElement();
}
