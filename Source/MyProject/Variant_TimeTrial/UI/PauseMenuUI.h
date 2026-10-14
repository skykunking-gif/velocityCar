// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RaceMenuUI.h"
#include "PauseMenuUI.generated.h"

/**
 *  The menu over a paused race: carry on, run the race again from the countdown, or go back to the title screen.
 *  Backspace (Escape, gamepad back) carries on as well.
 *
 *  The whole layout is built in code.
 */
UCLASS()
class UPauseMenuUI : public URaceMenuUI
{
	GENERATED_BODY()

public:

	UPauseMenuUI(const FObjectInitializer& ObjectInitializer);

protected:

	/** Builds the layout */
	virtual void NativeOnInitialized() override;

	/** Carries on with the race */
	virtual void OnBack() override;

	/** Builds the heading, the items and the hint in a panel in the middle of the screen */
	void CreateLayout();

	/** Heading of the panel */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FText TitleLabel = NSLOCTEXT("PauseMenuUI", "Title", "PAUSE");

	/** Label of the item that carries on with the race */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FText ResumeLabel = NSLOCTEXT("PauseMenuUI", "Resume", "再開");

	/** Label of the item that runs the race again */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FText RetryLabel = NSLOCTEXT("PauseMenuUI", "Retry", "リトライ");

	/** Label of the item back to the title screen */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FText ReturnLabel = NSLOCTEXT("PauseMenuUI", "Return", "タイトルへ戻る");

	/** Controls hint under the items */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FText HintLabel = NSLOCTEXT("PauseMenuUI", "Hint", "↑ ↓ で選択　Enter で決定　Esc で再開");

	/** Font of the heading */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FSlateFontInfo TitleFont;

	/** Font of the hint */
	UPROPERTY(EditAnywhere, Category="Pause Menu|Layout")
	FSlateFontInfo SmallFont;
};
