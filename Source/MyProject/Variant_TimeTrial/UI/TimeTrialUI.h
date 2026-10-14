// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/TextLayout.h"
#include "TimeTrialRecords.h"
#include "TimeTrialUI.generated.h"

class UTimeTrialStartUI;
class UTimeTrialRaceSelectUI;
class UTitleScreenUI;
class UCarSelectUI;
class URaceResultUI;
class UTimeTrialLapRowUI;
class UVerticalBox;
class UTextBlock;
class UTexture2D;
class UWidget;
struct FSlateBrush;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStartRaceDelegate);

/**
 *  Simple UI for a Time Trial racing game
 *  Keeps track of lap number and best time
 *  Spawns a sub-widget to do the initial countdown
 */
UCLASS(abstract)
class UTimeTrialUI : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Type of start countdown UI widget to spawn */
	UPROPERTY(EditAnywhere, Category="Start Countdown")
	TSubclassOf<UTimeTrialStartUI> StartUIClass;

	/** Type of race select UI widget to spawn before the countdown begins */
	UPROPERTY(EditAnywhere, Category="Race Select")
	TSubclassOf<UTimeTrialRaceSelectUI> RaceSelectUIClass;

	/** Type of title screen UI widget to spawn when the level first loads, if any */
	UPROPERTY(EditAnywhere, Category="Title Screen")
	TSubclassOf<UTitleScreenUI> TitleScreenUIClass;

	/** Type of car select UI widget to spawn after the title screen, if any */
	UPROPERTY(EditAnywhere, Category="Car Select")
	TSubclassOf<UCarSelectUI> CarSelectUIClass;

	/** Type of results screen shown once the player has run all the laps */
	UPROPERTY(EditAnywhere, Category="Race Result")
	TSubclassOf<URaceResultUI> ResultUIClass;

	/** Time between crossing the finish line for the last time and the results coming up */
	UPROPERTY(EditAnywhere, Category="Race Result", meta = (Units = "s", ClampMin = "0.0"))
	float ResultDelay = 3.0f;

	/** Type of row widget added to LapList for each lap. The native class builds its own text,
	 *  so a Blueprint subclass is only needed to restyle the rows in the designer. */
	UPROPERTY(EditAnywhere, Category="Race HUD")
	TSubclassOf<UTimeTrialLapRowUI> LapRowClass;

	/** If true, LapList, PositionText and BestLapText are created in code when the designer
	 *  doesn't have widgets with those names: the lap list in the top left corner,
	 *  the race position with the best lap under it in the top right */
	UPROPERTY(EditAnywhere, Category="Race HUD")
	bool bCreateMissingHudWidgets = true;

	/** Distance of the generated HUD widgets from the top corners of the screen */
	UPROPERTY(EditAnywhere, Category="Race HUD", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FVector2D HudCornerOffset = FVector2D(48.0f, 40.0f);

	/** Font of the generated lap list rows */
	UPROPERTY(EditAnywhere, Category="Race HUD", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FSlateFontInfo LapListFont;

	/** Font of the generated race position text */
	UPROPERTY(EditAnywhere, Category="Race HUD", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FSlateFontInfo PositionFont;

	/** Font of the generated best lap text */
	UPROPERTY(EditAnywhere, Category="Race HUD", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FSlateFontInfo BestLapFont;

	/** Bars drawn behind the generated lap rows, starting with lap 1. The artwork carries the lap label,
	 *  so a row with a bar only shows the time. Laps past the end of the list get plain text rows. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	TArray<TObjectPtr<UTexture2D>> LapBarTextures;

	/** Bar drawn behind the generated best lap text. Leave empty for plain text. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	TObjectPtr<UTexture2D> BestLapBarTexture;

	/** Bar drawn behind the generated race position text. Leave empty for plain text. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	TObjectPtr<UTexture2D> PositionBarTexture;

	/** Size the bars are drawn at relative to their textures. 0.5 draws @2x artwork at its 1080p size. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets", ClampMin = "0.01"))
	float BarImageScale = 0.5f;

	/** Gap between stacked bars. Negative values overlap the transparent margins around the artwork. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	float BarSpacing = -28.0f;

	/** Padding of the time inside a lap or best lap bar. The time is right aligned. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FMargin BarTimePadding = FMargin(0.0f, 0.0f, 40.0f, 0.0f);

	/** Padding of the race position inside its bar. The text is right aligned. */
	UPROPERTY(EditAnywhere, Category="Race HUD|Bars", meta = (EditCondition = "bCreateMissingHudWidgets"))
	FMargin PositionBarPadding = FMargin(0.0f, 0.0f, 40.0f, 0.0f);

	/** Format for PositionText. {0} is the race position and {1} the number of racers. */
	UPROPERTY(EditAnywhere, Category="Race HUD")
	FText PositionFormat = NSLOCTEXT("TimeTrialUI", "PositionFormat", "P {0}/{1}");

	/** Format for BestLapText. {0} is the best lap time. */
	UPROPERTY(EditAnywhere, Category="Race HUD")
	FText BestLapFormat = NSLOCTEXT("TimeTrialUI", "BestLapFormat", "BEST {0}");

	/** List that gets one LapRowClass row per lap. Created in code if the designer doesn't have one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> LapList;

	/** Text that shows the race position, formatted with PositionFormat. Created in code if the designer doesn't have one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PositionText;

	/** Text that shows the best lap time, or dashes until a lap has been completed. Created in code if the designer doesn't have one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BestLapText;

	/** Rows currently in LapList. Entry 0 is lap 1. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTimeTrialLapRowUI>> LapRows;

	/** Time when the previous lap started, in seconds */
	float LastLapTime = 0.0f;

	/** Best lap time, in seconds. Negative until a full lap has been completed. */
	float BestLapTime = -1.0f;

	/** Game time when this lap started */
	float LapStartTime = 0.0f;

	/** Game time when the race started */
	float RaceStartTime = 0.0f;

	/** Duration of the most recently completed lap, in seconds */
	float LastCompletedLapTime = 0.0f;

	/** Lap time the timer was frozen at when the race finished */
	float FinalLapTime = 0.0f;

	/** Total race time recorded when the race finished */
	float FinalRaceTime = 0.0f;

	/** Current lap number */
	int32 CurrentLap = 0;

	/** Duration of every completed lap, in the order they were run */
	TArray<float> LapTimes;

	/** Where the player currently sits in the race. There are no opponents yet, so this stays at 1
	 *  until something calls SetRacePosition */
	int32 RacePosition = 1;

	/** How many racers are on track, the player included */
	int32 TotalRacers = 1;

	/** True once all laps have been completed */
	bool bRaceFinished = false;

	/** What the finished race did to the course records, for the results */
	FRaceRecordUpdate RecordUpdate;

	/** True for the second player's HUD in a two player game */
	bool bSecondaryPlayer = false;

public:

	UTimeTrialUI(const FObjectInitializer& ObjectInitializer);

	/** Delegate to broadcast when the race starts */
	FStartRaceDelegate OnRaceStart;

protected:

	/** Creates any HUD widgets the designer left out */
	virtual void NativeOnInitialized() override;

	/** Widget initialization */
	virtual void NativeConstruct() override;

	/** Builds LapList, PositionText and BestLapText if they aren't in the widget tree */
	void CreateMissingHudWidgets();

	/** Adds a generated widget to the root panel, pinned to the top left or top right corner */
	void AddToHudCorner(UWidget* Widget, bool bRightCorner);

	/** Makes a text widget for the generated HUD */
	UTextBlock* CreateHudText(FName Name, const FSlateFontInfo& Font, ETextJustify::Type Justification);

	/** Puts a generated text on top of a bar, right aligned. Returns the text itself if there's no bar texture. */
	UWidget* CreateHudBar(FName Name, UTextBlock* Text, UTexture2D* Texture, const FMargin& TextPadding);

	/** Keeps the time of the lap being driven ticking on the lap list */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Rebuilds the lap rows if the lap count changed, then refills every row and the best lap text */
	void RefreshLapList();

	/** Writes the race position into PositionText */
	void RefreshPositionText();

	/** Returns the row of the lap being driven, or nullptr if no lap is running */
	UTimeTrialLapRowUI* GetCurrentLapRow() const;

public:

	/** Increments the lap and updates the lap counter */
	void UpdateLapCount(int32 Lap, float NewLapStartTime);

	/** Freezes the lap timer at the final lap time and notifies Blueprint that the race is over.
	 *  InRecordUpdate is what the race did to the course records, shown with the results */
	void FinishRace(const FRaceRecordUpdate& InRecordUpdate);

	/** Runs the race again: takes down the results or the countdown under way, clears the laps and starts the countdown */
	void RetryRace();

	/** Holds the countdown where it is while the game is paused, or lets it run on */
	void SetCountdownPaused(bool bPaused);

	/** Makes this the second player's HUD in a two player game: just the laps and the place, as the first player's
	 *  HUD runs the screens before and after the race for both. Call before adding it to the screen */
	void SetSecondaryPlayer(bool bInSecondaryPlayer) { bSecondaryPlayer = bInSecondaryPlayer; };

	/** Brings the results up a moment from now if every player has finished. On the first player's HUD */
	void ShowResultsOnceAllFinished();

	/** Tells the HUD which race route is being run */
	void NotifyRaceStarted(int32 RaceIndex, const FLinearColor& RaceColor);

	/** Allows Blueprint control to update the lap tracker widgets */
	UFUNCTION(BlueprintImplementableEvent, Category="Time Trial", meta = (DisplayName = "Update Laps"))
	void BP_UpdateLaps();

	/** Called once the race has been completed. Use it to show results; the lap timer is frozen at the final lap time. */
	UFUNCTION(BlueprintImplementableEvent, Category="Time Trial", meta = (DisplayName = "Race Finished"))
	void BP_RaceFinished();

	/** Called when the race starts, with the selected route and its display color */
	UFUNCTION(BlueprintImplementableEvent, Category="Time Trial", meta = (DisplayName = "Race Started"))
	void BP_RaceStarted(int32 RaceIndex, FLinearColor RaceColor);

	/** Called whenever the race position changes. Use it to refresh the position readout */
	UFUNCTION(BlueprintImplementableEvent, Category="Time Trial", meta = (DisplayName = "Race Position Changed"))
	void BP_RacePositionChanged(int32 NewPosition, int32 NewTotalRacers);

protected:

	/** Called from the countdown delegate to start the race */
	UFUNCTION()
	void StartRace();

	/** Called once the player has chosen a race route. Removes the race select widget and begins the countdown. */
	UFUNCTION()
	void OnRaceChosen();

	/** Called once the player presses Start on the title screen. Fades the camera back to the
	 *  vehicle and moves on to car select (or race select, if no car select is configured). */
	UFUNCTION()
	void OnTitleStartPressed();

	/** Brings the race HUD back and shows the next screen, once the fade from the title is done */
	void FinishTitleSequence();

	/** Visibility of this HUD before the title screen hid it */
	ESlateVisibility VisibilityBeforeTitle = ESlateVisibility::SelfHitTestInvisible;

	/** Called once the player has chosen a car. Removes the car select widget and moves on to race select. */
	UFUNCTION()
	void OnCarSelected();

	/** Called when the player backs out of the car select. Goes back to the title screen, on the mode menu */
	UFUNCTION()
	void OnCarSelectBack();

	/** Called when the player backs out of the race select. Goes back to the car select */
	UFUNCTION()
	void OnRaceSelectBack();

	/** Puts the title screen up. With bAtModeChoice it opens on the menu the race mode was picked from */
	void ShowTitleScreen(bool bAtModeChoice);

	/** Takes the screens before the race down and goes back to the title flythrough and its screen */
	void BackToTitleScreen();

	/** Spawns and starts the race start countdown widget */
	void BeginCountdown();

	/** Puts up the results screen and takes the controls off the car */
	void ShowResults();

	/** Shows the car select widget, or moves on to race select directly if none is configured */
	void ShowCarSelectOrNext();

	/** Shows the race select widget, or begins the countdown directly if none is configured */
	void ShowRaceSelectOrBeginCountdown();

	/** Pointer to the currently displayed race select widget, if any */
	UPROPERTY()
	TObjectPtr<UTimeTrialRaceSelectUI> RaceSelectWidget;

	/** Pointer to the currently displayed title screen widget, if any */
	UPROPERTY()
	TObjectPtr<UTitleScreenUI> TitleScreenWidget;

	/** Pointer to the currently displayed car select widget, if any */
	UPROPERTY()
	TObjectPtr<UCarSelectUI> CarSelectWidget;

	/** The second player's car select, on their half of the screen, in a two player game */
	UPROPERTY()
	TObjectPtr<UCarSelectUI> SecondCarSelectWidget;

	/** Takes both players' car selects down */
	void RemoveCarSelects();

	/** Pointer to the race start countdown widget. It takes itself off the screen once it's done */
	UPROPERTY()
	TObjectPtr<UTimeTrialStartUI> CountdownWidget;

	/** Pointer to the results screen, once it's up */
	UPROPERTY()
	TObjectPtr<URaceResultUI> ResultWidget;

	/** Brings up the results a moment after the finish line */
	FTimerHandle ResultTimer;

	/** Gets the current lap number */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetCurrentLap() const { return CurrentLap; };

	/** Gets the number of laps in the selected race */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetTotalLaps() const;

	/** Gets the best lap time saved. Negative until a full lap has been completed. */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	float GetBestLapTime() const { return BestLapTime; };

	/** Gets the game time at which the current lap started */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	float GetLapStartTime() const { return LapStartTime; };

	/** Time elapsed in the current lap, in seconds. Zero before the race starts, and frozen at the final lap time once it's finished. */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	double GetCurrentLapTime() const;

	/** Total time since the race started, in seconds. Frozen once the race is finished. */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	double GetTotalRaceTime() const;

	/** Duration of the most recently completed lap, in seconds */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	float GetLastLapTime() const { return LastCompletedLapTime; };

	/** True once the race has started */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	bool HasRaceStarted() const { return CurrentLap > 0; };

	/** True once all laps have been completed */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	bool IsRaceFinished() const { return bRaceFinished; };

public:

	/** Durations of every completed lap, in order. Empty until the first lap is finished */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	TArray<float> GetLapTimes() const { return LapTimes; };

	/** Duration of a single lap, numbered from 1. Negative if that lap hasn't been completed */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	float GetLapTime(int32 LapNumber) const;

	/** Number of the fastest lap so far, counting from 1. Zero until a lap has been completed */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetBestLapNumber() const;

	/** Sets where the player sits in the race. Nothing calls this yet: wire it up once opponents exist */
	UFUNCTION(BlueprintCallable, Category="Time Trial")
	void SetRacePosition(int32 NewPosition, int32 NewTotalRacers);

	/** Where the player currently sits in the race, counting from 1 */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetRacePosition() const { return RacePosition; };

	/** How many racers are on track, the player included */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	int32 GetTotalRacers() const { return TotalRacers; };

	/** Formats a time in seconds as m:ss.mmm, or as dashes if the time is negative */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	static FText FormatLapTime(float TimeInSeconds);

	/** Makes a brush that draws a bar texture at Scale times its size */
	static FSlateBrush MakeBarBrush(UTexture2D* Texture, float Scale);
};
