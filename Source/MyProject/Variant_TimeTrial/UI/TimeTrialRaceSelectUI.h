// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "TimeTrialRaceSelectUI.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class ATimeTrialGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRaceChosenDelegate);

/**
 *  The race select screen shown before the countdown.
 *  Lists the races on the Time Trial game mode (Races) down the left of the screen, each with its color, laps and
 *  direction, with the time of day above them and the records of the highlighted race at that time under them. The
 *  right of the screen is kept clear for a map of the course (MinimapArea), which is empty for now.
 *
 *  Up/down (W/S, d-pad) move the highlight, left/right (A/D, d-pad) change the time of day, which lights the level
 *  for it straight away, and Enter (Space, gamepad accept) picks the race; the mouse works too.
 *  The whole layout is built in code.
 */
UCLASS()
class UTimeTrialRaceSelectUI : public UUserWidget
{
	GENERATED_BODY()

public:

	UTimeTrialRaceSelectUI(const FObjectInitializer& ObjectInitializer);

	/** Picks a race (numbered from 1) on the game mode and broadcasts OnRaceChosen so the owning UI can move on
	 *  to the countdown */
	UFUNCTION(BlueprintCallable, Category="Race Select")
	void ChooseRace(int32 RaceIndex);

	/** Moves the highlight to a race (numbered from 1) without committing to it */
	UFUNCTION(BlueprintCallable, Category="Race Select")
	void HighlightRace(int32 RaceIndex);

	/** Moves the highlight by Delta races, wrapping around the ends of the list */
	UFUNCTION(BlueprintCallable, Category="Race Select")
	void MoveHighlight(int32 Delta);

	/** Steps the time of day by Delta, wrapping around the ends of the list */
	UFUNCTION(BlueprintCallable, Category="Race Select")
	void MoveTimeOfDay(int32 Delta);

	/** Picks the race that's currently highlighted */
	UFUNCTION(BlueprintCallable, Category="Race Select")
	void ConfirmHighlightedRace();

	/** Returns the number of the highlighted race, counting from 1, or 0 if nothing is highlighted */
	UFUNCTION(BlueprintPure, Category="Race Select")
	int32 GetHighlightedRace() const { return HighlightedRace; };

	/** Returns how many races the game mode offers */
	UFUNCTION(BlueprintPure, Category="Race Select")
	int32 GetRaceCount() const;

	/** Returns the area kept for a map of the course, on the right of the screen. It's empty: put the map in it */
	USizeBox* GetMinimapArea() const { return MinimapArea; };

	/** Broadcast once a race has been chosen */
	FRaceChosenDelegate OnRaceChosen;

	/** Broadcast when the player backs out to the screen before, without choosing a race */
	FRaceChosenDelegate OnBack;

protected:

	/** Builds the layout */
	virtual void NativeOnInitialized() override;

	/** Highlights the race the game mode has selected */
	virtual void NativeConstruct() override;

	/** Hands input back to the game so the player can drive */
	virtual void NativeDestruct() override;

	/** Takes keyboard focus once the widget is on screen */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handles up/down to move the highlight and Enter to pick the race */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Highlights the race under the mouse */
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Picks the race clicked on */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Called whenever the highlighted race changes. RaceIndex counts from 1, or is 0 when nothing is highlighted */
	UFUNCTION(BlueprintImplementableEvent, Category="Race Select", meta = (DisplayName = "Highlight Changed"))
	void BP_HighlightChanged(int32 RaceIndex);

	/** Builds the race list on the left and the space for the map on the right */
	void CreateLayout();

	/** Adds a row to the race list for a race (numbered from 1) */
	void AddRaceRow(int32 RaceIndex);

	/** Makes a text widget for the layout */
	UTextBlock* CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** Colors the highlighted row and writes its records */
	void RefreshHighlight();

	/** Writes the selected time of day */
	void RefreshTimeOfDay();

