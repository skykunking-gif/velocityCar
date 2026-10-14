// Copyright Epic Games, Inc. All Rights Reserved.


#include "TimeTrialPlayerController.h"
#include "TimeTrialUI.h"
#include "PauseMenuUI.h"
#include "Engine/World.h"
#include "TimeTrialGameMode.h"
#include "TimeTrialTrackGate.h"
#include "TimeTrialRecords.h"
#include "TitleCameraDolly.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "MyProjectUI.h"
#include "MyProjectPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "MyProject.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "EngineSoundProfile.h"
#include "GameAudioSettings.h"
#include "DrivingSettings.h"
#include "TimerManager.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "LetterboxPlayerCameraManager.h"
#include "TwoPlayerInput.h"
#include "InputAction.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ATimeTrialPlayerController::ATimeTrialPlayerController()
{
	// lock the rendered view to a fixed aspect ratio
	PlayerCameraManagerClass = ALetterboxPlayerCameraManager::StaticClass();

	// the pause menu builds its own layout
	PauseMenuClass = UPauseMenuUI::StaticClass();

	// the editor takes Escape to stop Play In Editor, so P is there to pause with in the editor
	PauseKeys = { EKeys::Escape, EKeys::P, EKeys::Gamepad_Special_Right };

	// the car glass: see-through, with its edges catching the light
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassMaterial(TEXT("/Game/Vehicles/AutomotiveMaterials/Masters/M_Glass_Master"));

	if (GlassMaterial.Succeeded())
	{
		GhostMaterial = GlassMaterial.Object;
	}
}

void ATimeTrialPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// move the car to the right start position whenever a race is chosen, and restyle it whenever a car is
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->OnSelectedRaceChanged.AddDynamic(this, &ATimeTrialPlayerController::OnSelectedRaceChanged);
		GM->OnSelectedCarChanged.AddDynamic(this, &ATimeTrialPlayerController::OnSelectedCarChanged);
	}

	// the menu music plays from the title screen until the race. The first player's controller plays it for both
	if (IsLocalPlayerController() && IsPrimaryPlayer())
	{
		MusicTrack = 0;
		bPlayingRaceMusic = false;
		StartMusicTrack(0.0f);

		AudioSettingsChangedHandle = UGameAudioSettings::OnChanged().AddUObject(this, &ATimeTrialPlayerController::OnAudioSettingsChanged);
	}

	// AT or MT can be switched on the car select screen with the car already on screen
	DrivingSettingsChangedHandle = UDrivingSettings::OnChanged().AddUObject(this, &ATimeTrialPlayerController::ApplyTransmission);
}

void ATimeTrialPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameAudioSettings::OnChanged().Remove(AudioSettingsChangedHandle);
	UDrivingSettings::OnChanged().Remove(DrivingSettingsChangedHandle);

	Super::EndPlay(EndPlayReason);
}

void ATimeTrialPlayerController::OnAudioSettingsChanged()
{
	// a track fading out keeps fading from where it was, so only the one playing on needs it
	if (MusicComponent)
	{
		MusicComponent->SetVolumeMultiplier(UGameAudioSettings::GetMusicVolume());
	}
}

void ATimeTrialPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			if (bUseSteeringWheelControls)
			{
				Subsystem->AddMappingContext(SteeringWheelInputMappingContext, 0);
			}

			if (!IsPrimaryPlayer())
			{
				AddSecondPlayerKeyboardKeys(Subsystem);
			}
		}
	}

	// only spawn UI on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			} else {

				UE_LOG(LogMyProject, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}

		// create the UI widget
		UIWidget = CreateWidget<UTimeTrialUI>(this, UIWidgetClass);

		if (UIWidget)
		{
			// the second player's is only their race HUD: the first player's runs the screens for both
			UIWidget->SetSecondaryPlayer(!IsPrimaryPlayer());

			UIWidget->AddToPlayerScreen(0);

			// subscribe to the race start delegate
			UIWidget->OnRaceStart.AddDynamic(this, &ATimeTrialPlayerController::StartRace);

		} else {

			UE_LOG(LogMyProject, Error, TEXT("Could not spawn Time Trial UI widget."));

		}


		// spawn the UI widget and add it to the viewport
		VehicleUI = CreateWidget<UMyProjectUI>(this, VehicleUIClass);

		if (VehicleUI)
		{
			VehicleUI->AddToPlayerScreen(0);

			// show the tachometer of the car that's already picked
			ApplySelectedCar();

			// the title screen went up before this HUD existed, so hide it now
			if (bTitleSequenceActive)
			{
				VehicleUIVisibilityBeforeTitle = VehicleUI->GetVisibility();
				VehicleUI->SetVisibility(ESlateVisibility::Collapsed);
			}

		} else {

			UE_LOG(LogMyProject, Error, TEXT("Could not spawn vehicle UI widget."));

		}
	}
}

void ATimeTrialPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// get a pointer to the controlled pawn
	VehiclePawn = CastChecked<AMyProjectPawn>(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	VehiclePawn->OnDestroyed.AddDynamic(this, &ATimeTrialPlayerController::OnPawnDestroyed);

	// a reset puts the car back on the course rather than where it went off
	VehiclePawn->FindResetTransform.BindUObject(this, &ATimeTrialPlayerController::FindCourseResetTransform);

	// disable input on the pawn if the race hasn't started yet
	if (!bRaceStarted)
	{
		VehiclePawn->DisableInput(this);
	}

	// a respawned vehicle should look like the car the player picked
	ApplySelectedCar();

	// possessing points the view at the vehicle, so put it back on the flythrough while the title screen is up
	if (bTitleSequenceActive && TitleDolly)
	{
		SetViewTarget(TitleDolly);
	}
}

bool ATimeTrialPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// while a race is under way the pause keys only pause, ahead of anything the vehicle has bound to them. Either
	// player can pause; the first player's controller runs the pause for both
	if (Params.Event == IE_Pressed && PauseKeys.Contains(Params.Key))
	{
		ATimeTrialPlayerController* Primary = GetPrimaryController();

		if ((Primary ? Primary : this)->HandlePauseKey())
		{
			return true;
		}
	}

	return Super::InputKey(Params);
}

bool ATimeTrialPlayerController::HandlePauseKey()
{
	if (PauseMenu)
	{
		ResumeRace();
		return true;
	}

	if (CanPause())
	{
		PauseRace();
		return true;
	}

	return false;
}

bool ATimeTrialPlayerController::CanPause() const
{
	if (!bRaceUnderway || bLeavingRace)
	{
		return false;
	}

	// until the last player in is over the line
	for (const ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		if (!PC->bRaceFinished)
		{
			return true;
		}
	}

	return false;
}

ATimeTrialPlayerController* ATimeTrialPlayerController::GetPrimaryController() const
{
	for (ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		if (PC->IsPrimaryPlayer())
		{
			return PC;
		}
	}

	return nullptr;
}

TArray<ATimeTrialPlayerController*> ATimeTrialPlayerController::GetRaceControllers() const
{
	TArray<ATimeTrialPlayerController*> Controllers;

	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (ATimeTrialPlayerController* PC = Cast<ATimeTrialPlayerController>(It->Get()))
			{
				Controllers.Add(PC);
			}
		}
	}

	Controllers.StableSort([](const ATimeTrialPlayerController& A, const ATimeTrialPlayerController& B)
	{
		return GetLocalPlayerIndex(&A) < GetLocalPlayerIndex(&B);
	});

	return Controllers;
}

void ATimeTrialPlayerController::FadeAllCameras(float FromAlpha, float ToAlpha, float Duration, bool bShouldFadeAudio, bool bHoldWhenFinished) const
{
	for (const ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, Duration, FLinearColor::Black, bShouldFadeAudio, bHoldWhenFinished);
		}
	}
}

