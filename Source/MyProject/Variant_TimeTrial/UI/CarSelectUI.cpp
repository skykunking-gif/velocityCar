// Copyright Epic Games, Inc. All Rights Reserved.


#include "CarSelectUI.h"
#include "TimeTrialGameMode.h"
#include "DrivingSettings.h"
#include "TwoPlayerInput.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
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

UCarSelectUI::UCarSelectUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// the screen has to be focusable for key events to reach NativeOnKeyDown
	SetIsFocusable(true);

	// same default font as a Text widget dropped in the designer
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(*UWidget::GetDefaultFontName());
		TitleFont = FSlateFontInfo(RobotoFontObj.Object, 18, FName("Bold"));
		CarNameFont = FSlateFontInfo(RobotoFontObj.Object, 40, FName("Bold"));
		SmallFont = FSlateFontInfo(RobotoFontObj.Object, 18, FName("Regular"));
		DescriptionFont = FSlateFontInfo(RobotoFontObj.Object, 16, FName("Regular"));
	}
}

void UCarSelectUI::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// no designer layout: build the panel ourselves
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		CreateLayout();
	}

	if (PrevButton)
	{
		PrevButton->OnClicked.AddDynamic(this, &UCarSelectUI::HandlePrevClicked);
	}

	if (NextButton)
	{
		NextButton->OnClicked.AddDynamic(this, &UCarSelectUI::HandleNextClicked);
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UCarSelectUI::HandleConfirmClicked);
	}

	if (AutomaticButton)
	{
		AutomaticButton->OnClicked.AddDynamic(this, &UCarSelectUI::HandleAutomaticClicked);
	}

	if (ManualButton)
	{
		ManualButton->OnClicked.AddDynamic(this, &UCarSelectUI::HandleManualClicked);
	}
}

