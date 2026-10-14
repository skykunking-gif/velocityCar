// Copyright Epic Games, Inc. All Rights Reserved.


#include "RaceLine.h"
#include "Components/SplineComponent.h"
#include "MyProject.h"

namespace
{
	/** Step taken when walking along the road splines */
	constexpr float WalkStep = 200.0f;

	/** A gate further than this from a road isn't on it */
	constexpr float MaxGateToRoad = 1500.0f;

	/** A walk along a road has reached the next gate once it comes this close */
	constexpr float ReachRadius = 1500.0f;

	/** Length of the curve that smooths over the switch from one road to another */
	constexpr float BlendLength = 2000.0f;

	/** Samples either side of a point used to measure how sharply the line bends there */
	constexpr int32 CurvatureSpan = 3;

	FVector Hermite(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float S)
	{
		const float S2 = S * S;
		const float S3 = S2 * S;

		return P0 * (2.0f * S3 - 3.0f * S2 + 1.0f) + T0 * (S3 - 2.0f * S2 + S) + P1 * (-2.0f * S3 + 3.0f * S2) + T1 * (S3 - S2);
	}

	/** Appends a smooth curve from From to To, leaving and arriving along the given directions. To itself is left out. */
	void AppendCurve(const FVector& From, const FVector& FromDir, const FVector& To, const FVector& ToDir, TArray<FVector>& OutPoints)
	{
		const float Chord = FVector::Dist(From, To);
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(Chord / WalkStep));

		for (int32 Step = 0; Step < Steps; ++Step)
		{
			OutPoints.Add(Hermite(From, FromDir * Chord, To, ToDir * Chord, static_cast<float>(Step) / Steps));
		}
	}

	/** Point on a road's center line at a distance along it */
	FVector GetRoadLocation(const FRaceRoad& Road, float Distance)
	{
		return Road.Transform.TransformPosition(Road.Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local));
	}

	/** Distance along a road's center line closest to a location */
	float FindRoadDistance(const FRaceRoad& Road, const FVector& Location)
	{
		// the spline only searches in its own space, so carry the location over from the road's
		const FVector SplineWorldLocation = Road.Spline->GetComponentTransform().TransformPosition(Road.Transform.InverseTransformPosition(Location));

		return Road.Spline->GetDistanceAlongSplineAtSplineInputKey(Road.Spline->FindInputKeyClosestToWorldLocation(SplineWorldLocation));
	}

	/** Appends points along whichever road runs from From to To, stopping short of To. Returns false if no road does. */
	bool FollowRoad(const FVector& From, const FVector& To, const TArray<FRaceRoad>& Roads, TArray<FVector>& OutPoints)
	{
		const float Chord = FVector::Dist(From, To);
		const float MaxWalk = FMath::Max(Chord * 4.0f, Chord + 20000.0f);

		const FRaceRoad* BestRoad = nullptr;
		float BestStart = 0.0f;
		float BestWalk = 0.0f;
		float BestScore = TNumericLimits<float>::Max();
		int32 BestDirection = 1;

		for (const FRaceRoad& Road : Roads)
		{
			const float RoadLength = Road.Spline ? Road.Spline->GetSplineLength() : 0.0f;

			if (RoadLength <= 0.0f)
			{
				continue;
			}

			const float StartDistance = FindRoadDistance(Road, From);

			if (FVector::Dist(GetRoadLocation(Road, StartDistance), From) > MaxGateToRoad)
			{
				continue;
			}

			// a road whose ends meet (like a circuit) can be walked round past its seam
			const bool bLoops = Road.Spline->IsClosedLoop() || FVector::Dist(GetRoadLocation(Road, 0.0f), GetRoadLocation(Road, RoadLength)) < 1000.0f;

			// the route may run either way along the road
			for (const int32 Direction : { 1, -1 })
			{
				float Closest = TNumericLimits<float>::Max();
				float ClosestWalk = 0.0f;

				for (float Walked = WalkStep; Walked <= MaxWalk; Walked += WalkStep)
				{
					float Distance = StartDistance + Direction * Walked;

					if (bLoops)
					{
						Distance = FMath::Fmod(Distance + RoadLength, RoadLength);

					} else if (Distance < 0.0f || Distance > RoadLength) {

						break;
					}

					const float ToTarget = FVector::Dist(GetRoadLocation(Road, Distance), To);

					if (ToTarget < Closest)
					{
						Closest = ToTarget;
						ClosestWalk = Walked;

					} else if (Closest < ReachRadius && ToTarget > Closest + ReachRadius) {

						// went past the gate: the first approach is the one that counts
						break;
					}
				}

				// prefer the shorter way, and the road that actually runs through the gate where two roads part
				const float Score = ClosestWalk + Closest * 4.0f;

				if (Closest < ReachRadius && Score < BestScore)
				{
					BestRoad = &Road;
					BestStart = StartDistance;
					BestWalk = ClosestWalk;
					BestScore = Score;
					BestDirection = Direction;
				}
			}
		}

		if (!BestRoad)
		{
			return false;
		}

		const float RoadLength = BestRoad->Spline->GetSplineLength();

		for (float Walked = 0.0f; Walked < BestWalk; Walked += WalkStep)
		{
			OutPoints.Add(GetRoadLocation(*BestRoad, FMath::Fmod(BestStart + BestDirection * Walked + RoadLength, RoadLength)));
		}

		return true;
	}

	/** Joins a piece onto the line, bridging any jump where the piece switches to another road with a curve */
	void AppendPiece(TArray<FVector>& Line, const TArray<FVector>& Piece)
	{
		if (Line.Num() < 2 || Piece.Num() < 2 || FVector::Dist(Line.Last(), Piece[0]) <= WalkStep * 1.5f)
		{
			Line.Append(Piece);
			return;
		}

		// back off from the jump on both sides and join the two with a curve
		const int32 Trim = FMath::Max(0, FMath::Min(FMath::RoundToInt(BlendLength * 0.5f / WalkStep), FMath::Min(Line.Num(), Piece.Num()) - 2));

		Line.SetNum(Line.Num() - Trim);

		const FVector From = Line.Last();
		const FVector FromDir = (Line.Last() - Line[Line.Num() - 2]).GetSafeNormal();
		const FVector To = Piece[Trim];
		const FVector ToDir = (Piece[Trim + 1] - Piece[Trim]).GetSafeNormal();

		Line.Pop();
		AppendCurve(From, FromDir, To, ToDir, Line);
		Line.Append(&Piece[Trim], Piece.Num() - Trim);
	}

	/** Resamples a closed polyline at an even spacing as close to Spacing as fits a whole number of samples */
	void ResampleClosed(TArray<FVector>& Points, float& InOutSpacing, float& OutLength)
	{
		const int32 Count = Points.Num();

		TArray<float> Cumulative;
		Cumulative.SetNum(Count + 1);
		Cumulative[0] = 0.0f;

		for (int32 Index = 0; Index < Count; ++Index)
		{
			Cumulative[Index + 1] = Cumulative[Index] + FVector::Dist(Points[Index], Points[(Index + 1) % Count]);
		}

		OutLength = Cumulative[Count];

		const int32 NewCount = FMath::Max(4, FMath::RoundToInt(OutLength / InOutSpacing));
		InOutSpacing = OutLength / NewCount;

		TArray<FVector> Resampled;
		Resampled.Reserve(NewCount);

		int32 Segment = 0;

		for (int32 Sample = 0; Sample < NewCount; ++Sample)
		{
			const float Target = Sample * InOutSpacing;

			while (Segment < Count - 1 && Cumulative[Segment + 1] < Target)
			{
				++Segment;
			}

			const float SegmentLength = Cumulative[Segment + 1] - Cumulative[Segment];
			const float Alpha = SegmentLength > KINDA_SMALL_NUMBER ? (Target - Cumulative[Segment]) / SegmentLength : 0.0f;

			Resampled.Add(FMath::Lerp(Points[Segment], Points[(Segment + 1) % Count], FMath::Clamp(Alpha, 0.0f, 1.0f)));
		}

		Points = MoveTemp(Resampled);
	}
}