void ATimeTrialPlayerController::AddSecondPlayerKeyboardKeys(UEnhancedInputLocalPlayerSubsystem* Subsystem)
{
	const FTwoPlayerInput* TwoPlayerInput = FTwoPlayerInput::Find(this);

	if (!Subsystem || !TwoPlayerInput || !TwoPlayerInput->IsSecondPlayerOnKeyboard())
	{
		return;
	}

	// the vehicle's own actions, as the default mapping contexts know them
	auto FindAction = [this](FName ActionName) -> const UInputAction*
	{
		for (const UInputMappingContext* Context : DefaultMappingContexts)
		{
			if (!Context)
			{
				continue;
			}

			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Action && Mapping.Action->GetFName() == ActionName)
				{
					return Mapping.Action;
				}
			}
		}

		return nullptr;
	};

	// the arrow keys already drive through the default mapping context. These stand in for the first player's
	// Space, Q, E, Tab and Backspace. The keys follow FTwoPlayerInput::GetSecondPlayerKeys and the controls list
	const TPair<FName, FKey> Keys[] = {
		{ TEXT("IA_Handbrake"), EKeys::Enter },
		{ TEXT("IA_GearUp"), EKeys::RightShift },
		{ TEXT("IA_GearDown"), EKeys::RightControl },
		{ TEXT("IA_ToggleCamera"), EKeys::Slash },
		{ TEXT("IA_Reset"), EKeys::Delete }
	};

	SecondPlayerKeyboardContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SecondPlayerKeyboard"));

	for (const TPair<FName, FKey>& Key : Keys)
	{
		if (const UInputAction* Action = FindAction(Key.Key))
		{
			SecondPlayerKeyboardContext->MapKey(Action, Key.Value);

		} else {

			UE_LOG(LogMyProject, Warning, TEXT("[2P] No %s in the default mapping contexts, so %s does nothing for the second player"), *Key.Key.ToString(), *Key.Value.ToString());
		}
	}

	Subsystem->AddMappingContext(SecondPlayerKeyboardContext, 1);
}

void ATimeTrialPlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	{
		const UChaosWheeledVehicleMovementComponent* Movement = VehiclePawn->GetChaosVehicleMovement();

		VehicleUI->UpdateSpeed(Movement->GetForwardSpeed());
		VehicleUI->UpdateGear(Movement->GetCurrentGear());
		VehicleUI->UpdateEngineRPM(VehiclePawn->GetEngineRPM(), VehiclePawn->GetEngineMaxRPM());
	}

	// record where the car is for its ghost. Game time stands still while paused, so a pause leaves no gap
	if (GhostRecorder.IsRecording() && IsValid(VehiclePawn))
	{
		GhostRecorder.AddFrame(GetWorld()->GetTimeSeconds() - GhostRecordStartTime, GetGhostTransform());
	}

	// keep the race position up to date. Once we've finished it stays at the place we finished in
	if (bRaceStarted && IsValid(UIWidget))
	{
		if (const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
		{
			if (const int32 Position = GM->GetRacePosition(this))
			{
				UIWidget->SetRacePosition(Position, GM->GetRacerCount());
			}
		}
	}
}

void ATimeTrialPlayerController::PrepareRaceStart()
{
	const TArray<ATimeTrialPlayerController*> Players = GetRaceControllers();

	// from the countdown on, the race can be paused
	for (ATimeTrialPlayerController* PC : Players)
	{
		PC->bRaceUnderway = true;
	}

	// the race music comes in with the countdown
	PlayMusic(true);

	// in a race against NPCs this spawns them on the grid, and puts us in our slot
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->PrepareRaceGrid();
	}

	for (ATimeTrialPlayerController* PC : Players)
	{
		PC->PositionVehicleForRace();
	}

	// the ghost waits on the start through the countdown
	SpawnGhost();
}

void ATimeTrialPlayerController::StartRace()
{
	// the countdown is the first player's, and it sets both players off
	for (ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		PC->StartRaceForPlayer();
	}

	// start tracking race positions, and send any NPCs off
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->StartRacers();
	}
}

void ATimeTrialPlayerController::StartRaceForPlayer()
{
	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	if (GM)
	{
		// the first gate depends on the selected race: reversed routes start by heading the other way
		SetTargetGate(GM->GetFirstGateForSelectedRace());

		// make sure the car is lined up for the route, in case no race was ever explicitly chosen.
		// A grid start already put the car in its slot before the countdown, and it has settled there since
		if (!GM->UsesStartingGrid())
		{
			PositionVehicleForRace();
		}

		// let the HUD show which route is being run
		if (UIWidget)
		{
			UIWidget->NotifyRaceStarted(GM->GetSelectedRace(), GM->GetSelectedRaceColor());
		}
	}

	// raise the race started flag so any respawned vehicles start with controls unlocked
	bRaceStarted = true;
	bRaceFinished = false;

	// start the first lap
	CurrentLap = 0;
	IncrementLapCount();

	// the race is timed from here, the ghost's and the recording of ours alike
	GhostRecordStartTime = GetWorld()->GetTimeSeconds();

	if (IsValid(VehiclePawn))
	{
		GhostRecorder.Start(1.0f / FMath::Max(GhostSampleRate, 1.0f), GetGhostTransform());
	}

	if (GhostCar)
	{
		GhostCar->StartPlayback(GhostRecordStartTime);
	}

	// enable input on the pawn
	if (GetPawn())
	{
		GetPawn()->EnableInput(this);
	}
}