void UCarSelectUI::CreateLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// a dark panel along the bottom, leaving the middle of the screen to the car
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(40.0f, 16.0f));

	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Bottom);
	PanelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, PanelBottomOffset));

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Panel->SetContent(Content);

	const FLinearColor Dim(0.75f, 0.75f, 0.75f, 1.0f);

	TitleText = CreateText(TEXT("TitleText"), TitleLabel, TitleFont, Dim);
	Content->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);

	// < car name >
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CarRow"));

	UVerticalBoxSlot* RowSlot = Content->AddChildToVerticalBox(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	RowSlot->SetPadding(FMargin(0.0f, 6.0f));

	PrevButton = CreateButton(TEXT("PrevButton"), FText::FromString(TEXT("<")), CarNameFont);
	Row->AddChildToHorizontalBox(PrevButton)->SetVerticalAlignment(VAlign_Center);

	// a fixed width keeps the arrows still while names of different lengths come and go
	USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CarNameBox"));
	NameBox->SetWidthOverride(520.0f);

	CarNameText = CreateText(TEXT("CarNameText"), FText::GetEmpty(), CarNameFont, FLinearColor::White);
	CarNameText->SetJustification(ETextJustify::Center);
	NameBox->SetContent(CarNameText);

	UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameBox);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(16.0f, 0.0f));

	NextButton = CreateButton(TEXT("NextButton"), FText::FromString(TEXT(">")), CarNameFont);
	Row->AddChildToHorizontalBox(NextButton)->SetVerticalAlignment(VAlign_Center);

	CarIndexText = CreateText(TEXT("CarIndexText"), FText::GetEmpty(), SmallFont, Dim);
	Content->AddChildToVerticalBox(CarIndexText)->SetHorizontalAlignment(HAlign_Center);

	// ミッション [AT] [MT]
	UHorizontalBox* TransmissionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TransmissionRow"));

	UVerticalBoxSlot* TransmissionSlot = Content->AddChildToVerticalBox(TransmissionRow);
	TransmissionSlot->SetHorizontalAlignment(HAlign_Center);
	TransmissionSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	UTextBlock* TransmissionText = CreateText(TEXT("TransmissionText"), TransmissionLabel, SmallFont, FLinearColor::White);
	UHorizontalBoxSlot* TransmissionTextSlot = TransmissionRow->AddChildToHorizontalBox(TransmissionText);
	TransmissionTextSlot->SetVerticalAlignment(VAlign_Center);
	TransmissionTextSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));

	for (const bool bAutomatic : { true, false })
	{
		const FString Name = bAutomatic ? TEXT("AutomaticButton") : TEXT("ManualButton");
		UButton* Button = CreateButton(*Name, bAutomatic ? AutomaticLabel : ManualLabel, SmallFont);

		// the same width for both, so the lit one doesn't look bigger
		USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(Name + TEXT("Box")));
		ButtonBox->SetWidthOverride(TransmissionButtonWidth);
		ButtonBox->SetContent(Button);

		UHorizontalBoxSlot* ButtonSlot = TransmissionRow->AddChildToHorizontalBox(ButtonBox);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(4.0f, 0.0f));

		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			Label->SetJustification(ETextJustify::Center);
		}

		(bAutomatic ? AutomaticButton : ManualButton) = Button;
	}

	TransmissionNoteText = CreateText(TEXT("TransmissionNoteText"), FText::GetEmpty(), DescriptionFont, Dim);
	UVerticalBoxSlot* NoteSlot = Content->AddChildToVerticalBox(TransmissionNoteText);
	NoteSlot->SetHorizontalAlignment(HAlign_Center);
	NoteSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));

	ConfirmButton = CreateButton(TEXT("ConfirmButton"), ConfirmLabel, SmallFont);

	UVerticalBoxSlot* ConfirmSlot = Content->AddChildToVerticalBox(ConfirmButton);
	ConfirmSlot->SetHorizontalAlignment(HAlign_Center);
	ConfirmSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 6.0f));

	HintText = CreateText(TEXT("HintText"), HintLabel, SmallFont, Dim);
	Content->AddChildToVerticalBox(HintText)->SetHorizontalAlignment(HAlign_Center);

	// a line or two about the highlighted car in the lower left, level with the panel
	UBorder* Description = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DescriptionPanel"));
	Description->SetBrushColor(PanelColor);
	Description->SetPadding(FMargin(18.0f, 12.0f));
	DescriptionPanel = Description;

	UOverlaySlot* DescriptionSlot = Root->AddChildToOverlay(Description);
	DescriptionSlot->SetHorizontalAlignment(HAlign_Left);
	DescriptionSlot->SetVerticalAlignment(VAlign_Bottom);
	DescriptionSlot->SetPadding(FMargin(DescriptionLeftOffset, 0.0f, 0.0f, PanelBottomOffset));

	// a fixed width, so long descriptions wrap rather than run into the panel in the middle
	USizeBox* DescriptionBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DescriptionBox"));
	DescriptionBox->SetWidthOverride(DescriptionWidth);
	Description->SetContent(DescriptionBox);

	CarDescriptionText = CreateText(TEXT("CarDescriptionText"), FText::GetEmpty(), DescriptionFont, FLinearColor::White);
	CarDescriptionText->SetAutoWrapText(true);
	DescriptionBox->SetContent(CarDescriptionText);
}

UButton* UCarSelectUI::CreateButton(FName Name, const FText& Label, const FSlateFontInfo& Font)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

	// dark text reads better on the default light button
	UTextBlock* LabelText = CreateText(*(Name.ToString() + TEXT("Label")), Label, Font, FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
	LabelText->SetShadowColorAndOpacity(FLinearColor::Transparent);
	Button->SetContent(LabelText);

	return Button;
}

