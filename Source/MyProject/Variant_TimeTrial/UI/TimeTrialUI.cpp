// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialUI.h"
#include "TimeTrialStartUI.h"
#include "TimeTrialRaceSelectUI.h"
#include "TitleScreenUI.h"
#include "CarSelectUI.h"
#include "RaceResultUI.h"
#include "TimeTrialLapRowUI.h"
#include "TimeTrialPlayerController.h"
#include "TimeTrialGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MyProject.h"

UTimeTrialUI::UTimeTrialUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the native row builds its own text, so the lap list works without a row Blueprint
	LapRowClass = UTimeTrialLapRowUI::StaticClass();

	// likewise the results screen builds its own layout
	ResultUIClass = URaceResultUI::StaticClass();

	// same default font as a Text widget dropped in the designer
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		LapListFont = FSlateFontInfo(RobotoFontObj.Object, 22, FName("Bold"));
		PositionFont = FSlateFontInfo(RobotoFontObj.Object, 40, FName("Bold"));
		BestLapFont = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
}

void UTimeTrialUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (bCreateMissingHudWidgets)
	{
		CreateMissingHudWidgets();
	}
}

void UTimeTrialUI::CreateMissingHudWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	// top left: one row per lap
	if (!LapList)
	{
		LapList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LapList"));
		AddToHudCorner(LapList, false);
	}

	// top right: race position, with the best lap under it
	if (!PositionText || !BestLapText)
	{
		UVerticalBox* RaceInfo = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RaceInfo"));
		AddToHudCorner(RaceInfo, true);

		if (!PositionText)
		{
			PositionText = CreateHudText(TEXT("PositionText"), PositionFont, ETextJustify::Right);
			RaceInfo->AddChildToVerticalBox(CreateHudBar(TEXT("PositionBar"), PositionText, PositionBarTexture, PositionBarPadding))->SetHorizontalAlignment(HAlign_Right);
		}

		if (!BestLapText)
		{
			// stack the best lap bar under the position bar the way the lap rows stack
			const bool bStackOnPositionBar = RaceInfo->HasAnyChildren() && PositionBarTexture && BestLapBarTexture;

			BestLapText = CreateHudText(TEXT("BestLapText"), BestLapFont, ETextJustify::Right);
			UVerticalBoxSlot* BestSlot = RaceInfo->AddChildToVerticalBox(CreateHudBar(TEXT("BestLapBar"), BestLapText, BestLapBarTexture, BarTimePadding));
			BestSlot->SetHorizontalAlignment(HAlign_Right);

			if (bStackOnPositionBar)
			{
				BestSlot->SetPadding(FMargin(0.0f, BarSpacing, 0.0f, 0.0f));
			}
		}
	}
}

void UTimeTrialUI::AddToHudCorner(UWidget* Widget, bool bRightCorner)
{
	// an empty designer: give the generated widgets a canvas to sit on
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GeneratedRoot"));
	}

	UPanelWidget* Root = Cast<UPanelWidget>(WidgetTree->RootWidget);

	if (!Root)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[UI] %s: the root widget isn't a panel, so %s can't be added. Place it in the designer instead."),
			*GetName(), *Widget->GetName());
		return;
	}

	UPanelSlot* PanelSlot = Root->AddChild(Widget);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
	{
		const float AnchorX = bRightCorner ? 1.0f : 0.0f;

		CanvasSlot->SetAutoSize(true);
		CanvasSlot->SetAnchors(FAnchors(AnchorX, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(AnchorX, 0.0f));
		CanvasSlot->SetPosition(FVector2D(bRightCorner ? -HudCornerOffset.X : HudCornerOffset.X, HudCornerOffset.Y));

	} else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PanelSlot)) {

		OverlaySlot->SetHorizontalAlignment(bRightCorner ? HAlign_Right : HAlign_Left);
		OverlaySlot->SetVerticalAlignment(VAlign_Top);
		OverlaySlot->SetPadding(FMargin(bRightCorner ? 0.0f : HudCornerOffset.X, HudCornerOffset.Y, bRightCorner ? HudCornerOffset.X : 0.0f, 0.0f));

	} else {

		UE_LOG(LogMyProject, Warning, TEXT("[UI] %s: the root widget is a %s, which can't pin %s to a corner. Use a Canvas Panel or Overlay as the root."),
			*GetName(), *Root->GetClass()->GetName(), *Widget->GetName());
	}
}

