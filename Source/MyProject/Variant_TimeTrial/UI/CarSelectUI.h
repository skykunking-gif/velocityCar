// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "CarSelectUI.generated.h"

class UButton;
class UTextBlock;
class ACameraActor;
class ATimeTrialGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCarChosenDelegate);

/**
 *  The car select screen shown before the race.
 *  Offers the cars listed on the Time Trial game mode (CarChoices). Moving the highlight puts the player's
 *  vehicle in that car straight away, so the car on screen doubles as the preview, and a camera circles it
 *  while the screen is up. Left/right (A/D, d-pad) change the car, up/down (W/S) switch between the automatic and the
 *  manual gearbox, and Enter (Space, gamepad accept) confirms it.
 *
 *  Works as-is from C++: CarNameText, CarIndexText, CarDescriptionText, PrevButton, NextButton, AutomaticButton,
 *  ManualButton, TransmissionNoteText and ConfirmButton are built in code unless a Blueprint subclass places its own
 *  widgets with those names.
 */
UCLASS()
class UCarSelectUI : public UUserWidget
{
	GENERATED_BODY()

public:

	UCarSelectUI(const FObjectInitializer& ObjectInitializer);

	/** Commits to a car (0-based index into the game mode's car choices) and moves on to race select */
	UFUNCTION(BlueprintCallable, Category="Car Select")
	void ChooseCar(int32 CarIndex);

	/** Shows a car (0-based) without committing to it. The player's vehicle switches to it for the preview. */
	UFUNCTION(BlueprintCallable, Category="Car Select")
	void HighlightCar(int32 CarIndex);

	/** Moves the highlight by Delta cars, wrapping around the ends of the list */
	UFUNCTION(BlueprintCallable, Category="Car Select")
	void MoveHighlight(int32 Delta);

	/** Commits to the highlighted car */
	UFUNCTION(BlueprintCallable, Category="Car Select")
	void ConfirmHighlightedCar();

	/** Returns the 0-based index of the highlighted car */
	UFUNCTION(BlueprintPure, Category="Car Select")
	int32 GetHighlightedCar() const { return HighlightedCar; };

	/** Returns how many cars the game mode offers */
	UFUNCTION(BlueprintPure, Category="Car Select")
	int32 GetCarCount() const;

	/** Returns the name shown for a car (0-based) */
	UFUNCTION(BlueprintPure, Category="Car Select")
	FText GetCarName(int32 CarIndex) const;

	/** Returns the description of a car (0-based), or empty text if it has none */
	UFUNCTION(BlueprintPure, Category="Car Select")
	FText GetCarDescription(int32 CarIndex) const;

	/** Picks the automatic (AT) or the manual (MT) gearbox. The choice is saved and the car on screen gets it at once */
	UFUNCTION(BlueprintCallable, Category="Car Select")
	void SetAutomaticTransmission(bool bAutomatic);

	/** Makes this one of the two car selects of a two player game, for the first (0) or the second (1) player. Each
	 *  player picks on their own half of the screen; picking only says they're ready, and they can take it back until
	 *  the other is ready too. Call before adding it to the screen */
	void SetTwoPlayerSelect(int32 InPlayerIndex);

	/** Returns true once the player has picked their car */
	bool IsCarChosen() const { return bCarChosen; };

	/** Broadcast once a car has been chosen */
	FOnCarChosenDelegate OnCarChosen;

	/** Broadcast when the player backs out to the title screen without choosing a car */
	FOnCarChosenDelegate OnBack;

protected:

	/** Builds the layout if the designer didn't, and hooks up the buttons */
	virtual void NativeOnInitialized() override;

	/** Highlights the car the game mode has selected and starts the preview camera */
	virtual void NativeConstruct() override;

	/** Hands the camera and the input back to the game */
	virtual void NativeDestruct() override;

	/** Circles the preview camera around the car */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handles left/right to change the car and Enter to confirm it */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Called whenever the highlighted car changes, so Blueprint can drive its own visuals */
	UFUNCTION(BlueprintImplementableEvent, Category="Car Select", meta = (DisplayName = "Highlight Changed"))
	void BP_HighlightChanged(int32 CarIndex, const FText& CarName);