	/** Returns the race whose row is under a screen position, counting from 1, or 0 */
	int32 FindRaceUnder(const FVector2D& ScreenPosition) const;

	/** Routes keyboard/gamepad input to this widget */
	void ClaimKeyboardFocus();

	/** Returns the Time Trial game mode, which holds the race list */
	ATimeTrialGameMode* GetTimeTrialGameMode() const;

	/** One row of the race list */
	struct FRaceRow
	{
		TWeakObjectPtr<UBorder> Bar;
		TWeakObjectPtr<UTextBlock> Name;
		TWeakObjectPtr<UTextBlock> Details;
	};

	/** The rows of the race list. Entry 0 is race 1 */
	TArray<FRaceRow> RaceRows;

	/** The list the rows go in */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RaceList;

	/** Bar behind the time of day, which is clicked to change it */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TimeOfDayBar;

	/** Shows the selected time of day */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeOfDayText;

	/** Shows the highlighted race's records */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecordText;

	/** The space kept for a map of the course */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> MinimapArea;

	/** Heading of the panel */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText TitleLabel = NSLOCTEXT("TimeTrialRaceSelectUI", "Title", "COURSE SELECT");

	/** Name shown for a race that has none. {0} is the race's number */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText UnnamedRaceFormat = NSLOCTEXT("TimeTrialRaceSelectUI", "UnnamedRace", "コース{0}");

	/** Line under a race's name. {0} is the number of laps */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText LapsFormat = NSLOCTEXT("TimeTrialRaceSelectUI", "Laps", "{0} LAPS");

	/** Added to the line under the name of a race that runs its route the other way round */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText ReversedLabel = NSLOCTEXT("TimeTrialRaceSelectUI", "Reversed", "逆走");

	/** The time of day line. {0} is the time of day's name */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText TimeOfDayFormat = NSLOCTEXT("TimeTrialRaceSelectUI", "TimeOfDay", "時間帯　◀ {0} ▶");

	/** Records of the highlighted race at the selected time of day. {0} is the best total time, {1} the car it was set in, {2} the best lap and
	 *  {3} its car */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText RecordFormat = NSLOCTEXT("TimeTrialRaceSelectUI", "Record", "COURSE RECORD   {0}   {1}\nLAP RECORD      {2}   {3}");

	/** Controls hint under the panel */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FText HintLabel = NSLOCTEXT("TimeTrialRaceSelectUI", "Hint", "↑ ↓ でコース　← → で時間帯　Enter で決定　Backspace で戻る");

	/** Font of the heading and the hint */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FSlateFontInfo SmallFont;

	/** Font of the race names */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FSlateFontInfo RaceNameFont;

	/** Font of the line under each race's name */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FSlateFontInfo DetailsFont;

	/** Font of the records */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FSlateFontInfo RecordFont;

	/** Width of the race rows */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	float RaceRowWidth = 560.0f;

	/** Width and height of the space kept for the map */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FVector2D MinimapSize = FVector2D(600.0f, 600.0f);

	/** Gap between the panels and the left and right edges of the screen */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	float ScreenEdgeOffset = 96.0f;

	/** Background of the panel */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** Bar behind the highlighted race */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FLinearColor HighlightColor = FLinearColor(1.0f, 0.32f, 0.02f, 0.92f);

	/** Bar behind the other races */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FLinearColor ItemColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	/** Text color of the highlighted race */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FLinearColor HighlightTextColor = FLinearColor::White;

	/** Text color of the other races */
	UPROPERTY(EditAnywhere, Category="Race Select|Layout")
	FLinearColor ItemTextColor = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);

	/** Number of the highlighted race, counting from 1, or 0 if nothing is highlighted */
	int32 HighlightedRace = 0;

	/** True once a race has been picked, so a stray second confirm can't fire the delegate twice */
	bool bRaceChosen = false;

	/** True once the widget has taken keyboard focus */
	bool bFocusClaimed = false;
};