UTextBlock* UTimeTrialUI::CreateHudText(FName Name, const FSlateFontInfo& Font, ETextJustify::Type Justification)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

	if (Font.HasValidFont())
	{
		Text->SetFont(Font);
	}

	Text->SetJustification(Justification);

	// soft drop shadow so the text stays readable over bright scenery
	Text->SetShadowOffset(FVector2D(2.0f, 2.0f));
	Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	return Text;
}

UWidget* UTimeTrialUI::CreateHudBar(FName Name, UTextBlock* Text, UTexture2D* Texture, const FMargin& TextPadding)
{
	if (!Texture)
	{
		return Text;
	}

	UOverlay* Bar = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), Name);

	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Background->SetBrush(MakeBarBrush(Texture, BarImageScale));
	Bar->AddChildToOverlay(Background);

	UOverlaySlot* TextSlot = Bar->AddChildToOverlay(Text);
	TextSlot->SetHorizontalAlignment(HAlign_Right);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(TextPadding);

	return Bar;
}

FSlateBrush UTimeTrialUI::MakeBarBrush(UTexture2D* Texture, float Scale)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight()) * Scale;

	return Brush;
}

void UTimeTrialUI::NativeConstruct()
{
	Super::NativeConstruct();

	// fill the HUD readouts with their pre-race values
	RefreshLapList();
	RefreshPositionText();

	// the second player joins after the title screen, and the first player's HUD runs the screens for both
	if (bSecondaryPlayer)
	{
		return;
	}

	if (TitleScreenUIClass)
	{
		// show the title screen first; the rest of the flow begins once Start is pressed
		ShowTitleScreen(false);

		// keep the race HUD off the flythrough
		VisibilityBeforeTitle = GetVisibility();
		SetVisibility(ESlateVisibility::Collapsed);

		if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(GetOwningPlayer()))
		{
			PC->BeginTitleSequence();
		}

	} else {

		// no title screen configured: go straight to car select / race select
		ShowCarSelectOrNext();
	}
}

void UTimeTrialUI::ShowTitleScreen(bool bAtModeChoice)
{
	TitleScreenWidget = CreateWidget<UTitleScreenUI>(GetOwningPlayer(), TitleScreenUIClass);
	TitleScreenWidget->AddToViewport(0);

	TitleScreenWidget->OnStartGame.AddDynamic(this, &UTimeTrialUI::OnTitleStartPressed);

	// the screen opens on the main menu as it goes up, so this has to come after
	if (bAtModeChoice)
	{
		TitleScreenWidget->ShowModeChoice();
	}
}

void UTimeTrialUI::BackToTitleScreen()
{
	// no title screen to go back to
	if (!TitleScreenUIClass || TitleScreenWidget)
	{
		return;
	}

	RemoveCarSelects();

	if (RaceSelectWidget)
	{
		RaceSelectWidget->RemoveFromParent();
		RaceSelectWidget = nullptr;
	}

	// keep the race HUD off the flythrough again
	VisibilityBeforeTitle = GetVisibility();
	SetVisibility(ESlateVisibility::Collapsed);

	TWeakObjectPtr<UTimeTrialUI> WeakThis(this);

	auto ShowTitle = [WeakThis]()
	{
		if (UTimeTrialUI* StrongThis = WeakThis.Get())
		{
			StrongThis->ShowTitleScreen(true);
		}
	};

	if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(GetOwningPlayer()))
	{
		PC->BackToTitleSequence(ShowTitle);

	} else {

		ShowTitle();
	}
}