void ATimeTrialPlayerController::BeginTitleSequence()
{
	bTitleSequenceActive = true;

	// if there's a title camera dolly in the level, cut to it for the title screen flythrough
	TitleDolly = Cast<ATitleCameraDolly>(UGameplayStatics::GetActorOfClass(GetWorld(), ATitleCameraDolly::StaticClass()));

	if (TitleDolly)
	{
		// it stops when the view leaves it for the car, and starts again from there when the player comes back
		TitleDolly->SetActorTickEnabled(true);

		SetViewTarget(TitleDolly);
	}

	// the flythrough is no place for the speedometer
	if (VehicleUI)
	{
		VehicleUIVisibilityBeforeTitle = VehicleUI->GetVisibility();
		VehicleUI->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ATimeTrialPlayerController::EndTitleSequence(TFunction<void()> OnVehicleShown)
{
	if (!bTitleSequenceActive)
	{
		if (OnVehicleShown)
		{
			OnVehicleShown();
		}

		return;
	}

	bTitleSequenceActive = false;

	// the dolly can be anywhere on the map, so fade through black rather than swooping across to the car
	if (TitleDolly && PlayerCameraManager && TitleFadeTime > 0.0f)
	{
		PlayerCameraManager->StartCameraFade(0.0f, 1.0f, TitleFadeTime, FLinearColor::Black, false, true);

		// the cut has to come before the callback: the next screen may point the camera somewhere else
		FTimerHandle CutTimer;
		GetWorldTimerManager().SetTimer(CutTimer, FTimerDelegate::CreateWeakLambda(this, [this, OnVehicleShown]()
		{
			CutFromTitleToVehicle();

			if (OnVehicleShown)
			{
				OnVehicleShown();
			}

		}), TitleFadeTime, false);

		return;
	}

	CutFromTitleToVehicle();

	if (OnVehicleShown)
	{
		OnVehicleShown();
	}
}

void ATimeTrialPlayerController::BackToTitleSequence(TFunction<void()> OnTitleShown)
{
	if (bTitleSequenceActive || !PlayerCameraManager || TitleFadeTime <= 0.0f)
	{
		BeginTitleSequence();

		if (OnTitleShown)
		{
			OnTitleShown();
		}

		return;
	}

	// through black, as on the way to the car
	FadeAllCameras(0.0f, 1.0f, TitleFadeTime, false, true);

	FTimerHandle CutTimer;
	GetWorldTimerManager().SetTimer(CutTimer, FTimerDelegate::CreateWeakLambda(this, [this, OnTitleShown]()
	{
		// the title screen is the first player's alone, on the whole screen
		if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
		{
			GM->RemoveSecondPlayer();
		}

		BeginTitleSequence();

		if (PlayerCameraManager)
		{
			PlayerCameraManager->StartCameraFade(1.0f, 0.0f, TitleFadeTime, FLinearColor::Black, false, false);
		}

		if (OnTitleShown)
		{
			OnTitleShown();
		}

	}), TitleFadeTime, false);
}

void ATimeTrialPlayerController::CutFromTitleToVehicle()
{
	if (VehiclePawn)
	{
		SetViewTarget(VehiclePawn);
	}

	// in a two player game the screen splits here, while it's black, with both cars side by side on the start
	if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
	{
		if (GM->IsTwoPlayers() && GM->AddSecondPlayer())
		{
			for (ATimeTrialPlayerController* PC : GetRaceControllers())
			{
				PC->PositionVehicleForRace();
			}
		}
	}

	if (TitleDolly)
	{
		// nobody's watching the flythrough anymore
		TitleDolly->SetActorTickEnabled(false);

		if (TitleFadeTime > 0.0f)
		{
			FadeAllCameras(1.0f, 0.0f, TitleFadeTime, false, false);
		}
	}

	if (VehicleUI)
	{
		VehicleUI->SetVisibility(VehicleUIVisibilityBeforeTitle);
	}
}

void ATimeTrialPlayerController::IncrementLapCount()
{
	// once the race is over, further finish line crossings don't count
	if (bRaceFinished)
	{
		return;
	}

	// increment the lap counter
	++CurrentLap;

	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());
	const int32 TotalLaps = GM ? GM->GetLapsForSelectedRace() : 0;

	UE_LOG(LogMyProject, Log, TEXT("[Lap] IncrementLapCount -> CurrentLap=%d/%d, Time=%.2f"),
		CurrentLap, TotalLaps, GetWorld()->GetTimeSeconds());

	// update the UI. This also records the lap time of the lap that just ended
	if (UIWidget)
	{
		UIWidget->UpdateLapCount(CurrentLap, GetWorld()->GetTimeSeconds());
	}

	// crossing the finish line after the last lap ends the race
	if (TotalLaps > 0 && CurrentLap > TotalLaps)
	{
		bRaceFinished = true;

		// no more gates to chase; this also clears the target highlight
		SetTargetGate(nullptr);

		// fixes our place in the race
		if (GM)
		{
			GM->NotifyPlayerFinished(this);
		}

		if (UIWidget)
		{
			// every lap is in, the last one included, so the race can go down in the records
			FRaceRecordUpdate RecordUpdate;

			UTimeTrialRecords* Records = UTimeTrialRecords::Get(this);

			// a two player race doesn't go down in the course records, nor leave a ghost
			if (GM && Records && !GM->IsTwoPlayers())
			{
				const TArray<FTimeTrialCarChoice>& Cars = GM->GetCarChoices();
				const FString CarName = Cars.IsValidIndex(GM->GetSelectedCar()) ? Cars[GM->GetSelectedCar()].DisplayName.ToString() : FString();
				const TArray<float> LapTimes = UIWidget->GetLapTimes();

				RecordUpdate = Records->SubmitRace(GM->GetSelectedCourseId(), LapTimes, CarName);

				// the recording ends on the line, and becomes the course's ghost if it's the fastest yet
				if (GhostRecorder.IsRecording())
				{
					if (IsValid(VehiclePawn))
					{
						GhostRecorder.AddFrame(GetWorld()->GetTimeSeconds() - GhostRecordStartTime, GetGhostTransform());
					}

					GhostRecorder.Stop();

					FGhostTrack& Track = GhostRecorder.Track;
					Track.CarName = CarName;
					Track.TotalTime = 0.0f;

					for (const float LapTime : LapTimes)
					{
						Track.TotalTime += LapTime;
					}

					Records->SubmitGhost(GM->GetSelectedCourseId(), LapTimes.Num(), Track);
				}
			}

			UIWidget->FinishRace(RecordUpdate);
		}

		// the results come up once every player is in
		if (const ATimeTrialPlayerController* Primary = GetPrimaryController())
		{
			if (Primary->UIWidget)
			{
				Primary->UIWidget->ShowResultsOnceAllFinished();
			}
		}
	}
}

ATimeTrialTrackGate* ATimeTrialPlayerController::GetTargetGate() const
{
	return TargetGate.Get();
}

void ATimeTrialPlayerController::SetTargetGate(ATimeTrialTrackGate* Gate)
{
	ATimeTrialTrackGate* PreviousGate = TargetGate.Get();

	TargetGate = Gate;

	// move the highlight from the old target to the new one
	if (PreviousGate)
	{
		PreviousGate->RefreshTargetHighlight();
	}

	if (Gate)
	{
		Gate->RefreshTargetHighlight();

		// gates placed close together overlap each other. If the car is already inside the new target
		// (because it entered while the previous gate was still the target), no further overlap event
		// will ever fire for it, so count it as passed right away. Bounded in case several gates stack.
		// (kept as a file-local so the class layout stays unchanged for Live Coding)
		static int32 PassChainDepth = 0;

		if (VehiclePawn && PassChainDepth < 8 && Gate->IsOverlappingActor(VehiclePawn))
		{
			UE_LOG(LogMyProject, Log, TEXT("[Gate] %s became the target while already overlapped; passing it immediately"), *Gate->GetName());

			++PassChainDepth;
			Gate->HandlePlayerPass(this);
			--PassChainDepth;
		}
	}
}

void ATimeTrialPlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	// respawn at the race-specific start if the level has one for the selected race
	AActor* SpawnPoint = GM ? GM->FindRaceStart(GM->GetSelectedRace()) : nullptr;

	if (!SpawnPoint)
	{
		// otherwise use the first regular player start
		TArray<AActor*> ActorList;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

		for (AActor* Start : ActorList)
		{
			if (!GM || !GM->IsRaceStart(Start))
			{
				SpawnPoint = Start;
				break;
			}
		}
	}

	if (SpawnPoint)
	{
		// spawn a vehicle at the player start
		const FTransform SpawnTransform = SpawnPoint->GetActorTransform();

		if (AMyProjectPawn* RespawnedVehicle = GetWorld()->SpawnActor<AMyProjectPawn>(VehiclePawnClass, SpawnTransform))
		{
			// possess the vehicle
			Possess(RespawnedVehicle);
		}
	}
}

