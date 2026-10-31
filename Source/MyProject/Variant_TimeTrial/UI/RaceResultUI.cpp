// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "RaceResultUI.h"
#include "TimeTrialUI.h"
#include "TimeTrialGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Widths of the lap list columns: lap, time */
	TArray<float> LapColumns() { return { 110.0f, 170.0f }; }

	/** Widths of the standings columns: place, name, car, time */
	TArray<float> StandingColumns() { return { 50.0f, 110.0f, 110.0f, 170.0f }; }
}

URaceResultUI::URaceResultUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// two items side by side under the results
	MenuItemWidth = 300.0f;

	// same default font as a Text widget dropped in the designer. Its fallback face covers Japanese.
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		HeadlineFont = FSlateFontInfo(RobotoFontObj.Object, 56, FName("Bold Italic"));
		BodyFont = FSlateFontInfo(RobotoFontObj.Object, 20, FName("Regular"));
		SmallFont = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Bold"));
		MenuFont = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
}

void URaceResultUI::SetPlayerResult(const TArray<float>& InLapTimes, float InTotalTime, const FRaceRecordUpdate& InRecordUpdate)
{
	LapTimes = InLapTimes;
	TotalTime = InTotalTime;
	RecordUpdate = InRecordUpdate;

	Refresh();
}

void URaceResultUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CreateLayout();
	}
}

void URaceResultUI::CreateLayout()
{
	UVerticalBox* Content = CreatePanelLayout();

	const FLinearColor Dim(0.7f, 0.7f, 0.7f, 1.0f);

	Content->AddChildToVerticalBox(CreateText(TEXT("TitleText"), TitleLabel, SmallFont, Dim))->SetHorizontalAlignment(HAlign_Center);

	HeadlineText = CreateText(TEXT("HeadlineText"), FText::GetEmpty(), HeadlineFont, FLinearColor::White);
	UVerticalBoxSlot* HeadlineSlot = Content->AddChildToVerticalBox(HeadlineText);
	HeadlineSlot->SetHorizontalAlignment(HAlign_Center);
	HeadlineSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));

	TimesText = CreateText(TEXT("TimesText"), FText::GetEmpty(), BodyFont, FLinearColor::White);
	Content->AddChildToVerticalBox(TimesText)->SetHorizontalAlignment(HAlign_Center);

	// the course records under the race's own times, with any it broke announced above them
	NewRecordText = CreateText(TEXT("NewRecordText"), FText::GetEmpty(), SmallFont, LineHighlightColor);
	UVerticalBoxSlot* NewRecordSlot = Content->AddChildToVerticalBox(NewRecordText);
	NewRecordSlot->SetHorizontalAlignment(HAlign_Center);
	NewRecordSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	RecordText = CreateText(TEXT("RecordText"), FText::GetEmpty(), SmallFont, Dim);
	UVerticalBoxSlot* RecordSlot = Content->AddChildToVerticalBox(RecordText);
	RecordSlot->SetHorizontalAlignment(HAlign_Center);
	RecordSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 24.0f));

	// lap times on the left, the standings on the right
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Columns"));
	Content->AddChildToVerticalBox(Columns)->SetHorizontalAlignment(HAlign_Center);

	UVerticalBox* Laps = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LapColumn"));
	LapColumn = Laps;
	Columns->AddChildToHorizontalBox(Laps)->SetPadding(FMargin(0.0f, 0.0f, 48.0f, 0.0f));

	Laps->AddChildToVerticalBox(CreateText(TEXT("LapsHeading"), LapsHeading, SmallFont, Dim))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	LapList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LapList"));
	Laps->AddChildToVerticalBox(LapList);

	UVerticalBox* Standings = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StandingsColumn"));
	StandingsColumn = Standings;
	Columns->AddChildToHorizontalBox(Standings);

	Standings->AddChildToVerticalBox(CreateText(TEXT("StandingsHeading"), StandingsHeading, SmallFont, Dim))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	StandingsList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StandingsList"));
	Standings->AddChildToVerticalBox(StandingsList);

	// run the race again, or back to the title screen
	UHorizontalBox* Menu = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Menu"));

	UVerticalBoxSlot* MenuSlot = Content->AddChildToVerticalBox(Menu);
	MenuSlot->SetHorizontalAlignment(HAlign_Center);
	MenuSlot->SetPadding(FMargin(0.0f, 28.0f, 0.0f, 8.0f));

	Menu->AddChildToHorizontalBox(CreateMenuItem(RetryLabel, ERaceMenuAction::Retry));
	Menu->AddChildToHorizontalBox(CreateMenuItem(ReturnLabel, ERaceMenuAction::ReturnToTitle))->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));

	Content->AddChildToVerticalBox(CreateText(TEXT("HintText"), HintLabel, SmallFont, Dim))->SetHorizontalAlignment(HAlign_Center);
}

