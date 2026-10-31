// Copyright (c) 2026 skykunking-gif. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USplineComponent;

/**
 *  A road the race line can follow: its center line spline, and the transform its road surface is laid out in.
 *  That isn't always the spline's own: the BP_SplineMesh roads lay their meshes out from the spline points in the
 *  actor's space, so a spline component moved within its actor leaves the road where it was.
 */
struct FRaceRoad
{
	/** Center line of the road */
	const USplineComponent* Spline = nullptr;

	/** Transform the road surface is laid out in, applied to the spline's local points */
	FTransform Transform;
};

/**
 *  The line racers follow around a race route: a closed polyline sampled at a fixed spacing.
 *  Built from the route's track gates in order, following the road splines between them, so it runs
 *  down the middle of the road. NPC drivers steer along it, and race positions are measured along it.
 */
struct FRaceLine
{
	/** Builds the line through the gates, in order and back round to the first. Between two gates it follows
	 *  whichever road spline joins them, and falls back to a smooth curve where no road does. */
	void Build(const TArray<FTransform>& Gates, const TArray<FRaceRoad>& Roads);

	/** Forgets the line */
	void Reset();

	/** True once the line has been built */
	bool IsValid() const { return Points.Num() >= 4; }

	/** Length of one lap of the line */
	float GetLength() const { return Length; }

	/** Distance between neighboring samples */
	float GetSpacing() const { return Spacing; }

	/** Number of samples */
	int32 Num() const { return Points.Num(); }

	/** Wraps a distance into one lap */
	float Wrap(float Distance) const;

	/** Shortest signed distance from one point on the line to another, going forwards when positive */
	float Delta(float From, float To) const;

	/** Point on the line at a distance */
	FVector GetLocation(float Distance) const;

	/** Direction the line runs in at a distance */
	FVector GetDirection(float Distance) const;

	/** Horizontal direction to the right of the line at a distance */
	FVector GetRight(float Distance) const;

	/** How sharply the line bends at a sample, in 1/cm. Positive bends to the right */
	float GetCurvatureAtSample(int32 Index) const { return Curvatures.IsValidIndex(Index) ? Curvatures[Index] : 0.0f; }

	/** Location of a sample */
	const FVector& GetSample(int32 Index) const { return Points[Index]; }

	/** Distance along the line closest to a location, searching only within Window of Hint.
	 *  Keeps a racer from jumping to another part of the line where the road passes close to itself */
	float FindDistanceNear(const FVector& Location, float Hint, float Window) const;

	/** Distance along the line closest to a location, searching the whole line */
	float FindDistance(const FVector& Location) const;

private:

	/** Distance along the segment starting at sample Index that is closest to Location */
	float ProjectOntoSegment(int32 Index, const FVector& Location, float& OutDistSquared) const;

	/** Samples, evenly spaced */
	TArray<FVector> Points;

	/** Signed horizontal curvature at each sample */
	TArray<float> Curvatures;

	/** Distance between samples */
	float Spacing = 200.0f;

	/** Length of one lap */
	float Length = 0.0f;
};