void ATimeTrialPlayerController::OnSelectedRaceChanged(int32 RaceIndex)
{
	// the race is chosen before the countdown, which gives the car time to settle at its new start position
	if (!bRaceStarted)
	{
		PositionVehicleForRace();
	}
}

void ATimeTrialPlayerController::OnSelectedCarChanged(int32 CarIndex)
{
	ApplySelectedCar();
}

void ATimeTrialPlayerController::ApplyTransmission()
{
	if (VehiclePawn)
	{
		VehiclePawn->SetUseAutomaticGears(UDrivingSettings::IsAutomaticTransmission(GetLocalPlayerIndex(this)));
	}
}

void ATimeTrialPlayerController::ApplySelectedCar()
{
	ApplyTransmission();

	const ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	// levels that offer no cars keep the vehicle and HUD as they are. Each player drives the car they picked
	const FTimeTrialCarChoice* Car = GM ? GM->GetCarChoiceForPlayer(GetLocalPlayerIndex(this)) : nullptr;

	if (!Car)
	{
		return;
	}

	if (VehiclePawn)
	{
		VehiclePawn->SetBodyClass(Car->BodyClass);
		VehiclePawn->SetHandling(Car->Handling);

		if (Car->EngineSound)
		{
			VehiclePawn->SetEngineSound(Car->EngineSound);
		}
	}

	// each car can bring its own tachometer design
	if (VehicleUI)
	{
		VehicleUI->SetTachometerStyle(Car->TachometerStyle);
	}
}

