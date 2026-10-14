// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "MyProjectPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UChaosWheeledVehicleMovementComponent;
class UEngineSoundComponent;
class UCollisionFxComponent;
class UEngineSoundProfile;
struct FInputActionValue;

/** Asked where a reset puts the car. Returns false to leave the car where it is */
DECLARE_DELEGATE_RetVal_OneParam(bool, FFindResetTransformDelegate, FTransform& /*OutTransform*/);

/**
 *  Changes to how the vehicle pawn drives, so cars that share one pawn can handle differently.
 *  The defaults leave the pawn's own handling as it is.
 */
USTRUCT(BlueprintType)
struct FVehicleHandling
{
	GENERATED_BODY()

	/** Drives all four wheels instead of the wheels the pawn itself drives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling")
	bool bAllWheelDrive = false;

	/** Share of the engine torque sent to the rear wheels with all wheel drive. Leaning to the rear keeps the car willing to turn on the throttle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling", meta = (EditCondition = "bAllWheelDrive", ClampMin = "0.0", ClampMax = "1.0"))
	float RearTorqueShare = 0.5f;

	/** Multiplies the engine torque. Below 1 the car picks up speed more slowly, and its top speed drops a little too */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling", meta = (ClampMin = "0.1"))
	float EngineTorqueScale = 1.0f;

	/** Multiplies how far the front wheels steer at speed, where the pawn normally cuts the steering back.
	 *  The full lock the car has when slow stays the same */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling", meta = (ClampMin = "1.0"))
	float SteeringAtSpeedScale = 1.0f;

	/** Multiplies the grip of the front tires. Above 1 the nose pushes wide less in corners */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling", meta = (ClampMin = "0.1"))
	float FrontGripScale = 1.0f;

	/** Multiplies the grip of the rear tires. Below 1 the tail steps out more easily */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Handling", meta = (ClampMin = "0.1"))
	float RearGripScale = 1.0f;

	bool operator==(const FVehicleHandling& Other) const
	{
		return bAllWheelDrive == Other.bAllWheelDrive && RearTorqueShare == Other.RearTorqueShare && EngineTorqueScale == Other.EngineTorqueScale
			&& SteeringAtSpeedScale == Other.SteeringAtSpeedScale && FrontGripScale == Other.FrontGripScale && RearGripScale == Other.RearGripScale;
	}
};

/**
 *  Vehicle Pawn class
 *  Handles common functionality for all vehicle types,
 *  including input handling and camera management.
 *  
 *  Specific vehicle configurations are handled in subclasses.
 */
UCLASS(abstract)
class AMyProjectPawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

	/** Spring Arm for the front camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* FrontSpringArm;

	/** Front Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FrontCamera;

	/** Spring Arm for the back camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* BackSpringArm;

	/** Back Camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* BackCamera;

	/** Driver's seat camera component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* CockpitCamera;

	/** Plays the engine, following the revs */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UEngineSoundComponent* EngineSound;

	/** Sparks, light and noise where the car knocks and scrapes against guardrails and other cars */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Components", meta = (AllowPrivateAccess = "true"))
	UCollisionFxComponent* CollisionFx;

	/** Cast pointer to the Chaos Vehicle movement component */
	TObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosVehicleMovement;

