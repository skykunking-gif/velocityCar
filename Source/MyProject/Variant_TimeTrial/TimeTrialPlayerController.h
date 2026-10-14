// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/SlateWrapperTypes.h"
#include "TimeTrialGhost.h"
#include "TimeTrialPlayerController.generated.h"

class ATimeTrialTrackGate;
class UTimeTrialUI;
class UPauseMenuUI;
class UInputMappingContext;
class UMyProjectUI;
class AMyProjectPawn;
class ATitleCameraDolly;
class USoundBase;
class UAudioComponent;
class UMaterialInterface;

/**
 *  A simple PlayerController for a Time Trial racing game
 */
UCLASS(abstract, Config="Game")
class ATimeTrialPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** If true, the optional steering wheel input mapping context will be registered */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls")
	bool bUseSteeringWheelControls = false;

	/** Optional Input Mapping Context to be used for steering wheel input.
	 *  This is added alongside the default Input Mapping Context and does not block other forms of input.
	 */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls", meta = (EditCondition = "bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	/** Type of UI widget to spawn*/
	UPROPERTY(EditAnywhere, Category="Time Trial|UI")
	TSubclassOf<UTimeTrialUI> UIWidgetClass;

	/** Pointer to the UI Widget */
	UPROPERTY()
	TObjectPtr<UTimeTrialUI> UIWidget;

	/** Type of the UI to spawn */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UMyProjectUI> VehicleUIClass;

	/** Pointer to the UI widget */
	UPROPERTY()
	TObjectPtr<UMyProjectUI> VehicleUI;

	/** Type of the menu shown while the race is paused */
	UPROPERTY(EditAnywhere, Category="Time Trial|Pause")
	TSubclassOf<UPauseMenuUI> PauseMenuClass;

	/** The pause menu, while the race is paused */
	UPROPERTY()
	TObjectPtr<UPauseMenuUI> PauseMenu;

	/** Keys that pause the race, and carry on with it from the pause menu */
	UPROPERTY(EditAnywhere, Category="Time Trial|Pause")
	TArray<FKey> PauseKeys;

	/** How long the screen takes to fade to black before a retry, and to fade back in on the grid */
	UPROPERTY(EditAnywhere, Category="Time Trial|Pause", meta = (Units = "s", ClampMin = "0"))
	float RetryFadeTime = 0.4f;

	/** True from the countdown until the race is run again or left. The race can be paused until the player finishes */
	bool bRaceUnderway = false;

	/** True while the screen fades out to run the race again or to go back to the title screen */
	bool bLeavingRace = false;

	/** Next track gate the car should pass */
	TObjectPtr<ATimeTrialTrackGate> TargetGate;

	/** Lap counter */
	int32 CurrentLap = 0;

	/** If true, the race has already started */
	bool bRaceStarted = false;

	/** If true, the player has completed all laps of the race */
	bool bRaceFinished = false;

	/** Type of vehicle to automatically respawn when it's destroyed */
	UPROPERTY(EditAnywhere, Category="Vehicle|Respawn")
	TSubclassOf<AMyProjectPawn> VehiclePawnClass;

	/** Pointer to the controlled vehicle pawn */
	TObjectPtr<AMyProjectPawn> VehiclePawn;

	/** The second player's own keys on the right hand side of the keyboard, when the two share it. Made in code
	 *  from the actions of the default mapping contexts */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SecondPlayerKeyboardContext;

	/** Pointer to the title camera dolly in the level, if any */
	TObjectPtr<ATitleCameraDolly> TitleDolly;

	/** How long the screen takes to fade to black when leaving the title screen, and to fade back in on the vehicle */
	UPROPERTY(EditAnywhere, Category="Title Screen", meta = (Units = "s", ClampMin = "0"))
	float TitleFadeTime = 0.4f;

	/** True while the title screen is up: the view stays on the title dolly and the vehicle HUD is hidden */
	bool bTitleSequenceActive = false;

	/** Visibility of the vehicle HUD before the title screen hid it */
	ESlateVisibility VehicleUIVisibilityBeforeTitle = ESlateVisibility::SelfHitTestInvisible;

	/** Music for the title screen and the menus before a race. The tracks play in order, round and round */
	UPROPERTY(EditAnywhere, Category="Music")
	TArray<TObjectPtr<USoundBase>> MenuMusic;

	/** Music from the race countdown on. The tracks play in order, round and round */
	UPROPERTY(EditAnywhere, Category="Music")
	TArray<TObjectPtr<USoundBase>> RaceMusic;

	/** Volume of the music at the highest level of the music volume option */
	UPROPERTY(EditAnywhere, Category="Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicVolume = 0.6f;

	/** Time one piece of music takes to fade into the next when the menus give way to the race */
	UPROPERTY(EditAnywhere, Category="Music", meta = (Units = "s", ClampMin = "0.0"))
	float MusicFadeTime = 1.5f;

	/** The music playing now */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	/** True while the race music plays, false for the menu music */
	bool bPlayingRaceMusic = false;

	/** Index of the playing track in its list */
	int32 MusicTrack = 0;

	/** If true, a time trial is run against the ghost of the fastest race on the course */
	UPROPERTY(EditAnywhere, Category="Time Trial|Ghost")
	bool bShowGhost = true;

	/** How many times a second the car's position is recorded for its ghost */
	UPROPERTY(EditAnywhere, Category="Time Trial|Ghost", meta = (ClampMin = "5", ClampMax = "120"))
	float GhostSampleRate = 30.0f;

	/** Translucent material the ghost is drawn in. Its "Glass Color" and "Base Glass Opacity" parameters are set
	 *  from GhostColor and GhostOpacity. Leave empty to draw the ghost in the car's own paint */
	UPROPERTY(EditAnywhere, Category="Time Trial|Ghost")
	TObjectPtr<UMaterialInterface> GhostMaterial;

	/** Tint of the ghost */
	UPROPERTY(EditAnywhere, Category="Time Trial|Ghost")
	FLinearColor GhostColor = FLinearColor(0.35f, 0.7f, 1.0f, 1.0f);

	/** How solid the ghost looks, from 0 (invisible) to 1 */
	UPROPERTY(EditAnywhere, Category="Time Trial|Ghost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostOpacity = 0.3f;

	/** Records our race, to keep as the course's ghost if it's the fastest */
	FGhostRecorder GhostRecorder;

	/** Game time the race started at, which the ghost recording counts from */
	float GhostRecordStartTime = 0.0f;

	/** The ghost we're racing, if any */
	UPROPERTY(Transient)
	TObjectPtr<ATimeTrialGhostCar> GhostCar;

public:

	/** Constructor */
	ATimeTrialPlayerController();

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input initialization */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* aPawn) override;

	/** Opens and closes the pause menu with the pause keys while a race is under way */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

public:

	/** UI vehicle state update on tick */
	virtual void Tick(float Delta) override;

public:

	/** Sets up the race start */
	UFUNCTION()
	void StartRace();

	/** Shows the title flythrough: cuts to the level's title camera dolly (if any) and hides the vehicle HUD */
	void BeginTitleSequence();

	/** Leaves the title flythrough: fades out, cuts to the vehicle and fades back in.
	 *  OnVehicleShown runs once the view is on the vehicle, so the next screen can take over the camera from there. */
	void EndTitleSequence(TFunction<void()> OnVehicleShown);

	/** Fades the screen and the music out, then reloads the level, which starts again from the title screen */
	void ReturnToTitle();

	/** Goes back from the screens before the race to the title flythrough without reloading anything: fades out,
	 *  cuts to the title camera dolly and fades back in. OnTitleShown runs at the cut, so the title screen can go up */
	void BackToTitleSequence(TFunction<void()> OnTitleShown);

	/** Pauses the game and puts up the pause menu, if the race is at a point it can be paused */
	void PauseRace();

	/** Takes the pause menu down and carries on with the race */
	void ResumeRace();

	/** Fades out and runs the same race again from the countdown, with the same car, route and opponents */
	void RetryRace();

	/** Lines the cars up for the start once a race has been chosen: in a race against NPCs, spawns them on the
	 *  grid and puts the player's car in its slot. Call before the countdown, so the cars settle during it */
	void PrepareRaceStart();

	/** Moves on to the next lap, and ends the race once all laps are complete */
	void IncrementLapCount();

	/** Returns the current target track gate */
	ATimeTrialTrackGate* GetTargetGate() const;

	/** Sets the target track gate for this player */
	void SetTargetGate(ATimeTrialTrackGate* Gate);

	/** Returns true once the player has completed all laps */
	UFUNCTION(BlueprintPure, Category="Time Trial")
	bool IsRaceFinished() const { return bRaceFinished; };

	/** Returns the race HUD, which on the first player's controller also runs the screens before and after the race */
	UTimeTrialUI* GetRaceUI() const { return UIWidget; };

	/** Returns the first local player's controller, which runs the menus, the music and the race for both players */
	ATimeTrialPlayerController* GetPrimaryController() const;

	/** Returns every local player's controller, the first player's first */
	TArray<ATimeTrialPlayerController*> GetRaceControllers() const;

protected:

	/** Handles pawn destruction and respawning */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedPawn);

	/** Moves the vehicle to the start position for the newly selected race, as long as the race hasn't started yet */
	UFUNCTION()
	void OnSelectedRaceChanged(int32 RaceIndex);

	/** Gives the vehicle the looks and handling of the newly selected car */
	UFUNCTION()
	void OnSelectedCarChanged(int32 CarIndex);

	/** Gives the vehicle the looks and handling, and the HUD the tachometer, of the car selected on the game mode */
	void ApplySelectedCar();

	/** Places the vehicle at the race-specific PlayerStart for the selected race if the level has one,
	 *  otherwise turns it in place to face the first gate of the route */
	void PositionVehicleForRace();

	/** Switches the view from the title dolly to the vehicle once the screen has faded out, and fades back in */
	void CutFromTitleToVehicle();

	/** Returns true while the race is at a point it can be paused: from the countdown until every player finishes */
	bool CanPause() const;

	/** Opens or closes the pause menu for a pause key, if the race is at a point it can be. Returns true if it did */
	bool HandlePauseKey();

	/** Sets this player off at the start of the race: the first gate, the first lap and the controls */
	void StartRaceForPlayer();

	/** Puts this player back to before the countdown, for a retry */
	void ResetForRetry();

	/** Fades every local player's view, so a split screen goes black and comes back as one */
	void FadeAllCameras(float FromAlpha, float ToAlpha, float Duration, bool bShouldFadeAudio, bool bHoldWhenFinished) const;

	/** Gives the second player their keys on the right hand side of the keyboard, when the two players share it */
	void AddSecondPlayerKeyboardKeys(class UEnhancedInputLocalPlayerSubsystem* Subsystem);

	/** Takes the pause menu down and unpauses the game, leaving the countdown held */
	void ClosePauseMenu();

	/** Puts the race back to before the countdown once the screen is black, then runs the countdown again */
	void RestartRace();

	/** Switches to the menu or the race music, fading the one playing out. Does nothing if that music is already on */
	void PlayMusic(bool bRaceMusic);

	/** Starts the current track of the current music, fading it in over FadeTime */
	void StartMusicTrack(float FadeTime);

	/** Moves on to the next track once one finishes */
	UFUNCTION()
	void OnMusicTrackFinished();

	/** Follows the music volume set on the options screen */
	void OnAudioSettingsChanged();

	/** Our subscription to the volume settings */
	FDelegateHandle AudioSettingsChangedHandle;

	/** Gives the vehicle the automatic or the manual gearbox, as picked on the car select screen */
	void ApplyTransmission();

	/** Our subscription to the driving settings */
	FDelegateHandle DrivingSettingsChangedHandle;

	/** Where a reset puts the vehicle: the middle of the road at the nearest point of the race line, facing the way the
	 *  race runs. Returns false if there's no race line, leaving the car to right itself where it is */
	bool FindCourseResetTransform(FTransform& OutTransform);

	/** Puts the ghost of the course's fastest race on the start, in a time trial with a ghost recorded */
	void SpawnGhost();

	/** Takes the ghost off the track */
	void DestroyGhost();

	/** Where the car's ghost should be: the vehicle mesh, which a swapped-in body is attached to */
	FTransform GetGhostTransform() const;

	/** Teleports the vehicle, clearing any physics motion so it doesn't carry momentum through the move */
	void TeleportVehicle(const FVector& Location, const FRotator& Rotation);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;
};