UTextBlock* UCarSelectUI::CreateText(FName Name, const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(Color);

	if (Font.HasValidFont())
	{
		TextBlock->SetFont(Font);
	}

	TextBlock->SetShadowOffset(FVector2D(2.0f, 2.0f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	return TextBlock;
}

void UCarSelectUI::NativeConstruct()
{
	Super::NativeConstruct();

	bCarChosen = false;

	UWorld* World = GetWorld();

	if (GetCarCount() == 0)
	{
		// nothing to pick from: move straight on. Deferred a tick because the owner only
		// subscribes to OnCarChosen after adding the screen to the viewport.
		UE_LOG(LogMyProject, Warning, TEXT("[CarSelect] The game mode offers no cars (CarChoices is empty), so car select is skipped."));

		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UCarSelectUI::ConfirmHighlightedCar);
		}

		return;
	}

	// start on the car the player is already in
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();
	HighlightCar(GM ? GM->GetCarForPlayer(PlayerIndex) : 0);

	// and on the gearbox picked last time
	RefreshTransmission();
	RefreshPrompts();

	StartPreviewCamera();

	// the widget isn't in the Slate hierarchy yet while it's being constructed, so it can't
	// take focus until the next tick
	if (World)
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UCarSelectUI::ClaimKeyboardFocus);
	}
}

void UCarSelectUI::NativeDestruct()
{
	StopPreviewCamera();

	// hand input back to the game
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
	}

	Super::NativeDestruct();
}

void UCarSelectUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// the HUD puts this screen up before the player controller has possessed its car,
	// so the camera starts as soon as there's a car to look at
	if (!PreviewCamera && !bCarChosen && GetCarCount() > 0)
	{
		StartPreviewCamera();
	}

	if (PreviewCamera)
	{
		PreviewAngle = FMath::Fmod(PreviewAngle + PreviewCameraOrbitSpeed * InDeltaTime, 360.0f);
		UpdatePreviewCamera();
	}
}

void UCarSelectUI::ClaimKeyboardFocus()
{
	APlayerController* PC = GetOwningPlayer();

	if (!PC)
	{
		return;
	}

	// send keyboard and gamepad input to this widget while the screen is up. The game still
	// gets anything the widget doesn't handle, and mouse clicks on the buttons are unaffected.
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

	PC->SetInputMode(InputMode);
}

void UCarSelectUI::SetTwoPlayerSelect(int32 InPlayerIndex)
{
	bTwoPlayerSelect = true;
	PlayerIndex = FMath::Max(InPlayerIndex, 0);
}

