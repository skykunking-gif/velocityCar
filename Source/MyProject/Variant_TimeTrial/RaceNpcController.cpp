// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "RaceNpcController.h"
#include "RaceLine.h"
#include "TimeTrialGameMode.h"
#include "MyProjectPawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "ChaosVehicleManagerAsyncCallback.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "MyProject.h"

namespace
{
	/** Speed step of the learned steering table */
	constexpr float SteerTableStep = 400.0f;

	/** Number of speed steps in the learned steering table */
	constexpr int32 SteerTableSize = 18;

	/** Sideways acceleration full steering is assumed to give before the driver has learned the car */
	constexpr float InitialFullSteerAccel = 550.0f;

	/** The same with grounded handling, which turns the car several times harder */
	constexpr float InitialGroundedSteerAccel = 1500.0f;

	/** Fastest the steering moves from lock to lock, in full locks per second */
	constexpr float SteeringRate = 4.0f;

	/** Samples either side of a point used to measure how sharply the road crests there */
	constexpr int32 CrestSpan = 5;

	/** How quickly the learned steering follows what the car is doing, per second */
	constexpr float SteerLearnRate = 0.6f;

	/** How hard the steering corrects a gap between the turn wanted and the turn the car makes */
	constexpr float YawRateCorrection = 1.0f;

	/** Below this speed the car is steered by geometry alone, and its turning isn't learned from */
	constexpr float LowSpeed = 250.0f;

	/** Below this speed sideways sliding isn't measured: the slightest sideways drift reads as a big angle */
	constexpr float SlideMinSpeed = 500.0f;

	/** Sideways sliding, in degrees, past which the car is losing it and backs off the pedals */
	constexpr float SlideAngle = 10.0f;

	/** Furthest the car moves off the race line to either side, keeping clear of the guardrails */
	constexpr float MaxLaneOffset = 330.0f;

	/** How far apart two cars side by side need to be */
	constexpr float CarWidth = 250.0f;

	/** How far over the car moves to pass the car ahead */
	constexpr float PassOffset = 300.0f;

	/** How fast the car moves across the road, in cm/s */
	constexpr float LaneChangeRate = 150.0f;

	/** How far ahead the car looks for a bend tight enough to bring it back to the line */
	constexpr float BendLaneLookAhead = 4000.0f;

	/** Bends wider than this leave the car its whole lane; ones as tight as TightBendRadius bring it right back to
	 *  the line */
	constexpr float WideBendRadius = 4000.0f;
	constexpr float TightBendRadius = 1500.0f;

	constexpr float CmPerSecondToMph = 0.0223694f;
}

ARaceNpcController::ARaceNpcController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	FullSteerAccel.Init(InitialFullSteerAccel, SteerTableSize);
}

void ARaceNpcController::SetDifficulty(const FNpcDifficulty& InDifficulty, float InPaceFactor)
{
	Difficulty = InDifficulty;
	PaceFactor = InPaceFactor;

	if (AMyProjectPawn* Car = Vehicle.Get())
	{
		Car->SetGroundedTurnDamping(Difficulty.bGroundedHandling);
	}

	// start from a fair guess of how hard the car turns: guessing far too low steers far too hard at speed
	FullSteerAccel.Init(Difficulty.bGroundedHandling ? InitialGroundedSteerAccel : InitialFullSteerAccel, SteerTableSize);

	// replan with the new skill on the next tick
	ProfileLineLength = -1.0f;
}

void ARaceNpcController::SetPreferredLane(float InLaneOffset)
{
	PreferredLane = FMath::Clamp(InLaneOffset, -MaxLaneOffset, MaxLaneOffset);
	LaneOffset = PreferredLane;
}

void ARaceNpcController::StartDriving()
{
	bDriving = true;
	bFinished = false;
	TimeSinceRecovery = 0.0f;
	ReverseTimeLeft = 0.0f;
	ReverseAttempts = 0;

	// the lap time to aim for on this route, if the difficulty has one. Each NPC aims a little either side of it,
	// so they don't all arrive together
	TargetLapTime = 0.0f;

	if (const ATimeTrialGameMode* GM = GetRaceGameMode())
	{
		if (const float* Target = Difficulty.TargetLapTimes.Find(GM->GetSelectedRoute()))
		{
			TargetLapTime = *Target / PaceFactor;
		}
	}

	Push = 1.0f;
	LeadEase = 0.0f;
	LapEaseTime = 0.0f;
	bChasedThisLap = false;
	ActualRaceTime = 0.0f;
	PlannedRaceTime = 0.0f;
	LapStartTime = -1.0f;
	TargetBias = 1.0f;
	ProfileLineLength = -1.0f;

	if (AMyProjectPawn* Car = Vehicle.Get())
	{
		Car->GetChaosVehicleMovement()->SetHandbrakeInput(false);
	}
}

