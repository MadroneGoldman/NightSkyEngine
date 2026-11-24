// Fill out your copyright notice in the Description page of Project Settings.


#include "NickBattleHudTop.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "EngineUtils.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/UI/NightSkyBattleHudActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NickBattleHudTop)

void UNickBattleHudTop::NativeConstruct()
{
	Super::NativeConstruct();
	
	BingHealthDelegates();
}

void UNickBattleHudTop::NativePreConstruct()
{
	Super::NativePreConstruct();
#if WITH_EDITORONLY_DATA
	if (IsDesignTime() && DesignerPreviewStyle)
	{
		const FNSEBattleHudStyle* Style = DesignerPreviewStyle->GetStyle<FNSEBattleHudStyle>();
		ApplyBattleHudStyle(true, Style, DesignerPreviewStyle);
		ApplyBattleHudStyle(false, Style, DesignerPreviewStyle);
	}
#endif
}

void UNickBattleHudTop::ApplyBattleHudStyle(const bool bIsP1, const FNSEBattleHudStyle* Style, USlateWidgetStyleAsset* StyleAsset)
{
	UHealthBar* Bar = bIsP1 ? P1HealthBar : P2HealthBar;
	UTeammateBarBox* Teammates = bIsP1 ? P1TeammateBox : P2TeammateBox;
	const int32 Side = bIsP1 ? 0 : 1;
	if (Bar)
	{
		if (!AuthoredBarScale[Side].IsSet()) AuthoredBarScale[Side] = Bar->GetRenderTransform().Scale;
		if (Style)
		{
			Bar->SetStyleOverride(Style->MainHealthBar);
			Bar->SetRenderScale(AuthoredBarScale[Side].GetValue() * Style->MainBarScale);
		}
		else
		{
			Bar->ClearStyleOverride();
			Bar->SetRenderScale(AuthoredBarScale[Side].GetValue());
		}
	}
	if (Teammates)
	{
		if (Style) Teammates->SetStyleOverride(Style->TeammateHealthBar);
		else Teammates->ClearStyleOverride();
	}
	for (const TCHAR* Name : bIsP1 ? TArray<const TCHAR*>{TEXT("P1Name"), TEXT("P1TagName")} : TArray<const TCHAR*>{TEXT("P2Name"), TEXT("P2TagName")})
	{
		UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Name));
		if (!Text) continue;
		const TPair<FSlateFontInfo, FSlateColor>& Authored = AuthoredNames.FindOrAdd(Name, {Text->GetFont(), Text->GetColorAndOpacity()});
		FSlateFontInfo Font = Authored.Key;
		if (Style && Style->bOverrideNameFont)
		{
			const float Size = Font.Size; // keep each label's own size (partner names are smaller)
			Font = Style->NameFont;
			Font.Size = Size;
		}
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Style && Style->bOverrideNameColor ? Style->NameColor : Authored.Value);
	}
	Super::ApplyBattleHudStyle(bIsP1, Style, StyleAsset);
}

void UNickBattleHudTop::OnMainPlayersChanged()
{
	// The team arrays list the fighter on screen first, so the main bars always show index 0.
	if (P1HealthBar) P1HealthBar->SetMainPlayerIndex(0);
	if (P2HealthBar) P2HealthBar->SetMainPlayerIndex(0);
	RefreshPlayerNames();
}

void UNickBattleHudTop::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!IsDesignTime()) BuildCooldownPanels(); // Madrone change
	
//	P1TeammateBox->AddHealthbars(3);
//	P2TeammateBox->AddHealthbars(3);
}


void UNickBattleHudTop::BingHealthDelegates()
{
	OnP1HealthChangedDelegate.AddUObject(P1HealthBar, &UHealthBar::SetHealthPercent);
	OnP1RecoverableHealthChangedDelegate.AddUObject(P1HealthBar, &UHealthBar::SetRecoverableHealthPercent);
	OnP2HealthChangedDelegate.AddUObject(P2HealthBar, &UHealthBar::SetHealthPercent);
	OnP2RecoverableHealthChangedDelegate.AddUObject(P2HealthBar, &UHealthBar::SetRecoverableHealthPercent);
	
	
	OnP1HealthChangedDelegate.AddUObject(P1TeammateBox, &UTeammateBarBox::SetHealthPercent);
	OnP1RecoverableHealthChangedDelegate.AddUObject(P1TeammateBox, &UTeammateBarBox::SetRecoverableHealthPercent);
	OnP2HealthChangedDelegate.AddUObject(P2TeammateBox, &UTeammateBarBox::SetHealthPercent);
	OnP2RecoverableHealthChangedDelegate.AddUObject(P2TeammateBox, &UTeammateBarBox::SetRecoverableHealthPercent);
}


