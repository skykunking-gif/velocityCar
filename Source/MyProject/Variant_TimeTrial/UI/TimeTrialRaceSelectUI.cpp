// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TimeTrialRaceSelectUI.h"
#include "TimeTrialGameMode.h"
#include "TimeTrialRecords.h"
#include "TimeTrialUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MyProject.h"

UTimeTrialRaceSelectUI::UTimeTrialRaceSelectUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the screen has to be focusable for key events to reach NativeOnKeyDown
	SetIsFocusable(true);

	// same default font as a Text widget dropped in the designer. Its fallback face covers Japanese.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		SmallFont = FSlateFontInfo(RobotoFontObj.Object, 18, FName("Regular"));
		RaceNameFont = FSlateFontInfo(RobotoFontObj.Object, 30, FName("Bold"));
		DetailsFont = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Bold"));
		RecordFont = FSlateFontInfo(RobotoFontObj.Object, 18, FName("Bold"));
	}
}

void UTimeTrialRaceSelectUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CreateLayout();
	}
}

void UTimeTrialRaceSelectUI::CreateLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// the race list down the left of the screen
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(40.0f, 28.0f));

	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	PanelSlot->SetPadding(FMargin(ScreenEdgeOffset, 0.0f, 0.0f, 0.0f));

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	const FLinearColor Dim(0.7f, 0.7f, 0.7f, 1.0f);

	Content->AddChildToVerticalBox(CreateText(TEXT("TitleText"), TitleLabel, SmallFont, Dim))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	// the time of day, above the races, as wide as their rows
	USizeBox* TimeOfDayBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TimeOfDayBox"));
	TimeOfDayBox->SetWidthOverride(RaceRowWidth);
	Content->AddChildToVerticalBox(TimeOfDayBox)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	TimeOfDayBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TimeOfDayBar"));
	TimeOfDayBar->SetBrushColor(ItemColor);
	TimeOfDayBar->SetPadding(FMargin(32.0f, 10.0f));
	TimeOfDayBar->SetHorizontalAlignment(HAlign_Center);
	TimeOfDayBox->SetContent(TimeOfDayBar);

	TimeOfDayText = CreateText(TEXT("TimeOfDayText"), FText::GetEmpty(), RecordFont, HighlightTextColor);
	TimeOfDayBar->SetContent(TimeOfDayText);

	RaceList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RaceList"));
	Content->AddChildToVerticalBox(RaceList);

	for (int32 RaceIndex = 1; RaceIndex <= GetRaceCount(); ++RaceIndex)
	{
		AddRaceRow(RaceIndex);
	}

	RecordText = CreateText(TEXT("RecordText"), FText::GetEmpty(), RecordFont, FLinearColor::White);
	Content->AddChildToVerticalBox(RecordText)->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));

	Content->AddChildToVerticalBox(CreateText(TEXT("HintText"), HintLabel, SmallFont, Dim))->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));

	// the right of the screen is kept for a map of the course: an empty box of a fixed size, so the layout already
	// leaves room for it
	MinimapArea = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("MinimapArea"));
	MinimapArea->SetWidthOverride(MinimapSize.X);
	MinimapArea->SetHeightOverride(MinimapSize.Y);

	UOverlaySlot* MinimapSlot = Root->AddChildToOverlay(MinimapArea);
	MinimapSlot->SetHorizontalAlignment(HAlign_Right);
	MinimapSlot->SetVerticalAlignment(VAlign_Center);
	MinimapSlot->SetPadding(FMargin(0.0f, 0.0f, ScreenEdgeOffset, 0.0f));
}

void UTimeTrialRaceSelectUI::AddRaceRow(int32 RaceIndex)
{
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();
	const FTimeTrialRaceDefinition* Race = GM ? GM->FindRace(RaceIndex) : nullptr;

	if (!Race)
	{
		return;
	}

	const FString BaseName = FString::Printf(TEXT("Race%d"), RaceIndex);

	// a fixed width keeps the bars lined up whatever the name length
	USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(BaseName + TEXT("Box")));
	RowBox->SetWidthOverride(RaceRowWidth);

	UVerticalBoxSlot* RowSlot = RaceList->AddChildToVerticalBox(RowBox);
	RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(BaseName + TEXT("Bar")));
	Bar->SetPadding(FMargin(0.0f));
	RowBox->SetContent(Bar);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(BaseName + TEXT("Row")));
	Bar->SetContent(Row);

	// a strip of the race's color down the left of the bar, the color its gates show in
	USizeBox* StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(BaseName + TEXT("StripBox")));
	StripBox->SetWidthOverride(10.0f);

	UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(BaseName + TEXT("Strip")));
	Strip->SetBrushColor(Race->Color);
	StripBox->SetContent(Strip);

	Row->AddChildToHorizontalBox(StripBox)->SetVerticalAlignment(VAlign_Fill);

	// the name, with the laps and direction under it
	UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(BaseName + TEXT("Text")));

	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
	TextSlot->SetPadding(FMargin(22.0f, 10.0f, 22.0f, 12.0f));
	TextSlot->SetVerticalAlignment(VAlign_Center);

	const FText Name = Race->DisplayName.IsEmpty() ? FText::Format(UnnamedRaceFormat, FText::AsNumber(RaceIndex)) : Race->DisplayName;

	FText Details = FText::Format(LapsFormat, FText::AsNumber(FMath::Max(Race->Laps, 1)));

	if (Race->bReversed)
	{
		Details = FText::Join(FText::FromString(TEXT("　")), Details, ReversedLabel);
	}

	FRaceRow& RaceRow = RaceRows.AddDefaulted_GetRef();
	RaceRow.Bar = Bar;
	RaceRow.Name = CreateText(*(BaseName + TEXT("Name")), Name, RaceNameFont, ItemTextColor);
	RaceRow.Details = CreateText(*(BaseName + TEXT("Details")), Details, DetailsFont, ItemTextColor);

	Text->AddChildToVerticalBox(RaceRow.Name.Get());
	Text->AddChildToVerticalBox(RaceRow.Details.Get());
}