TArray<UTextBlock*> URaceResultUI::AddRow(UVerticalBox* List, const TArray<float>& ColumnWidths)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));

	TArray<UTextBlock*> Cells;

	// fixed width columns keep the rows lined up
	for (const float Width : ColumnWidths)
	{
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(Width);
		Row->AddChildToHorizontalBox(Cell);

		UTextBlock* Text = CreateText(NAME_None, FText::GetEmpty(), BodyFont, FLinearColor::White);
		Cell->SetContent(Text);
		Cells.Add(Text);
	}

	return Cells;
}

void URaceResultUI::NativeConstruct()
{
	Super::NativeConstruct();

	Refresh();
}

void URaceResultUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// NPCs still on track keep finishing behind the player
	RefreshTime -= InDeltaTime;

	if (RefreshTime <= 0.0f)
	{
		RefreshTime = 0.25f;
		Refresh();
	}
}

void URaceResultUI::Refresh()
{
	if (!HeadlineText)
	{
		return;
	}

	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));
	const bool bTwoPlayers = GM && GM->IsTwoPlayers();
	const bool bVersus = GM && (GM->IsVersusNpc() || bTwoPlayers);

	TArray<FRaceStanding> Standings;

	if (bVersus)
	{
		GM->GetStandings(Standings);
	}

	// two players: whoever of the two came in ahead, with both their places and times, and everyone's standings.
	// The race isn't one for the records
	if (bTwoPlayers)
	{
		TArray<FText> PlayerTimes;
		FText Winner;

		for (const FRaceStanding& Standing : Standings)
		{
			if (!Standing.bIsPlayer)
			{
				continue;
			}

			if (Winner.IsEmpty())
			{
				Winner = Standing.Name;
			}

			PlayerTimes.Add(FText::Format(TwoPlayerTimeFormat, Standing.Name, FText::AsNumber(Standing.Position),
				Standing.bFinished ? UTimeTrialUI::FormatLapTime(Standing.FinishTime) : RunningLabel));
		}

		// the standings come in race order, so the first player in them won. Their times read 1P first
		PlayerTimes.Sort([](const FText& A, const FText& B) { return A.ToString() < B.ToString(); });

		HeadlineText->SetText(FText::Format(TwoPlayerWinFormat, Winner));
		TimesText->SetText(FText::Join(FText::FromString(TEXT("　　　")), PlayerTimes));

		if (LapColumn)
		{
			LapColumn->SetVisibility(ESlateVisibility::Collapsed);
		}

	} else if (bVersus) {
		const int32 Position = GM->GetRacePosition(GetOwningPlayer());
		HeadlineText->SetText(FText::Format(PositionFormat, FText::AsNumber(Position), FText::AsNumber(GM->GetRacerCount())));

	} else {

		HeadlineText->SetText(FinishLabel);
	}

	// total and best lap
	float BestLap = -1.0f;
	int32 BestLapIndex = INDEX_NONE;

	for (int32 Index = 0; Index < LapTimes.Num(); ++Index)
	{
		if (BestLapIndex == INDEX_NONE || LapTimes[Index] < BestLap)
		{
			BestLap = LapTimes[Index];
			BestLapIndex = Index;
		}
	}

	if (!bTwoPlayers)
	{
		TimesText->SetText(FText::Format(TimesFormat, UTimeTrialUI::FormatLapTime(TotalTime), UTimeTrialUI::FormatLapTime(BestLap)));
	}

	// a two player race isn't one for the records. The empty line keeps the gap above the standings
	RecordText->SetText(bTwoPlayers ? FText::GetEmpty()
		: FText::Format(RecordFormat, UTimeTrialUI::FormatLapTime(RecordUpdate.BestTotal), UTimeTrialUI::FormatLapTime(RecordUpdate.BestLap)));

	TArray<FText> NewRecords;

	if (RecordUpdate.bNewBestTotal)
	{
		NewRecords.Add(NewBestTotalLabel);
	}

	if (RecordUpdate.bNewBestLap)
	{
		NewRecords.Add(NewBestLapLabel);
	}

	if (bVersus && !bTwoPlayers && GM->GetNpcDifficulties().IsValidIndex(GM->GetNewlyUnlockedDifficulty()))
	{
		NewRecords.Add(FText::Format(UnlockedFormat, GM->GetNpcDifficulties()[GM->GetNewlyUnlockedDifficulty()].DisplayName));
	}

	NewRecordText->SetText(FText::Join(FText::FromString(TEXT("　　")), NewRecords));
	NewRecordText->SetVisibility(NewRecords.IsEmpty() || bTwoPlayers ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// one row per lap, the best one picked out
	if (LapList && LapList->GetChildrenCount() != LapTimes.Num())
	{
		LapList->ClearChildren();

		for (int32 Index = 0; Index < LapTimes.Num(); ++Index)
		{
			const TArray<UTextBlock*> Cells = AddRow(LapList, LapColumns());
			const FLinearColor Color = Index == BestLapIndex ? LineHighlightColor : FLinearColor::White;

			Cells[0]->SetText(FText::Format(LapFormat, FText::AsNumber(Index + 1)));
			Cells[1]->SetText(UTimeTrialUI::FormatLapTime(LapTimes[Index]));
			Cells[0]->SetColorAndOpacity(Color);
			Cells[1]->SetColorAndOpacity(Color);
		}
	}

	// everyone's standings, only when racing NPCs or each other
	if (StandingsColumn)
	{
		StandingsColumn->SetVisibility(bVersus ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!bVersus || !StandingsList)
	{
		return;
	}

	while (StandingRows.Num() < Standings.Num())
	{
		StandingRows.Add(AddRow(StandingsList, StandingColumns()));
	}

	for (int32 Index = 0; Index < Standings.Num(); ++Index)
	{
		const FRaceStanding& Standing = Standings[Index];
		const TArray<UTextBlock*>& Cells = StandingRows[Index];
		const FLinearColor Color = Standing.bIsPlayer ? LineHighlightColor : FLinearColor::White;

		Cells[0]->SetText(FText::AsNumber(Standing.Position));
		Cells[1]->SetText(Standing.Name);
		Cells[2]->SetText(Standing.CarName);
		Cells[3]->SetText(Standing.bFinished ? UTimeTrialUI::FormatLapTime(Standing.FinishTime) : RunningLabel);

		for (int32 Column = 0; Column < Cells.Num(); ++Column)
		{
			// a racer still on track is greyed out
			Cells[Column]->SetColorAndOpacity(Column == 3 && !Standing.bFinished ? FLinearColor(0.6f, 0.6f, 0.6f, 1.0f) : Color);
		}
	}
}

void URaceResultUI::ReturnToTitle()
{
	ChooseAction(ERaceMenuAction::ReturnToTitle);
}
