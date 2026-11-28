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
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "NightSkyEngine/Data/PrimaryCharaData.h"
#include "EngineUtils.h"
#include "NightSkyEngine/Battle/NightSkyGameState.h"
#include "NightSkyEngine/Battle/Objects/PlayerObject.h"
#include "NightSkyEngine/UI/NightSkyBattleHudActor.h"
#include "NightSkyEngine/Battle/Script/NSECharacterExtension.h" // Madrone change

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

// Madrone change: character-extension bars
void UNickBattleHudTop::SetCharacterTeammateBars(const bool bIsP1, const TArray<FNSETeammateBar>& Virtual, const TArray<FNSETeammateBar>& RealOverrides)
{
	if (UTeammateBarBox* Box = bIsP1 ? P1TeammateBox : P2TeammateBox) Box->SetCharacterBars(Virtual, RealOverrides);
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
	if (!Root || !WidgetTree || AssistBox[0]) return;
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
		UHorizontalBoxSlot* ItemSlot = Row->AddChildToHorizontalBox(Assist);
		ItemSlot->SetVerticalAlignment(VAlign_Center);
		AssistBox[Side] = Assist;
		AssistFrame[Side] = Frame;
		AssistIcon[Side] = Icon;
		AssistShade[Side] = Shade;
		AssistText[Side] = Seconds;
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
		if (!AssistBox[Side]) continue;
		const APlayerObject* Assist = GameState ? (Side == 0 ? GameState->P1Assist : GameState->P2Assist) : nullptr;
		if (!IsValid(Assist)) Assist = nullptr;
		UTexture2D* Portrait = nullptr;
		if (Assist && Assist->CharaData)
		{
			Portrait = Assist->CharaData->CharaSelectIcon ? Assist->CharaData->CharaSelectIcon : Assist->CharaData->CharaHUDIcon;
		}
		if (Portrait)
		{
			// UI: all mips resident, as UImage::SetBrushFromTexture does (a streamed-out icon drew as a flat tile)
			Portrait->bForceMiplevelsToBeResident = true;
			Portrait->bIgnoreStreamingMipBias = true;
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
	const UWorld* World = GetWorld();
	const float Pulse = 0.5f + 0.5f * FMath::Sin((World ? World->GetRealTimeSeconds() : 0.f) * UE_TWO_PI * 1.2f);
	const FLinearColor ReadyPulse = FMath::Lerp(ReadyColor, FMath::Lerp(ReadyColor, FLinearColor::White, 0.6f), Pulse);
	const FLinearColor Outline = ReadyColor * FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
	const FLinearColor Cooling(0.5f, 0.5f, 0.5f, 1.f);
	for (int32 Side = 0; Side < 2; Side++)
	{
		const bool bSounds = Side == 0 || bReadySoundForP2;

		// Tag: seconds after the partner's name, the name pulsing when it's ready (team index 1: the team lists the
		// fighter on screen first).
		const TArray<float>& Cooldowns = Side == 0 ? P1Cooldowns : P2Cooldowns;
		const int32 TagSeconds = Cooldowns.IsValidIndex(1) ? CooldownSeconds(Cooldowns[1]) : 0;
		UTextBlock* TagName = Cast<UTextBlock>(GetWidgetFromName(Side == 0 ? TEXT("P1TagName") : TEXT("P2TagName")));
		const bool bHasPartner = TagName && !PartnerNames[Side].IsEmpty();
		if (bHasPartner)
		{
			if (!bTagNameFont[Side])
			{
				TagNameFont[Side] = TagName->GetFont();
				TagNameFont[Side].OutlineSettings = FFontOutlineSettings();
				bTagNameFont[Side] = true;
			}
			FSlateFontInfo Font = TagNameFont[Side];
			if (TagSeconds > 0)
			{
				TagName->SetText(FText::Format(NSLOCTEXT("NSE", "TagCooldown", "{0}  {1}"), PartnerNames[Side], TagSeconds));
				TagName->SetColorAndOpacity(FSlateColor(Cooling));
			}
			else
			{
				TagName->SetText(PartnerNames[Side]);
				TagName->SetColorAndOpacity(FSlateColor(ReadyPulse));
				Font.OutlineSettings.OutlineSize = 2;
				Font.OutlineSettings.OutlineColor = Outline;
			}
			TagName->SetFont(Font);
			if (bSounds && LastTagSeconds[Side] > 0 && TagSeconds == 0) PlayReadySound();
		}
		LastTagSeconds[Side] = bHasPartner ? TagSeconds : 0;

		// Assist: seconds over the portrait, the frame pulsing when it's ready.
		if (!AssistBox[Side]) continue;
		const bool bHasAssist = ShownAssist[Side].IsValid();
		const int32 AssistSeconds = CooldownSeconds(Side == 0 ? P1AssistCooldown : P2AssistCooldown);
		// Madrone change: a "forever" cooldown (a character extension's) shows no number: never ready
		const bool bNeverReady = (Side == 0 ? P1AssistCooldown : P2AssistCooldown) >= NSECooldownForever;
		AssistText[Side]->SetText(AssistSeconds > 0 && !bNeverReady ? FText::AsNumber(AssistSeconds) : FText::GetEmpty());
		AssistShade[Side]->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, AssistSeconds > 0 ? 0.65f : 0.f));
		AssistFrame[Side]->SetBrushColor(AssistSeconds > 0 ? Cooling : ReadyPulse);
		AssistFrame[Side]->SetPadding(FMargin(AssistSeconds > 0 ? 2.f : 4.f));
		if (bSounds && bHasAssist && LastAssistSeconds[Side] > 0 && AssistSeconds == 0) PlayReadySound();
		LastAssistSeconds[Side] = bHasAssist ? AssistSeconds : 0;
	}
	UpdateRoundHearts(); // Madrone change
	UpdateComboCounters(InDeltaTime); // Madrone change
}

