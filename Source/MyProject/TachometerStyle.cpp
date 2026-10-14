// Copyright Epic Games, Inc. All Rights Reserved.


#include "TachometerStyle.h"

float UTachometerStyle::GetNeedleAngle(float RPM) const
{
	if (NeedleAngles.IsEmpty())
	{
		return 0.0f;
	}

	if (RPM <= NeedleAngles[0].X)
	{
		return static_cast<float>(NeedleAngles[0].Y);
	}

	// find the pair of points the engine speed falls between
	for (int32 Index = 1; Index < NeedleAngles.Num(); ++Index)
	{
		const FVector2D& Low = NeedleAngles[Index - 1];
		const FVector2D& High = NeedleAngles[Index];

		if (RPM <= High.X)
		{
			const double Alpha = (RPM - Low.X) / FMath::Max(High.X - Low.X, UE_KINDA_SMALL_NUMBER);
			return static_cast<float>(FMath::Lerp(Low.Y, High.Y, Alpha));
		}
	}

	return static_cast<float>(NeedleAngles.Last().Y);
}

float UTachometerStyle::GetBarFillX(float RPM) const
{
	const float Steps = FMath::Max(RPM, 0.0f) / FMath::Max(BarRPMPerSegment, 1.0f);

	// whole segments: the first lights at 0 RPM, so there's always one on
	const float Lit = BarSegments > 0 ? FMath::Min(FMath::FloorToFloat(Steps) + 1.0f, static_cast<float>(BarSegments)) : Steps;

	return BarStartX + (Lit * BarSegmentWidth);
}