void ATimeTrialPlayerController::PositionVehicleForRace()
{
	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	if (!GM || !VehiclePawn)
	{
		return;
	}

	FTransform GridSlot;

	if (GM->GetPlayerGridTransform(this, GridSlot))
	{
		// racing NPCs or the other player: take our place on the starting grid
		TeleportVehicle(GridSlot.GetLocation(), GridSlot.Rotator());

	} else if (const APlayerStart* RaceStart = GM->FindRaceStart(GM->GetSelectedRace()))
	{
		TeleportVehicle(RaceStart->GetActorLocation(), RaceStart->GetActorRotation());

	} else if (const ATimeTrialTrackGate* FirstGate = GM->GetFirstGateForSelectedRace()) {

		// no dedicated start for this race: the level's own start, turned to face the first gate. A retry
		// or a switch from a race with its own start needs the car brought back there
		const APlayerStart* DefaultStart = GM->FindDefaultStart();
		const FVector StartLocation = DefaultStart ? DefaultStart->GetActorLocation() : VehiclePawn->GetActorLocation();

		FVector ToGate = FirstGate->GetActorLocation() - StartLocation;
		ToGate.Z = 0.0f;

		if (!ToGate.IsNearlyZero())
		{
			TeleportVehicle(StartLocation, ToGate.Rotation());
		}
	}
}

void ATimeTrialPlayerController::PauseRace()
{
	if (!CanPause() || PauseMenu || !PauseMenuClass)
	{
		return;
	}

	if (!SetPause(true))
	{
		return;
	}

	// widget animations play on through a pause, so the countdown has to be held as well
	if (UIWidget)
	{
		UIWidget->SetCountdownPaused(true);
	}

	PauseMenu = CreateWidget<UPauseMenuUI>(this, PauseMenuClass);

	if (PauseMenu)
	{
		PauseMenu->AddToViewport(20);
	}
}

