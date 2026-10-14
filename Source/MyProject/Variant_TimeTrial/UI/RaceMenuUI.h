// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "RaceMenuUI.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;
class UWidget;

/** What picking an item of a race menu does */
UENUM()
enum class ERaceMenuAction : uint8
{
	/** Closes the pause menu and carries on with the race */
	Resume,

	/** Runs the same race again from the countdown */
	Retry,

	/** Fades out and goes back to the title screen */
	ReturnToTitle
};

/** One item of a race menu */
USTRUCT()
struct FRaceMenuItem
{
	GENERATED_BODY()

	/** Bar behind the label, lit up while highlighted. Also the area the mouse picks the item with */
	UPROPERTY()
	TObjectPtr<UBorder> Bar;

	/** The item's label */
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	/** What picking the item does */
	ERaceMenuAction Action = ERaceMenuAction::Resume;
};

/**
 *  Base for the menus that come up over a race: the pause menu and the results screen. Builds a panel in the middle
 *  of the screen with the items drawn as bars, as the title screen draws them, one of them highlighted.
 *  Up/down or left/right (W/S/A/D, d-pad) move the highlight and Enter (Space, gamepad accept) picks it. The mouse works too.
 *  Picking an item hands its action to the player controller; only the first pick counts.
 *
 *  Subclasses build their layout in code, adding the items with CreateMenuItem.
 */
UCLASS(abstract)
class URaceMenuUI : public UUserWidget
{
	GENERATED_BODY()

public:

	URaceMenuUI(const FObjectInitializer& ObjectInitializer);

protected:

	/** Highlights the first item */
	virtual void NativeConstruct() override;

	/** Hands the input back to the game */
	virtual void NativeDestruct() override;

	/** Takes keyboard focus on the first tick */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handles menu navigation from the keyboard and gamepad */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Highlights the item under the mouse */
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Picks the item under the mouse */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Called on Backspace (Escape, gamepad back). Does nothing unless a subclass says otherwise */
	virtual void OnBack() {};

	/** Makes the root of the layout: a shade over the whole screen with a panel in the middle. Returns the panel's
	 *  content, top to bottom */
	UVerticalBox* CreatePanelLayout();

	/** Makes an item, to be added to the layout by the caller */
	UWidget* CreateMenuItem(const FText& Label, ERaceMenuAction Action);

	/** Makes a text widget for the generated layout */
	UTextBlock* CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color);

	/** Hands an action to the player controller. Ignored once something has been picked */
	void ChooseAction(ERaceMenuAction Action);

	/** Moves the highlight by Delta items, wrapping around the ends */
	void MoveHighlight(int32 Delta);

	/** Highlights an item (index into Items) */
	void HighlightItem(int32 ItemIndex);

	/** Returns the item under a screen position, or INDEX_NONE */
	int32 FindItemUnder(const FVector2D& ScreenPosition) const;

	/** Routes keyboard/gamepad input to this widget. Done from the tick rather than a timer, because timers stand
	 *  still while the game is paused */
	void ClaimKeyboardFocus();

	/** Every item, in the order they were made */
	UPROPERTY(Transient)
	TArray<FRaceMenuItem> Items;

	/** Font of the item labels */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FSlateFontInfo MenuFont;

	/** Width of the items */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	float MenuItemWidth = 440.0f;

	/** Shade over the whole screen behind the panel */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor ShadeColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.35f);

	/** Background of the panel */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor PanelColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);

	/** Bar behind the highlighted item */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor HighlightColor = FLinearColor(1.0f, 0.32f, 0.02f, 0.92f);

	/** Bar behind the other items */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor ItemColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);

	/** Label color of the highlighted item */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor HighlightTextColor = FLinearColor::White;

	/** Label color of the other items */
	UPROPERTY(EditAnywhere, Category="Race Menu|Layout")
	FLinearColor ItemTextColor = FLinearColor(0.78f, 0.78f, 0.78f, 1.0f);

	/** Index into Items of the highlighted item */
	int32 HighlightedItem = INDEX_NONE;

	/** True once an action has been picked, so a stray second confirm can't fire another */
	bool bActionChosen = false;

	/** True once the widget has taken keyboard focus */
	bool bFocusClaimed = false;
};