void UTimeTrialUI::OnCarSelectBack()
{
	BackToTitleScreen();
}

void UTimeTrialUI::OnRaceSelectBack()
{
	// with no car select, the screen before is the title
	if (!CarSelectUIClass)
	{
		BackToTitleScreen();
		return;
	}

	if (RaceSelectWidget)
	{
		RaceSelectWidget->RemoveFromParent();
		RaceSelectWidget = nullptr;
	}

	ShowCarSelectOrNext();
}

void UTimeTrialUI::OnTitleStartPressed()
{
	// remove the title screen now that the player has chosen to start
	if (TitleScreenWidget)
	{
		TitleScreenWidget->RemoveFromParent();
		TitleScreenWidget = nullptr;
	}

	// fade from the flythrough to the vehicle, and bring up the next screen once the vehicle is on screen
	if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(GetOwningPlayer()))
	{
		TWeakObjectPtr<UTimeTrialUI> WeakThis(this);

		PC->EndTitleSequence([WeakThis]()
		{
			if (UTimeTrialUI* StrongThis = WeakThis.Get())
			{
				StrongThis->FinishTitleSequence();
			}
		});

	} else {

		FinishTitleSequence();
	}
}

void UTimeTrialUI::FinishTitleSequence()
{
	SetVisibility(VisibilityBeforeTitle);

	ShowCarSelectOrNext();
}

void UTimeTrialUI::ShowCarSelectOrNext()
{
	if (!CarSelectUIClass)
	{
		ShowRaceSelectOrBeginCountdown();
		return;
	}

	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));
	APlayerController* SecondPlayer = GM && GM->IsTwoPlayers() ? GM->GetSecondPlayer() : nullptr;

	CarSelectWidget = CreateWidget<UCarSelectUI>(GetOwningPlayer(), CarSelectUIClass);
	CarSelectWidget->OnCarChosen.AddDynamic(this, &UTimeTrialUI::OnCarSelected);
	CarSelectWidget->OnBack.AddDynamic(this, &UTimeTrialUI::OnCarSelectBack);

	if (!SecondPlayer)
	{
		CarSelectWidget->AddToViewport(0);
		return;
	}

	// two players pick at the same time, each on their own half of the screen with their own car going round
	CarSelectWidget->SetTwoPlayerSelect(0);
	CarSelectWidget->AddToPlayerScreen(0);

	SecondCarSelectWidget = CreateWidget<UCarSelectUI>(SecondPlayer, CarSelectUIClass);
	SecondCarSelectWidget->SetTwoPlayerSelect(1);
	SecondCarSelectWidget->OnCarChosen.AddDynamic(this, &UTimeTrialUI::OnCarSelected);
	SecondCarSelectWidget->AddToPlayerScreen(0);
}

void UTimeTrialUI::RemoveCarSelects()
{
	for (TObjectPtr<UCarSelectUI>* Widget : { &CarSelectWidget, &SecondCarSelectWidget })
	{
		if (*Widget)
		{
			(*Widget)->RemoveFromParent();
			*Widget = nullptr;
		}
	}
}

void UTimeTrialUI::OnCarSelected()
{
	// two players move on once both have picked
	if (SecondCarSelectWidget && (!SecondCarSelectWidget->IsCarChosen() || (CarSelectWidget && !CarSelectWidget->IsCarChosen())))
	{
		return;
	}

	// remove the car select screen now that a car has been picked
	RemoveCarSelects();

	ShowRaceSelectOrBeginCountdown();
}