void UNickBattleHudTop::RoundInitImplementation(int P1TeamSize, int P2TeamSize)
{
	P1TeammateBox->AddHealthbars(P1TeamSize - 1);
	P2TeammateBox->AddHealthbars(P2TeamSize - 1);
	
	RoundInit(P1TeamSize, P2TeamSize);
	PlayRoundInitAnim();
	RefreshPlayerNames(); // Madrone change
	RefreshCooldownPanels(); // Madrone change
}

// ------------------------------------------------------------------------------------------- Madrone change: cooldowns

static int32 CooldownSeconds(const float Frames)
{
	return Frames > 0.f ? FMath::CeilToInt(Frames / 60.f) : 0;
}

const ANightSkyGameState* UNickBattleHudTop::FindGameState() const
{
	if (CachedGameState.IsValid()) return CachedGameState.Get();
	for (TActorIterator<ANightSkyGameState> It(GetWorld()); It; ++It)
	{
		if (It->BattleHudActor && It->BattleHudActor->TopWidget == this)
		{
			const_cast<UNickBattleHudTop*>(this)->CachedGameState = *It;
			return *It;
		}
	}
	return nullptr;
}

void UNickBattleHudTop::BuildCooldownPanels()
{
	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (!Root || !WidgetTree || TagBox[0]) return;
	// the names' font, smaller, so the numbers match the HUD
	FSlateFontInfo Font;
	if (const UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("P1Name")))) Font = Name->GetFont();
	Font.Size = 22;
	for (int32 Side = 0; Side < 2; Side++)
	{
		const bool bRight = Side == 1;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
		RowSlot->SetAnchors(FAnchors(bRight ? 1.f : 0.f, 0.f));
		RowSlot->SetAlignment(FVector2D(bRight ? 1.f : 0.f, 0.f));
		RowSlot->SetPosition(FVector2D(bRight ? -16.f : 16.f, 132.f));
		RowSlot->SetAutoSize(true);
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		// TAG + seconds
		UBorder* Tag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Tag->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Tag->SetPadding(FMargin(8.f, 2.f));
		Tag->SetVerticalAlignment(VAlign_Center);
		UTextBlock* TagLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TagLabel->SetFont(Font);
		Tag->SetContent(TagLabel);
		// assist portrait, shade and seconds
		USizeBox* Assist = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Assist->SetWidthOverride(64.f);
		Assist->SetHeightOverride(64.f);
		// a light frame, so the tile reads on any background (and with an icon that is dark or missing)
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrushColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f));
		Frame->SetPadding(FMargin(3.f));
		Assist->SetContent(Frame);
		// Nested borders, not an overlay: in the world-space top HUD brushes inside an overlay didn't draw.
		// frame > portrait > shade (alpha while cooling down) > seconds
		UBorder* Icon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Icon->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.f));
		Icon->SetPadding(FMargin(0.f));
		Frame->SetContent(Icon);
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		Shade->SetPadding(FMargin(0.f));
		Shade->SetHorizontalAlignment(HAlign_Center);
		Shade->SetVerticalAlignment(VAlign_Center);
		Icon->SetContent(Shade);
		UTextBlock* Seconds = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Seconds->SetFont(Font);
		Seconds->SetJustification(ETextJustify::Center);
		Shade->SetContent(Seconds);
		// P1: TAG then the assist; P2 mirrored
		for (UWidget* Item : bRight ? TArray<UWidget*>{Assist, Tag} : TArray<UWidget*>{Tag, Assist})
		{
			UHorizontalBoxSlot* ItemSlot = Row->AddChildToHorizontalBox(Item);
			ItemSlot->SetPadding(FMargin(bRight ? 8.f : 0.f, 0.f, bRight ? 0.f : 8.f, 0.f));
			ItemSlot->SetVerticalAlignment(VAlign_Center);
		}
		TagBox[Side] = Tag;
		TagText[Side] = TagLabel;
		AssistBox[Side] = Assist;
		AssistIcon[Side] = Icon;
		AssistShade[Side] = Shade;
		AssistText[Side] = Seconds;
		Tag->SetVisibility(ESlateVisibility::Collapsed);
		Assist->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNickBattleHudTop::RefreshCooldownPanels()
{
	BuildCooldownPanels();
	const ANightSkyGameState* GameState = FindGameState();
	bCooldownPanelsRefreshed = true;
	for (int32 Side = 0; Side < 2; Side++)
	{
		const APlayerObject* Current = GameState ? (Side == 0 ? GameState->P1Assist : GameState->P2Assist) : nullptr;
		ShownAssist[Side] = IsValid(Current) ? Current : nullptr;
		if (!TagBox[Side]) continue;
		const bool bHasPartner = GameState && GameState->BattleState.TeamData[Side].TeamCount > 1;
		TagBox[Side]->SetVisibility(bHasPartner ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		const APlayerObject* Assist = GameState ? (Side == 0 ? GameState->P1Assist : GameState->P2Assist) : nullptr;
		if (!IsValid(Assist)) Assist = nullptr;
		UTexture2D* Portrait = nullptr;
		if (Assist && Assist->CharaData)
		{
			Portrait = Assist->CharaData->CharaSelectIcon ? Assist->CharaData->CharaSelectIcon : Assist->CharaData->CharaHUDIcon;
		}
		if (Portrait)
		{
			// a plain image brush: the border's default background is drawn as a box and ignores the texture
			FSlateBrush Brush;
			Brush.SetResourceObject(Portrait);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = FVector2D(58.f, 58.f);
			AssistIcon[Side]->SetBrush(Brush);
			AssistIcon[Side]->SetBrushColor(FLinearColor::White);
		}
		else
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			AssistIcon[Side]->SetBrush(Brush);
			AssistIcon[Side]->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.f)); // no icon: a plain tile
		}
		AssistBox[Side]->SetVisibility(Assist ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNickBattleHudTop::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bCooldownPanelsRefreshed)
	{
		// an assist joined or changed since the last refresh
		if (const ANightSkyGameState* GameState = FindGameState();
			GameState && (ShownAssist[0].Get() != (IsValid(GameState->P1Assist) ? GameState->P1Assist : nullptr)
				|| ShownAssist[1].Get() != (IsValid(GameState->P2Assist) ? GameState->P2Assist : nullptr)))
		{
			RefreshCooldownPanels();
		}
	}
	for (int32 Side = 0; Side < 2; Side++)
	{
		if (!TagText[Side]) continue;
		// the next partner's cooldown (team index 1: the team lists the fighter on screen first)
		const TArray<float>& Cooldowns = Side == 0 ? P1Cooldowns : P2Cooldowns;
		const int32 TagSeconds = Cooldowns.IsValidIndex(1) ? CooldownSeconds(Cooldowns[1]) : 0;
		TagText[Side]->SetText(TagSeconds > 0 ? FText::Format(NSLOCTEXT("NSE", "TagCooldown", "TAG {0}"), TagSeconds) : NSLOCTEXT("NSE", "TagReady", "TAG"));
		TagText[Side]->SetColorAndOpacity(TagSeconds > 0 ? FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)) : FSlateColor(FLinearColor::White));
		const int32 AssistSeconds = CooldownSeconds(Side == 0 ? P1AssistCooldown : P2AssistCooldown);
		AssistText[Side]->SetText(AssistSeconds > 0 ? FText::AsNumber(AssistSeconds) : FText::GetEmpty());
		AssistShade[Side]->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, AssistSeconds > 0 ? 0.65f : 0.f));
	}
}