protected:

	/** Steering Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* SteeringAction;

	/** Throttle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ThrottleAction;

	/** Brake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BrakeAction;

	/** Handbrake Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HandbrakeAction;

	/** Gear Up Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* GearUpAction;

	/** Gear Down Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* GearDownAction;

	/** Look Around Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAroundAction;

	/** Toggle Camera Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ToggleCameraAction;

	/** Reset Vehicle Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* ResetVehicleAction;

	/** The views the toggle camera action cycles through, in order */
	enum class ECameraView : uint8
	{
		Back,
		Front,
		Cockpit,
		Count
	};

	/** Keeps track of which camera is active */
	ECameraView CameraView = ECameraView::Back;

	/** Driver's eye position relative to the steering wheel of a swapped-in body (see SetBodyClass) */
	UPROPERTY(EditAnywhere, Category="Camera")
	FVector CockpitEyeOffset = FVector(-50.0f, 0.0f, 26.0f);

	/** Driver's eye position on the vehicle mesh, used while the pawn shows its own meshes
	 *  or the swapped-in body has no steering wheel */
	UPROPERTY(EditAnywhere, Category="Camera")
	FVector DefaultCockpitLocation = FVector(-20.0f, -38.0f, 110.0f);

	/** Field of view of the driver's seat camera */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (Units = "deg", ClampMin = "30", ClampMax = "140"))
	float CockpitFieldOfView = 80.0f;

	/** How far the chase camera swings up and tips down for each degree the car points downhill, so it looks down
	 *  on the road dropping away ahead. 1 keeps the view of the road as on the flat, 0 keeps the camera level */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = "0.0"))
	float DownhillCameraPitchScale = 1.5f;

	/** Furthest the chase camera swings up on a descent */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (Units = "deg", ClampMin = "0.0", ClampMax = "80.0"))
	float MaxDownhillCameraPitch = 25.0f;

	/** On half of a split screen, how far the chase camera swings down and tips up for each degree the car points
	 *  uphill, so a climb stays in the view, which is only half as tall. 1 tips it with the car, 0 keeps it level as
	 *  on the whole screen */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = "0.0"))
	float SplitscreenUphillCameraPitchScale = 1.0f;

	/** Furthest the chase camera tips up on a climb on half of a split screen */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (Units = "deg", ClampMin = "0.0", ClampMax = "80.0"))
	float MaxSplitscreenUphillCameraPitch = 20.0f;

	/** Keeps track of whether the car is flipped. If this is true for two flip checks, resets the vehicle automatically */
	bool bPreviousFlipCheck = false;

	/** Time between automatic flip checks */
	UPROPERTY(EditAnywhere, Category="Flip Check", meta = (Units = "s"))
	float FlipCheckTime = 3.0f;

	/** Minimum dot product value for the vehicle's up direction that we still consider upright */
	UPROPERTY(EditAnywhere, Category="Flip Check")
	float FlipCheckMinDot = -0.2f;

	/** Flip check timer */
	FTimerHandle FlipCheckTimer;

	/** Lets the car react to what it hits: a drift broken off by a wall, and the impact stabilization below. Off, hits
	 *  only make sparks and noise, and the physics alone decides what happens to the car. Hit events never reached the
	 *  car until 2026-10-08, so it has always been driven this way; turning it on changes how the car takes walls */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	bool bReactToHits = false;

	/** Enables the impact stabilization that stops collisions from throwing the car around */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	bool bStabilizeImpacts = true;

	/** Impact normals pointing up more than this are treated as ground contacts rather than walls */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	float GroundImpactNormalZ = 0.7f;

	/** Closing speed an impact needs before it gets stabilized. Light scrapes are left alone */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "cm/s"))
	float ImpactSpeedThreshold = 150.0f;

	/** Fraction of the speed along a wall that survives a scrape */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallScrapeRetention = 0.92f;

	/** Fastest a wall is allowed to push the car back out */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "cm/s"))
	float MaxWallReboundSpeed = 80.0f;

	/** Fastest an impact is allowed to launch the car upwards */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "cm/s"))
	float MaxImpactLaunchSpeed = 50.0f;

	/** Roll and pitch rate the car may keep right after an impact */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "deg/s"))
	float MaxImpactTumbleRate = 40.0f;

	/** Yaw rate the car may keep right after an impact. Zero or less leaves yaw untouched */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "deg/s"))
	float MaxImpactSpinRate = 90.0f;

	/** Hard cap on the roll and pitch rate at all times. Zero or less disables the cap */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "deg/s"))
	float MaxTumbleRate = 120.0f;

	/** Angular damping applied while the wheels are on the ground */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	float GroundAngularDamping = 0.5f;

	/** Angular damping applied while the vehicle is in midair */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	float AirborneAngularDamping = 6.0f;

	/** Extra angular damping applied while recovering from an impact */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	float ImpactAngularDamping = 8.0f;

	/** Extra linear damping applied while recovering from an impact */
	UPROPERTY(EditAnywhere, Category="Impact Stability")
	float ImpactLinearDamping = 1.0f;

	/** How long the extra damping lasts after an impact */
	UPROPERTY(EditAnywhere, Category="Impact Stability", meta = (Units = "s"))
	float ImpactRecoveryTime = 0.5f;

	/** Time left on the current impact recovery */
	float ImpactRecoveryRemaining = 0.0f;

	/** Chassis velocity from the start of the frame, before the solver resolved any collision */
	FVector PreImpactVelocity = FVector::ZeroVector;

	/** Linear damping the chassis returns to once an impact recovery finishes */
	float BaseLinearDamping = 0.0f;

	/** Enables the arcade drift: a handbrake tap while steering at speed throws the car into a held sideways slide */
	UPROPERTY(EditAnywhere, Category="Drift")
	bool bEnableArcadeDrift = true;

	/** Slowest the car can be going for a handbrake tap to start a drift */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "cm/s"))
	float DriftMinSpeed = 1500.0f;

	/** A drift ends once the car slows below this */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "cm/s"))
	float DriftExitSpeed = 800.0f;

	/** How far the stick has to be pushed to one side for the handbrake to start a drift that way */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriftEntrySteer = 0.3f;

	/** Steering within this long after the handbrake was pressed still starts a drift */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "s"))
	float DriftEntryWindow = 0.25f;

	/** Angle between the nose and the direction of travel held while steering straight */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg"))
	float DriftNeutralAngle = 25.0f;

	/** Angle held while steering fully into the drift */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg"))
	float DriftMaxAngle = 45.0f;

	/** Angle held while counter-steering fully */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg"))
	float DriftMinAngle = 5.0f;

	/** Counter-steering until the car is this straight ends the drift */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg"))
	float DriftExitAngle = 8.0f;

	/** How fast the direction of travel swings round while sliding at the full drift angle */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg/s"))
	float DriftTurnRate = 55.0f;

	/** Extra yaw rate per degree the drift angle is off its target, pulling the nose onto it */
	UPROPERTY(EditAnywhere, Category="Drift")
	float DriftAngleStiffness = 4.0f;

	/** Fastest the nose may swing while drifting */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "deg/s"))
	float DriftMaxYawRate = 150.0f;

	/** How quickly the chassis yaw rate follows the drift's target yaw rate */
	UPROPERTY(EditAnywhere, Category="Drift")
	float DriftYawResponse = 10.0f;

	/** Speed bled off while sliding with the throttle released. Full throttle bleeds a fifth of it */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (Units = "cm/s"))
	float DriftSpeedLoss = 300.0f;

	/** Tire grip while drifting, as a fraction of the normal grip, so the tires don't fight the slide */
	UPROPERTY(EditAnywhere, Category="Drift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriftTireGrip = 0.4f;

	/** True while the car is in an arcade drift */
	bool bDrifting = false;

	/** +1 while drifting to the right, -1 to the left */
	float DriftDirection = 0.0f;

	/** Speed the slide carries, kept up against the tires scrubbing it off */
	float DriftSpeed = 0.0f;

	/** Time left to steer after a handbrake press and still start a drift */
	float DriftEntryTimeLeft = 0.0f;

	/** Latest driver inputs, which the drift reads every frame */
	float SteeringInput = 0.0f;
	float ThrottleInput = 0.0f;
	float BrakeInput = 0.0f;

	/** Keeps the car on the road over crests. When all four wheels leave the ground with the road close below,
	 *  the speed carrying the car away from the road is turned back along it, so the car follows the road down
	 *  instead of flying straight on over the guardrail */
	UPROPERTY(EditAnywhere, Category="Airborne")
	bool bEnableRoadStick = true;

	/** The road stick only acts while the road is at most this far below the car. Higher up, the car is on a real jump */
	UPROPERTY(EditAnywhere, Category="Airborne", meta = (Units = "cm"))
	float RoadStickDistance = 300.0f;

	/** How quickly the speed carrying the car away from the road is turned back along it, per second. Higher sticks tighter */
	UPROPERTY(EditAnywhere, Category="Airborne", meta = (ClampMin = "0.0"))
	float RoadStickRate = 10.0f;

	/** Extra downward acceleration in cm/s² while all four wheels are off the ground, so the car comes down sooner */
	UPROPERTY(EditAnywhere, Category="Airborne", meta = (ClampMin = "0.0"))
	float AirborneExtraGravity = 980.0f;

public:
	AMyProjectPawn();

	// Begin Pawn interface

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	// End Pawn interface

	// Begin Actor interface

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Update */
	virtual void Tick(float Delta) override;

	// End Actor interface

protected:

	/** Handles steering input */
	void Steering(const FInputActionValue& Value);

	/** Handles throttle input */
	void Throttle(const FInputActionValue& Value);

	/** Handles brake input */
	void Brake(const FInputActionValue& Value);

	/** Handles brake start/stop inputs */
	void StartBrake(const FInputActionValue& Value);
	void StopBrake(const FInputActionValue& Value);

	/** Handles handbrake start/stop inputs */
	void StartHandbrake(const FInputActionValue& Value);
	void StopHandbrake(const FInputActionValue& Value);

	/** Handles gear up/down inputs */
	void GearUp(const FInputActionValue& Value);
	void GearDown(const FInputActionValue& Value);

	/** Handles look around input */
	void LookAround(const FInputActionValue& Value);

	/** Handles toggle camera input */
	void ToggleCamera(const FInputActionValue& Value);

	/** Handles reset vehicle input */
	void ResetVehicle(const FInputActionValue& Value);

public:

	/** Handle steering input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSteering(float SteeringValue);

	/** Handle throttle input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoThrottle(float ThrottleValue);

	/** Handle brake input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrake(float BrakeValue);

	/** Handle brake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStart();

	/** Handle brake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoBrakeStop();

	/** Handle handbrake start input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStart();

	/** Handle handbrake stop input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoHandbrakeStop();

	/** Shift up to the next gear by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoGearUp();

	/** Shift down to the previous gear by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoGearDown();

	/** Handle look input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoLookAround(float YawDelta);

	/** Handle toggle camera input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoToggleCamera();

	/** Handle reset vehicle input by input actions or mobile interface */
	UFUNCTION(BlueprintCallable, Category="Input")
	void DoResetVehicle();

	/** Lets a race put the car back on the course when it resets, by the reset button or after flipping over.
	 *  Unbound, or returning false, the car rights itself where it stands */
	FFindResetTransformDelegate FindResetTransform;

	/** Puts the controls back the way a car starts a race: no throttle, brake or steering, neutral gear and no drift.
	 *  Inputs held when the controls are taken away stay applied otherwise. Doesn't move the car */
	void ResetDrivingState();

	/** Swaps the car's looks for an actor of this class, such as one of the showcase cars in Content/Vehicles.
	 *  Only the visuals change: the physics and handling stay those of this pawn. The actor's wheel meshes
	 *  (components with "Wheel" in their name) spin, steer and ride the suspension of the physics wheels,
	 *  and a component with "Steering" in its name turns with the steering. Pass null to go back to the pawn's own meshes. */
	UFUNCTION(BlueprintCallable, Category="Vehicle|Body")
	void SetBodyClass(TSubclassOf<AActor> NewBodyClass);

	/** Returns the class of the swapped-in body, or null if the pawn is showing its own meshes */
	UFUNCTION(BlueprintPure, Category="Vehicle|Body")
	TSubclassOf<AActor> GetBodyClass() const { return BodyClass; };

	/** Turns the swapped-in body's lights (its headlights and the like) on or off. Bodies swapped in later follow suit */
	UFUNCTION(BlueprintCallable, Category="Vehicle|Body")
	void SetBodyLightsOn(bool bOn);

protected:

	/** Degrees the steering wheel turns for each degree the front wheels steer */
	UPROPERTY(EditAnywhere, Category="Vehicle|Body")
	float BodySteeringWheelRatio = 6.0f;

	/** True while the swapped-in body's lights are on. Off by default */
	bool bBodyLightsOn = false;

	/** Class of the swapped-in body, or null for the pawn's own meshes */
	UPROPERTY(Transient)
	TSubclassOf<AActor> BodyClass;

	/** The swapped-in body actor, attached to the vehicle mesh */
	UPROPERTY(Transient)
	TObjectPtr<AActor> BodyActor;

	/** A wheel mesh of the swapped-in body and the physics wheel that drives it */
	struct FBodyWheel
	{
		TWeakObjectPtr<USceneComponent> Component;
		int32 PhysicsWheelIndex = INDEX_NONE;
		FVector BaseLocation = FVector::ZeroVector;
		FQuat BaseRotation = FQuat::Identity;
	};

	/** Wheel meshes of the swapped-in body */
	TArray<FBodyWheel> BodyWheels;

	/** Steering wheel mesh of the swapped-in body, if it has one */
	TWeakObjectPtr<USceneComponent> BodySteeringWheel;

	/** Rotation the steering wheel had in the body Blueprint */
	FQuat BodySteeringWheelBaseRotation = FQuat::Identity;

	/** The pawn's own meshes and lights that were hidden to make room for the swapped-in body */
	TArray<TWeakObjectPtr<USceneComponent>> HiddenOwnComponents;

	/** Removes the swapped-in body and shows the pawn's own meshes again */
	void DestroyBody();

	/** Finds the wheel and steering wheel meshes of the swapped-in body and lines the body up with the physics wheels */
	void SetUpBody();

	/** Moves the body's wheel and steering wheel meshes to match the physics wheels */
	void UpdateBodyWheels();

	/** Puts the driver's seat camera behind the steering wheel of the current body */
	void PlaceCockpitCamera();

public:

	/** Changes the engine the car sounds like. Null makes the car silent */
	UFUNCTION(BlueprintCallable, Category="Vehicle|Sound")
	void SetEngineSound(UEngineSoundProfile* NewProfile);

protected:

	/** Engine the car sounds like until a race picks the car's own. Leave empty for a silent car */
	UPROPERTY(EditAnywhere, Category="Vehicle|Sound")
	TObjectPtr<UEngineSoundProfile> EngineSoundProfile;

	/** Feeds the revs and the throttle to the engine sound */
	void UpdateEngineSound();

	/** How far the physics engine's revs are from idle (0) to its red line (1). Below idle it goes negative */
	float GetEngineRPMFraction() const;

public:

	/** Engine speed for the tachometer. The physics engine of every car runs the same rev range, so its revs are moved
	 *  onto the rev range of the engine the car sounds like, and the needle reads what's heard.
	 *  Without an engine sound it's the physics engine's own revs */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetEngineRPM() const;

	/** Red line of the engine GetEngineRPM reads */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetEngineMaxRPM() const;

public:

	/** Changes how the car drives, starting from the pawn's own setup each time. The drive, engine and steering changes
	 *  only take effect by rebuilding the physics vehicle, chassis body included, so call this while the car stands still */
	UFUNCTION(BlueprintCallable, Category="Vehicle|Handling")
	void SetHandling(const FVehicleHandling& NewHandling);

protected:

	/** Changes to the pawn's own handling currently in effect */
	FVehicleHandling Handling;

protected:

	/** Called when the brake lights are turned on or off */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void BrakeLights(bool bBraking);

	/** Checks if the car is flipped upside down and automatically resets it */
	UFUNCTION()
	void FlippedCheck();

	/** Absorbs collision impulses so that hitting something doesn't throw the car off course */
	UFUNCTION()
	void OnChassisHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Clamps the chassis rotation rate in its own local space. Limits of zero or less are ignored */
	void ClampChassisRotationRate(float MaxRollPitchRate, float MaxYawRate);

	/** Starts a drift if the car is fast enough and the driver is steering. Returns true if it did */
	bool TryStartDrift();

	/** Ends the drift and gives the tires their grip back */
	void StopDrift();

	/** Holds the car in its slide: swings the direction of travel round and keeps the nose at the drift angle */
	void UpdateDrift(float Delta);

	/** Brings the car back down while all four wheels are off the ground and keeps it on the road over crests */
	void UpdateAirborne(float Delta);

public:
	/** Returns true while the car is in an arcade drift */
	UFUNCTION(BlueprintPure, Category="Drift")
	bool IsDrifting() const { return bDrifting; }

	/** Lets the gearbox shift by itself (AT), as NPC drivers need, or leaves the shifting to the GearUp/GearDown
	 *  actions (MT), which is the default. The player's choice comes from the car select screen */
	UFUNCTION(BlueprintCallable, Category="Vehicle")
	void SetUseAutomaticGears(bool bAutomatic);

	/** Damps the car's turning by whether its wheels are on the ground. Off by default: the turn damping then always
	 *  takes the car for airborne, as the vehicle movement can't tell, which makes it slow to turn. The player's car
	 *  keeps that feel; this gives an NPC the handling the ground damping was meant for */
	UFUNCTION(BlueprintCallable, Category="Vehicle")
	void SetGroundedTurnDamping(bool bGrounded) { bGroundedTurnDamping = bGrounded; };

	/** Sets an extra push along the car's heading, as an acceleration in cm/s², such as a race gives a car in another's
	 *  slipstream or one catching up from behind. It only pushes while the car drives forwards on the ground under
	 *  throttle, in proportion to the throttle. 0 turns it off */
	UFUNCTION(BlueprintCallable, Category="Vehicle")
	void SetRaceBoost(float Accel) { RaceBoost = FMath::Max(Accel, 0.0f); };

	/** Returns the extra push the race gives the car, in cm/s² */
	UFUNCTION(BlueprintPure, Category="Vehicle")
	float GetRaceBoost() const { return RaceBoost; };

protected:

	/** Extra push along the car's heading the race gives it, in cm/s² */
	float RaceBoost = 0.0f;

	/** Pushes the car along by the race boost */
	void UpdateRaceBoost();

	/** True if the gearbox shifts by itself */
	bool bUseAutomaticGears = false;

	/** True if the turn damping goes by the wheels touching the ground */
	bool bGroundedTurnDamping = false;

	/** With the automatic gearbox, the player's car shifts down a gear from 2nd up once the tachometer revs
	 *  (GetEngineRPM) fall to this. The gearbox's own change down reads the physics revs and waits while any wheel
	 *  slips, so a car coming out of a drift could stay in a tall gear. 0 leaves it to the gearbox */
	UPROPERTY(EditAnywhere, Category="Vehicle", meta = (ClampMin = "0.0"))
	float AutoDownshiftRPM = 3000.0f;

	/** Shifts the automatic gearbox down a gear when the revs have fallen to AutoDownshiftRPM */
	void UpdateAutoDownshift();

	/** Gear the last auto downshift started from, and when, so a change under way isn't started over */
	int32 AutoDownshiftFromGear = 0;
	float AutoDownshiftTime = 0.0f;

public:
	/** Returns the front spring arm subobject */
	FORCEINLINE USpringArmComponent* GetFrontSpringArm() const { return FrontSpringArm; }
	/** Returns the front camera subobject */
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FrontCamera; }
	/** Returns the back spring arm subobject */
	FORCEINLINE USpringArmComponent* GetBackSpringArm() const { return BackSpringArm; }
	/** Returns the back camera subobject */
	FORCEINLINE UCameraComponent* GetBackCamera() const { return BackCamera; }
	/** Returns the driver's seat camera subobject */
	FORCEINLINE UCameraComponent* GetCockpitCamera() const { return CockpitCamera; }
	/** Returns the engine sound subobject */
	FORCEINLINE UEngineSoundComponent* GetEngineSound() const { return EngineSound; }
	/** Returns the knock and scrape effects subobject */
	FORCEINLINE UCollisionFxComponent* GetCollisionFx() const { return CollisionFx; }
	/** Returns the cast Chaos Vehicle Movement subobject */
	FORCEINLINE const TObjectPtr<UChaosWheeledVehicleMovementComponent>& GetChaosVehicleMovement() const { return ChaosVehicleMovement; }
};