void UTimeTrialUI::ShowRaceSelectOrBeginCountdown()
{
	if (RaceSelectUIClass)
	{
		// show the race select screen; the countdown begins once a race is chosen
		RaceSelectWidget = CreateWidget<UTimeTrialRaceSelectUI>(GetOwningPlayer(), RaceSelectUIClass);
		RaceSelectWidget->AddToViewport(0);

		RaceSelectWidget->OnRaceChosen.AddDynamic(this, &UTimeTrialUI::OnRaceChosen);
		RaceSelectWidget->OnBack.AddDynamic(this, &UTimeTrialUI::OnRaceSelectBack);

	} else {

		// no race select screen configured: go straight to the countdown, as before
		BeginCountdown();
	}
}

void UTimeTrialUI::OnRaceChosen()
{
	// remove the race select screen now that a race has been picked
	if (RaceSelectWidget)
	{
		RaceSelectWidget->RemoveFromParent();
		RaceSelectWidget = nullptr;
	}

	BeginCountdown();
}

void UTimeTrialUI::BeginCountdown()
{
	// line the cars up, so they settle on the grid during the countdown
	if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(GetOwningPlayer()))
	{
		PC->PrepareRaceStart();
	}

	// create the start countdown widget
	CountdownWidget = CreateWidget<UTimeTrialStartUI>(GetOwningPlayer(), StartUIClass);
	CountdownWidget->AddToViewport(0);

	// subscribe to the countdown finished delegate
	CountdownWidget->OnCountdownFinished.AddDynamic(this, &UTimeTrialUI::StartRace);

	// start the countdown
	CountdownWidget->StartCountdown();
}

void UTimeTrialUI::RetryRace()
{
	// take down whatever the last attempt left on screen. A countdown cut short must not start the race. The second
	// player's HUD just clears its laps: the countdown is the first player's
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResultTimer);
	}

	if (ResultWidget)
	{
		ResultWidget->RemoveFromParent();
		ResultWidget = nullptr;
	}

	if (CountdownWidget)
	{
		CountdownWidget->OnCountdownFinished.RemoveAll(this);
		CountdownWidget->RemoveFromParent();
		CountdownWidget = nullptr;
	}

	// back to the HUD of a race that hasn't started
	CurrentLap = 0;
	LapTimes.Reset();
	BestLapTime = -1.0f;
	LastLapTime = 0.0f;
	LapStartTime = 0.0f;
	RaceStartTime = 0.0f;
	LastCompletedLapTime = 0.0f;
	FinalLapTime = 0.0f;
	FinalRaceTime = 0.0f;
	bRaceFinished = false;
	RecordUpdate = FRaceRecordUpdate();

	RefreshLapList();
	SetRacePosition(1, 1);

	if (!bSecondaryPlayer)
	{
		BeginCountdown();
	}
}

void UTimeTrialUI::SetCountdownPaused(bool bPaused)
{
	if (CountdownWidget)
	{
		CountdownWidget->SetCountdownPaused(bPaused);
	}
}

void UTimeTrialUI::UpdateLapCount(int32 Lap, float NewLapStartTime)
{
	// save the new lap start time
	LapStartTime = NewLapStartTime;

	// calculate the lap time
	const float LapTime = NewLapStartTime - LastLapTime;

	UE_LOG(LogMyProject, Log, TEXT("[UI] UpdateLapCount Lap=%d NewLapStartTime=%.2f LastLapTime=%.2f LapTime=%.2f BestLapTime(before)=%.2f"),
		Lap, NewLapStartTime, LastLapTime, LapTime, BestLapTime);

	// is this the first lap?
	if (Lap > 1)
	{
		// a lap just ended: remember how long it took
		LastCompletedLapTime = LapTime;
		LapTimes.Add(LapTime);

		// do we have an invalid lap time?
		if (BestLapTime < 0.0f)
		{
			// save the current lap time
			BestLapTime = LapTime;

		} else {

			// not the first lap: do we have a lower lap time?
			if (LapTime < BestLapTime)
			{
				// save the best lap time
				BestLapTime = LapTime;
			}

		}

	} else {

		// first lap: reset the race state and save an invalid lap time
		BestLapTime = -1.0f;
		LastCompletedLapTime = 0.0f;
		LapTimes.Reset();
		RaceStartTime = NewLapStartTime;
		bRaceFinished = false;

	}

	// save the current lap
	CurrentLap = Lap;

	// save the lap start time
	LastLapTime = NewLapStartTime;

	RefreshLapList();

	// pass control to BP to update the widgets
	BP_UpdateLaps();
}

