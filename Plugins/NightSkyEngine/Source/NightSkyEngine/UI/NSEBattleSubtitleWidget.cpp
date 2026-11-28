// Madrone change: battle subtitles. See NSEBattleSubtitleWidget.h.

#include "NSEBattleSubtitleWidget.h"

#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NSEBattleSubtitleWidget)

void UNSEBattleSubtitleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetShownVisibility(false);
}

void UNSEBattleSubtitleWidget::ShowSubtitle(const FText& Speaker, const FText& Line, const float Seconds)
{
	CurrentSpeaker = Speaker;
	CurrentLine = Line;
	TimeLeft = Seconds;
	bShowing = true;
	if (SpeakerName)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Speaker"), Speaker);
		SpeakerName->SetText(Speaker.IsEmpty() ? FText::GetEmpty() : FText::Format(SpeakerFormat, Args));
		SpeakerName->SetColorAndOpacity(SpeakerColor);
		SpeakerName->SetVisibility(Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (LineText) LineText->SetText(Line);
	if (LineRichText)
	{
		if (Speaker.IsEmpty() || SpeakerName) LineRichText->SetText(Line);
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Speaker"), Speaker);
			Args.Add(TEXT("Line"), Line);
			LineRichText->SetText(FText::Format(RichLineFormat, Args));
		}
	}
	SetShownVisibility(true);
	OnSubtitleShown(Speaker, Line, Seconds);
}

void UNSEBattleSubtitleWidget::HideSubtitle()
{
	if (!bShowing) return;
	bShowing = false;
	TimeLeft = 0.f;
	SetShownVisibility(false);
	OnSubtitleHidden();
}

void UNSEBattleSubtitleWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bShowing || TimeLeft <= 0.f) return;
	TimeLeft -= InDeltaTime;
	if (TimeLeft <= 0.f) HideSubtitle();
}

void UNSEBattleSubtitleWidget::SetShownVisibility(const bool bShow)
{
	const ESlateVisibility NewVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (SubtitleRoot) SubtitleRoot->SetVisibility(NewVisibility);
	else SetVisibility(NewVisibility);
}
