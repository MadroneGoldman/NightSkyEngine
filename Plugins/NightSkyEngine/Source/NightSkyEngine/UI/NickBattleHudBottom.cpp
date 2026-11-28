// Madrone change: the bottom battle HUD (super meters) with the health bar's shader and characters' own HUD elements.

#include "NickBattleHudBottom.h"
#include "EngineUtils.h"
#include "HealthBar.h"
#include "NSEFighterHudElement.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInterface.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "NightSkyEngine/UI/NightSkyBattleHudActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NickBattleHudBottom)

static const TCHAR* MeterBarName(const int32 Side) { return Side == 0 ? TEXT("P1MeterBar") : TEXT("P2MeterBar"); }

void UNickBattleHudBottom::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!IsDesignTime()) BuildSuperMeters();
}

void UNickBattleHudBottom::BuildSuperMeters()
{
	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (!Root || !WidgetTree || ElementRow[0]) return;
	UMaterialInterface* Material = bShaderSuperMeter ? SuperMeterMaterial.LoadSynchronous() : nullptr;
	for (int32 Side = 0; Side < 2; Side++)
	{
		UProgressBar* Meter = Cast<UProgressBar>(GetWidgetFromName(MeterBarName(Side)));
		UCanvasPanelSlot* MeterSlot = Meter ? Cast<UCanvasPanelSlot>(Meter->Slot) : nullptr;
		if (!MeterSlot) continue;
		const FAnchorData Layout = MeterSlot->GetLayout();
		const FVector2D Size(Layout.Offsets.Right, Layout.Offsets.Bottom);
		const FVector2D TopLeft = FVector2D(Layout.Offsets.Left, Layout.Offsets.Top) - Layout.Alignment * Size;

		if (Material)
		{
			UHealthBar* Bar = WidgetTree->ConstructWidget<UHealthBar>(UHealthBar::StaticClass(), Side == 0 ? TEXT("P1SuperBar") : TEXT("P2SuperBar"));
			FSlateBrush Fill;
			Fill.SetResourceObject(Material);
			Fill.ImageSize = Size;
			Bar->SetImageFill(Fill);
			const EProgressBarFillType::Type FillType = Meter->GetBarFillType();
			Bar->SetBarFillType(FillType == EProgressBarFillType::RightToLeft ? EHealthBarFillType::RightToLeft : EHealthBarFillType::LeftToRight);
			Bar->SetMainPlayerIndex(0);
			UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(Bar);
			BarSlot->SetLayout(Layout);
			BarSlot->SetAutoSize(MeterSlot->GetAutoSize());
			BarSlot->SetZOrder(MeterSlot->GetZOrder());
			Bar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			// the progress bar keeps its place in the layout, unseen
			Meter->SetRenderOpacity(0.f);
			SuperBar[Side] = Bar;
		}

		// the fighter's own HUD elements: a row standing on the meter, from its outer edge
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
		RowSlot->SetAnchors(Layout.Anchors);
		RowSlot->SetAutoSize(true);
		RowSlot->SetAlignment(FVector2D(Side == 0 ? 0.f : 1.f, 1.f));
		RowSlot->SetPosition(FVector2D(Side == 0 ? TopLeft.X : TopLeft.X + Size.X, TopLeft.Y - ElementGap));
		RowSlot->SetZOrder(MeterSlot->GetZOrder() + 1);
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ElementRow[Side] = Row;
	}
}

const ANightSkyGameState* UNickBattleHudBottom::FindGameState() const
{
	if (CachedGameState.IsValid()) return CachedGameState.Get();
	for (TActorIterator<ANightSkyGameState> It(GetWorld()); It; ++It)
	{
		if (It->BattleHudActor && It->BattleHudActor->BottomWidget == this)
		{
			CachedGameState = *It;
			return *It;
		}
	}
	return nullptr;
}

void UNickBattleHudBottom::RoundInitImplementation(const int P1TeamSize, const int P2TeamSize)
{
	Super::RoundInitImplementation(P1TeamSize, P2TeamSize);
	RefreshFighterElements(true);
}

void UNickBattleHudBottom::OnMainPlayersChanged()
{
	Super::OnMainPlayersChanged();
	RefreshFighterElements(false);
}

void UNickBattleHudBottom::RefreshFighterElements(const bool bForce)
{
	const ANightSkyGameState* GameState = FindGameState();
	if (!GameState || !WidgetTree) return;
	for (int32 Side = 0; Side < 2; Side++)
	{
		UHorizontalBox* Row = ElementRow[Side];
		if (!Row) continue;
		APlayerObject* Main = GameState->GetMainPlayer(Side == 0);
		if (!bForce && ShownFighter[Side].Get() == Main) continue;
		ShownFighter[Side] = Main;
		TArray<TObjectPtr<UNSEFighterHudElement>>& Elements = Side == 0 ? Elements0 : Elements1;
		Row->ClearChildren();
		Elements.Reset();
		if (!IsValid(Main) || !Main->CharaData) continue;
		for (const TSubclassOf<UNSEFighterHudElement>& Class : Main->CharaData->BattleHudElements)
		{
			if (!Class) continue;
			UNSEFighterHudElement* Element = CreateWidget<UNSEFighterHudElement>(this, Class);
			if (!Element) continue;
			Element->InitElement(Main, Side == 0);
			// player 2's row runs from the right edge: newest element further in
			UHorizontalBoxSlot* ElementSlot = Side == 0 ? Row->AddChildToHorizontalBox(Element) : Cast<UHorizontalBoxSlot>(Row->InsertChildAt(0, Element));
			if (ElementSlot)
			{
				ElementSlot->SetVerticalAlignment(VAlign_Bottom);
				ElementSlot->SetPadding(FMargin(4.f, 0.f));
			}
			Elements.Add(Element);
		}
	}
}

void UNickBattleHudBottom::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ANightSkyGameState* GameState = FindGameState();
	for (int32 Side = 0; Side < 2; Side++)
	{
		const APlayerObject* Main = GameState ? GameState->GetMainPlayer(Side == 0) : nullptr;
		if (SuperBar[Side])
		{
			// P1Meter is Meter / 10000; the fighter's own Max Meter can differ
			const float Scale = IsValid(Main) && Main->MaxMeter > 0 ? 10000.f / Main->MaxMeter : 1.f;
			const float Percent = FMath::Clamp((Side == 0 ? P1Meter : P2Meter) * Scale, 0.f, 1.f);
			if (!FMath::IsNearlyEqual(Percent, ShownMeter[Side]))
			{
				ShownMeter[Side] = Percent;
				SuperBar[Side]->SetHealthPercent(0, Percent);
				// one value: no recoverable band behind it
				SuperBar[Side]->SetRecoverableHealthPercent(0, Percent);
			}
		}
	}
	if (GameState && (ShownFighter[0].Get() != GameState->GetMainPlayer(true) || ShownFighter[1].Get() != GameState->GetMainPlayer(false)))
	{
		RefreshFighterElements(false);
	}
	for (UNSEFighterHudElement* Element : Elements0) if (Element) Element->UpdateElement();
	for (UNSEFighterHudElement* Element : Elements1) if (Element) Element->UpdateElement();
}