void ARaceNpcController::NotifyFinished()
{
	bFinished = true;
}

void ARaceNpcController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Vehicle = Cast<AMyProjectPawn>(InPawn);

	if (AMyProjectPawn* Car = Vehicle.Get())
	{
		// NPCs don't shift by hand
		Car->SetUseAutomaticGears(true);

		MeasureVehicle();
	}

	bLocated = false;
	ProfileLineLength = -1.0f;
}

void ARaceNpcController::OnUnPossess()
{
	Vehicle = nullptr;

	Super::OnUnPossess();
}

void ARaceNpcController::MeasureVehicle()
{
	AMyProjectPawn* Car = Vehicle.Get();

	if (!Car)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* Movement = Car->GetChaosVehicleMovement();

	// wheel base from the wheel bones, and the steering lock from the steered wheels
	float FrontmostX = -TNumericLimits<float>::Max();
	float RearmostX = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Movement->WheelSetups.Num(); ++Index)
	{
		const FName Bone = Movement->WheelSetups[Index].BoneName;

		if (Bone != NAME_None && Car->GetMesh()->GetBoneIndex(Bone) != INDEX_NONE)
		{
			const float X = Car->GetMesh()->GetBoneLocation(Bone, EBoneSpaces::ComponentSpace).X;
			FrontmostX = FMath::Max(FrontmostX, X);
			RearmostX = FMath::Min(RearmostX, X);
		}

		if (Movement->Wheels.IsValidIndex(Index) && Movement->Wheels[Index] && Movement->Wheels[Index]->bAffectedBySteering)
		{
			MaxSteerAngle = FMath::Max(Movement->Wheels[Index]->MaxSteerAngle, 1.0f);
		}
	}

	if (FrontmostX > RearmostX + 50.0f)
	{
		WheelBase = FrontmostX - RearmostX;
	}
}

float ARaceNpcController::GetMaxSteerAngle(float Speed) const
{
	const AMyProjectPawn* Car = Vehicle.Get();

	if (!Car)
	{
		return MaxSteerAngle;
	}

	// the vehicle scales its steering lock down with speed along this curve, divided by the curve's peak
	const FRichCurve* Curve = Car->GetChaosVehicleMovement()->SteeringSetup.SteeringCurve.GetRichCurveConst();

	if (!Curve || Curve->GetNumKeys() == 0)
	{
		return MaxSteerAngle;
	}

	float MinValue = 0.0f;
	float MaxValue = 1.0f;
	Curve->GetValueRange(MinValue, MaxValue);

	const float Share = MaxValue > KINDA_SMALL_NUMBER ? Curve->Eval(FMath::Abs(Speed) * CmPerSecondToMph) / MaxValue : 1.0f;

	return FMath::Max(MaxSteerAngle * Share, 1.0f);
}

float ARaceNpcController::GetFullSteerAccel(float Speed) const
{
	const float Step = FMath::Clamp(Speed / SteerTableStep, 0.0f, SteerTableSize - 1.0f);
	const int32 Lower = FMath::FloorToInt(Step);
	const int32 Upper = FMath::Min(Lower + 1, SteerTableSize - 1);

	return FMath::Lerp(FullSteerAccel[Lower], FullSteerAccel[Upper], Step - Lower);
}

void ARaceNpcController::LearnSteering(float Speed, float Steering, float YawRate, float DeltaSeconds)
{
	// only a clear turn the car is making the way it's steered says how hard it turns
	if (Speed < LowSpeed || FMath::Abs(Steering) < 0.1f || YawRate * Steering <= 0.0f)
	{
		return;
	}

	const float Measured = FMath::Clamp(Speed * FMath::Abs(YawRate) / FMath::Abs(Steering), 150.0f, 3000.0f);

	// spread the measurement over the two steps of speed either side
	const float Step = FMath::Clamp(Speed / SteerTableStep, 0.0f, SteerTableSize - 1.0f);
	const int32 Lower = FMath::FloorToInt(Step);
	const int32 Upper = FMath::Min(Lower + 1, SteerTableSize - 1);
	const float UpperShare = Step - Lower;
	const float Rate = FMath::Clamp(SteerLearnRate * DeltaSeconds, 0.0f, 1.0f);

	FullSteerAccel[Lower] = FMath::Lerp(FullSteerAccel[Lower], Measured, Rate * (1.0f - UpperShare));
	FullSteerAccel[Upper] = FMath::Lerp(FullSteerAccel[Upper], Measured, Rate * UpperShare);
}