UTextBlock* UTimeTrialRaceSelectUI::CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(Color);

	if (Font.HasValidFont())
	{
		TextBlock->SetFont(Font);
	}

	// soft drop shadow so the text stays readable over bright scenery
	TextBlock->SetShadowOffset(FVector2D(2.0f, 2.0f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	return TextBlock;
}

void UTimeTrialRaceSelectUI::NativeConstruct()
{
	Super::NativeConstruct();

	bRaceChosen = false;
	bFocusClaimed = false;

	// the races are picked with the mouse too, so the screen has to take mouse events
	SetVisibility(ESlateVisibility::Visible);

	if (GetRaceCount() == 0)
	{
		// nothing to pick from: move straight on. Deferred a tick because the owner only
		// subscribes to OnRaceChosen after adding the screen to the viewport
		UE_LOG(LogMyProject, Warning, TEXT("[RaceSelect] The game mode offers no races (Races is empty), so race select is skipped."));

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bRaceChosen = true;
				OnRaceChosen.Broadcast();
			}));
		}

		return;
	}

	// start on the race the game mode has selected
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();
	HighlightRace(GM ? GM->GetSelectedRace() : 1);
	RefreshTimeOfDay();
}

void UTimeTrialRaceSelectUI::NativeDestruct()
{
	// hand input back to the game so the player can drive
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}

	Super::NativeDestruct();
}

void UTimeTrialRaceSelectUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// the widget isn't in the Slate hierarchy yet while it's being constructed, so it can't take focus until now
	if (!bFocusClaimed)
	{
		ClaimKeyboardFocus();
	}
}

void UTimeTrialRaceSelectUI::ClaimKeyboardFocus()
{
	APlayerController* PC = GetOwningPlayer();

	if (!PC)
	{
		return;
	}

	bFocusClaimed = true;

	// send keyboard and gamepad input to this widget while the screen is up. The game still
	// gets anything the widget doesn't handle, and mouse clicks on the rows are unaffected.
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
}

FReply UTimeTrialRaceSelectUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	// move the highlight up a race
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		MoveHighlight(-1);
		return FReply::Handled();
	}

	// move the highlight down a race
	if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		MoveHighlight(1);
		return FReply::Handled();
	}

	// change the time of day
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		MoveTimeOfDay(-1);
		return FReply::Handled();
	}

	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		MoveTimeOfDay(1);
		return FReply::Handled();
	}

	// pick the highlighted race
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		ConfirmHighlightedRace();
		return FReply::Handled();
	}

	// back to the car select, unless a race has already been picked
	if (Key == EKeys::BackSpace || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey())
	{
		if (!bRaceChosen)
		{
			bRaceChosen = true;
			OnBack.Broadcast();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UTimeTrialRaceSelectUI::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const int32 RaceIndex = FindRaceUnder(InMouseEvent.GetScreenSpacePosition());

	if (RaceIndex > 0 && RaceIndex != HighlightedRace)
	{
		HighlightRace(RaceIndex);
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTimeTrialRaceSelectUI::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const int32 RaceIndex = FindRaceUnder(InMouseEvent.GetScreenSpacePosition());

		if (RaceIndex > 0)
		{
			HighlightRace(RaceIndex);
			ChooseRace(RaceIndex);
			return FReply::Handled();
		}

		// the left half of the time of day steps back, the right half forward
		if (TimeOfDayBar && TimeOfDayBar->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
		{
			const FGeometry& BarGeometry = TimeOfDayBar->GetCachedGeometry();
			const float LocalX = BarGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).X;

			MoveTimeOfDay(LocalX < BarGeometry.GetLocalSize().X * 0.5f ? -1 : 1);
			return FReply::Handled();
		}
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

int32 UTimeTrialRaceSelectUI::FindRaceUnder(const FVector2D& ScreenPosition) const
{
	const int32 Index = RaceRows.IndexOfByPredicate([&ScreenPosition](const FRaceRow& Row)
	{
		return Row.Bar.IsValid() && Row.Bar->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	});

	return Index + 1;
}

int32 UTimeTrialRaceSelectUI::GetRaceCount() const
{
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();

	return GM ? GM->GetRaceCount() : 0;
}

void UTimeTrialRaceSelectUI::HighlightRace(int32 RaceIndex)
{
	HighlightedRace = RaceIndex >= 1 && RaceIndex <= GetRaceCount() ? RaceIndex : 0;

	RefreshHighlight();

	// let Blueprint drive any extra visuals
	BP_HighlightChanged(HighlightedRace);
}

void UTimeTrialRaceSelectUI::MoveHighlight(int32 Delta)
{
	const int32 Count = GetRaceCount();

	if (Count == 0 || Delta == 0)
	{
		return;
	}

	// wrap around the ends so the list cycles
	const int32 Current = FMath::Clamp(HighlightedRace, 1, Count) - 1;

	HighlightRace(((Current + Delta) % Count + Count) % Count + 1);
}

void UTimeTrialRaceSelectUI::MoveTimeOfDay(int32 Delta)
{
	ATimeTrialGameMode* GM = GetTimeTrialGameMode();
	const int32 Count = GM ? GM->GetTimesOfDay().Num() : 0;

	if (Count == 0 || Delta == 0 || bRaceChosen)
	{
		return;
	}

	// wrap around the ends so the list cycles. The level is lit for the new time straight away, behind the menu
	GM->SetSelectedTimeOfDay(((GM->GetSelectedTimeOfDay() + Delta) % Count + Count) % Count);

	RefreshTimeOfDay();

	// the records shown are those for the new time
	RefreshHighlight();
}

void UTimeTrialRaceSelectUI::RefreshTimeOfDay()
{
	if (!TimeOfDayText)
	{
		return;
	}

	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();

	if (!GM || !GM->GetTimesOfDay().IsValidIndex(GM->GetSelectedTimeOfDay()))
	{
		TimeOfDayBar->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TimeOfDayText->SetText(FText::Format(TimeOfDayFormat, GM->GetTimesOfDay()[GM->GetSelectedTimeOfDay()].DisplayName));
	TimeOfDayBar->SetVisibility(ESlateVisibility::Visible);
}

void UTimeTrialRaceSelectUI::ConfirmHighlightedRace()
{
	if (HighlightedRace > 0)
	{
		ChooseRace(HighlightedRace);
	}
}

void UTimeTrialRaceSelectUI::ChooseRace(int32 RaceIndex)
{
	// a click and a key press could both land before the screen is torn down, so only the
	// first choice counts
	if (bRaceChosen)
	{
		return;
	}

	bRaceChosen = true;

	// tell the game mode which race to run
	if (ATimeTrialGameMode* GM = GetTimeTrialGameMode())
	{
		GM->SetSelectedRace(RaceIndex);
	}

	// let the owning UI know a choice has been made
	OnRaceChosen.Broadcast();
}

void UTimeTrialRaceSelectUI::RefreshHighlight()
{
	for (int32 Index = 0; Index < RaceRows.Num(); ++Index)
	{
		const bool bHighlighted = Index == HighlightedRace - 1;
		const FLinearColor TextColor = bHighlighted ? HighlightTextColor : ItemTextColor;

		if (RaceRows[Index].Bar.IsValid())
		{
			RaceRows[Index].Bar->SetBrushColor(bHighlighted ? HighlightColor : ItemColor);
		}

		if (RaceRows[Index].Name.IsValid())
		{
			RaceRows[Index].Name->SetColorAndOpacity(TextColor);
		}

		if (RaceRows[Index].Details.IsValid())
		{
			RaceRows[Index].Details->SetColorAndOpacity(TextColor);
		}
	}

	if (!RecordText)
	{
		return;
	}

	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();
	const UTimeTrialRecords* Records = UTimeTrialRecords::Get(this);

	if (!GM || !Records || HighlightedRace <= 0)
	{
		RecordText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FCourseRecord Record = Records->GetCourseRecord(GM->GetCourseId(HighlightedRace, GM->GetSelectedTimeOfDay()));
	const FTimeRecord* BestTotal = Record.BestTotals.Find(GM->GetLapsForRace(HighlightedRace));
	const FTimeRecord NoRecord;

	if (!BestTotal)
	{
		BestTotal = &NoRecord;
	}

	RecordText->SetText(FText::Format(RecordFormat,
		UTimeTrialUI::FormatLapTime(BestTotal->Time), FText::FromString(BestTotal->CarName),
		UTimeTrialUI::FormatLapTime(Record.BestLap.Time), FText::FromString(Record.BestLap.CarName)));

	RecordText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

ATimeTrialGameMode* UTimeTrialRaceSelectUI::GetTimeTrialGameMode() const
{
	return Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));
}
