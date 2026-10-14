// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialLapRowUI.h"
#include "TimeTrialUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"

void UTimeTrialLapRowUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// no designer layout: build the row ourselves
	if (!LapText && !TimeText && WidgetTree && !WidgetTree->RootWidget)
	{
		CreateTextWidgets();
	}

	// remember the designer colors so a row that stops being the best lap can go back to them
	if (LapText)
	{
		DefaultLapColor = LapText->GetColorAndOpacity();
	}

	if (TimeText)
	{
		DefaultTimeColor = TimeText->GetColorAndOpacity();
	}
}

void UTimeTrialLapRowUI::CreateTextWidgets()
{
	// an overlay so SetGeneratedBar can slide a bar in behind the text later
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	BarImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BarImage"));
	BarImage->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToOverlay(BarImage);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Root->AddChildToOverlay(Row);

	LapText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LapText"));
	TimeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimeText"));

	// soft drop shadow so the text stays readable over bright scenery
	for (UTextBlock* Text : { LapText.Get(), TimeText.Get() })
	{
		Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	}

	UHorizontalBoxSlot* LapSlot = Row->AddChildToHorizontalBox(LapText);
	LapSlot->SetPadding(FMargin(0.0f, 0.0f, GeneratedTextSpacing, 0.0f));
	LapSlot->SetVerticalAlignment(VAlign_Center);

	UHorizontalBoxSlot* TimeSlot = Row->AddChildToHorizontalBox(TimeText);
	TimeSlot->SetVerticalAlignment(VAlign_Center);

	bGeneratedTextWidgets = true;
}

void UTimeTrialLapRowUI::SetGeneratedFont(const FSlateFontInfo& Font)
{
	if (!bGeneratedTextWidgets || !Font.HasValidFont())
	{
		return;
	}

	LapText->SetFont(Font);
	TimeText->SetFont(Font);
}

bool UTimeTrialLapRowUI::SetGeneratedBar(UTexture2D* Texture, float Scale, const FMargin& TimePadding)
{
	if (!bGeneratedTextWidgets || !Texture)
	{
		return false;
	}

	BarImage->SetBrush(UTimeTrialUI::MakeBarBrush(Texture, Scale));
	BarImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	// the bar artwork already says which lap this is, so only the time goes on top of it
	LapText->SetVisibility(ESlateVisibility::Collapsed);

	if (UOverlaySlot* TextSlot = Cast<UOverlaySlot>(TimeText->GetParent()->Slot))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Right);
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(TimePadding);
	}

	return true;
}

void UTimeTrialLapRowUI::SetLap(int32 Number, float Seconds, bool bIsBest, bool bIsCurrent)
{
	if (LapText)
	{
		LapText->SetText(FText::Format(LapFormat, FText::AsNumber(Number)));
	}

	SetTime(Seconds);

	ApplyColors(bIsBest, bIsCurrent);

	BP_LapSet(Number, Seconds, bIsBest, bIsCurrent);
}

void UTimeTrialLapRowUI::SetTime(float Seconds)
{
	if (TimeText)
	{
		TimeText->SetText(UTimeTrialUI::FormatLapTime(Seconds));
	}
}

void UTimeTrialLapRowUI::ApplyColors(bool bIsBest, bool bIsCurrent)
{
	FSlateColor LapColor = DefaultLapColor;
	FSlateColor TimeColor = DefaultTimeColor;

	// the best lap wins over the current one: once a lap is done it can't be current anymore anyway
	if (bIsBest && BestLapColor.A > 0.0f)
	{
		LapColor = TimeColor = BestLapColor;

	} else if (bIsCurrent && CurrentLapColor.A > 0.0f) {

		LapColor = TimeColor = CurrentLapColor;
	}

	if (LapText)
	{
		LapText->SetColorAndOpacity(LapColor);
	}

	if (TimeText)
	{
		TimeText->SetColorAndOpacity(TimeColor);
	}
}