void FRaceLine::Build(const TArray<FTransform>& Gates, const TArray<FRaceRoad>& Roads)
{
	Reset();

	if (Gates.Num() < 2)
	{
		return;
	}

	TArray<FVector> Line;
	int32 CurvedPieces = 0;

	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		const FVector From = Gates[Index].GetLocation();
		const FVector To = Gates[(Index + 1) % Gates.Num()].GetLocation();

		TArray<FVector> Piece;

		if (!FollowRoad(From, To, Roads, Piece))
		{
			// no road between these gates: leave and arrive along the gates, whichever way they face
			const FVector Chord = To - From;
			FVector FromDir = Gates[Index].GetRotation().GetForwardVector();
			FVector ToDir = Gates[(Index + 1) % Gates.Num()].GetRotation().GetForwardVector();

			FromDir *= FMath::Sign(FVector::DotProduct(FromDir, Chord) + KINDA_SMALL_NUMBER);
			ToDir *= FMath::Sign(FVector::DotProduct(ToDir, Chord) + KINDA_SMALL_NUMBER);

			AppendCurve(From, FromDir, To, ToDir, Piece);
			++CurvedPieces;
		}

		AppendPiece(Line, Piece);
	}

	// close the loop, bridging the seam the same way if the last piece doesn't run into the first
	const int32 Trim = FMath::RoundToInt(BlendLength * 0.5f / WalkStep);

	if (Line.Num() > Trim * 4 && FVector::Dist(Line.Last(), Line[0]) > WalkStep * 1.5f)
	{
		Line.RemoveAt(0, Trim);
		Line.SetNum(Line.Num() - Trim);

		const FVector From = Line.Last();
		const FVector FromDir = (Line.Last() - Line[Line.Num() - 2]).GetSafeNormal();
		const FVector To = Line[0];
		const FVector ToDir = (Line[1] - Line[0]).GetSafeNormal();

		Line.Pop();
		AppendCurve(From, FromDir, To, ToDir, Line);
	}

	if (Line.Num() < 4)
	{
		return;
	}

	// even spacing, then a light smoothing to take out the corners where pieces join
	ResampleClosed(Line, Spacing, Length);

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		TArray<FVector> Smoothed = Line;
		const int32 Count = Line.Num();

		for (int32 Index = 0; Index < Count; ++Index)
		{
			Smoothed[Index] = Line[(Index - 1 + Count) % Count] * 0.25f + Line[Index] * 0.5f + Line[(Index + 1) % Count] * 0.25f;
		}

		Line = MoveTemp(Smoothed);
	}

	ResampleClosed(Line, Spacing, Length);

	Points = MoveTemp(Line);

	// how sharply the line turns at each sample, measured across a few samples either side
	const int32 Count = Points.Num();
	Curvatures.SetNumZeroed(Count);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Before = (Points[Index] - Points[(Index - CurvatureSpan + Count) % Count]).GetSafeNormal2D();
		const FVector After = (Points[(Index + CurvatureSpan) % Count] - Points[Index]).GetSafeNormal2D();

		const float Turn = FMath::Atan2(Before.X * After.Y - Before.Y * After.X, FVector::DotProduct(Before, After));
		Curvatures[Index] = Turn / (CurvatureSpan * Spacing);
	}

	UE_LOG(LogMyProject, Log, TEXT("[RaceLine] Built through %d gates: %.0f cm in %d samples, %d stretches without a road"),
		Gates.Num(), Length, Count, CurvedPieces);
}