void UTimeTrialUI::FinishRace(const FRaceRecordUpdate& InRecordUpdate)
{
	RecordUpdate = InRecordUpdate;

	// the final finish line crossing already went through UpdateLapCount, so the lap that just ended is the last one
	FinalLapTime = LastCompletedLapTime;
	FinalRaceTime = LapStartTime - RaceStartTime;
	bRaceFinished = true;

	// no lap is running anymore, so drop the current lap highlight
	RefreshLapList();

	BP_RaceFinished();
}

void UTimeTrialUI::ShowResultsOnceAllFinished()
{
	UWorld* World = GetWorld();

	if (bSecondaryPlayer || !ResultUIClass || !World || ResultWidget || World->GetTimerManager().IsTimerActive(ResultTimer))
	{
		return;
	}

	// with two players, whoever finishes first drives on until the other is in too
	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));

	if (GM && !GM->AreAllPlayersFinished())
	{
		return;
	}

	// the results come up a moment after the line
	World->GetTimerManager().SetTimer(ResultTimer, this, &UTimeTrialUI::ShowResults, FMath::Max(ResultDelay, KINDA_SMALL_NUMBER), false);
}

void UTimeTrialUI::ShowResults()
{
	ResultWidget = CreateWidget<URaceResultUI>(GetOwningPlayer(), ResultUIClass);

	if (!ResultWidget)
	{
		return;
	}

	ResultWidget->SetPlayerResult(LapTimes, FinalRaceTime, RecordUpdate);
	ResultWidget->AddToViewport(10);

	// the race is over: let the cars roll to a stop rather than drive on under the results
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();

			if (APawn* Car = PC ? PC->GetPawn() : nullptr)
			{
				Car->DisableInput(PC);
			}
		}
	}
}

void UTimeTrialUI::NotifyRaceStarted(int32 RaceIndex, const FLinearColor& RaceColor)
{
	// the selected route decides the lap count, so the rows may need rebuilding
	RefreshLapList();

	BP_RaceStarted(RaceIndex, RaceColor);
}

void UTimeTrialUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (UTimeTrialLapRowUI* Row = GetCurrentLapRow())
	{
		Row->SetTime(GetCurrentLapTime());
	}
}

void UTimeTrialUI::RefreshLapList()
{
	if (BestLapText)
	{
		BestLapText->SetText(FText::Format(BestLapFormat, FormatLapTime(BestLapTime)));
	}

	if (!LapList || !LapRowClass)
	{
		return;
	}

	// rebuild the rows only when the lap count changes, so the list doesn't churn every lap
	const int32 TotalLaps = GetTotalLaps();

	if (LapRows.Num() != TotalLaps)
	{
		LapList->ClearChildren();
		LapRows.Reset();

		for (int32 LapNumber = 1; LapNumber <= TotalLaps; ++LapNumber)
		{
			UTimeTrialLapRowUI* Row = CreateWidget<UTimeTrialLapRowUI>(this, LapRowClass);
			Row->SetGeneratedFont(LapListFont);

			UTexture2D* BarTexture = LapBarTextures.IsValidIndex(LapNumber - 1) ? LapBarTextures[LapNumber - 1].Get() : nullptr;
			const bool bHasBar = Row->SetGeneratedBar(BarTexture, BarImageScale, BarTimePadding);

			UVerticalBoxSlot* RowSlot = LapList->AddChildToVerticalBox(Row);

			if (bHasBar && LapNumber > 1)
			{
				RowSlot->SetPadding(FMargin(0.0f, BarSpacing, 0.0f, 0.0f));
			}

			LapRows.Add(Row);
		}
	}

	const int32 BestLapNumber = GetBestLapNumber();
	const UTimeTrialLapRowUI* CurrentRow = GetCurrentLapRow();

	for (int32 Index = 0; Index < LapRows.Num(); ++Index)
	{
		const int32 LapNumber = Index + 1;
		UTimeTrialLapRowUI* Row = LapRows[Index];
		const bool bIsCurrent = Row == CurrentRow;

		// the running lap has no stored time yet: start it from the live timer instead of dashes
		const float Seconds = bIsCurrent ? GetCurrentLapTime() : GetLapTime(LapNumber);

		Row->SetLap(LapNumber, Seconds, LapNumber == BestLapNumber, bIsCurrent);
	}
}