void ATimeTrialPlayerController::ResumeRace()
{
	if (!PauseMenu)
	{
		return;
	}

	ClosePauseMenu();

	if (UIWidget)
	{
		UIWidget->SetCountdownPaused(false);
	}
}

void ATimeTrialPlayerController::ClosePauseMenu()
{
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
	}

	SetPause(false);
}

void ATimeTrialPlayerController::RetryRace()
{
	if (bLeavingRace)
	{
		return;
	}

	bLeavingRace = true;

	// timers stand still while the game is paused, and the restart waits on one. The countdown stays held
	ClosePauseMenu();

	for (ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		if (PC->VehiclePawn)
		{
			PC->VehiclePawn->DisableInput(PC);
		}
	}

	const float FadeTime = FMath::Max(RetryFadeTime, 0.1f);

	FadeAllCameras(0.0f, 1.0f, FadeTime, false, true);

	FTimerHandle RestartTimer;
	GetWorldTimerManager().SetTimer(RestartTimer, this, &ATimeTrialPlayerController::RestartRace, FadeTime, false);
}

void ATimeTrialPlayerController::RestartRace()
{
	for (ATimeTrialPlayerController* PC : GetRaceControllers())
	{
		PC->ResetForRetry();
	}

	// the countdown lines the cars up again: fresh NPCs on the grid, and our car back at the start
	if (UIWidget)
	{
		UIWidget->RetryRace();
	}

	FadeAllCameras(1.0f, 0.0f, FMath::Max(RetryFadeTime, 0.1f), false, false);
}

void ATimeTrialPlayerController::ResetForRetry()
{
	bLeavingRace = false;
	bRaceUnderway = false;
	bRaceStarted = false;
	bRaceFinished = false;
	CurrentLap = 0;

	// no gate to chase until the race starts again; this also clears the target highlight
	SetTargetGate(nullptr);

	// a race cut short leaves no ghost, and the countdown brings the ghost back to the start
	GhostRecorder.Stop();
	DestroyGhost();

	if (VehiclePawn)
	{
		VehiclePawn->DisableInput(this);
		VehiclePawn->ResetDrivingState();
	}

	// the second player's HUD clears its laps. The first player's runs the countdown again (RestartRace)
	if (UIWidget && !IsPrimaryPlayer())
	{
		UIWidget->RetryRace();
	}
}

void ATimeTrialPlayerController::ReturnToTitle()
{
	if (bLeavingRace)
	{
		return;
	}

	bLeavingRace = true;

	// timers stand still while the game is paused, and the reload waits on one
	ClosePauseMenu();

	const float FadeTime = FMath::Max(TitleFadeTime, 0.1f);

	FadeAllCameras(0.0f, 1.0f, FadeTime, true, true);

	if (MusicComponent)
	{
		MusicComponent->OnAudioFinished.RemoveDynamic(this, &ATimeTrialPlayerController::OnMusicTrackFinished);
		MusicComponent->FadeOut(FadeTime, 0.0f);
	}

	// starting the level over puts everything back as it was, the title screen included. Local players outlast the
	// reload, so the second player leaves first, or the title screen would come up split
	FTimerHandle RestartTimer;
	GetWorldTimerManager().SetTimer(RestartTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()))
		{
			GM->RemoveSecondPlayer();
		}

		RestartLevel();

	}), FadeTime, false);
}

void ATimeTrialPlayerController::PlayMusic(bool bRaceMusic)
{
	if (!IsLocalPlayerController() || (bRaceMusic == bPlayingRaceMusic && MusicComponent))
	{
		return;
	}

	// let the music that's on fade away, without it calling for its next track as it stops
	if (MusicComponent)
	{
		MusicComponent->OnAudioFinished.RemoveDynamic(this, &ATimeTrialPlayerController::OnMusicTrackFinished);
		MusicComponent->FadeOut(MusicFadeTime, 0.0f);
		MusicComponent = nullptr;
	}

	bPlayingRaceMusic = bRaceMusic;
	MusicTrack = 0;

	StartMusicTrack(MusicFadeTime);
}