void ARaceNpcController::BuildSpeedProfile(const FRaceLine& Line)
{
	ProfileLineLength = Line.GetLength();
	ReplanTime = 3.0f;

	const int32 Count = Line.Num();
	SpeedProfile.SetNum(Count);

	// keeping to a target lap time, the push sets the pace; otherwise the NPC's own pace factor does.
	// Scaling the sideways acceleration and the braking by its square scales every planned speed by it
	const float SpeedScale = TargetLapTime > 0.0f ? Push : PaceFactor;
	const float TopSpeed = Difficulty.MaxSpeed * SpeedScale;
	const float CornerShare = Difficulty.CornerLimit * SpeedScale * SpeedScale;
	const float Brake = Difficulty.BrakeDecel * SpeedScale * SpeedScale;

	// the tightest bend the car takes at each speed, from how hard it has been found to turn
	TArray<float> MaxCurvature;

	for (float Speed = 100.0f; Speed <= TopSpeed + 1.0f; Speed += 100.0f)
	{
		MaxCurvature.Add(GetFullSteerAccel(Speed) * CornerShare / FMath::Square(Speed));
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// the sharpest bend around this sample
		float Curvature = 0.0f;

		for (int32 Offset = -1; Offset <= 1; ++Offset)
		{
			Curvature = FMath::Max(Curvature, FMath::Abs(Line.GetCurvatureAtSample((Index + Offset + Count) % Count)));
		}

		// over a crest the road falls away under the car and takes weight off its tires, the more so the faster it
		// goes: the share of its weight left is 1 - speed² × crest / gravity. With less weight on them the tires
		// turn the car less, and with none it's in the air and can't steer at all
		const float Before = Line.GetSample((Index - CrestSpan + Count) % Count).Z;
		const float Here = Line.GetSample(Index).Z;
		const float After = Line.GetSample((Index + CrestSpan) % Count).Z;
		const float Crest = FMath::Max((2.0f * Here - Before - After) / FMath::Square(CrestSpan * Line.GetSpacing()), 0.0f);

		// the fastest speed at which the car still gets round
		int32 Step = MaxCurvature.Num() - 1;

		while (Step > 0)
		{
			const float Load = 1.0f - FMath::Square((Step + 1) * 100.0f) * Crest / 980.0f;

			if (Load > 0.0f && MaxCurvature[Step] * Load >= Curvature)
			{
				break;
			}

			--Step;
		}

		SpeedProfile[Index] = FMath::Min(TopSpeed, (Step + 1) * 100.0f);
	}

	// work backwards from every corner so the car brakes in time for it. Downhill, gravity eats into the braking.
	// Twice round, so the corners just after the start of the line reach back over the seam.
	const float Spacing = Line.GetSpacing();

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Index = Count - 1; Index >= 0; --Index)
		{
			const int32 Next = (Index + 1) % Count;
			const float Slope = (Line.GetSample(Next).Z - Line.GetSample(Index).Z) / Spacing;
			const float Deceleration = FMath::Max(Brake + 980.0f * Slope, 150.0f);

			SpeedProfile[Index] = FMath::Min(SpeedProfile[Index], FMath::Sqrt(FMath::Square(SpeedProfile[Next]) + 2.0f * Deceleration * Spacing));
		}
	}

	// the lap time the plan works out at, were the car to keep to it exactly
	PlannedLapTime = 0.0f;

	for (const float Speed : SpeedProfile)
	{
		PlannedLapTime += Spacing / FMath::Max(Speed, 100.0f);
	}
}

void ARaceNpcController::TrackPace(const FRaceLine& Line, float Moved, float DeltaSeconds)
{
	if (TargetLapTime <= 0.0f || SpeedProfile.Num() == 0)
	{
		return;
	}

	const int32 Sample = FMath::FloorToInt(LineDistance / Line.GetSpacing()) % SpeedProfile.Num();

	// only the last half minute or so counts, so a slow standing start doesn't make the NPC run its later laps
	// faster than the target to make up for it
	const float Keep = FMath::Exp(-DeltaSeconds / 30.0f);

	ActualRaceTime = ActualRaceTime * Keep + DeltaSeconds;
	LapEaseTime += LeadEase * DeltaSeconds;
	PlannedRaceTime = PlannedRaceTime * Keep + FMath::Max(Moved, 0.0f) / FMath::Max(SpeedProfile[Sample], 100.0f);
}