void UNickBattleHudTop::SetMainPlayerIndex(bool IsP1, int PlayerIndex)
{
	// Madrone change: PlayerIndex is the side, not a team index: the team arrays (GetTeam) list the fighter on screen
	// first, so the main bar always shows index 0 (P2's showed its partner after a tag). UpdateHUD also calls
	// OnMainPlayersChanged for this after a rollback.
	OnMainPlayersChanged();
}

void UNickBattleHudTop::RefreshPlayerNames()
{
	// The battle's game state is the one whose HUD this is (in overworld battles the world's game state is Narrative's).
	const ANightSkyGameState* GameState = nullptr;
	for (TActorIterator<ANightSkyGameState> It(GetWorld()); It; ++It)
	{
		if (It->BattleHudActor && It->BattleHudActor->TopWidget == this)
		{
			GameState = *It;
			break;
		}
	}
	if (!GameState) return;
	for (const bool bIsP1 : {true, false})
	{
		const TArray<APlayerObject*> Team = GameState->GetTeam(bIsP1); // the fighter on screen first
		const APlayerObject* Main = GameState->GetMainPlayer(bIsP1);
		if (!Main) continue;
		TArray<FString> Partners;
		for (const APlayerObject* Player : Team)
		{
			if (Player && Player != Main && !Player->CharacterName.IsEmpty()) Partners.Add(Player->CharacterName.ToString());
		}
		if (UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(bIsP1 ? TEXT("P1Name") : TEXT("P2Name"))))
		{
			Name->SetText(Main->CharacterName);
		}
		if (UTextBlock* Tag = Cast<UTextBlock>(GetWidgetFromName(bIsP1 ? TEXT("P1TagName") : TEXT("P2TagName"))))
		{
			Tag->SetText(FText::FromString(FString::Join(Partners, TEXT(" / "))));
			Tag->SetVisibility(Partners.Num() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}
