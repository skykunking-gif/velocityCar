// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Controller.h"
#include "RaceNpcController.generated.h"

class AMyProjectPawn;
class ATimeTrialGameMode;
struct FRaceLine;

/**
 *  How well an NPC drives. The game mode offers a few of these as difficulty levels.
 */
USTRUCT(BlueprintType)
struct FNpcDifficulty
{
	GENERATED_BODY()

	/** Name shown on the title screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	FText DisplayName;

	/** Name the save file knows the level by, to remember the player has beaten it. Never change it once released */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	FName Id;

	/** Keeps the level off the title screen until the player has won a race against the level before it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	bool bLockedUntilPreviousBeaten = false;

	/** Fastest the NPC will drive anywhere on the track */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (Units = "cm/s", ClampMin = "100"))
	float MaxSpeed = 4000.0f;

	/** How close to the most its car can corner the NPC dares to go, from 0 to 1. Higher takes corners faster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float CornerLimit = 0.75f;

	/** Deceleration the NPC plans its braking with, in cm/s². Higher brakes later */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "50"))
	float BrakeDecel = 700.0f;

	/** Most throttle the NPC ever uses */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MaxThrottle = 1.0f;

	/** How far the NPCs' pace is spread around this level, as a fraction, so they don't all drive alike */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PaceSpread = 0.04f;

	/** Lap time the NPCs aim for on each route, in seconds, keyed by route number (see FTimeTrialRaceDefinition::Route),
	 *  whichever way round it's driven. They speed up or ease off as they go to keep to it, never pushing past CornerLimit.
	 *  Routes without an entry are simply driven at CornerLimit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	TMap<int32, float> TargetLapTimes;

	/** Gives the NPC's car the turning it has on the ground, rather than the slow turning the player's car keeps
	 *  (see AMyProjectPawn::SetGroundedTurnDamping). Lets the NPCs corner much faster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	bool bGroundedHandling = false;

	/** How far ahead along the line the NPC steers for, in seconds of driving at its speed. Further ahead steers more
	 *  calmly at speed, so the car doesn't weave; nearer follows the line more tightly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (Units = "s", ClampMin = "0.1", ClampMax = "2.0"))
	float LookAheadTime = 0.4f;

	/** How far ahead the NPC slows for what's coming, in seconds of driving at its speed. The planned speeds already
	 *  brake in time for each corner, so this only covers how slowly the car answers its pedals. Shorter keeps the
	 *  car at speed for longer before a corner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (Units = "s", ClampMin = "0.0", ClampMax = "2.0"))
	float BrakeLookAheadTime = 0.6f;

	/** Multiplies the engine torque of the NPCs' cars, on top of the car's own handling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "0.1"))
	float CarTorqueScale = 1.0f;

	/** Multiplies the tire grip of the NPCs' cars, front and rear, on top of the car's own handling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta = (ClampMin = "0.1"))
	float CarGripScale = 1.0f;

	/** Push a car gets right behind another, the player's as much as an NPC's, as an acceleration in cm/s². It fades out
	 *  with the distance behind the car ahead (SlipstreamRange) and at low speed. 0 turns slipstreams off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race Battle", meta = (ClampMin = "0.0"))
	float SlipstreamAccel = 0.0f;

	/** Most push a car gets for being behind the leader, the player's as much as an NPC's, as an acceleration in cm/s².
	 *  It grows with the distance behind the leader up to CatchUpFullGap. An NPC behind the player also drives
	 *  flat out rather than keeping to its lap time. 0 leaves everyone to their own pace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race Battle", meta = (ClampMin = "0.0"))
	float CatchUpAccel = 0.0f;

	/** Share of CatchUpAccel the player gets. Under 1 so a player behind isn't pushed about too hard to drive */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race Battle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PlayerCatchUpScale = 1.0f;

	/** Most an NPC ahead of the player eases off its lap time, as a share of it, so a player who's dropped behind
	 *  can get back into the race. It grows with the distance the player is behind, up to CatchUpFullGap */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race Battle", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float LeadEase = 0.0f;

	/** Distance behind the leader at which a car gets the whole of CatchUpAccel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Race Battle", meta = (Units = "cm", ClampMin = "100.0"))
	float CatchUpFullGap = 30000.0f;
};

/**
 *  Drives an NPC car around the race line: steers for a point a little way ahead along it, plans its speed
 *  from how sharply the line bends further on, goes round slower cars, and gets itself going again when it
 *  gets stuck or knocked off.
 *
 *  How hard the car turns for a given steering input is learned while driving, so the same driver copes with
 *  every car on offer, however each one handles.
 */
UCLASS()
class ARaceNpcController : public AController
{
	GENERATED_BODY()

public:

	ARaceNpcController();

	/** Sets how well this NPC drives. PaceFactor scales its speed on top, so NPCs of one level differ a little */
	void SetDifficulty(const FNpcDifficulty& InDifficulty, float InPaceFactor);

	/** Sets where across the road this NPC likes to drive, as an offset to the right of the race line */
	void SetPreferredLane(float LaneOffset);

	/** Lets go of the brakes and starts racing */
	void StartDriving();

	/** Called once this NPC has run all its laps. It carries on round at an easy pace */
	void NotifyFinished();

	/** Distance along the race line this NPC last measured itself at */
	float GetLineDistance() const { return LineDistance; }