void ARaceNpcController::FinishLap()
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float LapTime = Now - LapStartTime;

	// crossing back over the line after being put back behind it isn't a lap
	if (LapStartTime >= 0.0f && LapTime < TargetLapTime * 0.5f)
	{
		return;
	}

	// the run from the grid to the line isn't a whole lap, so only time the ones after it
	if (LapStartTime >= 0.0f && TargetLapTime > 0.0f && !bFinished)
	{
		// the lap time this lap was meant to come out at, with the easing off for the player behind. A lap spent
		// partly chasing the player says nothing about how well the NPC keeps to its pace
		const float MeantLapTime = TargetLapTime * (1.0f + LapEaseTime / LapTime);

		if (!bChasedThisLap)
		{
			// aim the next lap off the target by as much as this one came out from it
			TargetBias = FMath::Clamp(TargetBias * MeantLapTime / LapTime, 0.85f, 1.15f);
		}

		UE_LOG(LogMyProject, Log, TEXT("[NPC] %s lap %.1fs (aiming for %.1fs%s), next aims for %.1fs"), *GetName(), LapTime, MeantLapTime,
			bChasedThisLap ? TEXT(", chased the player") : TEXT(""), TargetLapTime * TargetBias);
	}

	LapStartTime = Now;
	LapEaseTime = 0.0f;
	bChasedThisLap = false;
}

void ARaceNpcController::UpdatePace()
{
	if (TargetLapTime <= 0.0f || PlannedLapTime <= 0.0f || bFinished)
	{
		return;
	}

	// where the cars behind get help catching up, an NPC behind the player chases flat out: easing off to its lap
	// time would only throw that help away
	const ATimeTrialGameMode* GM = GetRaceGameMode();

	if (Difficulty.CatchUpAccel > 0.0f && GM && GM->IsPlayerAhead(this))
	{
		Push = FMath::Lerp(Push, 1.0f, 0.5f);
		LeadEase = 0.0f;
		bChasedThisLap = true;
		return;
	}

	// ahead of the player, ease off a little the further the player has dropped behind, so they can get back into it
	const float PlayerBehind = GM ? GM->GetPlayerDistanceBehind(this) : 0.0f;
	LeadEase = Difficulty.LeadEase * FMath::Clamp(PlayerBehind / FMath::Max(Difficulty.CatchUpFullGap, 100.0f), 0.0f, 1.0f);

	// how much slower than its plan the car really goes lately. Until there's enough to go by, assume a fair bit slower
	const float Slowness = PlannedRaceTime > 8.0f ? ActualRaceTime / PlannedRaceTime : 1.15f;

	// every planned speed scales with the push, so the planned lap time scales with its inverse
	const float WantedPush = PlannedLapTime * Push * Slowness / (TargetLapTime * TargetBias * (1.0f + LeadEase));

	// halfway there each time, as the slowness itself shifts with the push
	Push = FMath::Clamp(FMath::Lerp(Push, WantedPush, 0.5f), 0.5f, 1.0f);
}

