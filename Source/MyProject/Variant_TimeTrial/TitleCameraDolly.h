// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitleCameraDolly.generated.h"

class USplineComponent;
class UCineCameraComponent;

/**
 *  A camera that flies along a spline path, used for the title screen flythrough.
 *
 *  The path is either the Path spline as edited in the level, or, when PathSource is set, generated from
 *  that actor's spline (such as a road): sampled, smoothed, lifted into the air and kept clear of the ground.
 *  The camera keeps looking a little way ahead and down along the path, so it glides over the scenery.
 */
UCLASS()
class ATitleCameraDolly : public AActor
{
	GENERATED_BODY()

	/** Path the camera follows */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USplineComponent* Path;

	/** The camera used for the flythrough */
	UPROPERTY(VisibleAnywhere, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCineCameraComponent* Camera;

protected:

	/** Actor with a spline (such as a road) to generate the path from. Leave empty to fly along Path as edited. */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path")
	TObjectPtr<AActor> PathSource;

	/** Distance between the points sampled from the source spline. Larger values smooth out more of its bends. */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path", meta = (Units = "cm", ClampMin = "500"))
	float PathSampleSpacing = 6000.0f;

	/** How many times the sampled points are averaged with their neighbors */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path", meta = (ClampMin = "0"))
	int32 PathSmoothingPasses = 2;

	/** Height of the path above the source spline */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path", meta = (Units = "cm"))
	float PathHeight = 5000.0f;

	/** The path is raised wherever it would come closer than this to the ground, or to anything else static such as buildings */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path", meta = (Units = "cm", ClampMin = "0"))
	float MinGroundClearance = 4000.0f;

	/** Steepest climb or descent of the generated path, as height gained per horizontal distance. Lower values
	 *  start climbing over tall buildings earlier and come down after them more gently. */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Generated Path", meta = (ClampMin = "0.01"))
	float MaxClimbSlope = 0.3f;

	/** How fast the camera moves along the path */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly", meta = (Units = "cm/s", ClampMin = "0"))
	float Speed = 3000.0f;

	/** Where along the path the camera starts */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly", meta = (Units = "cm"))
	float StartDistance = 0.0f;

	/** If true, the camera loops back to the start of the path when it reaches the end */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly")
	bool bLoop = true;

	/** How far ahead along the path the camera looks */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Aim", meta = (Units = "cm", ClampMin = "1"))
	float LookAheadDistance = 9000.0f;

	/** How far below the path the camera looks, at the look ahead point. Higher values tilt the view down more. */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Aim", meta = (Units = "cm"))
	float LookDownHeight = 4000.0f;

	/** How quickly the camera turns towards where it's looking. Lower values give lazier, smoother turns. */
	UPROPERTY(EditAnywhere, Category = "Title Camera Dolly|Aim", meta = (ClampMin = "0"))
	float AimInterpSpeed = 1.2f;

	/** Distance travelled along the path so far */
	float TravelDistance = 0.0f;

	/** Current camera rotation, eased towards the look ahead point */
	FRotator AimRotation = FRotator::ZeroRotator;

	/** False until the first tick has placed the camera, so it starts already facing the right way */
	bool bAimInitialized = false;

public:

	/** Constructor */
	ATitleCameraDolly();

protected:

	/** Previews the generated path in the editor */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Generates the path, now that the level's collision is in place for the ground checks */
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Rebuilds Path from PathSource's spline. Does nothing without a source. */
	void BuildPathFromSource();

	/** Returns the height of the highest static geometry under or within MinGroundClearance of a point, or false if there's none */
	bool FindGroundHeight(const FVector& Location, float& OutGroundHeight) const;

	/** Wraps or clamps a distance along the path, depending on bLoop */
	float WrapDistance(float Distance, float PathLength) const;

public:

	/** Returns the camera component used for the flythrough */
	UCineCameraComponent* GetCamera() const { return Camera; };
};