	/** Builds the car select panel at the bottom of the screen */
	void CreateLayout();

	/** Makes a button with a text label for the generated layout */
	UButton* CreateButton(FName Name, const FText& Label, const FSlateFontInfo& Font);

	/** Makes a text widget for the generated layout */
	UTextBlock* CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** Writes the highlighted car's name and position in the list */
	void RefreshCarText();

	/** Lights up the button of the gearbox picked and explains how it shifts */
	void RefreshTransmission();

	/** Writes the heading and the controls hint for the player and whether they've picked yet */
	void RefreshPrompts();

	/** Takes back a two player pick, so the player can choose again */
	void CancelChoice();

	/** Routes keyboard/gamepad input to this widget. Deferred to the tick after Construct,
	 *  because the widget isn't in the Slate hierarchy yet while it's being constructed. */
	void ClaimKeyboardFocus();

	/** Spawns the camera that circles the car and cuts to it */
	void StartPreviewCamera();

	/** Places the preview camera on its circle around the car */
	void UpdatePreviewCamera();

	/** Blends back to the car's own camera and gets rid of the preview camera */
	void StopPreviewCamera();

	/** Returns the Time Trial game mode, which holds the car list */
	ATimeTrialGameMode* GetTimeTrialGameMode() const;

	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleAutomaticClicked();

	UFUNCTION()
	void HandleManualClicked();

	/** Shows the highlighted car's name */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CarNameText;

	/** Heading of the generated panel */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	/** Controls hint under the generated panel */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintText;

	/** Shows where the highlighted car sits in the list, formatted with CarIndexFormat */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CarIndexText;

	/** Shows the highlighted car's description, formatted with DescriptionFormat. Hidden for a car without one */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CarDescriptionText;

	/** Panel behind the generated description, hidden along with it */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> DescriptionPanel;

	/** Moves to the previous car */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PrevButton;

	/** Moves to the next car */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NextButton;

	/** Commits to the highlighted car */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	/** Picks the automatic gearbox */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AutomaticButton;

	/** Picks the manual gearbox */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ManualButton;

	/** Says how the picked gearbox shifts */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TransmissionNoteText;

	/** Heading of the generated panel */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText TitleLabel = NSLOCTEXT("CarSelectUI", "Title", "CAR SELECT");

	/** Label of the generated confirm button */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText ConfirmLabel = NSLOCTEXT("CarSelectUI", "Confirm", "決定");

	/** Controls hint under the generated panel */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText HintLabel = NSLOCTEXT("CarSelectUI", "Hint", "← → で車を選択　↑ ↓ で AT / MT　Enter で決定　Backspace で戻る");

	/** Heading in a two player game. {0} is the player's number */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText TwoPlayerTitleFormat = NSLOCTEXT("CarSelectUI", "TwoPlayerTitle", "{0}P  CAR SELECT");

	/** Hint for the first player while the second plays on the right hand side of the keyboard */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText FirstPlayerKeyboardHint = NSLOCTEXT("CarSelectUI", "FirstPlayerKeyboardHint", "A D で車を選択　W S で AT / MT　Space で決定　Backspace で戻る");

	/** Hint for the second player on the right hand side of the keyboard */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText SecondPlayerKeyboardHint = NSLOCTEXT("CarSelectUI", "SecondPlayerKeyboardHint", "← → で車を選択　↑ ↓ で AT / MT　Enter で決定");

	/** Hint for the second player on a gamepad */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText SecondPlayerGamepadHint = NSLOCTEXT("CarSelectUI", "SecondPlayerGamepadHint", "左右で車を選択　上下で AT / MT　A で決定");

	/** Shown once a player has picked, while the other is still choosing. {0} is the key that takes it back */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText ReadyFormat = NSLOCTEXT("CarSelectUI", "Ready", "準備OK！ 相手を待っています（{0} で取り消し）");

	/** How the manual gearbox shifts for the second player on the right hand side of the keyboard */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText SecondPlayerKeyboardManualNote = NSLOCTEXT("CarSelectUI", "SecondPlayerKeyboardManualNote", "右Shift でシフトアップ　右Ctrl でシフトダウン");