void ARaceNpcController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AMyProjectPawn* Car = Vehicle.Get();
	ATimeTrialGameMode* GM = GetRaceGameMode();

	if (!Car || !GM)
	{
		return;
	}

	const FRaceLine& Line = GM->GetRaceLine();

	if (!Line.IsValid())
	{
		return;
	}

	// the line changes with the race, and the plan with it. The plan also catches up with what's been learned about the car
	ReplanTime -= DeltaSeconds;

	if (ProfileLineLength != Line.GetLength() || ReplanTime <= 0.0f)
	{
		if (ProfileLineLength != Line.GetLength())
		{
			bLocated = false;
		}

		UpdatePace();
		BuildSpeedProfile(Line);
	}

	const float PreviousDistance = LineDistance;
	const bool bWasLocated = bLocated;

	LineDistance = bLocated ? Line.FindDistanceNear(Car->GetActorLocation(), LineDistance, 3000.0f) : Line.FindDistance(Car->GetActorLocation());
	bLocated = true;

	UChaosWheeledVehicleMovementComponent* Movement = Car->GetChaosVehicleMovement();

	if (!bDriving)
	{
		// wait on the grid
		Car->DoSteering(0.0f);
		Car->DoThrottle(0.0f);
		Movement->SetHandbrakeInput(true);
		return;
	}

	TimeSinceRecovery += DeltaSeconds;

	if (!bFinished && bWasLocated)
	{
		TrackPace(Line, Line.Delta(PreviousDistance, LineDistance), DeltaSeconds);

		// over the finish line: from just before it to just after it
		const float Finish = GM->GetFinishLineDistance();
		const float Before = Line.Wrap(PreviousDistance - Finish);
		const float After = Line.Wrap(LineDistance - Finish);

		if (Before > Line.GetLength() * 0.8f && After < Line.GetLength() * 0.2f)
		{
			FinishLap();
		}
	}

	const float Speed = Movement->GetForwardSpeed();
	const float ForwardSpeed = FMath::Max(Speed, 0.0f);

	// how the car is moving: its turn rate (positive turning right), and how far it's sliding sideways
	const float YawRate = FMath::DegreesToRadians(Car->GetMesh()->GetPhysicsAngularVelocityInDegrees().Z);
	const FVector LocalVelocity = Car->GetActorTransform().InverseTransformVectorNoScale(Car->GetVelocity());
	const float Slide = ForwardSpeed > SlideMinSpeed ? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)) : 0.0f;
	const bool bSliding = FMath::Abs(Slide) > SlideAngle;

	// with fewer than two wheels on the road, steering does nothing until the car lands
	int32 WheelsOnGround = 0;

	if (const TUniquePtr<FPhysicsVehicleOutput>& PhysicsOutput = Movement->PhysicsVehicleOutput(); PhysicsOutput.IsValid())
	{
		for (const FWheelsOutput& Wheel : PhysicsOutput->Wheels)
		{
			WheelsOnGround += Wheel.InContact ? 1 : 0;
		}
	}

	const bool bAirborne = WheelsOnGround < 2;

	// where the car is across the road, and how far it points off the line
	const FVector LineDirection = Line.GetDirection(LineDistance);
	const float Lateral = FVector::DotProduct(Car->GetActorLocation() - Line.GetLocation(LineDistance), Line.GetRight(LineDistance));
	const float Heading = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::CrossProduct(LineDirection.GetSafeNormal2D(), Car->GetActorForwardVector().GetSafeNormal2D()).Z,
		FVector::DotProduct(LineDirection.GetSafeNormal2D(), Car->GetActorForwardVector().GetSafeNormal2D())));

	bool bWantsBrake = false;
	float TargetSpeed = 0.0f;
	float PlannedSpeed = 0.0f;
	float Steering = 0.0f;

	if (ReverseTimeLeft > 0.0f)
	{
		// backing away from whatever the car ran into. Braking at a standstill puts the car in reverse
		ReverseTimeLeft -= DeltaSeconds;

		Steering = ReverseSteering;
		Car->DoSteering(Steering);
		Car->DoBrake(1.0f);

	} else {

		// the turn that takes the car through a point a little way ahead on the line (pure pursuit)
		const float LookAhead = FMath::Clamp(ForwardSpeed * Difficulty.LookAheadTime, 600.0f, 6250.0f * Difficulty.LookAheadTime);
		const float AimDistance = LineDistance + LookAhead;
		const FVector Aim = Line.GetLocation(AimDistance) + Line.GetRight(AimDistance) * LaneOffset;
		const FVector LocalAim = Car->GetActorTransform().InverseTransformPositionNoScale(Aim);

		const float AimAngle = FMath::Atan2(LocalAim.Y, LocalAim.X);
		const float AimRange = FMath::Max(LocalAim.Size2D(), 100.0f);
		const float WantedCurvature = 2.0f * FMath::Sin(AimAngle) / AimRange;

		// slow: the wheel angle that makes that turn
		const float WheelAngle = FMath::RadiansToDegrees(FMath::Atan(WantedCurvature * WheelBase));
		const float GeometricSteering = WheelAngle / GetMaxSteerAngle(ForwardSpeed);

		// at speed: as much of the steering as the turn needs of what full steering gives, corrected by how the car
		// is actually turning, so a car that's slow to turn gets more steering
		const float FullAccel = GetFullSteerAccel(ForwardSpeed);
		const float WantedYawRate = ForwardSpeed * WantedCurvature;
		const float DynamicSteering = (ForwardSpeed * WantedYawRate + YawRateCorrection * (WantedYawRate - YawRate) * ForwardSpeed) / FullAccel;

		const float Blend = FMath::GetMappedRangeValueClamped(FVector2f(LowSpeed, LowSpeed * 1.6f), FVector2f(0.0f, 1.0f), ForwardSpeed);
		Steering = FMath::Clamp(FMath::Lerp(GeometricSteering, DynamicSteering, Blend), -1.0f, 1.0f);

		// the line swings far round, as in a hairpin: full lock, whatever the estimate says
		if (FMath::Abs(AimAngle) > FMath::DegreesToRadians(35.0f))
		{
			Steering = FMath::Sign(AimAngle);
		}

		// at speed, no more steering than the bend needs with room to correct: on a straight a car steered hard spins
		if (ForwardSpeed > LowSpeed * 4.0f)
		{
			const float BendSteering = ForwardSpeed * ForwardSpeed * FMath::Abs(Line.GetCurvatureAtSample(FMath::FloorToInt(AimDistance / Line.GetSpacing()) % Line.Num())) / FullAccel;
			const float MaxSteering = FMath::Clamp(2.0f * BendSteering + 0.25f + FMath::Abs(Heading) / 40.0f, 0.25f, 1.0f);

			Steering = FMath::Clamp(Steering, -MaxSteering, MaxSteering);
		}

		// in the air the wheels have nothing to turn against: hold the steering until the car lands
		if (bAirborne)
		{
			Steering = Movement->GetSteeringInput();
		}

		// turn the wheel at a human rate, so a correction can't flick from lock to lock and spin the car
		Steering = FMath::FInterpConstantTo(Movement->GetSteeringInput(), Steering, DeltaSeconds, SteeringRate);

		Car->DoSteering(Steering);

		// the slowest planned speed over the next moment, so the car is down to speed as it reaches each corner
		const int32 Count = SpeedProfile.Num();
		const int32 FirstSample = FMath::FloorToInt(LineDistance / Line.GetSpacing());
		const int32 LookSamples = FMath::CeilToInt((ForwardSpeed * Difficulty.BrakeLookAheadTime + 400.0f) / Line.GetSpacing());

		TargetSpeed = TNumericLimits<float>::Max();

		for (int32 Step = 0; Step <= LookSamples; ++Step)
		{
			TargetSpeed = FMath::Min(TargetSpeed, SpeedProfile[(FirstSample + Step) % Count]);
		}

		PlannedSpeed = TargetSpeed;

		// running wide or pointing off the line means the car is carrying too much speed: ease off until it's back
		const float TrackingError = FMath::Abs(Lateral - LaneOffset);
		TargetSpeed *= FMath::GetMappedRangeValueClamped(FVector2f(150.0f, 450.0f), FVector2f(1.0f, 0.6f), TrackingError);
		TargetSpeed *= FMath::GetMappedRangeValueClamped(FVector2f(15.0f, 40.0f), FVector2f(1.0f, 0.6f), FMath::Abs(Heading));

		// a lap of honour once the race is run: easing off where the car goes fast, but not through the slow parts,
		// where any slower and it would crawl and stall on the steepest hills
		if (bFinished)
		{
			TargetSpeed = FMath::Min(TargetSpeed, FMath::Max(TargetSpeed * 0.6f, 1500.0f));
		}

		TargetSpeed = FMath::Min(TargetSpeed, AvoidOtherRacers(Line, ForwardSpeed, DeltaSeconds));

		const float SpeedError = TargetSpeed - Speed;

		if (bSliding)
		{
			// sliding: gentle on the throttle and off the brakes, so the tires can grip again
			Car->DoThrottle(SpeedError > 0.0f ? 0.2f * Difficulty.MaxThrottle : 0.0f);

		} else if (SpeedError > 0.0f) {

			Car->DoThrottle(FMath::Clamp(SpeedError / 250.0f, 0.0f, 1.0f) * Difficulty.MaxThrottle);

		} else if (SpeedError < -80.0f && Speed > 300.0f) {

			// braking near a standstill would put the car in reverse. Brake less while turning hard, so the tires
			// have grip left for the turn
			Car->DoBrake(FMath::Clamp(-SpeedError / 200.0f, 0.2f, 1.0f) * (1.0f - 0.5f * FMath::Abs(Steering)));
			bWantsBrake = true;

		} else {

			Car->DoThrottle(0.0f);
		}

		if (!bSliding && !bAirborne && TimeSinceRecovery > 2.0f)
		{
			LearnSteering(ForwardSpeed, Steering, YawRate, DeltaSeconds);
		}
	}

	if (bWantsBrake != bBraking)
	{
		bBraking = bWantsBrake;

		if (bBraking)
		{
			Car->DoBrakeStart();

		} else {

			Car->DoBrakeStop();
		}
	}

	// driving telemetry four times a second, for tuning. Turn LogMyProject up to Verbose to see it
	const float Now = GetWorld()->GetTimeSeconds();

	if (FMath::FloorToInt(Now * 4.0f) != FMath::FloorToInt((Now - DeltaSeconds) * 4.0f))
	{
		UE_LOG(LogMyProject, Verbose, TEXT("[NPCT] %s t=%.2f d=%.0f v=%.0f tgt=%.0f lat=%.0f lane=%.0f hdg=%.1f steer=%.2f thr=%.2f brk=%.2f yaw=%.3f slide=%.1f acc=%.0f curv=%.6f rev=%.1f push=%.3f slow=%.3f boost=%.0f plan=%.0f gear=%d rpm=%.0f wheels=%d at %s"),
			*GetName(), Now, LineDistance, Speed, TargetSpeed, Lateral, LaneOffset, Heading,
			Steering, Movement->GetThrottleInput(), Movement->GetBrakeInput(), YawRate, Slide, GetFullSteerAccel(ForwardSpeed),
			Line.GetCurvatureAtSample(FMath::FloorToInt(LineDistance / Line.GetSpacing()) % Line.Num()), ReverseTimeLeft, Push, PlannedRaceTime > 0.0f ? ActualRaceTime / PlannedRaceTime : 0.0f, Car->GetRaceBoost(), PlannedSpeed, Movement->GetCurrentGear(), Movement->GetEngineRotationSpeed(), WheelsOnGround, *Car->GetActorLocation().ToCompactString());
	}

	CheckForRecovery(Line, DeltaSeconds);
}

