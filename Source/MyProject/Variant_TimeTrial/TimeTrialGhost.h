// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "TimeTrialGhost.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;

/**
 *  A recorded race: where the car was, at even steps of time from the start
 */
USTRUCT(BlueprintType)
struct FGhostTrack
{
	GENERATED_BODY()

	/** Time between samples, in seconds */
	UPROPERTY()
	float SampleInterval = 1.0f / 30.0f;

	/** Location of the car at each sample. Sample N was taken N * SampleInterval after the start */
	UPROPERTY()
	TArray<FVector3f> Locations;

	/** Rotation of the car at each sample */
	UPROPERTY()
	TArray<FQuat4f> Rotations;

	/** Time the race took from start to finish, in seconds */
	UPROPERTY(BlueprintReadOnly, Category="Ghost")
	float TotalTime = -1.0f;

	/** Name of the car the race was run in, as the car select screen showed it */
	UPROPERTY(BlueprintReadOnly, Category="Ghost")
	FString CarName;

	/** True if there's anything to play back */
	bool IsValid() const { return Locations.Num() > 0 && Locations.Num() == Rotations.Num() && SampleInterval > 0.0f; }

	/** Time of the last sample */
	float GetDuration() const { return FMath::Max(Locations.Num() - 1, 0) * SampleInterval; }

	/** Where the car was a time after the start, between the samples either side of it */
	FTransform Sample(float Time) const;
};

/**
 *  Records a car's run into a ghost track. Feed it every frame; it works out where the car was at each sample time
 *  from the frames either side, so the samples are evenly spaced whatever the frame rate
 */
struct FGhostRecorder
{
	/** Starts a new track, with the car where it is at the start */
	void Start(float SampleInterval, const FTransform& StartTransform);

	/** Adds a frame, Time seconds after the start */
	void AddFrame(float Time, const FTransform& Transform);

	/** Returns true between Start and Stop */
	bool IsRecording() const { return bRecording; }

	/** Stops recording. The track stays until the next Start */
	void Stop() { bRecording = false; }

	/** The track recorded so far */
	FGhostTrack Track;

private:

	/** True between Start and Stop */
	bool bRecording = false;

	/** Time and transform of the last frame */
	float LastTime = 0.0f;
	FTransform LastTransform;

	/** Time the next sample falls due */
	float NextSampleTime = 0.0f;

	/** Appends a sample */
	void AddSample(const FVector& Location, const FQuat& Rotation);
};

/**
 *  What gets written to a ghost's save file. Each course and lap count has a file of its own, so the records
 *  file stays small and only the ghost that's needed gets loaded
 */
UCLASS()
class UTimeTrialGhostSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** Version of the file layout, for converting old files should the layout change */
	UPROPERTY()
	int32 SaveVersion = 1;

	/** The recorded race */
	UPROPERTY()
	FGhostTrack Track;
};

/**
 *  A see-through car that drives a recorded race. It holds at the start until the race starts, then follows the track
 *  and disappears once it has run the whole race. It's only for show: nothing collides with it.
 */
UCLASS()
class ATimeTrialGhostCar : public AActor
{
	GENERATED_BODY()

	/** Shows the car when it has no body of its own to wear */
	UPROPERTY(VisibleAnywhere, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* FallbackMesh;

public:

	ATimeTrialGhostCar();

	/** Gives the ghost the race to drive and the looks to drive it in. BodyClass is a car body actor as the car
	 *  choices use; without one the ghost shows FallbackMeshAsset instead. Every mesh is drawn in Material, a
	 *  translucent material whose "Glass Color" and "Base Glass Opacity" parameters are set from Color and Opacity */
	void SetUp(const FGhostTrack& InTrack, TSubclassOf<AActor> BodyClass, USkeletalMesh* FallbackMeshAsset,
		UMaterialInterface* Material, const FLinearColor& Color, float Opacity);

	/** Sets the ghost off, timing the race from StartTime in game time */
	void StartPlayback(float StartTime);

	/** Moves along the track */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Takes the body along with the ghost */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Makes every mesh of an actor see-through and harmless: no collision, no shadow, no lights */
	void MakeGhostly(AActor* Actor, UMaterialInterface* Material, const FLinearColor& Color, float Opacity);

	/** The race being driven */
	FGhostTrack Track;

	/** The body the ghost wears, attached to it */
	UPROPERTY(Transient)
	TObjectPtr<AActor> BodyActor;

	/** Game time the race started at */
	float PlaybackStartTime = 0.0f;

	/** True once the race has started */
	bool bPlaying = false;
};