	/** How the manual gearbox shifts for the second player on a gamepad */
	UPROPERTY(EditAnywhere, Category="Car Select|Two Players")
	FText SecondPlayerGamepadManualNote = NSLOCTEXT("CarSelectUI", "SecondPlayerGamepadManualNote", "X でシフトアップ　B でシフトダウン");

	/** Label in front of the gearbox buttons */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText TransmissionLabel = NSLOCTEXT("CarSelectUI", "Transmission", "ミッション");

	/** Label of the automatic gearbox button */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText AutomaticLabel = NSLOCTEXT("CarSelectUI", "Automatic", "AT");

	/** Label of the manual gearbox button */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText ManualLabel = NSLOCTEXT("CarSelectUI", "Manual", "MT");

	/** How the automatic gearbox shifts */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText AutomaticNote = NSLOCTEXT("CarSelectUI", "AutomaticNote", "ギアは自動で切り替わります");

	/** How the manual gearbox shifts */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText ManualNote = NSLOCTEXT("CarSelectUI", "ManualNote", "Q でシフトアップ　E でシフトダウン（パッドは X / B）");

	/** Button color of the gearbox picked */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FLinearColor PickedButtonColor = FLinearColor(1.0f, 0.32f, 0.02f, 1.0f);

	/** Label color on the button of the gearbox picked */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FLinearColor PickedButtonTextColor = FLinearColor::White;

	/** Width of each gearbox button */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	float TransmissionButtonWidth = 96.0f;

	/** Format for CarDescriptionText. {0} is the car's name and {1} its description. */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText DescriptionFormat = NSLOCTEXT("CarSelectUI", "DescriptionFormat", "{0} ー {1}");

	/** Format for CarIndexText. {0} is the car's number and {1} how many cars there are. */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FText CarIndexFormat = NSLOCTEXT("CarSelectUI", "IndexFormat", "{0} / {1}");

	/** Font of the generated heading */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FSlateFontInfo TitleFont;

	/** Font of the generated car name */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FSlateFontInfo CarNameFont;

	/** Font of the generated buttons, car number and hint */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FSlateFontInfo SmallFont;

	/** Font of the generated description */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FSlateFontInfo DescriptionFont;

	/** Width of the description text in the lower left. Longer lines wrap. The panel has to end short of the car select panel in the middle */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	float DescriptionWidth = 510.0f;

	/** Gap between the generated description panel and the left edge of the screen */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	float DescriptionLeftOffset = 28.0f;

	/** Background color of the generated panel */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	/** Gap between the generated panel and the bottom of the screen */
	UPROPERTY(EditAnywhere, Category="Car Select|Layout")
	float PanelBottomOffset = 56.0f;

	/** If true, a camera circles the car while the screen is up */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera")
	bool bUsePreviewCamera = true;

	/** Horizontal distance from the car to the preview camera */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "cm"))
	float PreviewCameraDistance = 620.0f;

	/** Height of the preview camera above the point it looks at */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "cm"))
	float PreviewCameraHeight = 170.0f;

	/** Height above the car's origin the preview camera looks at */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "cm"))
	float PreviewLookAtHeight = 60.0f;

	/** Where the preview camera starts, in degrees around the car from straight ahead of it */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "deg"))
	float PreviewCameraStartAngle = 40.0f;

	/** How fast the preview camera circles the car. Zero keeps it still. */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "deg/s"))
	float PreviewCameraOrbitSpeed = 12.0f;

	/** Time to blend to and from the preview camera */
	UPROPERTY(EditAnywhere, Category="Car Select|Camera", meta = (Units = "s"))
	float PreviewCameraBlendTime = 0.6f;

	/** The camera circling the car while the screen is up */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> PreviewCamera;

	/** Current position of the preview camera around the car, in degrees from straight ahead */
	float PreviewAngle = 0.0f;

	/** 0-based index of the highlighted car */
	int32 HighlightedCar = 0;

	/** True once a car has been picked, so a stray second confirm can't fire the delegate twice */
	bool bCarChosen = false;

	/** True for one of the two car selects of a two player game */
	bool bTwoPlayerSelect = false;

	/** Which local player picks here: 0 for the first, 1 for the second */
	int32 PlayerIndex = 0;
};