float ARaceNpcController::AvoidOtherRacers(const FRaceLine& Line, float Speed, float DeltaSeconds)
{
	const ATimeTrialGameMode* GM = GetRaceGameMode();
	const APawn* Car = GetPawn();

	float SpeedCap = TNumericLimits<float>::Max();
	float TargetLane = PreferredLane;
	float ClosestAhead = TNumericLimits<float>::Max();

	for (const FRacerState& Racer : GM->GetRacers())
	{
		const AController* OtherController = Racer.Controller.Get();
		const APawn* Other = OtherController ? OtherController->GetPawn() : nullptr;

		if (!Other || Other == Car)
		{
			continue;
		}

		const float Gap = Line.Delta(LineDistance, Racer.LineDistance);

		if (FMath::Abs(Gap) > 3500.0f)
		{
			continue;
		}

		const float OtherLane = FVector::DotProduct(Other->GetActorLocation() - Line.GetLocation(Racer.LineDistance), Line.GetRight(Racer.LineDistance));
		const float OtherSpeed = FVector::DotProduct(Other->GetVelocity(), Line.GetDirection(Racer.LineDistance));
		const float LaneGap = OtherLane - LaneOffset;

		if (FMath::Abs(Gap) < 450.0f && FMath::Abs(LaneGap) < CarWidth)
		{
			// alongside and too close: make room, but never further than the road allows
			TargetLane = OtherLane - (LaneGap >= 0.0f ? 1.0f : -1.0f) * CarWidth;

		} else if (Gap > 0.0f && Gap < 2500.0f && FMath::Abs(LaneGap) < CarWidth && Gap < ClosestAhead) {

			// in the way ahead: only matters if we're catching it
			if (Speed - OtherSpeed > 150.0f || Gap < 900.0f)
			{
				ClosestAhead = Gap;

				// go round on whichever side has room, preferring the one we're already nearer
				const float LeftLane = OtherLane - PassOffset;
				const float RightLane = OtherLane + PassOffset;
				const bool bLeftFits = LeftLane >= -MaxLaneOffset;
				const bool bRightFits = RightLane <= MaxLaneOffset;

				if (bLeftFits && (!bRightFits || FMath::Abs(LeftLane - LaneOffset) <= FMath::Abs(RightLane - LaneOffset)))
				{
					TargetLane = LeftLane;

				} else if (bRightFits) {

					TargetLane = RightLane;

				} else if (Gap < 1500.0f) {

					// no room to pass: follow it
					SpeedCap = FMath::Min(SpeedCap, FMath::Max(OtherSpeed - 50.0f, 0.0f));
				}

				// about to hit it whatever happens: back off
				if (Gap < 600.0f && FMath::Abs(LaneGap) < CarWidth - 40.0f)
				{
					SpeedCap = FMath::Min(SpeedCap, FMath::Max(OtherSpeed - 150.0f, 0.0f));
				}
			}
		}
	}

	// coming up to a tight bend, back to the line: round a hairpin the barriers reach in further than a lane's
	// width off it, and a car keeping to its lane runs into them
	float Tightest = 0.0f;

	for (float Ahead = 0.0f; Ahead <= BendLaneLookAhead; Ahead += Line.GetSpacing())
	{
		Tightest = FMath::Max(Tightest, FMath::Abs(Line.GetCurvatureAtSample(FMath::FloorToInt(Line.Wrap(LineDistance + Ahead) / Line.GetSpacing()) % Line.Num())));
	}

	const float LaneLimit = MaxLaneOffset * FMath::GetMappedRangeValueClamped(FVector2f(1.0f / WideBendRadius, 1.0f / TightBendRadius), FVector2f(1.0f, 0.0f), Tightest);

	LaneOffset = FMath::FInterpConstantTo(LaneOffset, FMath::Clamp(TargetLane, -LaneLimit, LaneLimit), DeltaSeconds, LaneChangeRate);

	return SpeedCap;
}

