// Copyright Epic Games, Inc. All Rights Reserved.


#include "MyProjectSportsWheelRear.h"

UMyProjectSportsWheelRear::UMyProjectSportsWheelRear()
{
	WheelRadius = 40.f;
	WheelWidth = 40.0f;

	// stiff damping so the body settles in one movement instead of wallowing
	SuspensionDampingRatio = 0.8f;
	FrictionForceMultiplier = 4.0f;
	SlipThreshold = 100.0f;
	SkidThreshold = 100.0f;
	MaxSteerAngle = 0.0f;
	MaxHandBrakeTorque = 6000.0f;
}