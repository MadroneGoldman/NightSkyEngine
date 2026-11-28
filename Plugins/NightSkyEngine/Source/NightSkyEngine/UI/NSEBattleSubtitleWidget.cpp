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
	RevealTimes.Reset();
	RevealElapsed = 0.f;
	const bool bTypewriter = RevealCharsPerSecond > 0.f && !Line.ToString().Contains(TEXT("<"));
	RevealedChars = bTypewriter ? 0 : -1;
	if (SpeakerName)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Speaker"), Speaker);
		SpeakerName->SetText(Speaker.IsEmpty() ? FText::GetEmpty() : FText::Format(SpeakerFormat, Args));
		SpeakerName->SetColorAndOpacity(SpeakerColor);
		SpeakerName->SetVisibility(Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	ApplyLine(RevealedChars);
	SetShownVisibility(true);
	OnSubtitleShown(Speaker, Line, Seconds);
}

void UNSEBattleSubtitleWidget::ApplyLine(const int32 Count)
{
	const FString Full = CurrentLine.ToString();
	const FText Shown = Count < 0 || Count >= Full.Len() ? CurrentLine : FText::FromString(Full.Left(Count));
	if (LineText) LineText->SetText(Shown);
	if (LineRichText)
	{
		if (CurrentSpeaker.IsEmpty() || SpeakerName) LineRichText->SetText(Shown);
		else
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Speaker"), CurrentSpeaker);
			Args.Add(TEXT("Line"), Shown);
			LineRichText->SetText(FText::Format(RichLineFormat, Args));
		}
	}
}

void UNSEBattleSubtitleWidget::SetRevealTimes(const TArray<float>& Times)
{
	if (RevealedChars < 0 || Times.Num() != CurrentLine.ToString().Len()) return; // not typing, or another text
	RevealTimes = Times;
}

void UNSEBattleSubtitleWidget::CompleteReveal()
{
	if (RevealedChars < 0) return;
	RevealedChars = -1;
	ApplyLine(-1);
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
	if (bShowing && RevealedChars >= 0)
	{
		RevealElapsed += InDeltaTime;
		const int32 Len = CurrentLine.ToString().Len();
		int32 Count = 0;
		if (RevealTimes.Num()) { while (Count < RevealTimes.Num() && RevealTimes[Count] <= RevealElapsed) ++Count; }
		else Count = FMath::FloorToInt32(RevealElapsed * RevealCharsPerSecond);
		if (Count >= Len) { RevealedChars = -1; ApplyLine(-1); }
		else if (Count != RevealedChars) { RevealedChars = Count; ApplyLine(Count); }
	}
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