FReply UCarSelectUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	const bool bBackKey = Key == EKeys::BackSpace || Key == EKeys::Escape || Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Right
		|| Key == EKeys::Virtual_Gamepad_Back.GetVirtualKey();

	// a player who's picked waits for the other, and can take it back
	if (bTwoPlayerSelect && bCarChosen)
	{
		if (bBackKey && !InKeyEvent.IsRepeat())
		{
			CancelChoice();
		}

		return FReply::Handled();
	}

	// previous car
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		MoveHighlight(-1);
		return FReply::Handled();
	}

	// next car
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		MoveHighlight(1);
		return FReply::Handled();
	}

	// the other gearbox. With only two, up and down both just switch
	if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Down || Key == EKeys::S
		|| Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up
		|| Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		SetAutomaticTransmission(!UDrivingSettings::IsAutomaticTransmission(PlayerIndex));
		return FReply::Handled();
	}

	// pick the highlighted car
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey())
	{
		ConfirmHighlightedCar();
		return FReply::Handled();
	}

	// back to the title screen, unless a car has already been picked. In a two player game only the first player
	// takes both back
	if (bBackKey)
	{
		if (!bCarChosen && PlayerIndex == 0)
		{
			bCarChosen = true;
			OnBack.Broadcast();
		}

		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UCarSelectUI::GetCarCount() const
{
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();

	return GM ? GM->GetCarChoices().Num() : 0;
}

FText UCarSelectUI::GetCarName(int32 CarIndex) const
{
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();

	if (!GM || !GM->GetCarChoices().IsValidIndex(CarIndex))
	{
		return FText::GetEmpty();
	}

	const FTimeTrialCarChoice& Car = GM->GetCarChoices()[CarIndex];

	if (!Car.DisplayName.IsEmpty())
	{
		return Car.DisplayName;
	}

	// no name set: fall back to something that tells the cars apart
	return Car.BodyClass
		? FText::FromString(Car.BodyClass->GetName().Replace(TEXT("BP_"), TEXT("")).Replace(TEXT("_C"), TEXT("")))
		: FText::Format(NSLOCTEXT("CarSelectUI", "UnnamedCar", "Car {0}"), FText::AsNumber(CarIndex + 1));
}

void UCarSelectUI::HighlightCar(int32 CarIndex)
{
	const int32 Count = GetCarCount();

	if (Count == 0)
	{
		return;
	}

	HighlightedCar = FMath::Clamp(CarIndex, 0, Count - 1);

	// switch the player's car right away: the car on screen is the preview
	if (ATimeTrialGameMode* GM = GetTimeTrialGameMode())
	{
		GM->SetCarForPlayer(PlayerIndex, HighlightedCar);
	}

	RefreshCarText();

	// let Blueprint drive any extra visuals
	BP_HighlightChanged(HighlightedCar, GetCarName(HighlightedCar));
}

void UCarSelectUI::MoveHighlight(int32 Delta)
{
	const int32 Count = GetCarCount();

	if (Count == 0 || Delta == 0)
	{
		return;
	}

	// wrap around the ends so the list cycles
	HighlightCar(((HighlightedCar + Delta) % Count + Count) % Count);
}

void UCarSelectUI::ConfirmHighlightedCar()
{
	ChooseCar(HighlightedCar);
}

void UCarSelectUI::ChooseCar(int32 CarIndex)
{
	// a click and a key press could both land before the screen is torn down, so only the
	// first choice counts
	if (bCarChosen)
	{
		return;
	}

	bCarChosen = true;

	if (ATimeTrialGameMode* GM = GetTimeTrialGameMode())
	{
		GM->SetCarForPlayer(PlayerIndex, CarIndex);
	}

	RefreshPrompts();

	// let the owning UI know a choice has been made
	OnCarChosen.Broadcast();
}

void UCarSelectUI::CancelChoice()
{
	if (!bTwoPlayerSelect || !bCarChosen)
	{
		return;
	}

	bCarChosen = false;

	RefreshPrompts();
}

void UCarSelectUI::RefreshPrompts()
{
	if (!bTwoPlayerSelect)
	{
		return;
	}

	const FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this);
	const bool bSharedKeyboard = TwoPlayerInput && TwoPlayerInput->IsSecondPlayerOnKeyboard();

	if (TitleText)
	{
		TitleText->SetText(FText::Format(TwoPlayerTitleFormat, FText::AsNumber(PlayerIndex + 1)));
	}

	if (!HintText)
	{
		return;
	}

	if (bCarChosen)
	{
		// the key that takes the pick back
		const FText CancelKey = FText::FromString(PlayerIndex == 0 ? TEXT("Backspace") : (bSharedKeyboard ? TEXT("Delete") : TEXT("B")));

		HintText->SetText(FText::Format(ReadyFormat, CancelKey));

	} else if (PlayerIndex == 0) {

		HintText->SetText(bSharedKeyboard ? FirstPlayerKeyboardHint : HintLabel);

	} else {

		HintText->SetText(bSharedKeyboard ? SecondPlayerKeyboardHint : SecondPlayerGamepadHint);
	}
}

