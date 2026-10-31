// Copyright (c) 2026 skykunking-gif. All Rights Reserved.


#include "TitleCameraDolly.h"
#include "Components/SplineComponent.h"
#include "CineCameraComponent.h"
#include "Engine/World.h"
#include "MyProject.h"

ATitleCameraDolly::ATitleCameraDolly()
{
	PrimaryActorTick.bCanEverTick = true;

	Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
	RootComponent = Path;

	Camera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	// a wide lens takes in more of the map from up in the air
	Camera->CurrentFocalLength = 20.0f;
}

void ATitleCameraDolly::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BuildPathFromSource();
}

void ATitleCameraDolly::BeginPlay()
{
	Super::BeginPlay();

	BuildPathFromSource();

	TravelDistance = StartDistance;
	bAimInitialized = false;
}

void ATitleCameraDolly::BuildPathFromSource()
{
	if (!PathSource || PathSource == this)
	{
		return;
	}

	const USplineComponent* Source = PathSource->FindComponentByClass<USplineComponent>();

	if (!Source)
	{
		UE_LOG(LogMyProject, Warning, TEXT("[TitleDolly] %s has no spline component, so the path can't be generated from it."), *PathSource->GetName());
		return;
	}

	const float SourceLength = Source->GetSplineLength();

	if (SourceLength <= 0.0f)
	{
		return;
	}

	const int32 NumSpans = FMath::Max(4, FMath::RoundToInt(SourceLength / FMath::Max(PathSampleSpacing, 1.0f)));

	// a source that ends where it began (like a circuit) makes a closed path the camera can lap forever
	const FVector SourceStart = Source->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
	const FVector SourceEnd = Source->GetLocationAtDistanceAlongSpline(SourceLength, ESplineCoordinateSpace::World);
	const bool bClosed = Source->IsClosedLoop() || FVector::Dist(SourceStart, SourceEnd) < PathSampleSpacing;

	// a closed path leaves out the end sample, which would sit on top of the first one
	const int32 NumPoints = bClosed ? NumSpans : NumSpans + 1;

	TArray<FVector> Points;
	Points.Reserve(NumPoints);

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		Points.Add(Source->GetLocationAtDistanceAlongSpline(SourceLength * Index / NumSpans, ESplineCoordinateSpace::World));
	}

	// average each point with its neighbors to round off hairpins the camera shouldn't follow
	for (int32 Pass = 0; Pass < PathSmoothingPasses; ++Pass)
	{
		TArray<FVector> Smoothed = Points;

		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			// the ends of an open path stay put
			if (!bClosed && (Index == 0 || Index == NumPoints - 1))
			{
				continue;
			}

			const FVector& Prev = Points[(Index - 1 + NumPoints) % NumPoints];
			const FVector& Next = Points[(Index + 1) % NumPoints];

			Smoothed[Index] = Prev * 0.25f + Points[Index] * 0.5f + Next * 0.25f;
		}

		Points = MoveTemp(Smoothed);
	}

	// lift the points into the air
	for (FVector& Point : Points)
	{
		Point.Z += PathHeight;
	}

	auto ApplyPoints = [&]()
	{
		Path->ClearSplinePoints(false);

		for (const FVector& Point : Points)
		{
			Path->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
		}

		Path->SetClosedLoop(bClosed, false);
		Path->UpdateSpline();
	};

	ApplyPoints();

	const int32 NumSegments = bClosed ? NumPoints : NumPoints - 1;
	const float CheckStep = FMath::Clamp(MinGroundClearance * 0.5f, 500.0f, 2000.0f);

	// a point may sit no lower than a neighbor minus the climb allowed over the span between them,
	// so the camera rises to obstacles early and comes down after them gently instead of bouncing
	auto LimitSlopes = [&]()
	{
		for (int32 Guard = 0; Guard < NumPoints * 2; ++Guard)
		{
			bool bChanged = false;

			for (int32 Segment = 0; Segment < NumSegments; ++Segment)
			{
				FVector& A = Points[Segment];
				FVector& B = Points[(Segment + 1) % NumPoints];

				const float MaxStep = MaxClimbSlope * FVector::Dist2D(A, B);

				if (A.Z < B.Z - MaxStep)
				{
					A.Z = B.Z - MaxStep;
					bChanged = true;
				}

				if (B.Z < A.Z - MaxStep)
				{
					B.Z = A.Z - MaxStep;
					bChanged = true;
				}
			}

			if (!bChanged)
			{
				break;
			}
		}
	};

	// how low the camera may go along each span: anything coming within MinGroundClearance of it, ground or
	// buildings, pushes it up. Only the heights change from here on, so the spans stay where they are.
	TArray<float> SpanMinHeights;
	SpanMinHeights.Init(-UE_BIG_NUMBER, NumSegments);

	for (int32 Segment = 0; Segment < NumSegments; ++Segment)
	{
		const float SegmentStart = Path->GetDistanceAlongSplineAtSplinePoint(Segment);
		const float SegmentEnd = Segment + 1 < NumPoints ? Path->GetDistanceAlongSplineAtSplinePoint(Segment + 1) : Path->GetSplineLength();

		for (float Distance = SegmentStart; Distance < SegmentEnd + CheckStep; Distance += CheckStep)
		{
			const FVector Location = Path->GetLocationAtDistanceAlongSpline(FMath::Min(Distance, SegmentEnd), ESplineCoordinateSpace::World);

			float GroundHeight = 0.0f;

			if (FindGroundHeight(Location, GroundHeight))
			{
				SpanMinHeights[Segment] = FMath::Max(SpanMinHeights[Segment], GroundHeight + MinGroundClearance);
			}
		}
	}

	// both ends of a span have to clear whatever is along it
	for (int32 Segment = 0; Segment < NumSegments; ++Segment)
	{
		FVector& A = Points[Segment];
		FVector& B = Points[(Segment + 1) % NumPoints];

		A.Z = FMath::Max(A.Z, SpanMinHeights[Segment]);
		B.Z = FMath::Max(B.Z, SpanMinHeights[Segment]);
	}

	LimitSlopes();

	// fill in dips between high points, which would otherwise read as the camera bobbing up and down
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		TArray<FVector> Filled = Points;

		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			if (!bClosed && (Index == 0 || Index == NumPoints - 1))
			{
				continue;
			}

			const float Average = Points[(Index - 1 + NumPoints) % NumPoints].Z * 0.25f + Points[Index].Z * 0.5f + Points[(Index + 1) % NumPoints].Z * 0.25f;
			Filled[Index].Z = FMath::Max(Points[Index].Z, Average);
		}

		Points = MoveTemp(Filled);
	}

	ApplyPoints();

	// the curve between two points can still sag into something: raise the ends of any span
	// that comes too close, and check again
	for (int32 Round = 0; Round < 4; ++Round)
	{
		bool bAnyRaised = false;

		for (int32 Segment = 0; Segment < NumSegments; ++Segment)
		{
			const float SegmentStart = Path->GetDistanceAlongSplineAtSplinePoint(Segment);
			const float SegmentEnd = Segment + 1 < NumPoints ? Path->GetDistanceAlongSplineAtSplinePoint(Segment + 1) : Path->GetSplineLength();

			float Deficit = 0.0f;

			for (float Distance = SegmentStart; Distance < SegmentEnd + CheckStep; Distance += CheckStep)
			{
				const FVector Location = Path->GetLocationAtDistanceAlongSpline(FMath::Min(Distance, SegmentEnd), ESplineCoordinateSpace::World);
				Deficit = FMath::Max(Deficit, SpanMinHeights[Segment] - Location.Z);
			}

			if (Deficit > 1.0f)
			{
				Points[Segment].Z += Deficit;
				Points[(Segment + 1) % NumPoints].Z += Deficit;
				bAnyRaised = true;
			}
		}

		if (!bAnyRaised)
		{
			break;
		}

		LimitSlopes();
		ApplyPoints();
	}
}