	/** True once the race has started for this NPC */
	bool IsDriving() const { return bDriving; }

protected:

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Reads the wheel base and steering lock off the car, for steering at low speed */
	void MeasureVehicle();

	/** Plans the speed for every sample of the race line: as fast as the car can take each corner, braking in time for the next */
	void BuildSpeedProfile(const FRaceLine& Line);

	/** Front wheel angle the steering allows at a speed, in degrees */
	float GetMaxSteerAngle(float Speed) const;

	/** Sideways acceleration full steering gives at a speed, as learned so far, in cm/s² */
	float GetFullSteerAccel(float Speed) const;

	/** Learns how hard the car turns for its steering from how it's turning now */
	void LearnSteering(float Speed, float Steering, float YawRate, float DeltaSeconds);

	/** Keeps the time spent against the time planned, so the pace can be set to make the target lap time */
	void TrackPace(const FRaceLine& Line, float Moved, float DeltaSeconds);

	/** Sets how hard to push to make the target lap time, from how the car has kept to its plan so far */
	void UpdatePace();

	/** Times the lap just finished, and aims the next one to make up for how far it came out from the target */
	void FinishLap();

	/** Moves round slower cars and out of the way of cars alongside. Returns the most this car should go
	 *  while it can't get past the car ahead, or a very large number if it's free to drive */
	float AvoidOtherRacers(const FRaceLine& Line, float Speed, float DeltaSeconds);

	/** Watches for the car being stuck, off the road or facing the wrong way. A stuck car backs off whatever
	 *  it ran into; one that stays stuck, or ends up off the road or the wrong way round, is put back on the line */
	void CheckForRecovery(const FRaceLine& Line, float DeltaSeconds);

	/** Puts the car back on the race line where it is, facing along it */
	void Recover();

	/** Returns the game mode, which holds the race line and the other racers */
	ATimeTrialGameMode* GetRaceGameMode() const;

	/** The car being driven */
	TWeakObjectPtr<AMyProjectPawn> Vehicle;

	/** Driving skill */
	FNpcDifficulty Difficulty;

	/** Speed scale on top of the difficulty */
	float PaceFactor = 1.0f;

	/** Lap time this NPC aims for on the race being run, or zero to drive at the difficulty's limits */
	float TargetLapTime = 0.0f;

	/** Share of the difficulty's speed the NPC drives at while keeping to a target lap time */
	float Push = 1.0f;

	/** Lap time the speed plan works out at */
	float PlannedLapTime = 0.0f;

	/** Time spent racing lately, and the time the plan said that stretch would take, both fading over half a minute.
	 *  Their ratio is how much slower than planned the car really goes, from accelerating, traffic and mistakes */
	float ActualRaceTime = 0.0f;
	float PlannedRaceTime = 0.0f;

	/** Time the lap being run started, or below zero before the first time over the finish line */
	float LapStartTime = -1.0f;

	/** Correction to the lap time aimed for, from how far each lap so far came out from the target */
	float TargetBias = 1.0f;

	/** Share this NPC eases off its lap time by while the player is behind it (see FNpcDifficulty::LeadEase) */
	float LeadEase = 0.0f;

	/** LeadEase summed over the lap being run, by time, to know the lap time it was meant to come out at */
	float LapEaseTime = 0.0f;

	/** True if the NPC chased the player flat out at some point of the lap being run */
	bool bChasedThisLap = false;

	/** Planned speed at each sample of the race line */
	TArray<float> SpeedProfile;

	/** Length of the race line the speed profile was planned for, to notice when the line changes */
	float ProfileLineLength = -1.0f;

	/** Time until the speed plan is next brought up to date with what's been learned about the car */
	float ReplanTime = 0.0f;

	/** Sideways acceleration full steering gives, for each step of speed, learned while driving */
	TArray<float> FullSteerAccel;

	/** Distance between the front and rear axles */
	float WheelBase = 270.0f;

	/** Most the front wheels steer at a standstill */
	float MaxSteerAngle = 40.0f;

	/** Where along the race line the car is */
	float LineDistance = 0.0f;

	/** True once LineDistance has been found on the whole line */
	bool bLocated = false;

	/** Current offset to the right of the race line */
	float LaneOffset = 0.0f;

	/** Offset the car drifts back to when nothing's in the way */
	float PreferredLane = 0.0f;

	/** True while racing */
	bool bDriving = false;

	/** True once all laps are run */
	bool bFinished = false;

	/** Time since the progress along the line was last checked */
	float ProgressCheckTime = 0.0f;

	/** Where along the line the car was at the last progress check */
	float ProgressCheckDistance = 0.0f;

	/** Time left backing away from whatever the car got stuck on */
	float ReverseTimeLeft = 0.0f;

	/** Steering held while backing away */
	float ReverseSteering = 0.0f;

	/** Times in a row the car has had to back away without getting going again */
	int32 ReverseAttempts = 0;

	/** How long the car has been off the road */
	float OffRoadTime = 0.0f;

	/** How long the car has been facing the wrong way */
	float WrongWayTime = 0.0f;

	/** Time since the car was last put back on the line, so it gets a moment to set off again */
	float TimeSinceRecovery = 0.0f;

	/** True while the brake lights are on */
	bool bBraking = false;
};