void ARaceNpcController::CheckForRecovery(const FRaceLine& Line, float DeltaSeconds)
{
	const APawn* Car = GetPawn();

	// give the car a moment to get going after the start or being put back
	if (!Car || TimeSinceRecovery < 2.0f)
	{
		ProgressCheckTime = 0.0f;
		ProgressCheckDistance = LineDistance;
		OffRoadTime = 0.0f;
		WrongWayTime = 0.0f;
		return;
	}

	const FVector FromLine = Car->GetActorLocation() - Line.GetLocation(LineDistance);
	const float Sideways = FMath::Abs(FVector::DotProduct(FromLine, Line.GetRight(LineDistance)));
	const float Below = -FromLine.Z;

	OffRoadTime = (Sideways > 1500.0f || Below > 1500.0f) ? OffRoadTime + DeltaSeconds : 0.0f;
	WrongWayTime = FVector::DotProduct(Car->GetActorForwardVector(), Line.GetDirection(LineDistance)) < -0.3f ? WrongWayTime + DeltaSeconds : 0.0f;

	if (OffRoadTime > 1.0f || WrongWayTime > 2.5f)
	{
		UE_LOG(LogMyProject, Log, TEXT("[NPC] %s put back on the line at %.0f (off road %.1fs, wrong way %.1fs)"), *GetName(), LineDistance, OffRoadTime, WrongWayTime);

		Recover();
		return;
	}

	// a long frame (a hitch, or an editor running in the background) says nothing about the car, so start the check over
	if (DeltaSeconds > 0.1f)
	{
		ProgressCheckTime = 0.0f;
		ProgressCheckDistance = LineDistance;
		return;
	}

	// a car pinned against a wall can show plenty of speed while it goes nowhere, so judge by the progress along the line
	ProgressCheckTime += DeltaSeconds;

	if (ProgressCheckTime < 1.5f || ReverseTimeLeft > 0.0f)
	{
		return;
	}

	const float Progress = Line.Delta(ProgressCheckDistance, LineDistance);
	const float CheckedTime = ProgressCheckTime;

	ProgressCheckTime = 0.0f;
	ProgressCheckDistance = LineDistance;

	if (Progress > 2000.0f)
	{
		// properly on the move again
		ReverseAttempts = 0;
	}

	// stuck if it has barely moved, or if it's getting nowhere near as far as its speed says it's going, as when it's
	// pressed against a wall. A car crawling round a hairpin covers little ground but is still on its way
	const float SpeedDistance = Car->GetVelocity().Size() * CheckedTime;
	const bool bStuck = Progress < 200.0f || (Progress < 800.0f && Progress < SpeedDistance * 0.25f);

	if (!bStuck)
	{
		return;
	}

	if (ReverseAttempts < 2)
	{
		// back off with the wheels turned so the nose swings back towards the line
		const FVector Forward = Car->GetActorForwardVector().GetSafeNormal2D();
		const float NoseToRight = FVector::CrossProduct(Line.GetDirection(LineDistance).GetSafeNormal2D(), Forward).Z;

		ReverseSteering = NoseToRight >= 0.0f ? 1.0f : -1.0f;
		ReverseTimeLeft = 1.3f;
		++ReverseAttempts;

		UE_LOG(LogMyProject, Log, TEXT("[NPC] %s stuck at %.0f, backing off (attempt %d) at %s"), *GetName(), LineDistance, ReverseAttempts, *Car->GetActorLocation().ToCompactString());

	} else {

		UE_LOG(LogMyProject, Log, TEXT("[NPC] %s still stuck at %.0f, put back on the line"), *GetName(), LineDistance);

		Recover();
	}
}

void ARaceNpcController::Recover()
{
	AMyProjectPawn* Car = Vehicle.Get();
	const ATimeTrialGameMode* GM = GetRaceGameMode();

	if (!Car || !GM)
	{
		return;
	}

	// a little way back, clear of any other car
	FTransform Spot;
	const float Distance = GM->FindResetSpot(Car, LineDistance, Spot);

	Car->SetActorTransform(Spot, false, nullptr, ETeleportType::TeleportPhysics);
	Car->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Car->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	LineDistance = Distance;
	LaneOffset = 0.0f;
	TimeSinceRecovery = 0.0f;
	ReverseTimeLeft = 0.0f;
	ReverseAttempts = 0;
	OffRoadTime = 0.0f;
	WrongWayTime = 0.0f;
}

ATimeTrialGameMode* ARaceNpcController::GetRaceGameMode() const
{
	return GetWorld() ? Cast<ATimeTrialGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
}