bool ATitleCameraDolly::FindGroundHeight(const FVector& Location, float& OutGroundHeight) const
{
	const UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	// start well above the path so bridges and buildings over it count too
	const FVector TraceStart = Location + FVector(0.0f, 0.0f, 50000.0f);
	const FVector TraceEnd = Location - FVector(0.0f, 0.0f, 200000.0f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TitleDollyGround), false, this);
	FHitResult Hit;

	// drop a ball as wide as the clearance, so a building beside the path keeps the camera away
	// as much as the ground under it does, not just whatever is straight below
	const float Radius = FMath::Max(MinGroundClearance, 1.0f);

	if (World->SweepSingleByObjectType(Hit, TraceStart, TraceEnd, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params))
	{
		// where the ball came to rest is the lowest the camera can go; report it as a height
		// MinGroundClearance below that, so callers add the clearance back on top
		OutGroundHeight = Hit.Location.Z - MinGroundClearance;
		return true;
	}

	return false;
}

float ATitleCameraDolly::WrapDistance(float Distance, float PathLength) const
{
	if (bLoop)
	{
		const float Wrapped = FMath::Fmod(Distance, PathLength);
		return Wrapped < 0.0f ? Wrapped + PathLength : Wrapped;
	}

	return FMath::Clamp(Distance, 0.0f, PathLength);
}

void ATitleCameraDolly::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float PathLength = Path->GetSplineLength();

	if (PathLength <= 0.0f)
	{
		return;
	}

	TravelDistance = WrapDistance(TravelDistance + Speed * DeltaSeconds, PathLength);

	const FVector Location = Path->GetLocationAtDistanceAlongSpline(TravelDistance, ESplineCoordinateSpace::World);

	// look at a point further along the path, dropped below it so the view tilts down onto the scenery
	const float AheadDistance = TravelDistance + LookAheadDistance;
	FVector Ahead;

	if (Path->IsClosedLoop())
	{
		Ahead = Path->GetLocationAtDistanceAlongSpline(FMath::Fmod(AheadDistance, PathLength), ESplineCoordinateSpace::World);

	} else if (AheadDistance <= PathLength) {

		Ahead = Path->GetLocationAtDistanceAlongSpline(AheadDistance, ESplineCoordinateSpace::World);

	} else {

		// past the end of an open path: carry on in the direction it ends in
		Ahead = Path->GetLocationAtDistanceAlongSpline(PathLength, ESplineCoordinateSpace::World)
			+ Path->GetDirectionAtDistanceAlongSpline(PathLength, ESplineCoordinateSpace::World) * (AheadDistance - PathLength);
	}

	Ahead.Z -= LookDownHeight;

	FRotator DesiredAim = (Ahead - Location).Rotation();
	DesiredAim.Roll = 0.0f;

	// ease into turns rather than snapping to every bend
	if (bAimInitialized && AimInterpSpeed > 0.0f)
	{
		AimRotation = FMath::RInterpTo(AimRotation, DesiredAim, DeltaSeconds, AimInterpSpeed);

	} else {

		AimRotation = DesiredAim;
		bAimInitialized = true;
	}

	Camera->SetWorldLocationAndRotation(Location, AimRotation);
}
