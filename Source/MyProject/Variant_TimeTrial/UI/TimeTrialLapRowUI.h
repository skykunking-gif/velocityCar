// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "TimeTrialLapRowUI.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/**
 *  One row of the lap time list on the race HUD.
 *  The owning UTimeTrialUI creates one of these per lap and fills it in whenever the lap changes.
 *
 *  Works as-is from C++: if there are no Text widgets named LapText and TimeText, it builds its own,
 *  optionally on top of a bar image (SetGeneratedBar).
 *  To restyle the row in the designer, make a Blueprint subclass with those two Text widgets.
 */
UCLASS()
class UTimeTrialLapRowUI : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fills in the row. Seconds is negative for a lap that hasn't been run yet, which shows as dashes. */
	UFUNCTION(BlueprintCallable, Category="Lap Row")
	void SetLap(int32 Number, float Seconds, bool bIsBest, bool bIsCurrent);

	/** Updates only the time readout. Used every frame for the lap being driven. */
	UFUNCTION(BlueprintCallable, Category="Lap Row")
	void SetTime(float Seconds);

	/** Sets the font of the text widgets this row built itself. Designer-made text keeps its own font. */
	void SetGeneratedFont(const FSlateFontInfo& Font);

	/** Draws a bar behind a row this widget built itself. The bar artwork carries the lap label, so only
	 *  the time is shown, right aligned inside TimePadding. Returns false if no bar was applied. */
	bool SetGeneratedBar(UTexture2D* Texture, float Scale, const FMargin& TimePadding);

protected:

	/** Widget initialization. Builds the text widgets if the designer didn't, and remembers the
	 *  original text colors so the highlight can be undone.
	 *  Done here rather than in NativeConstruct because the owner fills rows in right after CreateWidget. */
	virtual void NativeOnInitialized() override;

	/** Called after SetLap, so Blueprint can add its own visuals on top of the text colors */
	UFUNCTION(BlueprintImplementableEvent, Category="Lap Row", meta = (DisplayName = "Lap Set"))
	void BP_LapSet(int32 Number, float Seconds, bool bIsBest, bool bIsCurrent);

	/** Builds an overlay with a hidden bar image and a horizontal box of LapText and TimeText as the root of the widget tree */
	void CreateTextWidgets();

	/** Applies the best/current colors, or restores the defaults */
	void ApplyColors(bool bIsBest, bool bIsCurrent);

	/** Shows the lap number */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LapText;

	/** Shows the lap time */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;

	/** Bar behind the text on rows this widget builds itself. Collapsed until SetGeneratedBar is called. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> BarImage;

	/** Format for the lap number. {0} is replaced by the lap number. */
	UPROPERTY(EditAnywhere, Category="Lap Row")
	FText LapFormat = NSLOCTEXT("TimeTrialLapRowUI", "LapFormat", "LAP {0}");

	/** Text color for the fastest lap. Set its alpha to zero to leave the colors alone. */
	UPROPERTY(EditAnywhere, Category="Lap Row")
	FLinearColor BestLapColor = FLinearColor(0.6f, 0.2f, 1.0f, 1.0f);

	/** Text color for the lap being driven. Set its alpha to zero to leave the colors alone. */
	UPROPERTY(EditAnywhere, Category="Lap Row")
	FLinearColor CurrentLapColor = FLinearColor(1.0f, 0.72f, 0.15f, 1.0f);

	/** Gap between the lap number and the lap time on rows this widget builds itself */
	UPROPERTY(EditAnywhere, Category="Lap Row")
	float GeneratedTextSpacing = 24.0f;

	/** Colors the text widgets had in the designer */
	FSlateColor DefaultLapColor;
	FSlateColor DefaultTimeColor;

	/** True if the text widgets were built by CreateTextWidgets rather than the designer */
	bool bGeneratedTextWidgets = false;
};
