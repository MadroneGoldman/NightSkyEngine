// Madrone change: battle subtitles (a speaker's line on the battle screen), styled like Narrative Pro's dialogue
// subtitles. Docs: Plugins/NightSkyEngine/Docs/10-Character-Systems.md.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NSEBattleSubtitleWidget.generated.h"

class UTextBlock;
class URichTextBlock;
class UWidget;

/**
 * The base battle subtitle element: one line at a time, with the speaker's name, shown on the battle screen (a
 * screen-space widget added to the viewport, not the world-space top HUD). The game state makes one per battle from
 * Battle Rules > Character Interactions > Battle Subtitle Widget (WBP_BattleSubtitle in WitchTools BattleCommon, a
 * child of this class) and shows lines with ANightSkyGameState::ShowBattleSubtitle / UNSECharacterLibrary.
 *
 * Widgets found by name (all optional): SpeakerName (text), LineText (text) or LineRichText (rich text), and
 * SubtitleRoot (what is shown and hidden; the whole widget when missing). Override OnSubtitleShown / OnSubtitleHidden
 * in the Blueprint for animations. Presentation only (real time, not rollback state).
 */
UCLASS(Blueprintable, Abstract)
class NIGHTSKYENGINE_API UNSEBattleSubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Line (said by Speaker; empty: no name) for Seconds of real time (0 or less: until replaced or hidden). */
	UFUNCTION(BlueprintCallable, Category="Subtitle")
	void ShowSubtitle(const FText& Speaker, const FText& Line, float Seconds = 3.f);

	UFUNCTION(BlueprintCallable, Category="Subtitle")
	void HideSubtitle();

	UFUNCTION(BlueprintPure, Category="Subtitle")
	bool IsShowingSubtitle() const { return bShowing; }

	/** The line on screen now (tests). */
	UFUNCTION(BlueprintPure, Category="Subtitle")
	FText GetCurrentLine() const { return CurrentLine; }

	UFUNCTION(BlueprintPure, Category="Subtitle")
	FText GetCurrentSpeaker() const { return CurrentSpeaker; }

	/** Colour of the speaker's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle")
	FSlateColor SpeakerColor = FSlateColor(FLinearColor(1.f, 0.85f, 0.4f, 1.f));

	/** The rich text line when there's a speaker and no SpeakerName widget: {Speaker} and {Line}. Narrative's dialogue
	 * subtitle format (BP_NarrativeTalesOverlay), with its DT_RichText_NarrativeDialogue styles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle")
	FText RichLineFormat = NSLOCTEXT("NSEBattleSubtitle", "RichLineFormat", "<Narrative.Emphasis>{Speaker}</>: {Line}");

	/** Typewriter: letters per second the line types out at (0: all at once). Lines with rich-text markup show at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle", meta=(ClampMin="0"))
	float RevealCharsPerSecond = 30.f;

	/** Typewriter timing for the line just shown: when each character appears (seconds), from a voice that speaks it
	 * (Witch Voices), so text and voice stay together. Cleared by the next ShowSubtitle. */
	UFUNCTION(BlueprintCallable, Category="Subtitle")
	void SetRevealTimes(const TArray<float>& Times);

	/** Shows the rest of the line now. */
	UFUNCTION(BlueprintCallable, Category="Subtitle")
	void CompleteReveal();

	/** Typewriter, rich text line: the not-yet-shown rest is written in this rich-text style (make it transparent in
	 * the line's style table), so the whole line is laid out from the first letter and the speaker's name never moves.
	 * Empty: the rest is left out (Madrone change). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle")
	FString HiddenStyle = TEXT("WitchHidden");

	/** Speaker text format: {Speaker}. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Subtitle")
	FText SpeakerFormat = NSLOCTEXT("NSEBattleSubtitle", "SpeakerFormat", "{Speaker}");

protected:
	UFUNCTION(BlueprintImplementableEvent, Category="Subtitle")
	void OnSubtitleShown(const FText& Speaker, const FText& Line, float Seconds);

	UFUNCTION(BlueprintImplementableEvent, Category="Subtitle")
	void OnSubtitleHidden();

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Subtitle", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeakerName;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Subtitle", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Subtitle", meta=(BindWidgetOptional))
	TObjectPtr<URichTextBlock> LineRichText;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Subtitle", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SubtitleRoot;

private:
	void SetShownVisibility(bool bShow);
	/** Puts the first Count characters of the line (all: -1) on screen. */
	void ApplyLine(int32 Count);
	bool bShowing = false;
	float TimeLeft = 0.f;
	FText CurrentLine;
	FText CurrentSpeaker;
	TArray<float> RevealTimes;
	float RevealElapsed = 0.f;
	int32 RevealedChars = -1; // -1: whole line
};