void UCarSelectUI::RefreshCarText()
{
	if (CarNameText)
	{
		CarNameText->SetText(GetCarName(HighlightedCar));
	}

	if (CarIndexText)
	{
		CarIndexText->SetText(FText::Format(CarIndexFormat, FText::AsNumber(HighlightedCar + 1), FText::AsNumber(GetCarCount())));
	}

	if (CarDescriptionText)
	{
		const FText Description = GetCarDescription(HighlightedCar);

		CarDescriptionText->SetText(Description.IsEmpty() ? FText::GetEmpty() : FText::Format(DescriptionFormat, GetCarName(HighlightedCar), Description));

		// a car without a description gets no empty panel either
		UWidget* Shown = DescriptionPanel ? DescriptionPanel.Get() : CarDescriptionText.Get();
		Shown->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UCarSelectUI::SetAutomaticTransmission(bool bAutomatic)
{
	// the player controller hears about it and puts the car on screen in that gearbox
	UDrivingSettings::SetAutomaticTransmission(bAutomatic, PlayerIndex);

	RefreshTransmission();
}

void UCarSelectUI::RefreshTransmission()
{
	const bool bAutomatic = UDrivingSettings::IsAutomaticTransmission(PlayerIndex);

	for (UButton* Button : { AutomaticButton.Get(), ManualButton.Get() })
	{
		if (!Button)
		{
			continue;
		}

		const bool bPicked = (Button == AutomaticButton) == bAutomatic;

		// the picked one lit up like the highlighted title menu item, the other left as a plain button
		Button->SetBackgroundColor(bPicked ? PickedButtonColor : FLinearColor::White);

		if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
		{
			Label->SetColorAndOpacity(bPicked ? PickedButtonTextColor : FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
		}
	}

	if (TransmissionNoteText)
	{
		// the second player shifts with keys of their own
		FText Note = ManualNote;

		if (bTwoPlayerSelect && PlayerIndex > 0)
		{
			const FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this);
			Note = TwoPlayerInput && TwoPlayerInput->IsSecondPlayerOnKeyboard() ? SecondPlayerKeyboardManualNote : SecondPlayerGamepadManualNote;
		}

		TransmissionNoteText->SetText(bAutomatic ? AutomaticNote : Note);
	}
}

FText UCarSelectUI::GetCarDescription(int32 CarIndex) const
{
	const ATimeTrialGameMode* GM = GetTimeTrialGameMode();

	return GM && GM->GetCarChoices().IsValidIndex(CarIndex) ? GM->GetCarChoices()[CarIndex].Description : FText::GetEmpty();
}

void UCarSelectUI::StartPreviewCamera()
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = GetOwningPlayerPawn();
	UWorld* World = GetWorld();

	if (!bUsePreviewCamera || !PC || !Pawn || !World || PreviewCamera)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PreviewCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Pawn->GetActorTransform(), SpawnParams);

	if (!PreviewCamera)
	{
		return;
	}

	// the letterbox camera manager already keeps the view at a fixed aspect ratio
	PreviewCamera->GetCameraComponent()->SetConstraintAspectRatio(false);

	PreviewAngle = PreviewCameraStartAngle;
	UpdatePreviewCamera();

	PC->SetViewTargetWithBlend(PreviewCamera, PreviewCameraBlendTime);
}

void UCarSelectUI::UpdatePreviewCamera()
{
	const APawn* Pawn = GetOwningPlayerPawn();

	if (!PreviewCamera || !Pawn)
	{
		return;
	}

	// circle the car at a fixed distance and height, always looking at its middle
	const FVector Target = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, PreviewLookAtHeight);
	const FRotator AroundCar(0.0f, Pawn->GetActorRotation().Yaw + PreviewAngle, 0.0f);
	const FVector CameraLocation = Target + AroundCar.RotateVector(FVector(PreviewCameraDistance, 0.0f, 0.0f)) + FVector(0.0f, 0.0f, PreviewCameraHeight);

	PreviewCamera->SetActorLocationAndRotation(CameraLocation, (Target - CameraLocation).Rotation());
}

void UCarSelectUI::StopPreviewCamera()
{
	if (!PreviewCamera)
	{
		return;
	}

	// blend back to the car. This also overrides the blend to the preview camera if it's still running.
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, PreviewCameraBlendTime);
		}
	}

	// keep the camera around until the blend away from it is done
	PreviewCamera->SetLifeSpan(PreviewCameraBlendTime + 0.5f);
	PreviewCamera = nullptr;
}

ATimeTrialGameMode* UCarSelectUI::GetTimeTrialGameMode() const
{
	return Cast<ATimeTrialGameMode>(UGameplayStatics::GetGameMode(this));
}

void UCarSelectUI::HandlePrevClicked()
{
	MoveHighlight(-1);
}

void UCarSelectUI::HandleNextClicked()
{
	MoveHighlight(1);
}

void UCarSelectUI::HandleConfirmClicked()
{
	ConfirmHighlightedCar();
}

void UCarSelectUI::HandleAutomaticClicked()
{
	SetAutomaticTransmission(true);
}

void UCarSelectUI::HandleManualClicked()
{
	SetAutomaticTransmission(false);
}