void ATimeTrialPlayerController::StartMusicTrack(float FadeTime)
{
	const TArray<TObjectPtr<USoundBase>>& Tracks = bPlayingRaceMusic ? RaceMusic : MenuMusic;

	if (Tracks.IsEmpty())
	{
		return;
	}

	USoundBase* Track = Tracks[MusicTrack % Tracks.Num()];

	if (!Track)
	{
		return;
	}

	// not tied to any place in the world, and gone once it stops
	MusicComponent = UGameplayStatics::CreateSound2D(this, Track, 1.0f, 1.0f, 0.0f, nullptr, false, true);

	if (!MusicComponent)
	{
		return;
	}

	MusicComponent->OnAudioFinished.AddDynamic(this, &ATimeTrialPlayerController::OnMusicTrackFinished);
	MusicComponent->SetVolumeMultiplier(UGameAudioSettings::GetMusicVolume());
	MusicComponent->FadeIn(FadeTime, MusicVolume);

	UE_LOG(LogMyProject, Log, TEXT("[Music] Playing %s (%s music)"), *Track->GetName(), bPlayingRaceMusic ? TEXT("race") : TEXT("menu"));
}

void ATimeTrialPlayerController::OnMusicTrackFinished()
{
	// next in the list, back to the first after the last
	++MusicTrack;

	StartMusicTrack(0.0f);
}

bool ATimeTrialPlayerController::FindCourseResetTransform(FTransform& OutTransform)
{
	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());

	if (!GM || !VehiclePawn)
	{
		return false;
	}

	const FRaceLine& Line = GM->GetRaceLine();

	if (!Line.IsValid())
	{
		return false;
	}

	// during the race, the distance the race has tracked keeps us on our own stretch of road where the course passes
	// close to itself, such as under a bridge it crosses later
	float Distance = 0.0f;

	if (!GM->GetRacerLineDistance(this, Distance))
	{
		Distance = Line.FindDistance(VehiclePawn->GetActorLocation());
	}

	GM->FindResetSpot(VehiclePawn, Distance, OutTransform);

	return true;
}

void ATimeTrialPlayerController::SpawnGhost()
{
	DestroyGhost();

	ATimeTrialGameMode* GM = Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode());
	UTimeTrialRecords* Records = UTimeTrialRecords::Get(this);

	// ghosts are for time trials on your own: the NPCs or the other player are enough to race against
	if (!bShowGhost || !GM || !Records || GM->IsVersusNpc() || GM->IsTwoPlayers())
	{
		return;
	}

	FGhostTrack Track;

	if (!Records->FindGhost(GM->GetSelectedCourseId(), GM->GetLapsForSelectedRace(), Track))
	{
		return;
	}

	// the ghost drives the car it was recorded in, as long as that car is still on offer
	TSubclassOf<AActor> BodyClass = GM->GetSelectedCarBodyClass();

	for (const FTimeTrialCarChoice& Choice : GM->GetCarChoices())
	{
		if (Choice.DisplayName.ToString() == Track.CarName)
		{
			BodyClass = Choice.BodyClass;
			break;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GhostCar = GetWorld()->SpawnActor<ATimeTrialGhostCar>(ATimeTrialGhostCar::StaticClass(), Track.Sample(0.0f), SpawnParams);

	if (GhostCar)
	{
		USkeletalMesh* FallbackMesh = IsValid(VehiclePawn) ? VehiclePawn->GetMesh()->GetSkeletalMeshAsset() : nullptr;

		GhostCar->SetUp(Track, BodyClass, FallbackMesh, GhostMaterial, GhostColor, GhostOpacity);

		UE_LOG(LogMyProject, Log, TEXT("[Ghost] Racing the %.3f s ghost in %s"), Track.TotalTime, *Track.CarName);
	}
}

void ATimeTrialPlayerController::DestroyGhost()
{
	if (GhostCar)
	{
		GhostCar->Destroy();
		GhostCar = nullptr;
	}
}

FTransform ATimeTrialPlayerController::GetGhostTransform() const
{
	return VehiclePawn->GetMesh()->GetComponentTransform();
}

void ATimeTrialPlayerController::TeleportVehicle(const FVector& Location, const FRotator& Rotation)
{
	if (!VehiclePawn)
	{
		return;
	}

	// stop any residual motion so the car doesn't carry momentum through the move
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(VehiclePawn->GetRootComponent()))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// only yaw is used: the car should sit flat on the road regardless of how the start was rotated
	VehiclePawn->SetActorLocationAndRotation(Location, FRotator(0.0f, Rotation.Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
}

bool ATimeTrialPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