// ------------------------------------------------------------------------------------- Madrone change: round hearts

void UNickBattleHudTop::UpdateRoundHearts()
{
	if (!bRoundHearts) return;
	if (!bHeartTexturesLoaded)
	{
		LoadedHeartFull = RoundHeartFull.LoadSynchronous();
		LoadedHeartLost = RoundHeartLost.LoadSynchronous();
		bHeartTexturesLoaded = true;
	}
	if (!LoadedHeartFull || !LoadedHeartLost) return;
	for (int32 Side = 0; Side < 2; Side++)
	{
		// a side's hearts are the rounds it can still lose: one goes black for each round the other side won
		const int32 Lost = Side == 0 ? P2RoundsWon : P1RoundsWon;
		for (int32 Round = 1; Round <= 9; Round++)
		{
			UImage* Image = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("P%dRound%d"), Side + 1, Round)));
			if (!Image) break;
			// the Blueprint colours its round marks with a binding (won = lit): the hearts carry their own colours
			if (Image->ColorAndOpacityDelegate.IsBound()) Image->ColorAndOpacityDelegate.Unbind();
			// the round ticks are thin slanted bars (sheared, player 2's mirrored): lay the hearts out upright, square,
			// in a row from the first mark outwards
			if (!Image->GetRenderTransform().Shear.IsNearlyZero() || Image->GetRenderTransform().Scale.X < 0.f)
			{
				Image->SetRenderTransform(FWidgetTransform());
				const UImage* First = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("P%dRound1"), Side + 1)));
				UCanvasPanelSlot* HeartSlot = Cast<UCanvasPanelSlot>(Image->Slot);
				const UCanvasPanelSlot* FirstSlot = First ? Cast<UCanvasPanelSlot>(First->Slot) : nullptr;
				if (HeartSlot && FirstSlot)
				{
					constexpr float HeartSize = 32.f;
					constexpr float HeartGap = 6.f;
					const float Outward = Side == 0 ? -1.f : 1.f;
					FVector2D Position = FirstSlot->GetPosition();
					if (Round == 1) Position.X += Outward * HeartSize * 0.5f; // clear of the timer
					else Position.X += Outward * (Round - 1) * (HeartSize + HeartGap); // the first is laid out already
					HeartSlot->SetPosition(Position);
					HeartSlot->SetSize(FVector2D(HeartSize, HeartSize));
				}
			}
			UTexture2D* Texture = Round <= Lost ? LoadedHeartLost.Get() : LoadedHeartFull.Get();
			if (Image->GetBrush().GetResourceObject() != Texture)
			{
				FSlateBrush Brush = Image->GetBrush();
				Brush.SetResourceObject(Texture);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Image->SetBrush(Brush);
			}
			Image->SetColorAndOpacity(FLinearColor::White);
		}
	}
}

// ------------------------------------------------------------------------------------ Madrone change: combo counter

FLinearColor UNickBattleHudTop::ComboColor(const int32 Hits) const
{
	// white at 1, light blue at 2, then warming through yellow and orange to red at Combo Red Hits
	if (Hits <= 1) return FLinearColor::White;
	struct FKey { float T; FLinearColor C; };
	const FKey Keys[] = {
		{0.f, ComboColdColor},
		{0.4f, FLinearColor(1.f, 0.93f, 0.45f)},
		{0.7f, FLinearColor(1.f, 0.55f, 0.15f)},
		{1.f, ComboHotColor},
	};
	const float T = FMath::Clamp((Hits - 2.f) / FMath::Max(ComboRedHits - 2.f, 1.f), 0.f, 1.f);
	for (int32 i = 1; i < UE_ARRAY_COUNT(Keys); i++)
	{
		if (T <= Keys[i].T)
		{
			const float A = (T - Keys[i - 1].T) / (Keys[i].T - Keys[i - 1].T);
			return FLinearColor::LerpUsingHSV(Keys[i - 1].C, Keys[i].C, A);
		}
	}
	return ComboHotColor;
}