void UTimeTrialUI::RefreshPositionText()
{
	if (PositionText)
	{
		PositionText->SetText(FText::Format(PositionFormat, FText::AsNumber(RacePosition), FText::AsNumber(TotalRacers)));
	}
}

UTimeTrialLapRowUI* UTimeTrialUI::GetCurrentLapRow() const
{
	if (bRaceFinished)
	{
		return nullptr;
	}

	// CurrentLap is 1-based and zero before the race starts
	const int32 Index = CurrentLap - 1;

	return LapRows.IsValidIndex(Index) ? LapRows[Index].Get() : nullptr;
}

int32 UTimeTrialUI::GetTotalLaps() const
{
	if (const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		return GM->GetLapsForSelectedRace();
	}

	return 0;
}

double UTimeTrialUI::GetCurrentLapTime() const
{
	if (bRaceFinished)
	{
		return FinalLapTime;
	}

	if (CurrentLap <= 0)
	{
		return 0.0;
	}

	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - LapStartTime : 0.0;
}

double UTimeTrialUI::GetTotalRaceTime() const
{
	if (bRaceFinished)
	{
		return FinalRaceTime;
	}

	if (CurrentLap <= 0)
	{
		return 0.0;
	}

	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - RaceStartTime : 0.0;
}

float UTimeTrialUI::GetLapTime(int32 LapNumber) const
{
	// laps are numbered from 1
	const int32 Index = LapNumber - 1;

	return LapTimes.IsValidIndex(Index) ? LapTimes[Index] : -1.0f;
}

int32 UTimeTrialUI::GetBestLapNumber() const
{
	int32 BestIndex = INDEX_NONE;

	for (int32 Index = 0; Index < LapTimes.Num(); ++Index)
	{
		if (BestIndex == INDEX_NONE || LapTimes[Index] < LapTimes[BestIndex])
		{
			BestIndex = Index;
		}
	}

	return BestIndex + 1;
}

void UTimeTrialUI::SetRacePosition(int32 NewPosition, int32 NewTotalRacers)
{
	if (NewPosition == RacePosition && NewTotalRacers == TotalRacers)
	{
		return;
	}

	RacePosition = NewPosition;
	TotalRacers = NewTotalRacers;

	RefreshPositionText();

	BP_RacePositionChanged(RacePosition, TotalRacers);
}

FText UTimeTrialUI::FormatLapTime(float TimeInSeconds)
{
	if (TimeInSeconds < 0.0f)
	{
		// the lap hasn't been run yet
		return NSLOCTEXT("TimeTrialUI", "NoLapTime", "--:--.---");
	}

	const int32 WholeSeconds = FMath::FloorToInt(TimeInSeconds);
	const int32 Minutes = WholeSeconds / 60;
	const int32 Seconds = WholeSeconds % 60;
	const int32 Milliseconds = FMath::FloorToInt((TimeInSeconds - WholeSeconds) * 1000.0f);

	return FText::FromString(FString::Printf(TEXT("%d:%02d.%03d"), Minutes, Seconds, Milliseconds));
}

void UTimeTrialUI::StartRace()
{
	// broadcast the delegate
	OnRaceStart.Broadcast();
}
