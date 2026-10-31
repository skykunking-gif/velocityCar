// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RaceMenuUI.h"
#include "TimeTrialRecords.h"
#include "RaceResultUI.generated.h"

class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 *  The results screen shown once the player has run all the laps: the player's place (against NPCs) or
 *  just the finish (in a time trial), the total and lap times, and in a race against NPCs everyone's standings,
 *  which fill in as the NPCs still on track finish, and the course records with any the race broke picked out.
 *  Under them, a choice between running the same race again
 *  and going back to the title screen.
 *
 *  The whole layout is built in code.
 */
UCLASS()
class URaceResultUI : public URaceMenuUI
{
	GENERATED_BODY()

public:

	URaceResultUI(const FObjectInitializer& ObjectInitializer);

	/** Fills in the player's own race: the time of each lap in order, the total time, and what the race did to the
	 *  course records */
	void SetPlayerResult(const TArray<float>& InLapTimes, float InTotalTime, const FRaceRecordUpdate& InRecordUpdate);

	/** Fades out and goes back to the title screen */
	UFUNCTION(BlueprintCallable, Category="Race Result")
	void ReturnToTitle();

protected:

	/** Builds the layout */
	virtual void NativeOnInitialized() override;

	/** Fills in the results */
	virtual void NativeConstruct() override;

	/** Keeps the standings up to date as NPCs finish */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Builds the results panel in the middle of the screen */
	void CreateLayout();

	/** Adds a row of fixed width columns to a list */
	TArray<UTextBlock*> AddRow(UVerticalBox* List, const TArray<float>& ColumnWidths);

	/** Writes the place, the times and the standings */
	void Refresh();

	/** Shows the player's place, or that the time trial is finished */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadlineText;

	/** Shows the total time and the best lap */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimesText;

	/** Announces the records the race broke. Hidden if it broke none */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NewRecordText;

	/** Shows the course records */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecordText;

	/** One row per lap */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LapList;

	/** One row per racer. Only shown in a race against NPCs */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StandingsList;

	/** The column holding the standings, hidden in a time trial */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StandingsColumn;

	/** The column holding the lap times, hidden in a two player game */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> LapColumn;

	/** Text blocks of each standings row: place, name, car, time */
	TArray<TArray<UTextBlock*>> StandingRows;

	/** Heading of the panel */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText TitleLabel = NSLOCTEXT("RaceResultUI", "Title", "RESULT");

	/** Headline in a time trial */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText FinishLabel = NSLOCTEXT("RaceResultUI", "Finish", "FINISH");

	/** Headline in a race against NPCs. {0} is the player's place and {1} how many cars raced */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText PositionFormat = NSLOCTEXT("RaceResultUI", "Position", "{0}位 / {1}");

	/** Total and best lap line. {0} is the total time and {1} the best lap */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText TimesFormat = NSLOCTEXT("RaceResultUI", "Times", "TOTAL  {0}　　BEST LAP  {1}");

	/** Course record line. {0} is the best total time for this many laps and {1} the best lap */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText RecordFormat = NSLOCTEXT("RaceResultUI", "Record", "COURSE RECORD  {0}　　LAP RECORD  {1}");

	/** Headline in a two player game. {0} is the winner, 1P or 2P */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText TwoPlayerWinFormat = NSLOCTEXT("RaceResultUI", "TwoPlayerWin", "{0} WIN!");

	/** One player's total in a two player game. {0} is the player, {1} their place and {2} their total time */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText TwoPlayerTimeFormat = NSLOCTEXT("RaceResultUI", "TwoPlayerTime", "{0}  {1}位  {2}");

	/** Announced when the race set the best total time on the course */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText NewBestTotalLabel = NSLOCTEXT("RaceResultUI", "NewBestTotal", "NEW COURSE RECORD!");

	/** Announced when one of the race's laps set the best lap on the course */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText NewBestLapLabel = NSLOCTEXT("RaceResultUI", "NewBestLap", "NEW LAP RECORD!");

	/** Announced when winning the race unlocked the next NPC level. {0} is its name */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText UnlockedFormat = NSLOCTEXT("RaceResultUI", "Unlocked", "「{0}」が解放されました！");

	/** Label of each lap row. {0} is the lap number */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText LapFormat = NSLOCTEXT("RaceResultUI", "Lap", "LAP {0}");

	/** Heading of the lap list */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText LapsHeading = NSLOCTEXT("RaceResultUI", "LapsHeading", "LAP TIME");

	/** Heading of the standings */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText StandingsHeading = NSLOCTEXT("RaceResultUI", "StandingsHeading", "STANDINGS");

	/** Shown instead of the time of a racer still on track */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText RunningLabel = NSLOCTEXT("RaceResultUI", "Running", "走行中");

	/** Label of the item that runs the same race again */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText RetryLabel = NSLOCTEXT("RaceResultUI", "Retry", "リトライ");

	/** Label of the item back to the title screen */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText ReturnLabel = NSLOCTEXT("RaceResultUI", "Return", "タイトルへ戻る");

	/** Controls hint under the items */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FText HintLabel = NSLOCTEXT("RaceResultUI", "Hint", "← → で選択　Enter で決定");

	/** Font of the headline */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FSlateFontInfo HeadlineFont;

	/** Font of the times and the rows */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FSlateFontInfo BodyFont;

	/** Font of the headings and the hint */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FSlateFontInfo SmallFont;

	/** Color of the player's own lines and the best lap */
	UPROPERTY(EditAnywhere, Category="Race Result|Layout")
	FLinearColor LineHighlightColor = FLinearColor(1.0f, 0.45f, 0.08f, 1.0f);

	/** Time of each of the player's laps, in order */
	TArray<float> LapTimes;

	/** The player's total time */
	float TotalTime = 0.0f;

	/** What the race did to the course records */
	FRaceRecordUpdate RecordUpdate;

	/** Time until the standings are next refreshed */
	float RefreshTime = 0.0f;
};