void UNickBattleHudTop::UpdateComboCounters(const float DeltaTime)
{
	if (!bStyledComboCounter) return;
	if (!bComboFontLoaded)
	{
		LoadedComboFont = ComboFont.LoadSynchronous();
		bComboFontLoaded = true;
	}
	const UWorld* World = GetWorld();
	const float Time = World ? World->GetRealTimeSeconds() : 0.f;
	for (int32 Side = 0; Side < 2; Side++)
	{
		UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Side == 0 ? TEXT("P1ComboText") : TEXT("P2ComboText")));
		if (!Text) continue;
		const int32 Hits = Side == 0 ? P1ComboCounter : P2ComboCounter;
		const bool bBoiling = Hits > ComboRedHits;
		FSlateFontInfo Font = Text->GetFont();
		if (LoadedComboFont)
		{
			Font.FontObject = LoadedComboFont;
			Font.TypefaceFontName = NAME_None;
		}
		Font.Size = ComboFontSize;
		if (bBoiling)
		{
			// pink glow: a thick pink outline that breathes
			const float Breathe = 0.5f + 0.5f * FMath::Sin(Time * UE_TWO_PI * 1.6f);
			Font.OutlineSettings.OutlineSize = FMath::RoundToInt(FMath::Lerp(4.f, 8.f, Breathe));
			Font.OutlineSettings.OutlineColor = ComboGlowColor * FLinearColor(1.f, 1.f, 1.f, FMath::Lerp(0.55f, 1.f, Breathe));
		}
		else
		{
			Font.OutlineSettings.OutlineSize = 4;
			Font.OutlineSettings.OutlineColor = FLinearColor::Black;
		}
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(ComboColor(Hits)));

		// boiling: the number wobbles on a hand-drawn "boil" beat (a new random pose a few times a second). The
		// Blueprint's combo animation may move the text too: the wobble is added to whatever it set this frame.
		const FWidgetTransform Current = Text->GetRenderTransform();
		const bool bOurs = ComboBoilApplied[Side].IsSet()
			&& Current.Translation.Equals(ComboBoilApplied[Side]->Translation)
			&& FMath::IsNearlyEqual(Current.Angle, ComboBoilApplied[Side]->Angle)
			&& Current.Shear.Equals(ComboBoilApplied[Side]->Shear)
			&& Current.Scale.Equals(ComboBoilApplied[Side]->Scale);
		const FWidgetTransform Base = bOurs ? ComboBoilBase[Side] : Current;
		FWidgetTransform Result = Base;
		if (bBoiling)
		{
			ComboBoilTimer[Side] -= DeltaTime;
			if (ComboBoilTimer[Side] <= 0.f)
			{
				ComboBoilTimer[Side] = 1.f / FMath::Max(ComboBoilRate, 1.f);
				const float Heat = FMath::Clamp((Hits - ComboRedHits) / 10.f, 0.f, 1.f); // bigger combos boil harder
				const float Amount = FMath::Lerp(1.f, 2.f, Heat);
				ComboBoilPose[Side].Translation = FVector2D(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-4.f, 2.f)) * Amount;
				ComboBoilPose[Side].Angle = FMath::FRandRange(-3.f, 3.f) * Amount;
				ComboBoilPose[Side].Shear = FVector2D(FMath::FRandRange(-0.06f, 0.06f), 0.f) * Amount;
				ComboBoilPose[Side].Scale = FVector2D(1.f + FMath::FRandRange(-0.03f, 0.05f) * Amount, 1.f + FMath::FRandRange(-0.03f, 0.06f) * Amount);
			}
			Result.Translation += ComboBoilPose[Side].Translation;
			Result.Angle += ComboBoilPose[Side].Angle;
			Result.Shear += ComboBoilPose[Side].Shear;
			Result.Scale *= ComboBoilPose[Side].Scale;
		}
		if (bBoiling || bOurs)
		{
			Text->SetRenderTransform(Result);
			ComboBoilBase[Side] = Base;
			ComboBoilApplied[Side] = Result;
		}
		if (!bBoiling) ComboBoilApplied[Side].Reset();
	}
}

void UNickBattleHudTop::PlayReadySound()
{
	if (USoundBase* Sound = ReadySound.LoadSynchronous()) UGameplayStatics::PlaySound2D(this, Sound);
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
			PartnerNames[bIsP1 ? 0 : 1] = Tag->GetText(); // Madrone change: the cooldown panels add the seconds to it
			bTagNameFont[bIsP1 ? 0 : 1] = false;
		}
	}
}