void FRaceLine::Reset()
{
	Points.Reset();
	Curvatures.Reset();
	Length = 0.0f;
	Spacing = 200.0f;
}

float FRaceLine::Wrap(float Distance) const
{
	if (Length <= 0.0f)
	{
		return 0.0f;
	}

	const float Wrapped = FMath::Fmod(Distance, Length);
	return Wrapped < 0.0f ? Wrapped + Length : Wrapped;
}

float FRaceLine::Delta(float From, float To) const
{
	const float Forward = Wrap(To - From);
	return Forward > Length * 0.5f ? Forward - Length : Forward;
}

FVector FRaceLine::GetLocation(float Distance) const
{
	if (!IsValid())
	{
		return FVector::ZeroVector;
	}

	const float Samples = Wrap(Distance) / Spacing;
	const int32 Index = FMath::FloorToInt(Samples) % Points.Num();

	return FMath::Lerp(Points[Index], Points[(Index + 1) % Points.Num()], Samples - FMath::FloorToFloat(Samples));
}

FVector FRaceLine::GetDirection(float Distance) const
{
	if (!IsValid())
	{
		return FVector::ForwardVector;
	}

	const int32 Index = FMath::FloorToInt(Wrap(Distance) / Spacing) % Points.Num();

	return (Points[(Index + 1) % Points.Num()] - Points[Index]).GetSafeNormal();
}

FVector FRaceLine::GetRight(float Distance) const
{
	return FVector::CrossProduct(FVector::UpVector, GetDirection(Distance)).GetSafeNormal();
}

float FRaceLine::ProjectOntoSegment(int32 Index, const FVector& Location, float& OutDistSquared) const
{
	const FVector& A = Points[Index];
	const FVector& B = Points[(Index + 1) % Points.Num()];
	const FVector Segment = B - A;

	const float Alpha = FMath::Clamp(FVector::DotProduct(Location - A, Segment) / FMath::Max(Segment.SizeSquared(), KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	OutDistSquared = FVector::DistSquared(Location, A + Segment * Alpha);

	return (Index + Alpha) * Spacing;
}

float FRaceLine::FindDistanceNear(const FVector& Location, float Hint, float Window) const
{
	if (!IsValid())
	{
		return 0.0f;
	}

	const int32 First = FMath::FloorToInt((Hint - Window) / Spacing);
	const int32 Last = FMath::CeilToInt((Hint + Window) / Spacing);
	const int32 Count = Points.Num();

	float BestDistance = Hint;
	float BestDistSquared = TNumericLimits<float>::Max();

	for (int32 Step = First; Step <= Last; ++Step)
	{
		const int32 Index = ((Step % Count) + Count) % Count;

		float DistSquared = 0.0f;
		const float Distance = ProjectOntoSegment(Index, Location, DistSquared);

		if (DistSquared < BestDistSquared)
		{
			BestDistSquared = DistSquared;
			BestDistance = Distance;
		}
	}

	return Wrap(BestDistance);
}

float FRaceLine::FindDistance(const FVector& Location) const
{
	if (!IsValid())
	{
		return 0.0f;
	}

	float BestDistance = 0.0f;
	float BestDistSquared = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		float DistSquared = 0.0f;
		const float Distance = ProjectOntoSegment(Index, Location, DistSquared);

		if (DistSquared < BestDistSquared)
		{
			BestDistSquared = DistSquared;
			BestDistance = Distance;
		}
	}

	return Wrap(BestDistance);
}
